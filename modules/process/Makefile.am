# @(#) $Id$

processdir = $(gwymoduledir)/process

module_ldflags = -shared -Wl,-soname

process_PROGRAMS = \
	basicops.so \
	cwt.so \
	facet-level.so \
	fft.so \
	level.so \
	rotate.so \
	scale.so \
	shade.so \
        grain_mark.so \
	grain_rem_threshold.so \
	grain_wshed.so

EXTRA_DIST = \
	makefile.msc

basicops_so_SOURCES = basicops.c
basicops_so_LDFLAGS = $(module_ldflags),basicops.so

level_so_SOURCES = level.c
level_so_LDFLAGS = $(module_ldflags),level.so

rotate_so_SOURCES = rotate.c
rotate_so_LDFLAGS = $(module_ldflags),rotate.so

scale_so_SOURCES = scale.c
scale_so_LDFLAGS = $(module_ldflags),scale.so

fft_so_SOURCES = fft.c
fft_so_LDFLAGS = $(module_ldflags),fft.so

cwt_so_SOURCES = cwt.c
cwt_so_LDFLAGS = $(module_ldflags),cwt.so

shade_so_SOURCES = shade.c
shade_so_LDFLAGS = $(module_ldflags),shade.so

facet_level_so_SOURCES = facet-level.c
facet_level_so_LDFLAGS = $(module_ldflags),facet-level.so

grain_mark_so_SOURCES = grain_mark.c
grain_mark_so_LDFLAGS = $(module_ldflags),grain_mark.so

grain_rem_threshold_so_SOURCES = grain_rem_threshold.c
grain_rem_threshold_so_LDFLAGS = $(module_ldflags),grain_rem_threshold.so

grain_wshed_so_SOURCES = grain_wshed.c
grain_wshed_so_LDFLAGS = $(module_ldflags),grain_wshed.so



INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Module\"

clean-local:
	rm -f core.* *~
