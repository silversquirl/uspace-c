// vim: noet

#include "lib/utils.h"
#include <stdio.h>

const char *usage[] = {
	"[string...]",
	NULL,
};

_Bool print_nl = 1;

static const char *echo_oct(const char *s) {
	int n = 0;
	for (const char *end = s+3; *s && '0' <= *s && *s <= '7' && s < end; s++) {
		n *= 8;
		n += *s - '0';
	}
	putchar(n);
	return s;
}

static void echo_out(const char *s) {
	while (*s) {
		if (*s == '\\') {
			s++;
			if (!*s) break;

			switch (*s) {
			case 'a':
				putchar('\a');
				break;
			case 'b':
				putchar('\b');
				break;
			case 'c':
				print_nl = 0;
				break;
			case 'f':
				putchar('\f');
				break;
			case 'n':
				putchar('\n');
				break;
			case 'r':
				putchar('\r');
				break;
			case 't':
				putchar('\t');
				break;
			case 'v':
				putchar('\v');
				break;
			case '\\':
			default:
				putchar(*s);
				break;
			case '0':
				s = echo_oct(s+1);
				s--;
				break;
			}
		} else {
			putchar(*s);
		}

		s++;
	}
}

int main(int argc, char *argv[]) {
	for (int i = 1; i < argc; i++) {
		echo_out(argv[i]);
		if (i < argc - 1) putchar(' ');
	}
	if (print_nl) putchar('\n');
	return 0;
}
