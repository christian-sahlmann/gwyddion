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

# Remove titles from generated template files?
REMOVE_SECTION_TITLES = yes

# Extra options to supply to gtkdoc-mkdb.
MKDB_OPTIONS =

# Extra options to supply to gtkdoc-fixref.
FIXXREF_OPTIONS = \
	--extra-dir=$(srcdir)/../libgwyddion/html \
	--extra-dir=$(srcdir)/../libgwyprocess/html \
	--extra-dir=$(srcdir)/../libgwydraw/html \
	--extra-dir=$(srcdir)/../libgwymodule/html \
	--extra-dir=$(srcdir)/../libgwyapp/html

# Used for dependencies.
HFILE_GLOB = $(DOC_SOURCE_DIR)/*.h
CFILE_GLOB = $(DOC_SOURCE_DIR)/*.c

# Header files to ignore when scanning.
IGNORE_HFILES = stamp-gwydgettypes.h

# Images to copy into HTML directory.
HTML_IMAGES = \
	$(top_srcdir)/pixmaps/gwy_*-24.png \
	$(top_srcdir)/pixmaps/gwy_gwyddion-60.png \
	$(top_srcdir)/pixmaps/gwy_gwyddion-32.png \
	$(top_srcdir)/pixmaps/cd_line.png \
	$(top_srcdir)/pixmaps/cd_rline.png \
	$(top_srcdir)/pixmaps/cd_step.png \
	$(top_srcdir)/pixmaps/cd_rstep.png

# Includes required for types compilation
TYPES_INCLUDE = \#include \"$(DOC_DIRNAME)/gwydgets.h\"

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
content_files = standard-vector-layers.xml

# Other files to distribute.
extra_files = makefile.msc

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
