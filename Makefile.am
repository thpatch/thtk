AM_YFLAGS = -d
LDADD = @mb_LIBS@
BUILT_SOURCES = ecsparse.h ecsparse.c ecsscan.c
bin_PROGRAMS = thanm thdat thecl
EXTRA_DIST = \
	COPYING.autotools \
	COPYING.bison \
	COPYING.flex \
	COPYING.gcc \
	COPYING.glibc \
	COPYING.libpng \
	COPYING.zlib

if COMBINE
am_thanm_OBJECTS = thanm.o
am_thdat_OBJECTS = thdat.o
am_thecl_OBJECTS = thecl.o

thanm.o: $(thanm_SOURCES)
	$(COMPILE) $(thanm_CFLAGS) @combine_CFLAGS@ -c -o $@ $(filter %.c,$^)

thdat.o: $(thdat_SOURCES)
	$(COMPILE) $(thdat_CFLAGS) @combine_CFLAGS@ -c -o $@ $(filter %.c,$^)

thecl.o: $(thecl_SOURCES)
	$(COMPILE) $(thecl_CFLAGS) @combine_CFLAGS@ -c -o $@ $(filter %.c,$^) ecsparse.c ecsscan.c
endif

util_SOURCES = \
	args.c \
	args.h \
	program.c \
	program.h \
	util.c \
	util.h

thanm_CFLAGS = $(libpng_CFLAGS)
thanm_LDADD = $(LDADD) $(libpng_LIBS)
thanm_SOURCES = \
	thanm.c \
	thanm.h \
	$(util_SOURCES)

thdat_CFLAGS = $(OPENMP_CFLAGS)
thdat_SOURCES = \
	bits.c \
	bits.h \
	thcrypt.c \
	thcrypt.h \
	thdat.c \
	thdat.h \
	thdat02.c \
	thdat03.c \
	thdat06.c \
	thdat08.c \
	thdat75.c \
	thdat95.c \
	thlzss.c \
	thlzss.h \
	thrle.c \
	thrle.h \
	$(util_SOURCES)

thecl_LDADD = $(LDADD) -lm
thecl_SOURCES = \
	thecl.c \
	instr.c \
	instr.h \
	ecsparse.h \
	ecsscan.l \
	ecsparse.y \
	$(util_SOURCES)
