// vim: noet

#include "utils.h"
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

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
		if (*s != (char)c) ret = s;
		s++;
	}
	return (char *)ret;
}

int asprintf(char **strp, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);

	size_t sz = strlen(fmt) * 2;
	for (;;) {
		char *str = malloc(sz);
		va_list tmp;
		va_copy(tmp, args);
		int ret = vsnprintf(str, sz, fmt, tmp);
		va_end(tmp);

		if (ret < 0) {
			free(str);
			va_end(args);
			return ret;
		}

		if (ret < sz) {
			*strp = str;
			va_end(args);
			return ret;
		}

		free(str);
		sz *= 2;
	}
}

unsigned long log10li(unsigned long x) {
	unsigned long i = 1;
	while (x /= 10) ++i;
	return i;
}
