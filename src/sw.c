#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <termios.h>
#include <sys/stat.h>

#define NSEC_TO_SLEEP 10000000
#define HIDE_CURSOR printf("\e[?25l")
#define SHOW_CURSOR printf("\e[?25h")

#define OVER_HOUR_TEMPLATE "%2d:%02d:%02d.%02d"
#define UNDER_HOUR_TEMPLATE "%02d:%02d.%02d"

volatile sig_atomic_t timing = 1;

const struct timespec sleepduration = {0, NSEC_TO_SLEEP};

struct timespec starttime;
struct timespec currenttime;
struct timespec elapsedtime;
struct timespec restored_time;
struct timespec resumedtime;
struct timespec pausedtime;

char endchar = '\r';

int rflag = 0;
int sflag = 0;

void set_canonical_mode(int enable)
{
  struct termios term_settings;
  tcgetattr(STDIN_FILENO, &term_settings);

  if (enable)
    term_settings.c_lflag |= ICANON | ECHO;
  else
    term_settings.c_lflag &= ~(ICANON | ECHO);

  tcsetattr(STDIN_FILENO, TCSANOW, &term_settings);
}

void print_time(FILE *fd)
{
  if (elapsedtime.tv_sec >= 3600)
  {
    fprintf(fd, OVER_HOUR_TEMPLATE,
        (int)(elapsedtime.tv_sec / 3600),
        (int)(elapsedtime.tv_sec % 3600 / 60),
        (int)(elapsedtime.tv_sec % 60),
        (int)(elapsedtime.tv_nsec / 10000000));
  }
  else
  {
    fprintf(fd, UNDER_HOUR_TEMPLATE,
        (int)(elapsedtime.tv_sec / 60),
        (int)(elapsedtime.tv_sec % 60),
        (int)(elapsedtime.tv_nsec / 10000000));
  }
  fprintf(fd, "%c", endchar);
  fflush(stdout);
}

void clear_output()
{
  printf("\r");
  fflush(stdout);
}

FILE *get_saved_time_file(char *mode)
{
  FILE *savedtimef = NULL;
  char file[256];
  char *homedirectory = getenv("HOME");
  if (homedirectory != NULL)
  {
    strcat(strncpy(file, getenv("HOME"), 256), "/.sw");
    struct stat sb;
    if (stat(file, &sb) == 0 && S_ISDIR(sb.st_mode))
    {
      strcat(file, "/savedtime");
      savedtimef = fopen(file, mode);
    }
    else if (strncmp(mode, "w", 1) == 0)
    {
      mkdir(file, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
      strcat(file, "/savedtime");
      savedtimef = fopen(file, mode);
    }
  }
  return savedtimef;
}

void save_time()
{
  FILE *savedtimef = get_saved_time_file("w");
  if (savedtimef == NULL)
  {
    perror("Could not save to file");
    exit(1);
  }
  print_time(savedtimef);
}

void restore_time()
{
  int hours = 0;
  int minutes = 0;
  int seconds = 0;
  int centiseconds = 0;
  long nanoseconds = 0;

  FILE *savedtimef = get_saved_time_file("r");

  if (savedtimef == NULL)
  {
    restored_time.tv_sec = 0;
    restored_time.tv_nsec = 0;
    return;
  }

  int bufsize = 32;
  char *buf = malloc(bufsize * sizeof(char));
  if (buf == NULL)
  {
    perror("Could not allocate memory to restore previous time.\n");
  }
  memset(buf, 0, bufsize);

  fgets(buf, bufsize, savedtimef);

  if (strlen(buf) < 9)
  {
    perror("Restore file unparsable.");
  }
  else if (strlen(buf) == 9)
  {
    sscanf(buf, UNDER_HOUR_TEMPLATE, &minutes, &seconds, &centiseconds);
    restored_time.tv_sec = (minutes * 60) + seconds;
    restored_time.tv_nsec = (centiseconds * 10000000);
  }
  else
  {
    sscanf(buf, OVER_HOUR_TEMPLATE, &hours, &minutes, &seconds, &centiseconds);
    restored_time.tv_sec = (hours * 60 * 60) + (minutes * 60) + seconds;
    restored_time.tv_nsec = (centiseconds * 10000000);
  }
}

void cleanup() {
  timing = 0;
  endchar = '\n';
  clear_output();
  print_time(stdout);
  SHOW_CURSOR;
  set_canonical_mode(1);
  if (sflag)
  {
    save_time();
  }
}

void sigint_handler(int sig)
{
  signal(sig, sigint_handler);
}

void* input_thread(void* arg)
{
  char c;
  set_canonical_mode(0);
  while ((c = getchar()) != EOF)
  {
    switch (c) {
      case ' ':
        // pause or resume stopwatch
        timing = !timing;
        if (!timing) {
          clock_gettime(CLOCK_MONOTONIC, &pausedtime);
        }
        if (timing) {
          clock_gettime(CLOCK_MONOTONIC, &resumedtime);
          starttime.tv_sec += resumedtime.tv_sec - pausedtime.tv_sec;
          starttime.tv_nsec += resumedtime.tv_nsec - pausedtime.tv_nsec;
          if (starttime.tv_nsec < 0)
          {
            starttime.tv_nsec += 1000000000;
            --starttime.tv_sec;
          } else if (starttime.tv_nsec >= 1000000000)
          {
            starttime.tv_nsec -= 1000000000;
            ++starttime.tv_sec;
          }
        }
        break;
      case 's':
        save_time();
        break;
      case 'q':
        // quit
        cleanup();
        exit(0);
        break;
      default:
        break;
    }
  }
  return NULL;
}

int main(int argc, char *argv[])
{
  int c;
  static struct option long_options[] = {
    {"restore", no_argument, 0, 'r'},
    {"save", no_argument, 0, 's'}};
  int option_index = 0;

  while ((c = getopt_long(argc, argv, "sr", long_options, &option_index)) != -1)
  {
    switch (c)
    {
      case 'r':
        rflag = 1;
        restore_time();
        break;
      case 's':
        sflag = 1;
        break;
      case '?':
        break;
      default:
        exit(1);
    }
  }
  signal(SIGINT, sigint_handler);
  pthread_t tid;
  pthread_create(&tid, NULL, input_thread, NULL);
  HIDE_CURSOR;
  clock_gettime(CLOCK_MONOTONIC, &starttime);
  if (rflag)
  {
    starttime.tv_sec -= restored_time.tv_sec;
    starttime.tv_nsec -= restored_time.tv_nsec;
    if (starttime.tv_nsec < 0)
    {
      starttime.tv_nsec += 1000000000;
      --starttime.tv_sec;
    }
  }
  while (1) {
    while (timing)
    {
      clock_gettime(CLOCK_MONOTONIC, &currenttime);
      elapsedtime.tv_sec = currenttime.tv_sec - starttime.tv_sec;
      elapsedtime.tv_nsec = currenttime.tv_nsec - starttime.tv_nsec;
      if (elapsedtime.tv_nsec < 0)
      {
        elapsedtime.tv_nsec += 1000000000;
        --elapsedtime.tv_sec;
      }
      print_time(stdout);
      nanosleep(&sleepduration, NULL);
    }
  }
  return 0;
}
