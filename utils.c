#include "utils.h"

void print_usage(const char *argv0) {
	eprintf("Usage:\n");
	for (const char **p = usage; *p; p++) {
		eprintf("  %s %s\n", argv0, *p);
	}
}

char *strrxchr(const char *s, int c) {
	const char *ret = NULL;
	while (*s) {
		if (*s != c) ret = s;
		s++;
	}
	return (char *)ret;
}
