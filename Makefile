.POSIX:
include config.mk

.PHONY: clean
clean:
	rm -rf ${BUILDDIR}

${BUILDDIR}/bin/%: ${BUILDDIR}/obj/%.o ${BUILDDIR}/obj/lib/utils.o
	@mkdir -p $$(dirname $@)
	${CC} -o $@ $^ ${LDFLAGS}

${BUILDDIR}/obj/%.o: %.c config.mk
	@mkdir -p $$(dirname $@)
	${CC} ${CFLAGS} -c -o $@ $<
