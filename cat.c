// vim: noet

#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const char *usage[] = {
	"[-u] [file...]",
	NULL,
};

const char *optstring = "u";

int fcopy(FILE *out, FILE *in) {
	int ch;
	while ((ch = getc(in)) != EOF && putc(ch, out) != EOF);
	return ferror(in) || ferror(out);
}

int main(int argc, char *argv[]) {
	int ch;
	while ((ch = getopt(argc, argv, optstring)) >= 0) {
		switch (ch) {
		case 'u':
			setvbuf(stdout, NULL, _IONBF, 0);
			break;

		case '?':
		default:
			print_usage(*argv);
			return 1;
		}
	}

	// No arguments means copy stdin to stdout
	if (optind >= argc) {
		if (fcopy(stdout, stdin)) {
			perror("fcopy: -");
			return 1;
		}
		return 0;
	}

	for (int i = optind; i < argc; i++) {
		if (!strcmp(argv[i], "-")) {
			if (fcopy(stdout, stdin)) {
				perrorf("fcopy: %s", argv[i]);
				return 1;
			}
		} else {
			FILE *f = fopen(argv[i], "r");
			if (!f) {
				perrorf("fopen: %s", argv[i]);
				return 1;
			}

			if (fcopy(stdout, f)) {
				perrorf("fcopy: %s", argv[i]);
				return 1;
			}
		}
	}

	return 0;
}
