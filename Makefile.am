# @(#) $Id$

pkgconfigdatadir = $(libdir)/pkgconfig
gwyddionappdir = $(datadir)/applications
gwyddionmimedir = $(datadir)/mime/packages

pkgconfigdata_DATA = gwyddion.pc
gwyddionapp_DATA = data/gwyddion.desktop
gwyddionmime_DATA = data/gwyddion.xml
man1_MANS = data/gwyddion.1

SUBDIRS = \
	m4 \
	libgwyddion \
	libprocess \
	libdraw \
	libgwydgets \
	libgwymodule \
	app \
	modules \
	plugins \
	po \
	pixmaps \
	perl \
	python \
	ruby \
	devel-docs

EXTRA_DIST = \
	CODING-STANDARDS \
	INSTALL.gwyddion \
	autogen.sh \
	gwyddion-devel.iss \
	gwyddion-devel.iss.in \
	gwyddion.iss \
	gwyddion.iss.in \
	gwyddion.pc.in \
	gwyddion.spec.in \
	gwyddion.spec\
	make.msc \
	makefile.msc \
	mkinstalldirs \
	rules.msc \
	version.h \
	version.h.in \
	utils/update-msvc.py \
	utils/update-potfiles.sh \
	utils/vim-syn-gen.py \
	data/gwyddion-cfg.py \
	data/gwyddion.vim \
	data/gwyddion.1 \
	$(gwyddionapp_DATA) \
	$(gwyddionmime_DATA)

if MAINTAINER_MODE
dist-check-maintainer:
else
dist-check-maintainer:
	@echo "*** maintainer mode must be enabled in order to make dist"
	@false
endif

docs:
	(cd devel-docs && $(MAKE) $(AM_MAKEFLAGS) docs)

# gwyddion.vim generation
gwyddion_vim_sources = \
	devel-docs/libgwyapp/libgwyapp-decl.txt \
	devel-docs/libgwyddion/libgwyddion-decl.txt \
	devel-docs/libgwydgets/libgwydgets-decl.txt \
	devel-docs/libgwydraw/libgwydraw-decl.txt \
	devel-docs/libgwymodule/libgwymodule-decl.txt \
	devel-docs/libgwyprocess/libgwyprocess-decl.txt

gwyddion_vim_cfg = $(top_srcdir)/data/gwyddion-cfg.py

data/gwyddion.vim: $(gwyddion_vim_sources) $(gwyddion_vim_cfg)
	cd $(top_builddir) && $(PYTHON) $(top_srcdir)/utils/vim-syn-gen.py $(gwyddion_vim_cfg) >data/gwyddion.vim

# Desktop files
if DESKTOP_FILE_UPDATE
install-data-hook:
	$(UPDATE_DESKTOP_DATABASE) $(datadir)/applications
	$(UPDATE_MIME_DATABASE) $(datadir)/mime

uninstall-hook:
	$(UPDATE_DESKTOP_DATABASE) $(datadir)/applications
	$(UPDATE_MIME_DATABASE) $(datadir)/mime
endif

.PHONY: docs
