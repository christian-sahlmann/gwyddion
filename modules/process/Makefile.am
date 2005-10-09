# @(#) $Id$

processdir = $(pkglibdir)/modules/process

process_LTLIBRARIES = \
	angle_dist.la \
	arithmetic.la \
	basicops.la \
	calibrate.la \
	crosscor.la \
	cwt.la \
	dwt.la \
	dwtanisotropy.la \
	dwtdenoise.la \
	edge.la \
	entropy.la \
	facet-level.la \
	facet_analysis.la \
	fft.la \
	fft_filter_1d.la \
	fit_2d.la \
	fraccor.la \
	fractal.la \
	gradient.la \
	grain_dist.la \
	grain_mark.la \
	grain_rem_threshold.la \
	grain_wshed.la \
	hough.la \
	laplace.la \
	level.la \
	linecorrect.la \
	local_contrast.la \
	mask_grow_shrink.la \
	maskcor.la \
	maskops.la \
	median-bg.la \
	nanoindent_adjust.la \
	nanoindent_analyze.la \
	outliers.la \
	polylevel.la \
	presentationops.la \
	rotate.la \
	scale.la \
	scars.la \
	shade.la \
	slope_dist.la \
	sphere-revolve.la \
	tip_blind.la \
	tip_dilation.la \
	tip_map.la \
	tip_model.la \
	tip_reconstruction.la \
	unrotate.la


EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt

INCLUDES = -I$(top_srcdir)

AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Module\"


if OS_WIN32
no_undefined = -no-undefined
endif

AM_LDFLAGS = -avoid-version -module $(no_undefined)

if MODULE_DEPENDENCIES
module_libadd = \
	$(top_builddir)/app/libgwyapp.la		\
	$(top_builddir)/libdraw/libgwydraw.la		\
	$(top_builddir)/libgwyddion/libgwyddion.la	\
	$(top_builddir)/libgwydgets/libgwydgets.la	\
	$(top_builddir)/libgwymodule/libgwymodule.la	\
	$(top_builddir)/libprocess/libgwyprocess.la	\
	@GTK_LIBS@
endif


#############################################################################
# If you know what an alphabet is, please keep the alphabetic order.
# If you have no idea, please return to basic school.
#############################################################################

angle_dist_la_SOURCES = angle_dist.c
angle_dist_la_LIBADD = $(module_libadd)

arithmetic_la_SOURCES = arithmetic.c
arithmetic_la_LIBADD = $(module_libadd)

basicops_la_SOURCES = basicops.c
basicops_la_LIBADD = $(module_libadd)

calibrate_la_SOURCES = calibrate.c
calibrate_la_LIBADD = $(module_libadd)

crosscor_la_SOURCES = crosscor.c
crosscor_la_LIBADD = $(module_libadd)

cwt_la_SOURCES = cwt.c
cwt_la_LIBADD = $(module_libadd)

dwt_la_SOURCES = dwt.c
dwt_la_LIBADD = $(module_libadd)

dwtanisotropy_la_SOURCES = dwtanisotropy.c
dwtanisotropy_la_LIBADD = $(module_libadd)

#dwtcorrection_la_SOURCES = dwtcorrection.c
#dwtcorrection_la_LIBADD = $(module_libadd)

fft_filter_1d_la_SOURCES = fft_filter_1d.c
fft_filter_1d_la_LIBADD = $(module_libadd)

fractal_la_SOURCES = fractal.c
fractal_la_LIBADD = $(module_libadd)

dwtdenoise_la_SOURCES = dwtdenoise.c
dwtdenoise_la_LIBADD = $(module_libadd)

edge_la_SOURCES = edge.c
edge_la_LIBADD = $(module_libadd)

entropy_la_SOURCES = entropy.c
entropy_la_LIBADD = $(module_libadd)

facet_analysis_la_SOURCES = facet_analysis.c
facet_analysis_la_LIBADD = $(module_libadd)

facet_level_la_SOURCES = facet-level.c
facet_level_la_LIBADD = $(module_libadd)

fit_2d_la_SOURCES = fit_2d.c
fit_2d_la_LIBADD = $(module_libadd)

fft_la_SOURCES = fft.c
fft_la_LIBADD = $(module_libadd)

#fractal_la_SOURCES = fractal.c
#fractal_la_LIBADD = $(module_libadd)

fraccor_la_SOURCES = fraccor.c
fraccor_la_LIBADD = $(module_libadd)

gradient_la_SOURCES = gradient.c
gradient_la_LIBADD = $(module_libadd)

grain_mark_la_SOURCES = grain_mark.c
grain_mark_la_LIBADD = $(module_libadd)

grain_rem_threshold_la_SOURCES = grain_rem_threshold.c
grain_rem_threshold_la_LIBADD = $(module_libadd)

grain_wshed_la_SOURCES = grain_wshed.c
grain_wshed_la_LIBADD = $(module_libadd)

grain_dist_la_SOURCES = grain_dist.c
grain_dist_la_LIBADD = $(module_libadd)

hough_la_SOURCES = hough.c
hough_la_LIBADD = $(module_libadd)


#indent_analyze_la_SOURCES = indent_analyze.c
#indent_analyze_la_LIBADD = $(module_libadd)

laplace_la_SOURCES = laplace.c
laplace_la_LIBADD = $(module_libadd)

level_la_SOURCES = level.c
level_la_LIBADD = $(module_libadd)

linecorrect_la_SOURCES = linecorrect.c
linecorrect_la_LIBADD = $(module_libadd)

local_contrast_la_SOURCES = local_contrast.c
local_contrast_la_LIBADD = $(module_libadd)

nanoindent_adjust_la_SOURCES = nanoindent_adjust.c
nanoindent_adjust_la_LIBADD = $(module_libadd)

nanoindent_analyze_la_SOURCES = nanoindent_analyze.c
nanoindent_analyze_la_LIBADD = $(module_libadd)

mask_grow_shrink_la_SOURCES = mask_grow_shrink.c
mask_grow_shrink_la_LIBADD = $(module_libadd)

maskcor_la_SOURCES = maskcor.c
maskcor_la_LIBADD = $(module_libadd)

maskops_la_SOURCES = maskops.c
maskops_la_LIBADD = $(module_libadd)

median_bg_la_SOURCES = median-bg.c
median_bg_la_LIBADD = $(module_libadd)

outliers_la_SOURCES = outliers.c
outliers_la_LIBADD = $(module_libadd)

polylevel_la_SOURCES = polylevel.c
polylevel_la_LIBADD = $(module_libadd)

presentationops_la_SOURCES = presentationops.c
presentationops_la_LIBADD = $(module_libadd)

rotate_la_SOURCES = rotate.c
rotate_la_LIBADD = $(module_libadd)

scale_la_SOURCES = scale.c
scale_la_LIBADD = $(module_libadd)

scars_la_SOURCES = scars.c
scars_la_LIBADD = $(module_libadd)

shade_la_SOURCES = shade.c
shade_la_LIBADD = $(module_libadd)

#stary_extract_la_SOURCES = stary_extract.c
#stary_extract_la_LIBADD = $(module_libadd)

slope_dist_la_SOURCES = slope_dist.c
slope_dist_la_LIBADD = $(module_libadd)

sphere_revolve_la_SOURCES = sphere-revolve.c
sphere_revolve_la_LIBADD = $(module_libadd)

tip_blind_la_SOURCES = tip_blind.c
tip_blind_la_LIBADD = $(module_libadd)

tip_dilation_la_SOURCES = tip_dilation.c
tip_dilation_la_LIBADD = $(module_libadd)

tip_map_la_SOURCES = tip_map.c
tip_map_la_LIBADD = $(module_libadd)

tip_model_la_SOURCES = tip_model.c
tip_model_la_LIBADD = $(module_libadd)

tip_reconstruction_la_SOURCES = tip_reconstruction.c
tip_reconstruction_la_LIBADD = $(module_libadd)

unrotate_la_SOURCES = unrotate.c
unrotate_la_LIBADD = $(module_libadd)


clean-local:
	rm -f core.* *~
