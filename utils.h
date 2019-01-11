#ifndef _USPACE_UTILS_H
#define _USPACE_UTILS_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#define eprintf(...) fprintf(stderr, __VA_ARGS__)

extern const char *usage[];
void print_usage(const char *argv0);
char *strrxchr(const char *s, int c);

#endif
