# @(#) $Id$

processdir = $(gwyplugindir)/process

process_SCRIPTS = \
	divide_by_2.py \
	cutoff.pl \
	yellow.sh

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Plug-in\"

clean-local:
	rm -f core.* *~
