#ifndef _USPACE_UTILS_H
#define _USPACE_UTILS_H

#include <stdio.h>
#define eprintf(...) fprintf(stderr, __VA_ARGS__)

#define _POSIX_C_SOURCE 200809L

const char *usage[];
static void print_usage(const char *argv0) {
	eprintf("Usage:\n");
	for (const char **p = usage; *p; p++) {
		eprintf("  %s %s\n", argv0, *p);
	}
}

#endif
