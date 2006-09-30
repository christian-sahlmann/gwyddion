# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion2.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule2.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess2.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw2.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets2.la
libgwyapp = $(top_builddir)/app/libgwyapp2.la
moduledir = $(top_srcdir)/modules

GUIDE_MAP = user-guide-modules

EXTRA_DIST = \
	genmarshal.mk \
	gwywin32.bat.in \
	mkenum.mk \
	modules-web.xsl \
	update-msvc.py \
	update-potfiles.sh \
	$(GUIDE_MAP) \
	vim-syn-gen.py

module-list: modules.xml module-list.html module-list-nocss.html

noinst_PROGRAMS = \
	dump-modules

dump_modules_SOURCES = \
	dump-modules.c

dump_modules_LDADD = @GTKGLEXT_LIBS@ \
	$(libgwyapp) \
	$(libgwymodule) \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)

CLEANFILES = \
	modules.xml \
	module-list.html \
	module-list-nocss.html

DISTCLEANFILES = \
	gwywin32.bat

INCLUDES = -I$(top_srcdir)

modules.xml: dump-modules $(GUIDE_MAP) $(moduledir)/*.c $(moduledir)/*/*.c
	./dump-modules $(GUIDE_MAP) >modules.xml

module-list.html: modules.xml modules-web.xsl
	$(XSLTPROC) modules-web.xsl modules.xml >module-list.html

module-list-nocss.html: modules.xml modules-web-nocss.xsl
	$(XSLTPROC) modules-web-nocss.xsl modules.xml >module-list-nocss.html

clean-local:
	rm -f core.* *~
