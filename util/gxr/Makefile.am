
# we borrow the autoconf'd settings from liblivejournal,
# because they're the same here: glib and libxml.

AM_CFLAGS=@LIVEJOURNAL_CFLAGS@

noinst_LTLIBRARIES = libgxr.la

libgxr_la_LDFLAGS = -static
libgxr_la_LIBADD = @LIVEJOURNAL_LIBS@
libgxr_la_SOURCES = \
	gxr.c       gxr.h

