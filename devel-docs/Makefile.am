# @(#) $Id$

SUBDIRS = \
	libgwyddion \
	libgwyprocess \
	libgwydraw \
	libgwydgets \
	libgwymodule
EXTRA_DIST = \
	README \
	gtk-doc.mk

docs:
	@set fnord $$MAKEFLAGS; amf=$$2; \
	target=`echo $@ | sed s/-recursive//`; \
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  echo "Making $$target in $$subdir"; \
	  if test "$$subdir" = "."; then :; else \
	    local_target="$$target"; \
	  fi; \
	  (cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) $$local_target) \
	   || case "$$amf" in *=*) exit 1;; *k*) fail=yes;; *) exit 1;; esac; \
	done; test -z "$$fail"

.PHONY: docs
