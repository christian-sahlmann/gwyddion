# @(#) $Id$

toolsdir = $(gwymoduledir)/tool

module_ldflags = -shared -Wl,-soname

tools_PROGRAMS = \
	crop.so \
	level3.so \
	pointer.so \
	profile.so \
	stats.so \
	sfunctions.so \
	filter.so \
	polynom.so

EXTRA_DIST = \
	makefile.msc

crop_so_SOURCES = crop.c
crop_so_LDFLAGS = $(module_ldflags),crop.so

level3_so_SOURCES = level3.c
level3_so_LDFLAGS = $(module_ldflags),level3.so

pointer_so_SOURCES = pointer.c
pointer_so_LDFLAGS = $(module_ldflags),pointer.so

profile_so_SOURCES = profile.c
profile_so_LDFLAGS = $(module_ldflags),profile.so

stats_so_SOURCES = stats.c
stats_so_LDFLAGS = $(module_ldflags),stats.so

sfunctions_so_SOURCES = sfunctions.c
sfunctions_so_LDFLAGS = $(module_ldflags),sfunctions.so

filter_so_SOURCES = filter.c
filter_so_LDFLAGS = $(module_ldflags),filter.so

polynom_so_SOURCES = polynom.c
polynom_so_LDFLAGS = $(module_ldflags),polynom.so

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Module\"

clean-local:
	rm -f core.* *~
