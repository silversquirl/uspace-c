// vim: noet

#include "utils.h"
#include <string.h>
#include <stdbool.h>

const char *usage[] = {"string", NULL};

int main(int argc, char *argv[]) {
	if (argc != 2) return print_usage(*argv), 1;
	char *string = argv[1], *p;

	// Step 1 in POSIX spec
	if (strcmp(string, "//")) {
		// Step 2/3 in POSIX spec
		p = strrxchr(string, '/');
		if (p) {
			// Step 3 bit
			p[1] = 0;
		} else {
			// Step 2 bit
			// Output a single slash and exit
			puts("/");
			return 0;
		}

		// Step 4 in POSIX spec
		p = strrchr(string, '/');
		if (!p) {
			puts(".");
			return 0;
		}

		// Step 5 in POSIX spec
		p[1] = 0;
	}

	// Step 6 in POSIX spec
	// Step 7/8 in POSIX spec
	p = strrxchr(string, '/');
	if (p) p[1] = 0; // 7
	else string = "/"; // 8

	// Output the result
	puts(string);

	return 0;
}
