pixmapdir = $(datadir)/pixmaps
pixmap_DATA = logjam_goat.png logjam_pencil.png

IMAGES = \
	logjam_ljuser.png     \
	logjam_ljcomm.png     \
	logjam_twuser.png     \
	logjam_private.png    \
	logjam_protected.png  \
	logjam_pencil.png     \
	logjam_rarrow.png     \
	logjam_larrow.png     \
	logjam_throbber_1.png \
	logjam_throbber_2.png \
	logjam_throbber_3.png \
	logjam_throbber_4.png \
	logjam_throbber_5.png \
	logjam_throbber_6.png \
	logjam_throbber_7.png \
	logjam_throbber_8.png \
	logjam_lrarrow.png    \
	logjam_goat.png       \
	logjam_blogger.png

LIST = \
	logjam_ljuser        $(srcdir)/logjam_ljuser.png       \
	logjam_ljcomm        $(srcdir)/logjam_ljcomm.png       \
	logjam_twuser        $(srcdir)/logjam_twuser.png       \
	logjam_private       $(srcdir)/logjam_private.png      \
	logjam_protected     $(srcdir)/logjam_protected.png    \
	logjam_rarrow        $(srcdir)/logjam_rarrow.png       \
	logjam_larrow        $(srcdir)/logjam_larrow.png       \
	logjam_lrarrow       $(srcdir)/logjam_lrarrow.png

# images that are larger; we RLE encode them.
BIG_LIST = \
	logjam_pencil        $(srcdir)/logjam_pencil.png       \
	logjam_goat          $(srcdir)/logjam_goat.png         \
	logjam_blogger       $(srcdir)/logjam_blogger.png

THROBBER_LIST = \
	logjam_throbber_1    $(srcdir)/logjam_throbber_1.png   \
	logjam_throbber_2    $(srcdir)/logjam_throbber_2.png   \
	logjam_throbber_3    $(srcdir)/logjam_throbber_3.png   \
	logjam_throbber_4    $(srcdir)/logjam_throbber_4.png   \
	logjam_throbber_5    $(srcdir)/logjam_throbber_5.png   \
	logjam_throbber_6    $(srcdir)/logjam_throbber_6.png   \
	logjam_throbber_7    $(srcdir)/logjam_throbber_7.png   \
	logjam_throbber_8    $(srcdir)/logjam_throbber_8.png

noinst_DATA = pixbufs.h
CLEANFILES = $(noinst_DATA)

if WITH_RSVG
pixbufs.h: $(IMAGES)
	gdk-pixbuf-csource --raw --build-list $(LIST) >$(srcdir)/pixbufs.h
	gdk-pixbuf-csource --rle --build-list $(BIG_LIST) >>$(srcdir)/pixbufs.h
else
pixbufs.h: $(IMAGES)
	gdk-pixbuf-csource --raw --build-list $(LIST) >$(srcdir)/pixbufs.h
	gdk-pixbuf-csource --rle --build-list $(THROBBER_LIST) $(BIG_LIST) >>$(srcdir)/pixbufs.h
endif

EXTRA_DIST = $(IMAGES)
noinst_HEADERS = inline.h

