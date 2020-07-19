#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const char *usage[] = {
	"[message...]",
	NULL
};

enum {BUFFER_SIZE = 1<<15}; // 32KiB

int main(int argc, char *argv[]) {
	char buf[BUFFER_SIZE], *p = buf;
	size_t len = 0;

	if (argc == 1) {
		argv[0] = "y";
	} else {
		argv++;
		argc--;
	}

	for (;;) {
		size_t blen = 0;
		for (int i = 0; i < argc; i++) {
			// Space-separate arguments
			if (i) {
				if (++blen + len > BUFFER_SIZE) goto buf_done;
				*p++ = ' ';
			}

			size_t slen = strlen(argv[i]);
			blen += slen;
			if (blen + len > BUFFER_SIZE) goto buf_done;

			memcpy(p, argv[i], slen);
			p += slen;
		}

		if (++blen + len > BUFFER_SIZE) goto buf_done;
		*p++ = '\n';
		len += blen;
	}
buf_done:;

	size_t offset = 0;
	for (;;) {
		offset += write(STDOUT_FILENO, buf + offset, len - offset);
		switch (errno) {
		case 0:
			offset = 0;
			break;

		case EAGAIN:
			errno = 0;
			break;

		default:
			perror("write");
			return 1;
		}
	}
}
