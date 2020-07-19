#include "utils.h"
#include <errno.h>
#include <string.h>
#include <stdarg.h>

void perrorf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	eprintf(": %s\n", strerror(errno));
}

void print_usage(const char *argv0) {
	eprintf("Usage:\n");
	for (const char **p = usage; *p; p++) {
		eprintf("  %s %s\n", argv0, *p);
	}
}

char *strrxchr(const char *s, int c) {
	const char *ret = NULL;
	while (*s) {
		if ((unsigned char)*s != c) ret = s;
		s++;
	}
	return (char *)ret;
}
