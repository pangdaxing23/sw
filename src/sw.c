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
#include "sw.h"

#define NSEC_TO_SLEEP 10000000
#define HIDE_CURSOR printf("\e[?25l")
#define SHOW_CURSOR printf("\e[?25h")

#define OVER_HOUR_TEMPLATE "%2d:%02d:%02d.%02d"
#define UNDER_HOUR_TEMPLATE "%02d:%02d.%02d"

volatile sig_atomic_t paused = 0;

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
int xflag = 0;

static char *program_name;

void set_raw_mode(int enable)
{
  struct termios term_settings;
  tcgetattr(STDIN_FILENO, &term_settings);

  if (enable)
    term_settings.c_lflag &= ~(ICANON | ECHO);
  else
    term_settings.c_lflag |= ICANON | ECHO;

  tcsetattr(STDIN_FILENO, TCSANOW, &term_settings);
}

void print_time(FILE *fd)
{
  if (elapsedtime.tv_sec < 3600)
  {
    fprintf(fd, UNDER_HOUR_TEMPLATE,
        (int)(elapsedtime.tv_sec / 60),
        (int)(elapsedtime.tv_sec % 60),
        (int)(elapsedtime.tv_nsec / 10000000));
  }
  else
  {
    fprintf(fd, OVER_HOUR_TEMPLATE,
        (int)(elapsedtime.tv_sec / 3600),
        (int)(elapsedtime.tv_sec % 3600 / 60),
        (int)(elapsedtime.tv_sec % 60),
        (int)(elapsedtime.tv_nsec / 10000000));
  }
  fprintf(fd, "%c", endchar);
  fflush(stdout);
}

void resume_timer() {
  clock_gettime(CLOCK_MONOTONIC, &pausedtime);
  if (xflag) {
    cleanup();
    exit(0);
  }
}

void pause_timer() {
  clock_gettime(CLOCK_MONOTONIC, &resumedtime);
  starttime.tv_sec += resumedtime.tv_sec - pausedtime.tv_sec;
  starttime.tv_nsec += resumedtime.tv_nsec - pausedtime.tv_nsec;
  if (starttime.tv_nsec < 0)
  {
    starttime.tv_nsec += 1000000000;
    --starttime.tv_sec;
  }
  else if (starttime.tv_nsec >= 1000000000)
  {
    starttime.tv_nsec -= 1000000000;
    ++starttime.tv_sec;
  }
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
    strncpy(file, getenv("HOME"), 255);
    file[255] = '\0';
    strcat(file, "/.sw");
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
  starttime.tv_sec -= restored_time.tv_sec;
  starttime.tv_nsec -= restored_time.tv_nsec;
  if (starttime.tv_nsec < 0)
  {
    starttime.tv_nsec += 1000000000;
    --starttime.tv_sec;
  }
  free(buf);
}

void reset_time()
{
  clock_gettime(CLOCK_MONOTONIC, &starttime);
  if (paused)
  {
    clock_gettime(CLOCK_MONOTONIC, &pausedtime);
    elapsedtime.tv_sec = 0;
    elapsedtime.tv_nsec = 0;
    print_time(stdout);
  }
}

void cleanup()
{
  paused = 1;
  endchar = '\n';
  clear_output();
  print_time(stdout);
  SHOW_CURSOR;
  set_raw_mode(0);
  if (sflag)
  {
    save_time();
  }
}

void sigint_handler(int sig)
{
  cleanup();
  exit(sig);
}

void* input_thread()
{
  char c;
  set_raw_mode(1);
  while ((c = getchar()) != EOF)
  {
    switch (c)
    {
      case ' ':
        // pause or resume stopwatch
        paused = !paused;
        if (paused)
        {
          resume_timer();
        }
        else
        {
          pause_timer();
        }
        break;
      case 's':
        save_time();
        break;
      case 'r':
        reset_time();
        break;
      case 'q':
        // quit
        cleanup();
        exit(0);
        break;
      default:
        break;
    }
    nanosleep(&sleepduration, NULL);
  }
  return NULL;
}

void print_help(FILE *out)
{
  fprintf(out, "Usage: %s [-hsr]\n", program_name);
  fprintf(out, "\nOptions:\n");
  fprintf(out, "  -h, --help    Show this help message and exit.\n");
  fprintf(out, "  -s, --save    Save the final time to ~/.sw/savedtime\n");
  fprintf(out, "  -r, --restore Restore time from ~/.sw/savedtime\n");
  fprintf(out, "  -x, --exit    Exit instead of pausing.\n");
}

void print_short_help(FILE *out)
{
  fprintf(out, "Usage: %s [-hsrq]\n", program_name);
}

int main(int argc, char *argv[])
{
  program_name = argv[0];

  static struct option long_options[] = {
    {"help",    no_argument, 0, 'h'},
    {"save",    no_argument, 0, 's'},
    {"restore", no_argument, 0, 'r'},
    {"exit",    no_argument, 0, 'x'},
  };

  int opt;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "hsrx", long_options, &option_index)) != -1)
  {
    switch (opt)
    {
      case 'h':
        print_help(stdout);
        exit(0);
      case 's':
        sflag = 1;
        break;
      case 'r':
        rflag = 1;
        break;
      case 'x':
        xflag = 1;
        break;
      case '?':
        print_short_help(stderr);
        break;
      default:
        exit(1);
    }
  }

  signal(SIGINT, sigint_handler);
  signal(SIGTERM, sigint_handler);
  pthread_t tid;
  pthread_create(&tid, NULL, input_thread, NULL);
  HIDE_CURSOR;
  clock_gettime(CLOCK_MONOTONIC, &starttime);
  if (rflag)
  {
    restore_time();
  }
  while (1)
  {
    while (!paused)
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
    nanosleep(&sleepduration, NULL);
  }
  return 0;
}
