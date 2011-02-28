# @(#) $Id$

# Some combinations of the tools do not give these us automatically.
abs_top_srcdir = @abs_top_srcdir@
abs_srcdir = @abs_srcdir@

SUBDIRS = \
	glmaterials \
	gradients

pkgconfigdatadir = $(libdir)/pkgconfig
gwyddionappdir = $(datadir)/applications
gwyddionmimedir = $(datadir)/mime/packages
schemadir = $(GCONF_SCHEMA_FILE_DIR)

docsrc = $(top_builddir)/devel-docs
vim_syn_gen = $(abs_top_srcdir)/utils/vim-syn-gen.py
gwyddion_vim_cfg = $(abs_srcdir)/gwyddion-cfg.py

pkgconfigdata_DATA = gwyddion.pc
gwyddionapp_DATA = gwyddion.desktop
gwyddionmime_DATA = gwyddion.xml
man1_MANS = gwyddion.1
if GCONF_SCHEMAS_INSTALL
schema_DATA = gwyddion-thumbnailer.schemas
endif

EXTRA_DIST = \
	gwyddion-thumbnailer.schemas.in \
	gwyddion.nsit.in \
	gwyddion.pc.in \
	gwyddion.spec.in \
	gwyddion-cfg.py \
	gwyddion.vim \
	gwyddion.1 \
	mingw32-gwyddion-libs.spec.in \
	thumbnailer-schemas.xsl \
	$(gwyddionapp_DATA) \
	$(gwyddionmime_DATA)

# gwyddion.vim generation
gwyddion_vim_sources = \
	$(docsrc)/libgwyapp/libgwyapp-decl.txt \
	$(docsrc)/libgwyddion/libgwyddion-decl.txt \
	$(docsrc)/libgwydgets/libgwydgets-decl.txt \
	$(docsrc)/libgwydraw/libgwydraw-decl.txt \
	$(docsrc)/libgwymodule/libgwymodule-decl.txt \
	$(docsrc)/libgwyprocess/libgwyprocess-decl.txt

file_magic_sources = $(top_srcdir)/modules/file/*.c

CLEANFILES = \
	gwyddion-thumbnailer.schemas

MAINTAINERCLEANFILES = \
	gwyddion.vim \
	gwyddion.xml

# FIXME: this breaks VPATH build
# Maintainer mode and VPATH are exclusive!
if MAINTAINER_MODE
gwyddion.vim: $(gwyddion_vim_sources) $(gwyddion_vim_cfg) $(vim_syn_gen)
	$(AM_V_GEN)cd $(top_builddir); \
	$(PYTHON) $(vim_syn_gen) $(gwyddion_vim_cfg) >data/gwyddion.vim

# Freedesktop MIME associations
gwyddion.xml: $(top_srcdir)/utils/extract-file-magic.py $(file_magic_sources)
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/utils/extract-file-magic.py \
	  FREEDESKTOP $(file_magic_sources) >gwyddion.xml

# File format table for user guide
file-format-table.xml: $(top_srcdir)/utils/extract-file-magic.py $(file_magic_sources)
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/utils/extract-file-magic.py \
	  USERGUIDE $(file_magic_sources) >file-format-table.xml

# Generate Gnome thumbnailer schemas
gwyddion-thumbnailer.schemas.in: thumbnailer-schemas.xsl $(srcdir)/gwyddion.xml
	$(AM_V_GEN)$(XSLTPROC) $(srcdir)/thumbnailer-schemas.xsl $(srcdir)/gwyddion.xml \
	  >gwyddion-thumbnailer.schemas.in
endif

# Fix paths in Gnome thumbnailer schemas
# NB: We must use sed, not autoconf @-replacements, because that would leave
# unexpanded ${prefix} in the schemas file.
# Depend on Makefile, this may run this rule perhaps too often but ensures
# the paths are updated every time they should.
gwyddion-thumbnailer.schemas: gwyddion-thumbnailer.schemas.in Makefile
	$(AM_V_GEN)$(SED) -e 's/^\(<gconfschemafile\) .*\(>\)/\1\2/' \
	  -e 's#[@]bindir[@]#$(bindir)#' \
	   $(srcdir)/gwyddion-thumbnailer.schemas.in \
	   >gwyddion-thumbnailer.schemas

# Desktop file installation
# FIXME: Maybe not according to current standards
install-data-hook:
if DESKTOP_FILE_UPDATE
	if test -z "$(DESTDIR)"; then \
		$(UPDATE_DESKTOP_DATABASE) $(DESTDIR)$(datadir)/applications; \
		$(UPDATE_MIME_DATABASE) $(DESTDIR)$(datadir)/mime; \
	fi
endif
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)" -a -z "$(DISABLE_GCONF_SCHEMAS_INSTALL)"; then \
	  GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	    $(GCONFTOOL) --makefile-install-rule $(schema_DATA); \
	fi
endif

uninstall-hook:
if DESKTOP_FILE_UPDATE
	if test -z "$(DESTDIR)"; then \
		$(UPDATE_DESKTOP_DATABASE) $(DESTDIR)$(datadir)/applications; \
		$(UPDATE_MIME_DATABASE) $(DESTDIR)$(datadir)/mime; \
	fi
endif
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)" -a -n "$(DISABLE_GCONF_SCHEMAS_INSTALL)"; then \
	  GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	    $(GCONFTOOL) --makefile-uninstall-rule $(schema_DATA); \
	fi
endif
