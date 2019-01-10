#include "utils.h"
#include <string.h>
#include <stdbool.h>

const char *usage[] = {"string [suffix]", NULL};

bool all_chars_are_slashes(const char *string) {
	while (*string == '/') string++;
	return !*string;
}

int main(int argc, char *argv[]) {
	if (argc < 2 || argc > 3) return print_usage(*argv), 1;
	char *string = argv[1];
	size_t str_len = strlen(string);

	char *suffix = argc > 2 ? argv[2] : NULL;
	size_t suf_len = suffix ? strlen(suffix) : -1;

	// Step 1 in POSIX spec
	if (str_len) {
		// Step 2 in POSIX spec
		// Step 3 in POSIX spec
		if (all_chars_are_slashes(string)) {
			string = "/";
		} else {
			// Step 4 in POSIX spec
			// Find the last non-slash character
			char *p = string + str_len;
			while (*--p == '/');
			// Truncate the string 
			p[1] = 0;

			// Step 5 in POSIX spec
			p = strrchr(string, '/');
			if (p) string = p + 1;

			// Step 6 in POSIX spec
			// Check if suffix is a suffix of, but not the whole of, string
			// If suffix is NULL, suf_len is -1 so bigger than str_len
			if (suf_len < str_len && !strcmp(string + str_len - suf_len, suffix)) {
				// Truncate the string
				string[str_len - suf_len] = 0;
			}
		}
	}

	// Output the result
	puts(string);

	return 0;
}
