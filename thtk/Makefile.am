pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = thtk.pc.in

lib_LTLIBRARIES = libthtk.la

libthtk_la_LDFLAGS = -version-info 1:0:0 -no-undefined $(OPENMP_LDFLAGS)
libthtk_la_CFLAGS = $(OPENMP_CFLAGS) @LIBCFLAGS@
libthtk_la_SOURCES = \
	bits.c \
	bits.h \
	error.c \
	io.c \
	thcrypt.c \
	thcrypt.h \
	thcrypt105.c \
	thcrypt105.h \
	thdat.c \
	thdat.h \
	thdat02.c \
	thdat06.c \
	thdat08.c \
	thdat95.c \
	thdat105.c \
	thlzss.c \
	thlzss.h \
	thrle.c \
	thrle.h \
	util.c \
	util.h \
	rng_mt.c \
	rng_mt.h
