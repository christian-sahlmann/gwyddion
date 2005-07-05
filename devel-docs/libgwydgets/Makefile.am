## Process this file with automake to produce Makefile.in

# This is a blank Makefile.am for using gtk-doc.
# Copy this to your project's API docs directory and modify the variables to
# suit your project. See the GTK+ Makefiles in gtk+/docs/reference for examples
# of using the various options.

# The name of the module, e.g. 'glib'.
DOC_MODULE = libgwydgets
# A misguided directory name...
DOC_DIRNAME = $(DOC_MODULE)

# The top-level SGML file. Change it if you want.
DOC_MAIN_SGML_FILE = $(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting functions and macros.
DOC_SOURCE_DIR = ${top_srcdir}/$(DOC_DIRNAME)

# Extra options to pass to gtkdoc-scanobj or gtkdoc-scangobj.
SCANOBJ_OPTIONS =

# Extra options to supply to gtkdoc-scan.
SCAN_OPTIONS=--deprecated-guards="GWY_DISABLE_DEPRECATED"

# Extra options to supply to gtkdoc-mkdb.
MKDB_OPTIONS =

# Extra options to supply to gtkdoc-fixref.
FIXXREF_OPTIONS = \
	--extra-dir=$(srcdir)/../libgwyddion/html \
	--extra-dir=$(srcdir)/../libgwydraw/html \
	--extra-dir=$(srcdir)/../libgwyprocess/html

# Used for dependencies.
HFILE_GLOB = $(DOC_SOURCE_DIR)/*.h
CFILE_GLOB = $(DOC_SOURCE_DIR)/*.c

# Header files to ignore when scanning.
IGNORE_HFILES =

# Images to copy into HTML directory.
HTML_IMAGES = \
	$(top_srcdir)/pixmaps/gwy_3d_base-24.png \
	$(top_srcdir)/pixmaps/gwy_bold-24.png \
	$(top_srcdir)/pixmaps/gwy_color_range-24.png \
	$(top_srcdir)/pixmaps/gwy_crop-24.png \
	$(top_srcdir)/pixmaps/gwy_cwt-24.png \
	$(top_srcdir)/pixmaps/gwy_dwt-24.png \
	$(top_srcdir)/pixmaps/gwy_data_arithmetic-24.png \
	$(top_srcdir)/pixmaps/gwy_distance-24.png \
	$(top_srcdir)/pixmaps/gwy_facet_level-24.png \
	$(top_srcdir)/pixmaps/gwy_filter-24.png \
	$(top_srcdir)/pixmaps/gwy_fit_plane-24.png \
	$(top_srcdir)/pixmaps/gwy_fit_triangle-24.png \
	$(top_srcdir)/pixmaps/gwy_fix_zero-24.png \
	$(top_srcdir)/pixmaps/gwy_fft-24.png \
	$(top_srcdir)/pixmaps/gwy_flip_horizontally-24.png \
	$(top_srcdir)/pixmaps/gwy_flip_vertically-24.png \
	$(top_srcdir)/pixmaps/gwy_fractal-24.png \
	$(top_srcdir)/pixmaps/gwy_grains-24.png \
	$(top_srcdir)/pixmaps/gwy_grains_graph-24.png \
	$(top_srcdir)/pixmaps/gwy_grains_remove-24.png \
	$(top_srcdir)/pixmaps/gwy_grains_water-24.png \
	$(top_srcdir)/pixmaps/gwy_graph-24.png \
	$(top_srcdir)/pixmaps/gwy_graph_fit_func-24.png \
	$(top_srcdir)/pixmaps/gwy_graph_gauss-24.png \
	$(top_srcdir)/pixmaps/gwy_graph_halfgauss-24.png \
	$(top_srcdir)/pixmaps/gwy_graph_measure-24.png \
	$(top_srcdir)/pixmaps/gwy_graph_palette-24.png \
	$(top_srcdir)/pixmaps/gwy_graph_pointer-24.png \
	$(top_srcdir)/pixmaps/gwy_graph_ruler-24.png \
	$(top_srcdir)/pixmaps/gwy_graph_zoom_fit-24.png \
	$(top_srcdir)/pixmaps/gwy_graph_zoom_in-24.png \
	$(top_srcdir)/pixmaps/gwy_graph_zoom_out-24.png \
	$(top_srcdir)/pixmaps/gwy_gwyddion-60.png \
	$(top_srcdir)/pixmaps/gwy_gwyddion-32.png \
	$(top_srcdir)/pixmaps/gwy_less-24.png \
	$(top_srcdir)/pixmaps/gwy_light_rotate-24.png \
	$(top_srcdir)/pixmaps/gwy_line_correction-24.png \
	$(top_srcdir)/pixmaps/gwy_mask_add-24.png \
	$(top_srcdir)/pixmaps/gwy_mask_editor-24.png \
	$(top_srcdir)/pixmaps/gwy_mask_grow-24.png \
	$(top_srcdir)/pixmaps/gwy_mask_intersect-24.png \
	$(top_srcdir)/pixmaps/gwy_mask_invert-24.png \
	$(top_srcdir)/pixmaps/gwy_mask_remove-24.png \
	$(top_srcdir)/pixmaps/gwy_mask_set-24.png \
	$(top_srcdir)/pixmaps/gwy_mask_shrink-24.png \
	$(top_srcdir)/pixmaps/gwy_mask_subtract-24.png \
	$(top_srcdir)/pixmaps/gwy_more-24.png \
	$(top_srcdir)/pixmaps/gwy_none-24.png \
	$(top_srcdir)/pixmaps/gwy_pointer-24.png \
	$(top_srcdir)/pixmaps/gwy_pointer_measure-24.png \
	$(top_srcdir)/pixmaps/gwy_polynom_remove-24.png \
	$(top_srcdir)/pixmaps/gwy_profile-24.png \
	$(top_srcdir)/pixmaps/gwy_rotate-24.png \
	$(top_srcdir)/pixmaps/gwy_rotate_180-24.png \
	$(top_srcdir)/pixmaps/gwy_rotate_90_ccw-24.png \
	$(top_srcdir)/pixmaps/gwy_rotate_90_cw-24.png \
	$(top_srcdir)/pixmaps/gwy_scale-24.png \
	$(top_srcdir)/pixmaps/gwy_scars-24.png \
	$(top_srcdir)/pixmaps/gwy_shader-24.png \
	$(top_srcdir)/pixmaps/gwy_spot_remove-24.png \
	$(top_srcdir)/pixmaps/gwy_stat_quantities-24.png \
	$(top_srcdir)/pixmaps/gwy_subscript-24.png \
	$(top_srcdir)/pixmaps/gwy_superscript-24.png \
	$(top_srcdir)/pixmaps/gwy_unrotate-24.png \
	$(top_srcdir)/pixmaps/gwy_value_invert-24.png \
	$(top_srcdir)/pixmaps/gwy_window_measure-24.png \
	$(top_srcdir)/pixmaps/gwy_z_scale-24.png \
	$(top_srcdir)/pixmaps/gwy_zoom_1_1-24.png \
	$(top_srcdir)/pixmaps/gwy_zoom_fit-24.png \
	$(top_srcdir)/pixmaps/gwy_zoom_in-24.png \
	$(top_srcdir)/pixmaps/gwy_zoom_out-24.png \
	$(top_srcdir)/pixmaps/cd_line.png \
	$(top_srcdir)/pixmaps/cd_rline.png \
	$(top_srcdir)/pixmaps/cd_step.png \
	$(top_srcdir)/pixmaps/cd_rstep.png

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
content_files = standard-vector-layers.xml

# Other files to distribute.
extra_files = $(DOC_MODULE).types makefile.msc

# CFLAGS and LDFLAGS for compiling scan program. Only needed if your app/lib
# contains GtkObjects/GObjects and you want to document signals and properties.
GTKDOC_CFLAGS = -I${top_srcdir} @CFLAGS@
GTKDOC_LIBS = \
	${top_srcdir}/$(DOC_DIRNAME)/$(DOC_MODULE).la \
	${top_srcdir}/libgwyddion/libgwyddion.la \
	${top_srcdir}/libdraw/libgwydraw.la \
	${top_srcdir}/libprocess/libgwyprocess.la \
	@LDFLAGS@ @GTK_LIBS@

GTKDOC_CC = $(LIBTOOL) --mode=compile $(CC)
GTKDOC_LD = $(LIBTOOL) --mode=link $(CC)

# If you need to override some of the declarations, place them in the
# $(DOC_MODULE)-overrides.txt file and uncomment the second line here.
DOC_OVERRIDES =
#DOC_OVERRIDES = $(DOC_MODULE)-overrides.txt

##### Include generic gtk-doc rules #####
include $(top_srcdir)/devel-docs/gtk-doc.mk
