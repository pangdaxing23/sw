#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#define NSEC_TO_SLEEP 10000000
#define HIDE_CURSOR printf("\e[?25l")
#define SHOW_CURSOR printf("\e[?25h")

volatile sig_atomic_t timing = 1;

const struct timespec sleepduration = {0, NSEC_TO_SLEEP};

struct timespec starttime;
struct timespec currenttime;
struct timespec elapsedtime;

char endchar = '\r';

void print_time()
{
  if (elapsedtime.tv_sec >= 3600)
  {
    printf("%ld:%02ld:%02ld.%02ld%c",
           (long int)(elapsedtime.tv_sec / 3600),
           (long int)(elapsedtime.tv_sec % 3600 / 60),
           (long int)(elapsedtime.tv_sec % 60),
           (long int)(elapsedtime.tv_nsec / 10000000),
           endchar);
  }
  else
  {
    printf("%02ld:%02ld.%02ld%c",
           (long int)(elapsedtime.tv_sec / 60),
           (long int)(elapsedtime.tv_sec % 60),
           (long int)(elapsedtime.tv_nsec / 10000000),
           endchar);
  }
  fflush(stdout);
}

void clear_output()
{
  printf("\r");
  fflush(stdout);
}

void sigint_handler(int sig)
{
  timing = 0;
  endchar = '\n';
  clear_output();
  print_time();
  SHOW_CURSOR;
  signal(sig, sigint_handler);
}

int main(int argc, char *argv[])
{
  signal(SIGINT, sigint_handler);
  HIDE_CURSOR;
  clock_gettime(CLOCK_MONOTONIC, &starttime);
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
    print_time();
    nanosleep(&sleepduration, NULL);
  }
  return 0;
}
