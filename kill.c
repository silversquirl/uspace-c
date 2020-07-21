// vim: noet

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/utils.h"

const char *usage[] = {
	"-s signal_name pid...",
	"-l [exit_status]",
	"[-signal_name] pid...",
	"[-signal_number] pid...",
	NULL
};

struct {const char *name; int sig;} signames[] = {
#define sig(name) {#name, SIG##name}
	{"0", 0},
	sig(ABRT),
	sig(ALRM),
	sig(BUS),
	sig(CHLD),
	sig(CONT),
	sig(FPE),
	sig(HUP),
	sig(ILL),
	sig(INT),
	sig(KILL),
	sig(PIPE),
	sig(POLL),
	sig(PROF),
	sig(QUIT),
	sig(SEGV),
	sig(STOP),
	sig(SYS),
	sig(TERM),
	sig(TRAP),
	sig(TSTP),
	sig(TTIN),
	sig(TTOU),
	sig(URG),
	sig(USR1),
	sig(USR2),
	sig(VTALRM),
	sig(XCPU),
	sig(XFSZ),
#undef sig
};
const size_t numsig = sizeof signames / sizeof *signames;

static inline int signum(int num) {
	switch (num) {
	case 1:
		return SIGHUP;
	case 2:
		return SIGINT;
	case 3:
		return SIGQUIT;
	case 6:
		return SIGABRT;
	case 9:
		return SIGKILL;
	case 14:
		return SIGALRM;
	case 15:
		return SIGTERM;
	default:
		return num;
	}
}

int getsig(const char *name) {
	char *nend;
	long num = strtol(name, &nend, 10);
	if (!*nend) return signum(num);

	int start = 0, end = numsig;
	while (start < end) {
		int mid = (start + end) / 2;
		int cmp = strcmp(name, signames[mid].name);
		if (cmp < 0) end = mid;
		else if (cmp > 0) start = mid;
		else if (cmp == 0) return signames[mid].sig;
	}
	return -1;
}

const char *nameofsig(int num) {
	switch (num) {
	case 1:
		return "HUP";
	case 2:
		return "INT";
	case 3:
		return "QUIT";
	case 6:
		return "ABRT";
	case 9:
		return "KILL";
	case 14:
		return "ALRM";
	case 15:
		return "TERM";
	default:
		for (int i = 0; i < numsig; i++) {
			if (signames[i].sig == num) return signames[i].name;
		}
		return NULL;
	}
}

int main(int argc, char **argv) {
	_Bool list = 0;
	int sig = SIGTERM;

	if (argc <= 1) {
		print_usage(*argv);
		return 1;
	}

	int idx = 1;
	if (argv[1][0] == '-') {
		switch (argv[1][1]) {
		case 's':
			if (argv[1][2]) {
				sig = getsig(argv[1]+2);
			} else if (argc > 2) {
				sig = getsig(argv[2]);
				idx++;
			} else {
				eprintf("Option 's' requires an argument\n\n");
				print_usage(*argv);
				return 1;
			}
			break;

		case 'l':
			list = 1;
			break;

		default:
			sig = getsig(argv[1]+1);
			break;
		}
		idx++;
	}

	if (sig < 0) {
		eprintf("Invalid signal name\n");
		return 1;
	}

	if (list) {
		if (argc <= idx) {
			for (int i = 1; i < numsig; i++) {
				printf("%s\t%d\n", signames[i].name, signames[i].sig);
			}
			return 0;
		}

		char *arg = argv[idx];

		// Parse number
		char *end;
		long val = strtol(arg, &end, 10);
		if (*end) {
			eprintf("exit_status must be a number\n\n");
			print_usage(*argv);
			return 1;
		}

		// Compute signal name from number
		if (val > 128) val -= 128;
		const char *name = nameofsig(val);

		if (!name) {
			eprintf("Invalid signal/exit code\n");
			return 1;
		}

		puts(name);
		return 0;
	} else {
		if (argc <= idx) {
			eprintf("Not enough arguments\n\n");
			print_usage(*argv);
			return 1;
		}

		int ret = 0;
		for (int i = idx; i < argc; i++) {
			char *arg = argv[i];

			char *end;
			long pid = strtol(arg, &end, 10);
			if (*end) {
				eprintf("pid must be a number\n");
				print_usage(*argv);
				return 1;
			}

			if (kill(pid, sig)) {
				perror("kill");
				ret = 1;
			}
		}
		return ret;
	}
}
