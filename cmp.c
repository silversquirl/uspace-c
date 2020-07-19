#include "utils.h"
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

const char *usage[] = {
	"[-l|-s] file1 file2",
	NULL,
};

const char *optstring = "ls";
struct cmp_options {
	bool list, silent;
} opt = {0};

#define error(...) return (opt.silent ? 0 : (perrorf(__VA_ARGS__), 0)), 2
//#define error(...) return opt.silent || (perrorf(__VA_ARGS__), 0), 2

static int do_cmp(const char *file1, const char *file2) {
	FILE *f1;
	if (!strcmp(file1, "-")) {
		f1 = stdin;
	} else {
		f1 = fopen(file1, "rb");
		if (!f1) error("fopen: %s", file1);
	}

	FILE *f2;
	if (!strcmp(file1, "-")) {
		f2 = stdin;
	} else {
		f2 = fopen(file2, "rb");
		if (!f2) error("fopen: %s", file2);
	}

	size_t byten = 0, linen = 1;
	bool identical = true;
	int c1 = 0, c2 = 0;

#define get1char(n) if ((c##n = fgetc(f##n)) == EOF && ferror(f##n)) error("fgetc: %s", file##n)

	while (c1 != EOF && c2 != EOF) {
		get1char(1);
		get1char(2);
		byten++;

		if (c1 != c2) {
			if (c1 == EOF || c2 == EOF) {
				if (opt.list || !opt.silent) {
					eprintf("cmp: EOF on %s after byte %zu\n", c1 == EOF ? file1 : file2, byten);
				}
				return 1;
			} else {
				if (opt.list) {
					printf("%zu %o %o\n", byten, c1, c2);
					identical = 0;
				} else {
					if (!opt.silent) printf("%s %s differ: char %zu, line %zu\n", file1, file2, byten, linen);
					return 1;
				}
			}
		} else if (c1 == '\n' && !opt.list) {
			// Count lines
			linen++;
		}
	}

#undef get1char

	return !identical;
}

int main(int argc, char *argv[]) {
	int ch;
	while ((ch = getopt(argc, argv, optstring)) >= 0) {
		switch (ch) {
		case 'l':
			opt.list = true;
			break;

		case 's':
			opt.silent = true;
			break;

		case '?':
		default:
			print_usage(*argv);
			return 1;
		}
	}

	// Needs exactly 2 arguments
	if (optind + 2 != argc) {
		print_usage(*argv);
		return 1;
	}

	return do_cmp(argv[optind], argv[optind+1]);
}
