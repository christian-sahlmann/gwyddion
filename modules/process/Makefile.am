# @(#) $Id$

processdir = $(gwymoduledir)/process

module_ldflags = -shared -Wl,-soname

process_PROGRAMS = \
	angle_dist.so \
	basicops.so \
	cwt.so \
	facet-level.so \
	fft.so \
	grain_mark.so \
	grain_rem_threshold.so \
	grain_wshed.so \
 	grain_dist.so \
	level.so \
	rotate.so \
	scale.so \
	shade.so \
	slope_dist.so \
	fractal.so \
	laplace.so

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

grain_dist_so_SOURCES = grain_dist.c
grain_dist_so_LDFLAGS = $(module_ldflags),grain_dist.so

slope_dist_so_SOURCES = slope_dist.c
slope_dist_so_LDFLAGS = $(module_ldflags),slope_dist.so

angle_dist_so_SOURCES = angle_dist.c
angle_dist_so_LDFLAGS = $(module_ldflags),angle_dist.so

fractal_so_SOURCES = fractal.c
fractal_so_LDFLAGS = $(module_ldflags),fractal.so

laplace_so_SOURCES = laplace.c
laplace_so_LDFLAGS = $(module_ldflags),laplace.so

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Module\"

clean-local:
	rm -f core.* *~
