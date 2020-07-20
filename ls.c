// vim: noet

#include "utils.h"
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>

const char *usage[] = {
	"[-ikqrs] [-glno] [-A|-a] [-C|-m|-x|-1] [-F|-p] [-H|-L] [-R|-d] [-S|-f|-t] [-c|-u] [file...]",
	NULL,
};

const char *optstring = "ikqrsglnoAaCmx1FpHLRdSftcu";

struct file_info {
	char *name;
	mode_t mode;
	const char *uname;
	const char *gname;
	size_t size;
	size_t blocks;
	size_t nlink;
	size_t ino;
	struct timespec modified;

	// NULL if not a symlink
	char *link_target;
};

// Argument datastructures {{{

#define LONG_OUT_ENABLE (1<<0)
#define LONG_OUT_USER_GROUP_ID (1<<1)
#define LONG_OUT_NO_USER (1<<2)
#define LONG_OUT_NO_GROUP (1<<3)

bool out_serial = false;
bool out_only_printable = false;
bool out_blocks = false;
bool sort_reverse = false;
size_t block_size = 512;
int long_output_flags = 0;
unsigned output_width = 80;

bool output_dirnames = false;

enum out_mode {
	OUT_MODE_DEFAULT,
	OUT_MODE_ONE_PER_LINE,
	OUT_MODE_COLS_DOWN,
	OUT_MODE_COLS_ACROSS,
	OUT_MODE_COMMA_SEP,
} out_mode = OUT_MODE_DEFAULT;

enum time_mode {
	TIME_MODE_MODIFIED,
	TIME_MODE_ACCESSED,
	TIME_MODE_STATUS_MODIFIED,
} time_mode = TIME_MODE_MODIFIED;

enum classify_mode {
	CLASSIFY_MODE_NONE,
	CLASSIFY_MODE_DIRS,
	CLASSIFY_MODE_ALL,
} classify_mode = CLASSIFY_MODE_NONE;

enum sort_mode {
	SORT_MODE_GIVEN,
	SORT_MODE_ALPHA,
	SORT_MODE_SIZE,
	SORT_MODE_TIME,
} sort_mode = SORT_MODE_ALPHA;

enum hidden_mode {
	HIDDEN_MODE_NONE,
	HIDDEN_MODE_REAL,
	HIDDEN_MODE_ALL,
} hidden_mode = HIDDEN_MODE_NONE;

enum dir_mode {
	DIR_MODE_NO_ENTER,
	DIR_MODE_ENTER,
	DIR_MODE_RECURSE,
} dir_mode = DIR_MODE_ENTER;

enum link_mode {
	LINK_MODE_DEFAULT,
	LINK_MODE_FOLLOW_NEVER,
	LINK_MODE_FOLLOW_OPERAND,
	LINK_MODE_FOLLOW_ALL,
} link_mode = LINK_MODE_DEFAULT;

// }}}

// Loop detection {{{

struct {
	char **dirs;
	size_t ndirs;
	size_t alloc;
} dir_stack;

bool dir_loop_push(const char *path) {
	char *abs = realpath(path, NULL);

	for (size_t i = 0; i < dir_stack.ndirs; ++i) {
		if (!strcmp(dir_stack.dirs[i], abs)) {
			return true;
		}
	}

	if (dir_stack.alloc == dir_stack.ndirs) {
		dir_stack.alloc *= 2;
		dir_stack.dirs = realloc(dir_stack.dirs, dir_stack.alloc * sizeof dir_stack.dirs[0]);
	}

	dir_stack.dirs[dir_stack.ndirs++] = abs;

	return false;
}

void dir_loop_pop() {
	free(dir_stack.dirs[--dir_stack.ndirs]);
}

// }}}

char *argv0;

struct timespec ts_now;

bool had_err = false;

int (*root_stat)(const char *, struct stat *);
int (*nonroot_stat)(const char *, struct stat *);

// File info construction {{{

struct file_info get_file_info(const char *path, char *name, struct stat f_stat) {
	size_t sizeb = f_stat.st_blocks * 512;
	struct timespec mod;
	switch (time_mode) {
	case TIME_MODE_MODIFIED: mod = f_stat.st_mtim; break;
	case TIME_MODE_ACCESSED: mod = f_stat.st_atim; break;
	case TIME_MODE_STATUS_MODIFIED: mod = f_stat.st_ctim; break;
	}

	char *uname, *gname;

	asprintf(&uname, "%u", f_stat.st_uid);
	asprintf(&gname, "%u", f_stat.st_gid);

	if (!(long_output_flags & LONG_OUT_USER_GROUP_ID)) {
		struct passwd *user = getpwuid(f_stat.st_uid);
		if (user) {
			free(uname);
			asprintf(&uname, "%s", user->pw_name);
		}

		struct group *group = getgrgid(f_stat.st_gid);
		if (group) {
			free(gname);
			asprintf(&gname, "%s", group->gr_name);
		}
	}

	if (long_output_flags & LONG_OUT_NO_USER) {
		free(uname);
		asprintf(&uname, "");
	}

	if (long_output_flags & LONG_OUT_NO_GROUP) {
		free(gname);
		asprintf(&gname, "");
	}

	char *link_target = NULL;

	if (S_ISLNK(f_stat.st_mode)) {
		size_t buf_sz = 16;
		link_target = malloc(buf_sz);
		int written;

		do {
			written = readlink(path, link_target, buf_sz);
			if (written == -1) {
				perrorf("%s: cannot read symbolic link '%s':", argv0, path);
				had_err = true;
				free(link_target);
				link_target = NULL;
				break;
			} else if (written == buf_sz) {
				buf_sz *= 2;
				link_target = realloc(link_target, buf_sz);
			}
		}	while (written == buf_sz);

		if (link_target) link_target[written] = 0;
	}

	return (struct file_info) {
		.name = name,
		.mode = f_stat.st_mode,
		.uname = uname,
		.gname = gname,
		.size = f_stat.st_size,
		.blocks = sizeb / block_size + !!(sizeb % 512),
		.nlink = f_stat.st_nlink,
		.ino = f_stat.st_ino,
		.modified = mod,
		.link_target = link_target,
	};
}

// }}}

// Listing output {{{

// Formatting info {{{
struct {
	struct {
		int links_cols;
		int user_cols;
		int group_cols;
		int size_cols;
	} long_out;
	
	int serial_cols;
	int block_cols;
	int name_cols;
} format_info;

void init_format_info(struct file_info *files, size_t nfiles) {
	format_info.block_cols = 0;
	format_info.serial_cols = 0;
	format_info.long_out.links_cols = 0;
	format_info.long_out.user_cols = 0;
	format_info.long_out.group_cols = 0;
	format_info.long_out.size_cols = 0;

#define update_cols_i(a,b) do { size_t x = log10li(b); if (a < x) a = x; } while (0)
#define update_cols_s(a,b) do { size_t x = strlen(b);  if (a < x) a = x; } while (0)
	for (size_t i = 0; i < nfiles; ++i) {
		update_cols_i(format_info.serial_cols, files[i].ino);
		update_cols_i(format_info.block_cols, files[i].blocks);
		update_cols_i(format_info.long_out.links_cols, files[i].nlink);
		update_cols_i(format_info.long_out.size_cols, files[i].size);
		update_cols_s(format_info.long_out.user_cols, files[i].uname);
		update_cols_s(format_info.long_out.group_cols, files[i].gname);
	}
#undef update_cols_i
#undef update_cols_s
}
// }}}

char *format_file(struct file_info file) {
	char *fmt = malloc(strlen(file.name) + 1);
	strcpy(fmt, file.name);

	// Classifier symbols {{{
	if (classify_mode != CLASSIFY_MODE_NONE) {
		char c = 0; // NUL indicates no suffix
		if (S_ISDIR(file.mode)) c = '/';
		else if (classify_mode == CLASSIFY_MODE_ALL) {
			if (file.mode & S_IXUSR) c = '*';
			else if (S_ISFIFO(file.mode)) c = '|';
			else if (S_ISLNK(file.mode)) c = '@';
		}

		if (c) {
			char *tmp;
			asprintf(&tmp, "%s%c", fmt, c);
			free(fmt);
			fmt = tmp;
		}
	}
	// }}}
	
	// Long output {{{
	if (long_output_flags & LONG_OUT_ENABLE) {
		if (file.link_target) {
			char *tmp;
			asprintf(&tmp, "%s -> %s", fmt, file.link_target);
			free(fmt);
			fmt = tmp;
		}

		char type = '-';
		if (S_ISDIR(file.mode)) type = 'd';
		else if (S_ISBLK(file.mode)) type = 'b';
		else if (S_ISCHR(file.mode)) type = 'c';
		else if (S_ISLNK(file.mode)) type = 'l';
		else if (S_ISFIFO(file.mode)) type = 'p';

		char type_str[11] = {
			type,
			file.mode & S_IRUSR ? 'r' : '-',
			file.mode & S_IWUSR ? 'w' : '-',
			file.mode & S_IXUSR ? 'x' : '-',
			file.mode & S_IRGRP ? 'r' : '-',
			file.mode & S_IWGRP ? 'w' : '-',
			file.mode & S_IXGRP ? 'x' : '-',
			file.mode & S_IROTH ? 'r' : '-',
			file.mode & S_IWOTH ? 'w' : '-',
			file.mode & S_IXOTH ? 'x' : '-',
			0
		};

		const char *time_fmt =
			(ts_now.tv_sec - file.modified.tv_sec < 60*60*24*30*6)
			? "%b %e %H:%M"
			: "%b %e  %Y";

		char time_str[13]; // Should always be big enough
		strftime(time_str, 13, time_fmt, localtime(&file.modified.tv_sec));

		char *tmp;
		asprintf(&tmp, "%s %*ld %-*s %-*s %*ld  %12s  %s", type_str, format_info.long_out.links_cols, file.nlink, format_info.long_out.user_cols, file.uname, format_info.long_out.group_cols, file.gname, format_info.long_out.size_cols, file.size, time_str, fmt);
		free(fmt);
		fmt = tmp;
	}
	// }}}
	
	// Block count {{{
	if (out_blocks) {
		char *tmp;
		asprintf(&tmp, "%lu %s", file.blocks, fmt);
		free(fmt);
		fmt = tmp;
	}
	// }}}

	// Serial number {{{
	if (out_serial) {
		char *tmp;
		asprintf(&tmp, "%lu %s", file.ino, fmt);
		free(fmt);
		fmt = tmp;
	}
	// }}}

	return fmt;
}

void output_files(struct file_info *files, size_t nfiles) {
	if (nfiles == 0) return;

	init_format_info(files, nfiles);

	int longest = 0;

	char **lines = malloc(nfiles * sizeof *lines);

	for (size_t i = 0; i < nfiles; ++i) {
		char *line = format_file(files[i]);

		if (out_only_printable) {
			for (char *c = line; *c; ++c) {
				if (*c < 0x20 || *c > 0x7E) *c = '?';
			}
		}

		size_t len = strlen(line);
		if (len > longest) longest = len;

		lines[i] = line;
	}
	
	if (longest == 0) longest = 1;

	unsigned cols = output_width / (longest + 2);
	if (cols == 0) cols = 1;
	unsigned rows = nfiles / cols + !!(nfiles % cols);

	switch (out_mode) {
	case OUT_MODE_COMMA_SEP:
		printf("%s", lines[0]);
		for (size_t i = 1; i < nfiles; ++i) {
			printf(", %s", lines[i]);
		}
		printf("\n");
		break;

	case OUT_MODE_COLS_DOWN:
		for (unsigned r = 0; r < rows; ++r) {
			for (unsigned c = 0; c < cols; ++c) {
				if (c*rows + r >= nfiles) break;
				printf("%-*s  ", longest, lines[c*rows + r]);
			}
			printf("\n");
		}
		break;

	case OUT_MODE_COLS_ACROSS:
		for (unsigned r = 0; r < rows; ++r) {
			for (unsigned c = 0; c < cols; ++c) {
				if (r*cols + c >= nfiles) break;
				printf("%-*s  ", longest, lines[r*cols + c]);
			}
			printf("\n");
		}
		break;

	default:
	case OUT_MODE_ONE_PER_LINE:
		for (size_t i = 0; i < nfiles; ++i) puts(lines[i]);
		break;
	}

	for (size_t i = 0; i < nfiles; ++i) {
		free(lines[i]);
	}

	free(lines);
}

// }}}

// Sorting {{{

int compare_files(struct file_info a, struct file_info b) {
	int ordering = 0;

	switch (sort_mode) {
		case SORT_MODE_SIZE:
			if (a.size < b.size) ordering = 1;
			else if (a.size > b.size) ordering = -1;
			break;
		case SORT_MODE_TIME:;
			long int ta = a.modified.tv_sec;
			long int tb = b.modified.tv_sec;
			if (ta < tb) ordering = 1;
			else if (ta > tb) ordering = -1;
			break;
		case SORT_MODE_ALPHA:
		default:
			break; // Immediately fallover to the secondary sort
	}

	if (ordering == 0) {
		ordering = strcoll(a.name, b.name); // Secondary ordering is always strcoll
	}

	return sort_reverse ? -ordering : ordering;
}

void insert_sorted_entry(struct file_info new, struct file_info **ents, size_t *nents, size_t *alloc) {
	if (*alloc == *nents) {
		*alloc *= 2;
		*ents = realloc(*ents, *alloc * sizeof **ents);
	}

	for (size_t i = 0; i < *nents; ++i) {
		if (compare_files(new, (*ents)[i]) < 0) {
			// We've passed the one directly below us; insert here
			memmove(*ents + i + 1, *ents + i, (*nents-i) * sizeof **ents);
			(*ents)[i] = new;
			++*nents;
			return;
		}
	}

	// It's the largest; insert at the end
	(*ents)[(*nents)++] = new;
}

// }}}

// Path normalization {{{

// Allocates memory on the heap; must be freed by the caller
char *normalize_dir(const char *path) {
	if (!path[0]) {
		// Empty string - bad case, return NULL
		return NULL;
	}

	const char *end = strrxchr(path, '/');

	if (!end) {
		// Purely slashes - normalised version is '/'
		char *new = malloc(2);
		new[0] = '/';
		new[1] = 0;
		return new;
	}	

	size_t len = end - path + 3;
	char *new = malloc(len);
	strncpy(new, path, len - 2);
	new[len-2] = '/';
	new[len-1] = 0;
	return new;
}

// }}}

// Directory listing handler {{{

void handle_dir(const char *base) {
	DIR *dir = opendir(base);

	if (!dir) {
		perrorf("%s: '%s'", argv0, base);
		had_err = true;
		closedir(dir);
		return;
	}

	if (dir_loop_push(base)) {
		eprintf("%s: detected loop in directory '%s'\n", argv0, base);
		closedir(dir);
		return;
	}

	if (output_dirnames) printf("\n%s:\n", base);

	// Note: this means we have to free base
	base = normalize_dir(base);
	size_t base_len = strlen(base);

	size_t ents_alloc = 8, dirs_alloc = 8;
	size_t nents = 0, ndirs = 0;
	struct file_info *ents = malloc(ents_alloc * sizeof ents[0]);
	struct file_info *dirs = malloc(dirs_alloc * sizeof dirs[0]);

	size_t total_dir_size = 0;

	struct dirent *ent;
	while (errno=0, ent=readdir(dir)) {
		if (hidden_mode == HIDDEN_MODE_NONE && ent->d_name[0] == '.') {
			continue;
		}

		bool is_fake = !strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..");

		if (hidden_mode == HIDDEN_MODE_REAL && is_fake) continue;

		size_t name_len = strlen(ent->d_name);

		char *name = malloc(name_len + 1);
		strcpy(name, ent->d_name);
		name[name_len] = 0;

		char path[base_len + name_len + 1];
		strcpy(path, base);
		strcpy(path + base_len, name);
		path[base_len + name_len] = 0;

		struct stat f_stat;

		if (nonroot_stat(path, &f_stat)) {
			perrorf("%s: '%s'", argv0, path);
			had_err = true;
			continue;
		}

		struct file_info info = get_file_info(path, name, f_stat);

		if (S_ISDIR(f_stat.st_mode) && dir_mode == DIR_MODE_RECURSE && !is_fake) {
			insert_sorted_entry(info, &dirs, &ndirs, &dirs_alloc);
		}

		insert_sorted_entry(info, &ents, &nents, &ents_alloc);

		total_dir_size += f_stat.st_blocks * 512;
	}

	if (errno != 0) {
		perrorf("%s: '%s'", argv0, base);
		had_err = true;
	}

	if (out_blocks || (long_output_flags & LONG_OUT_ENABLE)) {
		size_t blocks = total_dir_size / block_size + !!(total_dir_size % block_size);
		printf("total %ld\n", blocks);
	}

	output_files(ents, nents);

	for (size_t i = 0; i < ndirs; ++i) {
		const char *name = dirs[i].name;
		size_t name_len = strlen(name);

		char path[base_len + name_len + 1];
		strcpy(path, base);
		strcpy(path + base_len, name);
		path[base_len + name_len] = 0;

		handle_dir(path);
	}

	for (size_t i = 0; i < nents; ++i) {
		free(ents[i].name);
		if (ents[i].link_target) free(ents[i].link_target);
	}

	free(dirs);
	free(ents);

	free((char *)base);

	closedir(dir);
}

// }}}

int main(int argc, char **argv) {
	argv0 = *argv;

	// Argument parsing {{{

	int ch;
	while ((ch = getopt(argc, argv, optstring)) >= 0) {
		switch (ch) {
		case 'i': out_serial = true; break;
		case 'k': block_size = 1024; break;
		case 'q': out_only_printable = true; break;
		case 'r': sort_reverse = true; break;
		case 's': out_blocks = true; break;

		case 'g':
			long_output_flags |= LONG_OUT_ENABLE | LONG_OUT_NO_USER;
			out_mode = OUT_MODE_ONE_PER_LINE;
			break;
		case 'l':
			if (link_mode == LINK_MODE_DEFAULT) link_mode = LINK_MODE_FOLLOW_NEVER;
			long_output_flags |= LONG_OUT_ENABLE;
			out_mode = OUT_MODE_ONE_PER_LINE;
			break;
		case 'n':
			long_output_flags |= LONG_OUT_ENABLE | LONG_OUT_USER_GROUP_ID;
			out_mode = OUT_MODE_ONE_PER_LINE;
			break;
		case 'o':
			long_output_flags |= LONG_OUT_ENABLE | LONG_OUT_NO_GROUP;
			out_mode = OUT_MODE_ONE_PER_LINE;
			break;

		case 'A': hidden_mode = HIDDEN_MODE_REAL; break;
		case 'a': hidden_mode = HIDDEN_MODE_ALL; break;

		case 'C':
			if (out_mode != OUT_MODE_ONE_PER_LINE) out_mode = OUT_MODE_COLS_DOWN;
			long_output_flags &= ~LONG_OUT_ENABLE;
			break;
		case 'm':
			if (out_mode != OUT_MODE_ONE_PER_LINE) out_mode = OUT_MODE_COMMA_SEP;
			long_output_flags &= ~LONG_OUT_ENABLE;
			break;
		case 'x':
			if (out_mode != OUT_MODE_ONE_PER_LINE) out_mode = OUT_MODE_COLS_ACROSS;
			long_output_flags &= ~LONG_OUT_ENABLE;
			break;
		case '1':
			out_mode = OUT_MODE_ONE_PER_LINE;
			break;

		case 'F':
			classify_mode = CLASSIFY_MODE_ALL;
			if (link_mode == LINK_MODE_DEFAULT) link_mode = LINK_MODE_FOLLOW_NEVER;
			break;
		case 'p':
			classify_mode = CLASSIFY_MODE_DIRS;
			break;

		case 'H': link_mode = LINK_MODE_FOLLOW_OPERAND; break;
		case 'L': link_mode = LINK_MODE_FOLLOW_ALL; break;

		case 'R':
			dir_mode = DIR_MODE_RECURSE;
			break;
		case 'd':
			if (link_mode == LINK_MODE_DEFAULT) link_mode = LINK_MODE_FOLLOW_NEVER;
			dir_mode = DIR_MODE_NO_ENTER;
			break;

		case 'S':
			if (sort_mode != SORT_MODE_GIVEN) sort_mode = SORT_MODE_SIZE;
			break;
		case 'f':
			sort_mode = SORT_MODE_GIVEN;
			break;
		case 't':
			if (sort_mode != SORT_MODE_GIVEN) sort_mode = SORT_MODE_TIME;
			break;

		case 'c': time_mode = TIME_MODE_STATUS_MODIFIED; break;
		case 'u': time_mode = TIME_MODE_ACCESSED; break;

		case '?':
		default:
			print_usage(*argv);
			return 1;
		}
	}

	if (out_mode == OUT_MODE_DEFAULT) {
		out_mode =
			isatty(STDOUT_FILENO)
			? OUT_MODE_COLS_DOWN
			: OUT_MODE_ONE_PER_LINE;
	}

	if (link_mode == LINK_MODE_DEFAULT) {
		link_mode = LINK_MODE_FOLLOW_OPERAND;
	}

	// Dumb special case
	if (sort_mode == SORT_MODE_GIVEN) sort_reverse = false;

	// }}}

	{
		char *str_cols;
		if ((str_cols = getenv("COLUMNS"))) {
			unsigned cols;
			if (sscanf(str_cols, "%ud", &cols) == 1) {
				output_width = cols;
			}
		} else if (isatty(STDOUT_FILENO)) {
			struct winsize ws;
			ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
			output_width = ws.ws_col;
		}
	}

	clock_gettime(CLOCK_REALTIME, &ts_now);
	
	dir_stack.ndirs = 0;
	dir_stack.alloc = 8;
	dir_stack.dirs = malloc(dir_stack.alloc * sizeof dir_stack.dirs[0]);

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		argc = 1;
		*argv = ".";
	}

	if (link_mode == LINK_MODE_FOLLOW_ALL) {
		root_stat = &stat;
		nonroot_stat = &stat;
	} else if (link_mode == LINK_MODE_FOLLOW_OPERAND) {
		root_stat = &stat;
		nonroot_stat = &lstat;
	} else { // LINK_MODE_FOLLOW_NEVER
		root_stat = &lstat;
		nonroot_stat = &lstat;
	}

	// Output dir names if recursive
	if (dir_mode == DIR_MODE_RECURSE) {
		output_dirnames = true;
	}

	size_t nfiles = 0, files_alloc = 8;
	size_t ndirs = 0, dirs_alloc = 8;
	struct file_info *files = malloc(files_alloc * sizeof *files);
	struct file_info *dirs = malloc(dirs_alloc * sizeof *dirs);

	// Sort args into directories and other things
	// Also, sanity check: are all args accessible?
	for (size_t i = 0; i < argc; ++i) {
		struct stat f_stat;

		if (root_stat(argv[i], &f_stat)) {
			perrorf("%s: '%s'", argv0, argv[i]);
			had_err = true;
			continue;
		}

		struct file_info info = get_file_info(argv[i], argv[i], f_stat);

		if (S_ISDIR(info.mode) && dir_mode != DIR_MODE_NO_ENTER) {
			insert_sorted_entry(info, &dirs, &ndirs, &dirs_alloc);
		} else {
			insert_sorted_entry(info, &files, &nfiles, &files_alloc);
		}

		// Also output dir names if a combination of non-directories and
		// directories are given, or if multiple directories are given
		if (argc > 1 && S_ISDIR(f_stat.st_mode)) {
			output_dirnames = true;
		}
	}

	output_files(files, nfiles);

	for (size_t i = 0; i < ndirs; ++i) {
		handle_dir(dirs[i].name);
	}

	for (size_t i = 0; i < nfiles; ++i) {
		if (files[i].link_target) free(files[i].link_target);
	}

	for (size_t i = 0; i < ndirs; ++i) {
		if (dirs[i].link_target) free(dirs[i].link_target);
	}

	free(files);
	free(dirs);

	free(dir_stack.dirs);

	return had_err ? 1 : 0;
}
