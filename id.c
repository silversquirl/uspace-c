// vim: noet

#include "lib/utils.h"
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include <string.h>

const char *usage[] = {
	"[user]",
	"-G [-n] [user]",
	"-g [-nr] [user]",
	"-u [-nr] [user]",
	NULL,
};

const char *optstring = "Ggnru";

char *argv0;
bool had_err = false;

void user_group_list(const char *user, int *ngroups, gid_t **groups) {
	size_t alloc = 4;
	*ngroups = 0;
	*groups = malloc(alloc * sizeof **groups);

	struct group *gr;

	setgrent();
	while ((gr = getgrent())) {
		size_t i;
		for (i = 0; gr->gr_mem[i] && strcmp(user, gr->gr_mem[i]); ++i);
		if (!gr->gr_mem[i]) continue;
		if (alloc == *ngroups) {
			alloc *= 2;
			*groups = realloc(*groups, alloc * sizeof **groups);
		}
		(*groups)[(*ngroups)++] = gr->gr_gid;
	}
	endgrent();
}

const char *uid_name(uid_t uid) {
	errno = 0;
	struct passwd *pass = getpwuid(uid);
	if (!pass) return NULL;
	else return pass->pw_name;
}

const char *gid_name(gid_t gid) {
	errno = 0;
	struct group *grp = getgrgid(gid);
	if (!grp) return NULL;
	else return grp->gr_name;
}

void print_group(gid_t gid, bool name) {
	if (name) {
		const char *name = gid_name(gid);
		if (!name) {
			if (errno) perrorf("%s: gid %s", argv0, name);
			else eprintf("%s: gid %s: no such group\n", argv0, name);
			had_err = true;
			return;
		}
		fputs(name, stdout);
	} else {
		printf("%u", gid);
	}
}

void print_user(uid_t uid, bool name) {
	if (name) {
		const char *name = uid_name(uid);
		if (!name) {
			if (errno) perrorf("%s: uid %s", argv0, name);
			else eprintf("%s: uid %s: no such user\n", argv0, name);
			had_err = true;
			return;
		}
		fputs(name, stdout);
	} else {
		printf("%u", uid);
	}
}

void print_id_name(unsigned id, const char *name) {
	printf("%u", id);
	if (name) printf("(%s)", name);
}

int main(int argc, char **argv) {
	argv0 = argv[0];

	enum {
		MODE_DEFAULT,
		MODE_ALL_GROUP,
		MODE_EFFECTIVE_GROUP,
		MODE_EFFECTIVE_USER,
	} mode = MODE_DEFAULT;

	bool output_real = false;
	bool output_name = false;

	int ch;
	while ((ch = getopt(argc, argv, optstring)) >= 0) {
		switch (ch) {
		case 'G': mode = MODE_ALL_GROUP; break;
		case 'g': mode = MODE_EFFECTIVE_GROUP; break;
		case 'u': mode = MODE_EFFECTIVE_USER; break;

		case 'r': output_real = true; break;
		case 'n': output_name = true; break;

		case '?':
		default:
			print_usage(*argv);
			return 1;
		}
	}

	uid_t uid, euid;
	gid_t gid, egid;

	bool output_groups = false;
	int ngroups = 0;
	gid_t *groups;

	if (optind == argc) {
		uid = getuid();
		euid = geteuid();
		gid = getgid();
		egid = getegid();

		ngroups = getgroups(0, NULL);
		groups = malloc(ngroups * sizeof groups[0]);
		getgroups(ngroups, groups);

		if (ngroups > 0) {
			if (ngroups > 1 || groups[0] != egid) output_groups = true;
		}
	} else {
		const char *user = argv[optind];
		errno = 0;
		struct passwd *pass = getpwnam(user);
		if (!pass) {
			if (errno) perrorf("%s: %s", argv[0], user);
			else eprintf("%s: %s: no such user\n", argv[0], user);
			return 1;
		}
		euid = uid = pass->pw_uid;
		egid = gid = pass->pw_gid;

		user_group_list(user, &ngroups, &groups);

		output_groups = true;
	}

	switch (mode) {
	case MODE_DEFAULT:
		fputs("uid=", stdout);
		print_id_name(uid, uid_name(uid));
		if (euid != uid) {
			fputs(" euid=", stdout);
			print_id_name(euid, uid_name(euid));
		}
		fputs(" gid=", stdout);
		print_id_name(gid, gid_name(gid));
		if (egid != gid) {
			fputs(" egid=", stdout);
			print_id_name(egid, gid_name(egid));
		}
		if (output_groups) {
			fputs(" groups=", stdout);
			print_id_name(egid, gid_name(egid));
			for (size_t i = 0; i < ngroups; ++i) {
				if (groups[i] == egid) continue;
				fputs(",", stdout);
				print_id_name(groups[i], gid_name(groups[i]));
			}
		}
		puts("");
		break;

	case MODE_ALL_GROUP:
		print_group(gid, output_name);
		if (gid != egid) {
			fputs(" ", stdout);
			print_group(egid, output_name);
		}
		for (size_t i = 0; i < ngroups; ++i) {
			if (groups[i] == gid) continue;
			if (groups[i] == egid) continue;
			fputs(" ", stdout);
			print_group(groups[i], output_name);
		}
		puts("");
		break;

	case MODE_EFFECTIVE_USER:
		print_user(output_real ? uid : euid, output_name);
		puts("");
		break;

	case MODE_EFFECTIVE_GROUP:
		print_group(output_real ? gid : egid, output_name);
		puts("");
		break;

	default:
		break;
	}

	return had_err ? 1 : 0;
}
