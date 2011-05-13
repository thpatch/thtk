# vim:noet
ACLOCAL_AMFLAGS = -I m4
AM_YFLAGS = -d
BUILT_SOURCES = ecsparse.h ecsparse.c ecsscan.c
bin_PROGRAMS = thanm thdat thecl thmsg
noinst_LIBRARIES = libutil.a
EXTRA_DIST = \
	COPYING.autotools \
	COPYING.bison \
	COPYING.flex \
	COPYING.gcc \
	COPYING.glibc \
	COPYING.libpng \
	COPYING.zlib
dist_man1_MANS = \
	thanm.1 \
	thdat.1 \
	thecl.1 \
	thmsg.1

libutil_a_SOURCES = \
	args.c \
	args.h \
	bits.c \
	bits.h \
	file.c \
	file.h \
	list.c \
	list.h \
	program.c \
	program.h \
	util.c \
	util.h \
	value.c \
	value.h

thanm_CFLAGS = $(libpng_CFLAGS)
thanm_LDADD = libutil.a $(libpng_LIBS)
thanm_SOURCES = \
	image.c \
	image.h \
	thanm.c \
	thanm.h

thdat_CFLAGS = $(OPENMP_CFLAGS)
thdat_LDADD = libutil.a
thdat_SOURCES = \
	thcrypt.c \
	thcrypt.h \
	thdat.c \
	thdat.h \
	thdat02.c \
	thdat03.c \
	thdat06.c \
	thdat08.c \
	thdat95.c \
	thlzss.c \
	thlzss.h \
	thrle.c \
	thrle.h

thecl_LDADD = libutil.a -lm
thecl_SOURCES = \
	thecl.c \
	thecl.h \
	thecl06.c \
	thecl10.c \
	ecsparse.h \
	ecsscan.l \
	ecsparse.y \
	expr.c \
	expr.h

thmsg_LDADD = libutil.a
thmsg_SOURCES = \
	thmsg.c \
	thmsg.h \
	thmsg06.c \
	thmsg95.c

release: all
	rm -Rf "$(distdir)"
	test -d "$(distdir)" || mkdir "$(distdir)"
	cp $(bin_PROGRAMS) "$(distdir)"
	for i in NEWS README COPYING $(EXTRA_DIST); do cp $$i "$(distdir)/`basename $$i`.txt"; done
	for i in $(dist_man1_MANS); do mandoc -Thtml $$i > "$(distdir)/`basename $$i`.html"; done
	rm -f $(PACKAGE)-bin-$(VERSION).zip
	zip -rq $(PACKAGE)-bin-$(VERSION).zip $(distdir)
	rm -Rf "$(distdir)"
