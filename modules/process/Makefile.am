# @(#) $Id$

processdir = $(pkglibdir)/modules/process

process_LTLIBRARIES = \
	acf2d.la \
	angle_dist.la \
	arithmetic.la \
	basicops.la \
	calibrate.la \
	convolution_filter.la \
	crosscor.la \
	cwt.la \
	drift.la \
	dwt.la \
	dwtanisotropy.la \
	edge.la \
	facet-level.la \
	facet_analysis.la \
	fft.la \
	fft_filter_1d.la \
	fft_filter_2d.la \
	fft_synth.la \
	fit_2d.la \
	fraccor.la \
	fractal.la \
	gradient.la \
	grain_dist.la \
	grain_mark.la \
	grain_rem_threshold.la \
	grain_wshed.la \
	hough.la \
	immerse.la \
	laplace.la \
	level.la \
	linecorrect.la \
	local_contrast.la \
	maskcor.la \
	maskops.la \
	median-bg.la \
	merge.la \
	outliers.la \
	polylevel.la \
	polydistort.la \
	presentationops.la \
	rotate.la \
	scale.la \
	scars.la \
	shade.la \
	slope_dist.la \
	sphere-revolve.la \
	tip_blind.la \
	tip_model.la \
	tipops.la \
	unrotate.la

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt

if OS_WIN32
no_undefined = -no-undefined
endif

AM_CPPFLAGS = -I$(top_buildir) -DG_LOG_DOMAIN=\"Module\"
AM_CFLAGS = @COMMON_CFLAGS@
AM_LDFLAGS = -avoid-version -module $(no_undefined) $(module_libadd)

if MODULE_DEPENDENCIES
module_libadd = \
	$(top_builddir)/app/libgwyapp2.la \
	$(top_builddir)/libdraw/libgwydraw2.la \
	$(top_builddir)/libgwyddion/libgwyddion2.la \
	$(top_builddir)/libgwydgets/libgwydgets2.la \
	$(top_builddir)/libgwymodule/libgwymodule2.la \
	$(top_builddir)/libprocess/libgwyprocess2.la \
	@GTK_LIBS@
endif

acf2d_la_SOURCES = acf2d.c
angle_dist_la_SOURCES = angle_dist.c
arithmetic_la_SOURCES = arithmetic.c
basicops_la_SOURCES = basicops.c
calibrate_la_SOURCES = calibrate.c
convolution_filter_la_SOURCES = convolution_filter.c convolutionfilterpreset.h
crosscor_la_SOURCES = crosscor.c
cwt_la_SOURCES = cwt.c
drift_la_SOURCES = drift.c
dwt_la_SOURCES = dwt.c
dwtanisotropy_la_SOURCES = dwtanisotropy.c
edge_la_SOURCES = edge.c
facet_analysis_la_SOURCES = facet_analysis.c
facet_level_la_SOURCES = facet-level.c
fft_filter_1d_la_SOURCES = fft_filter_1d.c
fft_filter_2d_la_SOURCES = fft_filter_2d.c
fft_la_SOURCES = fft.c
fft_synth_la_SOURCES = fft_synth.c
fit_2d_la_SOURCES = fit_2d.c
fractal_la_SOURCES = fractal.c
fraccor_la_SOURCES = fraccor.c
gradient_la_SOURCES = gradient.c
grain_mark_la_SOURCES = grain_mark.c
grain_rem_threshold_la_SOURCES = grain_rem_threshold.c
grain_wshed_la_SOURCES = grain_wshed.c
grain_dist_la_SOURCES = grain_dist.c
hough_la_SOURCES = hough.c
immerse_la_SOURCES = immerse.c
laplace_la_SOURCES = laplace.c
level_la_SOURCES = level.c
linecorrect_la_SOURCES = linecorrect.c
local_contrast_la_SOURCES = local_contrast.c
maskcor_la_SOURCES = maskcor.c
maskops_la_SOURCES = maskops.c
median_bg_la_SOURCES = median-bg.c
merge_la_SOURCES = merge.c
#nanoindent_adjust_la_SOURCES = nanoindent_adjust.c
outliers_la_SOURCES = outliers.c
polylevel_la_SOURCES = polylevel.c
polydistort_la_SOURCES = polydistort.c
presentationops_la_SOURCES = presentationops.c
rotate_la_SOURCES = rotate.c
scale_la_SOURCES = scale.c
scars_la_SOURCES = scars.c
shade_la_SOURCES = shade.c
slope_dist_la_SOURCES = slope_dist.c
sphere_revolve_la_SOURCES = sphere-revolve.c
#superresolution_la_SOURCES = superresolution.c
tip_blind_la_SOURCES = tip_blind.c
tip_model_la_SOURCES = tip_model.c
tipops_la_SOURCES = tipops.c
unrotate_la_SOURCES = unrotate.c

MAINTAINERCLEANFILES = makefile.msc

clean-local:
	rm -f core.* *~

