#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

const char *usage[] = {
	"[-i] [name=value]... [command [arg]...]",
	NULL,
};

extern char **environ;

// clearenv(3) is not in POSIX.1 for some reason
int clear_env(void) {
	while (*environ) {
		char *split = strchr(*environ, '=');
		size_t len = split - *environ;
		char name[len + 1];
		strncpy(name, *environ, len);
		name[len] = 0;
		if (unsetenv(name)) return 1;
	}
	return 0;
}

int main(int argc, char **argv) {
	const char *argv0 = *argv;

	--argc, ++argv;

	char *path = NULL;
	{
		char *path_env = getenv("PATH");
		if (path_env) {
			// By spec, this can we overwritten by subsequent getenv calls, so
			// we need to copy the string
			path = malloc(strlen(path_env) + 1);
			strcpy(path, path_env);
		}
	}

	if (argc && !strcmp(*argv, "-i")) {
		--argc, ++argv;
		if (clear_env()) {
			eprintf("%s: Failed to clear environment\n", argv0);
			exit(1);
		}
	}

	for (; argc; --argc, ++argv) {
		if (!strchr(*argv, '=')) break;
		putenv(*argv);
	}

	// PATH may have changed; we should use the new one if it exists
	{
		char *path_env = getenv("PATH");
		// We don't need to copy this time as we never getenv again
		if (path_env) {
			free(path);
			path = path_env;
		}
	}

	if (!argc) {
		for (char **env = environ; *env; ++env) {
			puts(*env);
		}
		return 0;
	} else {
		if (strchr(*argv, '/')) {
			execv(*argv, argv);
		} else if (path) {
			char *name = *argv;
			size_t name_len = strlen(*argv);
			char *dir = strtok(path, ":");
			while (dir) {
				size_t dir_len = strlen(dir);

				size_t len = dir_len + name_len + 1;
				char path[len + 1];

				// Construct string "path/name" followed by null terminator
				strcpy(path, dir);
				path[dir_len] = '/';
				strcpy(path + dir_len + 1, name);
				path[len] = 0;

				execv(path, argv);

				if (errno != ENOENT) break;

				dir = strtok(NULL, ":");
			}
		}

		perrorf("%s: '%s'", argv0, *argv);

		if (errno == ENOENT) return 127;
		else return 126;
	}
}
