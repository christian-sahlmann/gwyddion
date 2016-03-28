# @(#) $Id$

# Put sources that are for some reason disabled yet present in svn checkouts
# here.  This makes them distributed and translations of their messages are
# kept.  If we remove them completely, various bits can get lost.
disabled_sources = \
	nanoindent_adjust.c \
	superresolution.c

processdir = $(pkglibdir)/modules/process

process_LTLIBRARIES = \
	acf2d.la \
	angle_dist.la \
	arithmetic.la \
	averaging.la \
	basicops.la \
	bdep_synth.la \
	calcoefs_load.la \
	calcoefs_new.la \
	calcoefs_simple.la \
	calcoefs_view.la \
	calibrate.la \
	coerce.la \
	col_synth.la \
	convolution_filter.la \
	correct_affine.la \
	crosscor.la \
	curvature.la \
	cwt.la \
	deposit_synth.la \
	diff_synth.la \
	domain_synth.la \
	drift.la \
	dwt.la \
	dwtanisotropy.la \
	edge.la \
	entropy.la \
	extend.la \
	extract_path.la \
	facet-level.la \
	facet_analysis.la \
	fbm_synth.la \
	fft.la \
	fft_filter_1d.la \
	fft_filter_2d.la \
	fft_profile.la \
	fft_synth.la \
	fit_2d.la \
	flatten_base.la \
	fraccor.la \
	fractal.la \
	gradient.la \
	grain_cross.la \
	grain_dist.la \
	grain_edge.la \
	grain_filter.la \
	grain_mark.la \
	grain_stat.la \
	grain_wshed.la \
	hough.la \
	immerse.la \
	indent_analyze.la \
	laplace.la \
	latsim.la \
	lat_synth.la \
	level.la \
	level_grains.la \
	linecorrect.la \
	linematch.la \
	lno_synth.la \
	local_contrast.la \
	mark_disconn.la \
	mark_with.la \
	maskcor.la \
	mask_distribute.la \
	mask_edt.la \
	mask_morph.la \
	maskops.la \
	mcrop.la \
	measure_lattice.la \
	median-bg.la \
	merge.la \
	neural.la \
	noise_synth.la \
	obj_synth.la \
	otsu_threshold.la \
	outliers.la \
	pat_synth.la \
	pid.la \
	polylevel.la \
	polydistort.la \
	presentationops.la \
	psdf_logphi.la \
	rank.la \
	rotate.la \
	semsim.la \
	scale.la \
	scars.la \
	shade.la \
	slope_dist.la \
	sphere-revolve.la \
	straighten_path.la \
	threshold.la \
	tilt.la \
	tip_blind.la \
	tip_model.la \
	tipops.la \
	unrotate.la \
	volumize.la \
	volumize_layers.la \
	wave_synth.la \
	wpour_mark.la \
	xydenoise.la \
	xyzize.la

EXTRA_DIST = \
	$(disabled_sources)

if OS_MSWIN
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
angle_dist_la_SOURCES          = angle_dist.c preview.h
arithmetic_la_SOURCES          = arithmetic.c preview.h
averaging_la_SOURCES           = averaging.c preview.h
basicops_la_SOURCES            = basicops.c
bdep_synth_la_SOURCES          = bdep_synth.c dimensions.h synth.h preview.h
calcoefs_load_la_SOURCES       = calcoefs_load.c preview.h
calcoefs_new_la_SOURCES        = calcoefs_new.c preview.h
calcoefs_simple_la_SOURCES     = calcoefs_simple.c
calcoefs_view_la_SOURCES       = calcoefs_view.c preview.h
calibrate_la_SOURCES           = calibrate.c preview.h
coerce_la_SOURCES              = coerce.c preview.h
col_synth_la_SOURCES           = col_synth.c dimensions.h synth.h preview.h
convolution_filter_la_SOURCES  = convolution_filter.c convolutionfilterpreset.h preview.h
correct_affine_la_SOURCES      = correct_affine.c preview.h
crosscor_la_SOURCES            = crosscor.c
cwt_la_SOURCES                 = cwt.c preview.h
curvature_la_SOURCES           = curvature.c preview.h
deposit_synth_la_SOURCES       = deposit_synth.c dimensions.h preview.h
diff_synth_la_SOURCES          = diff_synth.c dimensions.h synth.h preview.h
drift_la_SOURCES               = drift.c preview.h
dwt_la_SOURCES                 = dwt.c preview.h
dwtanisotropy_la_SOURCES       = dwtanisotropy.c
edge_la_SOURCES                = edge.c preview.h
entropy_la_SOURCES             = entropy.c preview.h
extend_la_SOURCES              = extend.c preview.h
extract_path_la_SOURCES        = extract_path.c
facet_analysis_la_SOURCES      = facet_analysis.c preview.h
facet_level_la_SOURCES         = facet-level.c preview.h
fbm_synth_la_SOURCES           = fbm_synth.c dimensions.h synth.h preview.h
fft_filter_1d_la_SOURCES       = fft_filter_1d.c preview.h
fft_filter_2d_la_SOURCES       = fft_filter_2d.c preview.h
fft_profile_la_SOURCES         = fft_profile.c preview.h
fft_la_SOURCES                 = fft.c preview.h
fft_synth_la_SOURCES           = fft_synth.c dimensions.h synth.h preview.h
fit_2d_la_SOURCES              = fit_2d.c preview.h
flatten_base_la_SOURCES        = flatten_base.c
fractal_la_SOURCES             = fractal.c preview.h
fraccor_la_SOURCES             = fraccor.c
gradient_la_SOURCES            = gradient.c
grain_cross_la_SOURCES         = grain_cross.c
grain_dist_la_SOURCES          = grain_dist.c
grain_edge_la_SOURCES          = grain_edge.c preview.h
grain_filter_la_SOURCES        = grain_filter.c preview.h
grain_mark_la_SOURCES          = grain_mark.c preview.h
grain_stat_la_SOURCES          = grain_stat.c
grain_wshed_la_SOURCES         = grain_wshed.c preview.h
hough_la_SOURCES               = hough.c preview.h
immerse_la_SOURCES             = immerse.c preview.h
indent_analyze_la_SOURCES      = indent_analyze.c preview.h
laplace_la_SOURCES             = laplace.c
lat_synth_la_SOURCES           = lat_synth.c dimensions.h synth.h preview.h
latsim_la_SOURCES              = latsim.c preview.h
level_la_SOURCES               = level.c preview.h
level_grains_la_SOURCES        = level_grains.c
linecorrect_la_SOURCES         = linecorrect.c
linematch_la_SOURCES           = linematch.c preview.h
lno_synth_la_SOURCES           = lno_synth.c dimensions.h synth.h preview.h
local_contrast_la_SOURCES      = local_contrast.c preview.h
mark_disconn_la_SOURCES        = mark_disconn.c preview.h
mark_with_la_SOURCES           = mark_with.c preview.h
maskcor_la_SOURCES             = maskcor.c
mask_distribute_la_SOURCES     = mask_distribute.c preview.h
mask_edt_la_SOURCES            = mask_edt.c
mask_morph_la_SOURCES          = mask_morph.c
maskops_la_SOURCES             = maskops.c
mcrop_la_SOURCES               = mcrop.c
measure_lattice_la_SOURCES     = measure_lattice.c preview.h
median_bg_la_SOURCES           = median-bg.c preview.h
merge_la_SOURCES               = merge.c
#nanoindent_adjust_la_SOURCES  = nanoindent_adjust.c
neural_la_SOURCES              = neural.c neuraldata.h
noise_synth_la_SOURCES         = noise_synth.c dimensions.h synth.h preview.h
obj_synth_la_SOURCES           = obj_synth.c dimensions.h synth.h preview.h
otsu_threshold_la_SOURCES      = otsu_threshold.c
outliers_la_SOURCES            = outliers.c
pat_synth_la_SOURCES           = pat_synth.c dimensions.h synth.h preview.h
pid_la_SOURCES                 = pid.c preview.h
polylevel_la_SOURCES           = polylevel.c preview.h
polydistort_la_SOURCES         = polydistort.c preview.h
presentationops_la_SOURCES     = presentationops.c
psdf_logphi_la_SOURCES         = psdf_logphi.c preview.h
rank_la_SOURCES                = rank.c preview.h
rotate_la_SOURCES              = rotate.c preview.h
scale_la_SOURCES               = scale.c preview.h
scars_la_SOURCES               = scars.c preview.h
semsim_la_SOURCES              = semsim.c preview.h
shade_la_SOURCES               = shade.c preview.h
slope_dist_la_SOURCES          = slope_dist.c preview.h
sphere_revolve_la_SOURCES      = sphere-revolve.c preview.h
straighten_path_la_SOURCES     = straighten_path.c preview.h
#superresolution_la_SOURCES    = superresolution.c
threshold_la_SOURCES           = threshold.c preview.h
tilt_la_SOURCES                = tilt.c preview.h
tip_blind_la_SOURCES           = tip_blind.c preview.h
tip_model_la_SOURCES           = tip_model.c preview.h
tipops_la_SOURCES              = tipops.c
unrotate_la_SOURCES            = unrotate.c preview.h
volumize_la_SOURCES            = volumize.c
volumize_layers_la_SOURCES     = volumize_layers.c preview.h
wave_synth_la_SOURCES          = wave_synth.c dimensions.h synth.h preview.h
wpour_mark_la_SOURCES          = wpour_mark.c preview.h
xydenoise_la_SOURCES           = xydenoise.c
xyzize_la_SOURCES              = xyzize.c

clean-local:
	rm -f core.* *~

