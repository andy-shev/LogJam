bin_PROGRAMS = logjam-xmms-client

localedir=$(datadir)/locale
pixmapdir=$(datadir)/pixmaps

AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" -DPIXMAPDIR=\"$(pixmapdir)\" @XMMS_CFLAGS@

logjam_xmms_client_SOURCES = logjam-xmms-client.c
logjam_xmms_client_LDADD = @XMMS_LIBS@ @INTLLIBS@
