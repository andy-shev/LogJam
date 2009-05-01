# we borrow the autoconf'd settings from liblivejournal,
# because they're the same here: glib and libxml.

AM_CFLAGS=-I$(top_srcdir)/util/gxr @LIVEJOURNAL_CFLAGS@

noinst_LTLIBRARIES = libblogger.la

libblogger_la_LDFLAGS = -static
libblogger_la_LIBADD = @LIVEJOURNAL_LIBS@ $(top_builddir)/util/gxr/libgxr.la
libblogger_la_SOURCES = \
	blogger.c       blogger.h

