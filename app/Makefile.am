# @(#) $Id$

bin_PROGRAMS = gwyddion

gwyddion_SOURCES = \
	init.c \
	gwyddion.c
gwyddion_LDADD = \
	../libgwydgets/libgwydgets.la \
	../libgwyddion/libgwyddion.la \
	../libprocess/libprocess.la \
	../libgwymodule/libgwymodule.la \
	../libdraw/libdraw.la

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG

clean-local:
	rm -f core.* *~
