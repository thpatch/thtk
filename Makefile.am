AM_YFLAGS = -d -r all
AM_CFLAGS = -pedantic -Wall -Wextra
LDADD = @mb_LIBS@
BUILT_SOURCES = ecsparse.h ecsparse.c ecsscan.c
bin_PROGRAMS = datpacker eclc ecldump thanm

if COMBINE
am_datpacker_OBJECTS = datpacker.o
am_eclc_OBJECTS = eclc.o
am_ecldump_OBJECTS = ecldump.o
am_thanm_OBJECTS = thanm.o

datpacker.o: $(datpacker_SOURCES)
	$(COMPILE) $(datpacker_CFLAGS) @combine_CFLAGS@ -c -o $@ $(filter %.c,$^)

eclc.o: $(eclc_SOURCES)
	$(COMPILE) $(eclc_CFLAGS) @combine_CFLAGS@ -c -o $@ $(filter %.c,$^) ecsparse.c ecsscan.c

ecldump.o: $(ecldump_SOURCES)
	$(COMPILE) $(ecldump_CFLAGS) @combine_CFLAGS@ -c -o $@ $(filter %.c,$^)

thanm.o: $(thanm_SOURCES)
	$(COMPILE) $(thanm_CFLAGS) @combine_CFLAGS@ -c -o $@ $(filter %.c,$^)
endif

util_SOURCES = \
	args.c \
	args.h \
	program.c \
	program.h \
	util.c \
	util.h

datpacker_CFLAGS = $(OPENMP_CFLAGS)
datpacker_SOURCES = \
	bits.c \
	bits.h \
	datpacker-th02.c \
	datpacker-th06.c \
	datpacker-th08.c \
	datpacker-th75.c \
	datpacker-th95.c \
	datpacker.c \
	datpacker.h \
	thcrypt.c \
	thcrypt.h \
	thdat.c \
	thdat.h \
	thlzss.c \
	thlzss.h \
	thrle.c \
	thrle.h \
	$(util_SOURCES)

eclc_SOURCES = \
	instr.c \
	instr.h \
	ecsparse.h \
	ecsscan.l \
	ecsparse.y \
	$(util_SOURCES)

ecldump_LDADD = $(LDADD) -lm
ecldump_SOURCES = \
	ecldump.c \
	instr.c \
	instr.h \
	ecsparse.h \
	$(util_SOURCES)

thanm_CFLAGS = $(libpng_CFLAGS)
thanm_LDADD = $(LDADD) $(libpng_LIBS)
thanm_SOURCES = \
	thanm.c \
	thanm.h \
	$(util_SOURCES)
