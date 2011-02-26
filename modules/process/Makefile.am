# @(#) $Id$

processdir = $(pkglibdir)/modules/process

process_LTLIBRARIES = \
	acf2d.la \
	angle_dist.la \
	arithmetic.la \
	basicops.la \
	calcoefs_load.la \
	calcoefs_new.la \
	calcoefs_view.la \
	calibrate.la \
	convolution_filter.la \
	crosscor.la \
	curvature.la \
	cwt.la \
	deposit_synth.la \
	drift.la \
	dwt.la \
	dwtanisotropy.la \
	edge.la \
	facet-level.la \
	facet_analysis.la \
	fft.la \
	fft_filter_1d.la \
	fft_filter_2d.la \
	fft_profile.la \
	fft_synth.la \
	fit_2d.la \
	fraccor.la \
	fractal.la \
	gradient.la \
	grain_cross.la \
	grain_dist.la \
	grain_edge.la \
	grain_mark.la \
	grain_rem_threshold.la \
	grain_wshed.la \
	hough.la \
	immerse.la \
	indent_analyze.la \
	laplace.la \
	level.la \
	level_grains.la \
	linecorrect.la \
	local_contrast.la \
	mark_with.la \
	maskcor.la \
	maskops.la \
	mcrop.la \
	median-bg.la \
	merge.la \
	noise_synth.la \
	obj_synth.la \
	outliers.la \
	pat_synth.la \
	polylevel.la \
	polydistort.la \
	presentationops.la \
	rotate.la \
	scale.la \
	scars.la \
	shade.la \
	slope_dist.la \
	sphere-revolve.la \
	threshold.la \
	tilt.la \
	tip_blind.la \
	tip_model.la \
	tipops.la \
	unrotate.la

# Put sources that are for some reason disabled yet present in svn checkouts
# here.  This makes them distributed and translations of their messages are
# kept.  If we remove them completely, various bits can get lost.
disabled_sources = \
	fft_synth.c \
	nanoindent_adjust.c \
	superresolution.c

EXTRA_DIST = \
	$(disabled_sources)

if OS_WIN32
no_undefined = -no-undefined
endif

AM_CPPFLAGS = -I$(top_srcdir) -DG_LOG_DOMAIN=\"Module\"
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

acf2d_la_SOURCES               = acf2d.c
angle_dist_la_SOURCES          = angle_dist.c
arithmetic_la_SOURCES          = arithmetic.c
basicops_la_SOURCES            = basicops.c
calcoefs_new_la_SOURCES        = calcoefs_new.c
calcoefs_load_la_SOURCES       = calcoefs_load.c
#calcoefs_simple_la_SOURCES    = calcoefs_simple.c
calcoefs_view_la_SOURCES       = calcoefs_view.c
calibrate_la_SOURCES           = calibrate.c
convolution_filter_la_SOURCES  = convolution_filter.c convolutionfilterpreset.h
crosscor_la_SOURCES            = crosscor.c
cwt_la_SOURCES                 = cwt.c
curvature_la_SOURCES           = curvature.c
deposit_synth_la_SOURCES       = deposit_synth.c
drift_la_SOURCES               = drift.c
dwt_la_SOURCES                 = dwt.c
dwtanisotropy_la_SOURCES       = dwtanisotropy.c
edge_la_SOURCES                = edge.c
facet_analysis_la_SOURCES      = facet_analysis.c
facet_level_la_SOURCES         = facet-level.c
fft_filter_1d_la_SOURCES       = fft_filter_1d.c
fft_filter_2d_la_SOURCES       = fft_filter_2d.c
fft_profile_la_SOURCES         = fft_profile.c
fft_la_SOURCES                 = fft.c
fft_synth_la_SOURCES           = fft_synth.c dimensions.h synth.h
fit_2d_la_SOURCES              = fit_2d.c
fractal_la_SOURCES             = fractal.c
fraccor_la_SOURCES             = fraccor.c
gradient_la_SOURCES            = gradient.c
grain_mark_la_SOURCES          = grain_mark.c
grain_rem_threshold_la_SOURCES = grain_rem_threshold.c
grain_wshed_la_SOURCES         = grain_wshed.c
grain_cross_la_SOURCES         = grain_cross.c
grain_dist_la_SOURCES          = grain_dist.c
grain_edge_la_SOURCES          = grain_edge.c
hough_la_SOURCES               = hough.c
immerse_la_SOURCES             = immerse.c
indent_analyze_la_SOURCES      = indent_analyze.c
laplace_la_SOURCES             = laplace.c
level_la_SOURCES               = level.c
level_grains_la_SOURCES        = level_grains.c
linecorrect_la_SOURCES         = linecorrect.c
local_contrast_la_SOURCES      = local_contrast.c
mark_with_la_SOURCES           = mark_with.c
maskcor_la_SOURCES             = maskcor.c
maskops_la_SOURCES             = maskops.c
mcrop_la_SOURCES               = mcrop.c
median_bg_la_SOURCES           = median-bg.c
merge_la_SOURCES               = merge.c
#nanoindent_adjust_la_SOURCES  = nanoindent_adjust.c
noise_synth_la_SOURCES         = noise_synth.c dimensions.h synth.h
obj_synth_la_SOURCES           = obj_synth.c dimensions.h synth.h
outliers_la_SOURCES            = outliers.c
pat_synth_la_SOURCES           = pat_synth.c dimensions.h synth.h
polylevel_la_SOURCES           = polylevel.c
polydistort_la_SOURCES         = polydistort.c
presentationops_la_SOURCES     = presentationops.c
rotate_la_SOURCES              = rotate.c
scale_la_SOURCES               = scale.c
scars_la_SOURCES               = scars.c
shade_la_SOURCES               = shade.c
slope_dist_la_SOURCES          = slope_dist.c
sphere_revolve_la_SOURCES      = sphere-revolve.c
#superresolution_la_SOURCES    = superresolution.c
threshold_la_SOURCES           = threshold.c
tilt_la_SOURCES                = tilt.c
tip_blind_la_SOURCES           = tip_blind.c
tip_model_la_SOURCES           = tip_model.c
tipops_la_SOURCES              = tipops.c
unrotate_la_SOURCES            = unrotate.c

clean-local:
	rm -f core.* *~

