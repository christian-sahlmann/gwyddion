# @(#) $Id$

SUBDIRS = \
	libgwyddion \
	libgwyprocess \
	libgwydraw \
	libgwydgets \
	libgwymodule \
	libgwyapp

EXTRA_DIST = \
	CODING-STANDARDS \
	README \
	README.QA \
	makefile.msc \
	add-objects.py \
	extract-types.py

if ENABLE_GTK_DOC
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
endif

.PHONY: docs
