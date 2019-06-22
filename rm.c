#include "utils.h"
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

const char *usage[] = {
	"[-iRr] file...",
	"-f [-iRr] [file...]",
	NULL,
};

const char *optstring = "fiRr";
struct rm_options {
	bool force, confirm, recurse;
} opt = {0};
bool stdin_is_term = false;

static bool confirm(void) {
	int c = getchar(), c2 = c; // Save the first char
	while (c2 != EOF && c2 != '\n') c2 = getchar(); // Discard the rest of the line
	switch (c) {
	case 'y':
	case 'Y':
		return true;
	default:
		return false;
	}
}

static int do_rm(const char *fn) {
	char cwd[PATH_MAX];
	if (!getcwd(cwd, sizeof cwd)) return perror("getcwd"), 1;

	struct stat st;
	// Step 1 in POSIX spec
	if (stat(fn, &st)) {
		if (opt.force && errno == ENOENT) return 0;
		perrorf("%s/%s", cwd, fn);
		return 1;
	}

	// Step 3 in POSIX spec (no clue why they made it step 3)
	if (stdin_is_term && access(fn, W_OK)) {
		eprintf("Remove non-writeable '%s/%s'? [y/N] ", cwd, fn);
		if (!confirm()) return 0;
	} else if (opt.confirm) {
		eprintf("Remove '%s/%s'? [y/N] ", cwd, fn);
		if (!confirm()) return 0;
	}

	// Step 2 in POSIX spec
	if (S_ISDIR(st.st_mode)) {
		if (!opt.recurse) return eprintf("%s/%s: is a directory. Try using -r\n", cwd, fn), 1;

		DIR *dp = opendir(fn);
		if (!dp) return perror(fn), 1;
		struct dirent *de;
		// Traverse the directory and remove each entity
		if (chdir(fn)) perrorf("chdir: %s/%s", cwd, fn);
		while ((errno = 0, de = readdir(dp))) {
			if (!strcmp(de->d_name, ".")) continue;
			if (!strcmp(de->d_name, "..")) continue;
			do_rm(de->d_name);
		}
		if (chdir(cwd)) perrorf("chdir: %s", cwd);
		closedir(dp);
		// readdir is a little bit odd, so we gotta check errno
		// Bear in mind that this will show closedir's error if it errored, rather than readdir's
		if (errno) return perrorf("%s/%s", cwd, fn), 1;

		// Delete the directory
		if (rmdir(fn)) return perrorf("%s/%s", cwd, fn), 1;
	} else {
		// Step 4 in POSIX spec
		if (unlink(fn)) return perrorf("%s/%s", cwd, fn), 1;
	}
	return 0;
}

int main(int argc, char *argv[]) {
	int ch;
	while ((ch = getopt(argc, argv, optstring)) >= 0) {
		switch (ch) {
		case 'f':
			opt.force = true;
			break;

		case 'i':
			opt.confirm = true;
			break;

		case 'R':
		case 'r':
			opt.recurse = true;
			break;

		case '?':
		default:
			print_usage(*argv);
			return 1;
		}
	}

	stdin_is_term = isatty(STDIN_FILENO); // Needed by do_rm
	int ret = 0;
	for (int i = optind; i < argc; i++) ret = do_rm(argv[i]) || ret;
	return ret;
}
