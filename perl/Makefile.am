
if HAVE_PERL
PERL_TARGET_DIR = ${prefix}@PERL_SITE_LIB@/Gwyddion
man3dir = $(mandir)/man3
POD2MAN_FLAGS = --center=$(PACKAGE)
EXTRA_DIST = $(Gwyddion_PERL)

Gwyddion_PERL = \
	Gwyddion/dump.pm

man3_pages = \
	Gwyddion::dump.3pm

all-local: man3.stamp

man3.stamp: $(Gwyddion_PERL)
	for i in $(man3_pages); do \
	    pm=`echo $$i | sed -e 's#::#/#g' -e 's/3pm$$/pm/'`; \
	    $(POD2MAN) $(POD2MAN_FLAGS) $$pm $$i; \
	done
	@touch man3.stamp

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(PERL_TARGET_DIR)
	for i in $(Gwyddion_PERL); do \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(PERL_TARGET_DIR); \
	done
	$(mkinstalldirs) $(DESTDIR)$(man3dir)
	for i in $(man3_pages); do \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(man3dir); \
	done

uninstall-local:
	for i in $(Gwyddion_PERL); do \
	    rm -f $(DESTDIR)$(PERL_TARGET_DIR)/$$i; \
	done
	-rmdir $(DESTDIR)$(PERL_TARGET_DIR)
	for i in $(man3_pages); do \
	    rm -f $(DESTDIR)$(man3dir)/$$i; \
	done
	-rmdir $(DESTDIR)$(man3dir)
endif

clean-local:
	rm -f *~ *.bak $(man3_pages) man3.stamp

