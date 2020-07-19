// vim: noet

#ifndef _USPACE_UTILS_H
#define _USPACE_UTILS_H

#include <stdio.h>
#define eprintf(...) fprintf(stderr, __VA_ARGS__)

void perrorf(const char *fmt, ...);
extern const char *usage[];
void print_usage(const char *argv0);

// Finds the last character that is not c
char *strrxchr(const char *s, int c);

#endif
