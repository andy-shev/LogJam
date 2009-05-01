LJPATH=$(top_srcdir)/livejournal
AM_CFLAGS = -g -I$(LJPATH) @LIVEJOURNAL_CFLAGS@ @CURL_CFLAGS@
AM_LDFLAGS = -no-install
LDADD=$(LJPATH)/liblivejournal.la @CURL_LIBS@

if ENABLE_TESTS
noinst_PROGRAMS=login request escape
login_SOURCES=login.c curl.c curl.h
request_SOURCES=request.c
escape_SOURCES=escape.c
endif
