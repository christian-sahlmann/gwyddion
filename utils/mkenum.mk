# Generic glib-mkenum rules.
# @(#) $Id$
#
# Parameters:
# MKENUM_NAME -- output base name
# MKENUM_HFILES -- input header files
#
# Sets common variables (use += after inclusion to add content):
# BUILT_SOURCES
# CLEANFILES
# EXTRA_DIST
# MAINTAINERCLEANFILES

mkenum_built_sources = \
	$(MKENUM_NAME).h \
	$(MKENUM_NAME).c

EXTRA_DIST = \
	$(mkenum_built_sources) \
	$(MKENUM_NAME).c.template \
	$(MKENUM_NAME).h.template

CLEANFILES = \
	$(MKENUM_NAME).c.xgen \
	$(MKENUM_NAME).h.xgen

if MAINTAINER_MODE
mkenum_stamp_files = stamp-$(MKENUM_NAME).h

MAINTAINERCLEANFILES = $(mkenum_built_sources) $(mkenum_stamp_files)

BUILT_SOURCES = $(mkenum_built_sources)

$(MKENUM_NAME).h: stamp-$(MKENUM_NAME).h
	@true

stamp-$(MKENUM_NAME).h: $(MKENUM_HFILES) $(MKENUM_NAME).h.template
	( cd $(srcdir) \
	  && glib-mkenums --template $(MKENUM_NAME).h.template $(MKENUM_HFILES) ) \
	  | sed -e 's/_\([123]\)_D/_\1D_/g' >$(MKENUM_NAME).h.xgen \
	&& (cmp -s $(MKENUM_NAME).h.xgen $(MKENUM_NAME).h \
		|| cp $(MKENUM_NAME).h.xgen $(MKENUM_NAME).h ) \
	&& rm -f $(MKENUM_NAME).h.xgen \
	&& echo timestamp >stamp-$(MKENUM_NAME).h

$(MKENUM_NAME).c: $(MKENUM_HFILES) $(MKENUM_NAME).c.template
	( cd $(srcdir) \
	  && glib-mkenums --template $(MKENUM_NAME).c.template $(MKENUM_HFILES) ) \
	>$(MKENUM_NAME).c.xgen \
	&& cp $(MKENUM_NAME).c.xgen $(MKENUM_NAME).c  \
	&& rm -f $(MKENUM_NAME).c.xgen
else
# Always set the variables
MAINTAINERCLEANFILES =
BUILT_SOURCES =
endif

