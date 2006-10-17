# Generic glib-genmarshal rules.
# @(#) $Id$
#
# Parameters:
# GENMARSHAL_NAME -- output base name
# GENMARSHAL_PREFIX -- prefix of generated function names
#
# Adds to common variables (must set before, at least to empty):
# BUILT_SOURCES
# CLEANFILES
# EXTRA_DIST
# MAINTAINERCLEANFILES

# TODO: Detect
GLIB_GENMARSHAL = glib-genmarshal

genmarshal_built_sources = \
	$(GENMARSHAL_NAME).h \
	$(GENMARSHAL_NAME).c

EXTRA_DIST += \
	$(genmarshal_built_sources) \
	$(GENMARSHAL_NAME).list

CLEANFILES += \
	$(GENMARSHAL_NAME).c.xgen \
	$(GENMARSHAL_NAME).h.xgen

if MAINTAINER_MODE
genmarshal_self = $(top_srcdir)/utils/genmarshal.mk
genmarshal_stamp_files = $(GENMARSHAL_NAME).h.stamp

MAINTAINERCLEANFILES += $(genmarshal_built_sources) $(genmarshal_stamp_files)

BUILT_SOURCES += $(genmarshal_built_sources)

$(GENMARSHAL_NAME).h: $(GENMARSHAL_NAME).h.stamp
	@true

# XXX: Add G_GNUC_INTERNAL manually, glib-genmarshal can do it, but too new
# version is required (2.13+)
$(GENMARSHAL_NAME).h.stamp: $(GENMARSHAL_NAME).list $(genmarshal_self)
	echo '/* This is a GENERATED file */' >$(GENMARSHAL_NAME).h.xgen \
	&& $(GLIB_GENMARSHAL) --header --prefix=$(GENMARSHAL_PREFIX) \
		$(srcdir)/$(GENMARSHAL_NAME).list \
		| sed -e 's/^extern /G_GNUC_INTERNAL /' \
		>>$(GENMARSHAL_NAME).h.xgen \
	&& ( cmp -s $(GENMARSHAL_NAME).h.xgen $(GENMARSHAL_NAME).h \
		|| cp $(GENMARSHAL_NAME).h.xgen $(GENMARSHAL_NAME).h) \
	&& rm -f $(GENMARSHAL_NAME).h.xgen \
	&& echo timestamp >$(GENMARSHAL_NAME).h.stamp

$(GENMARSHAL_NAME).c: $(GENMARSHAL_NAME).list $(genmarshal_self)
	echo '/* This is a GENERATED file */' >$(GENMARSHAL_SELF).c.xgen \
	&& echo '#include "$(GENMARSHAL_NAME).h"' >>$(GENMARSHAL_NAME).c.xgen \
	&& $(GLIB_GENMARSHAL) --body --prefix=$(GENMARSHAL_PREFIX) \
		$(srcdir)/$(GENMARSHAL_NAME).list \
		| sed -e 's/^\( *\)\(GValue *\* *return_value,\)/\1G_GNUC_UNUSED \2/' \
			  -e 's/^\( *\)\(gpointer *invocation_hint,\)/\1G_GNUC_UNUSED \2/' \
		>>$(GENMARSHAL_NAME).c.xgen \
	&& cp $(GENMARSHAL_NAME).c.xgen $(GENMARSHAL_NAME).c \
	&& rm -f $(GENMARSHAL_NAME).c.xgen
endif
