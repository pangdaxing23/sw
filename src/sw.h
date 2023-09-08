#ifndef STOPWATCH_H
#define STOPWATCH_H

#include <stdio.h>

void set_canonical_mode(int enable);
void print_time(FILE *fd);
void clear_output();
FILE *get_saved_time_file(char *mode);
void save_time();
void restore_time();
void add_one_second();
void subtract_one_second();
void reset_time();
void cleanup();
void sigint_handler(int sig);
void* input_thread();
void print_help(FILE *out);
void print_short_help(FILE *out);

#endif

