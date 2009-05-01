AM_CFLAGS=-g -I$(srcdir)/.. @LIVEJOURNAL_CFLAGS@

noinst_LTLIBRARIES = liblivejournal.la

liblivejournal_la_LDFLAGS = -static
liblivejournal_la_LIBADD = @LIVEJOURNAL_LIBS@
liblivejournal_la_SOURCES = \
	md5.c              md5.h            \
	protocol.c         protocol.h       \
	friends.h                           \
	serveruser.c       serveruser.h     \
	entry.c            entry.h          \
	types.c            types.h          \
	verb.c             verb.h           \
	login.c            login.h          \
	checkfriends.c     checkfriends.h   \
	consolecommand.c   consolecommand.h \
	editfriends.c      editfriends.h    \
	editfriendgroups.c editfriendgroups.h \
	editpostevent.c    editpostevent.h  \
	getchallenge.c     getchallenge.h   \
	getevents.c        getevents.h      \
	getfriends.c       getfriends.h     \
	syncitems.c        syncitems.h      \
	sync.c             sync.h           \
	livejournal.h

