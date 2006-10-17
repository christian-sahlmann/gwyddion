# Generic glib-mkenum rules.
# @(#) $Id$
#
# Parameters:
# MKENUM_NAME -- output base name
# MKENUM_HFILES -- input header files
#
# Adds to common variables (must set before, at least to empty):
# BUILT_SOURCES
# CLEANFILES
# EXTRA_DIST
# MAINTAINERCLEANFILES

# TODO: Detect
GLIB_MKENUMS = glib-mkenums

mkenum_built_sources = \
	$(MKENUM_NAME).h \
	$(MKENUM_NAME).c

EXTRA_DIST += \
	$(mkenum_built_sources) \
	$(MKENUM_NAME).c.template \
	$(MKENUM_NAME).h.template

CLEANFILES += \
	$(MKENUM_NAME).c.xgen \
	$(MKENUM_NAME).h.xgen

if MAINTAINER_MODE
mkenum_stamp_files = $(MKENUM_NAME).h.stamp
mkenum_self = $(top_srcdir)/utils/mkenum.mk

MAINTAINERCLEANFILES += $(mkenum_built_sources) $(mkenum_stamp_files)

BUILT_SOURCES += $(mkenum_built_sources)

$(MKENUM_NAME).h: $(MKENUM_NAME).h.stamp
	@true

$(MKENUM_NAME).h.stamp: $(MKENUM_HFILES) $(MKENUM_NAME).h.template $(mkenum_self)
	echo '/* This is a GENERATED file */' >$(MKENUM_NAME).h.xgen \
	&& $(GLIB_MKENUMS) --template $(srcdir)/$(MKENUM_NAME).h.template \
		$(srcdir)/$(MKENUM_HFILES) \
		| sed -e 's/_\([123]\)_D/_\1D_/g' \
		>>$(MKENUM_NAME).h.xgen \
	&& ( cmp -s $(MKENUM_NAME).h.xgen $(MKENUM_NAME).h \
		|| cp $(MKENUM_NAME).h.xgen $(MKENUM_NAME).h ) \
	&& rm -f $(MKENUM_NAME).h.xgen \
	&& echo timestamp >$(MKENUM_NAME).h.stamp

$(MKENUM_NAME).c: $(MKENUM_HFILES) $(MKENUM_NAME).c.template $(mkenum_self)
	echo '/* This is a GENERATED file */' >$(MKENUM_NAME).c.xgen \
	&& $(GLIB_MKENUMS) --template $(srcdir)/$(MKENUM_NAME).c.template \
		$(srcdir)/$(MKENUM_HFILES) \
		| sed -e 's/_\([123]\)_D/_\1D_/g' \
		>>$(MKENUM_NAME).c.xgen \
	&& cp $(MKENUM_NAME).c.xgen $(MKENUM_NAME).c  \
	&& rm -f $(MKENUM_NAME).c.xgen
endif

