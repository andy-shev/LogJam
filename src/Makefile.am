bin_PROGRAMS = logjam

localedir=$(datadir)/locale

# AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" -I$(top_srcdir)/protocol/liblivejournal -I$(top_srcdir)/protocol/blogger -I$(top_srcdir)/util/gxr @LOGJAM_CFLAGS@ @NETWORK_CFLAGS@
AM_CFLAGS = -DLOCALEDIR=\"$(localedir)\" -I$(top_srcdir)/protocol/liblivejournal -I$(top_srcdir)/util/gxr @LOGJAM_CFLAGS@ @NETWORK_CFLAGS@

ACCOUNT = account.h account.c host.c livejournal.c
# blogger.c
DOC = jamdoc.c jamdoc.h
VIEW = jamview.c jamview.h
FRIENDGROUPS = friendgroups.c friendgroups.h friendgroupedit.c friendgroupedit.h
FRIENDS = friends.h friends.c friendedit.c friendedit.h $(FRIENDGROUPS)
CONFIG = conf.c conf.h conf_xml.c conf_xml.h
MD5 = md5.c md5.h
THROBBER = throbber.c throbber.h logo-svg.h
NETWORK = network.c network.h network-internal.h
NETWORKGTK = network-gtk.c $(THROBBER)
GENERATED = cmdline_data.h helptext.h
HISTORY = history_recent.c history.c history.h
LOGIN = login.c login.h
TOOLS = tools.c tools.h link.c link-journal.c imagelink.c console.c console.h
TEXTMODES = structuredtext.c
USEJOURNAL = usejournal.c usejournal.h
DRAFTSTORE = draftstore.c draftstore.h 
JOURNALSTORE = journalstore.c journalstore.h
SYNC = sync.c sync.h
OFFLINE = offline.c
GROUPEDBOX = groupedbox.c groupedbox.h
DOCKLET = eggtrayicon.c eggtrayicon.h docklet.c
MARSHALERS = marshalers.h marshalers.c
PREVIEW = preview.c preview.h
HTML_MARKUP = html_markup.c html_markup.h
REMOTE = remote.c remote.h
GETOPT = getopt.c getopt1.c getopt.h
PROGRESS = progress.c progress.h

src_base = glib-all.h init.c cmdline.h cmdline.c util.c util.h \
           $(ACCOUNT) $(DOC) $(DRAFTSTORE) \
           $(CONFIG) $(GENERATED) $(NETWORK) \
           $(JOURNALSTORE) $(SYNC) $(GETOPT) \
           checkfriends.c checkfriends.h \
           jam_xml.c jam_xml.h gettext.h

src_gtk = gtk-all.h util-gtk.c util-gtk.h \
          $(VIEW) draftstore_ui.c \
          $(REMOTE) $(MARSHALERS) \
          $(NETWORKGTK) $(PROGRESS) \
          $(FRIENDS) checkfriends-gtk.c \
          settings.c settings.h $(GROUPEDBOX) tie.c tie.h \
          $(HISTORY) $(OFFLINE) \
          jam.c jam.h $(USEJOURNAL) \
          security.c security.h about.c thanks.h \
          menu.c menu.h $(TOOLS) smartquotes.c smartquotes.h \
          $(LOGIN) manager.c \
          spawn.c spawn.h icons.c icons.h \
          music.c music.h datesel.c datesel.h moodpicker.c \
          pollcreator.c pollcreator.h poll.h poll-questions.c \
          undo.c undo.h \
          userlabel.c userlabel.h \
          get_cmd_out.c get_cmd_out.h \
          tags.c tags.h \
          $(HTML_MARKUP)

src_preview = $(PREVIEW)

src_win32 = network-win32.c
src_gtkunix = network-fork.c
src_curl = network-curl.c
src_soup = network-soup.c
src_libxmlnano = network-libxml.c

if ENABLE_PCH
BUILT_SOURCES = gtk-all.h.gch
gtk-all.h.gch: gtk-all.h
	-rm -f gtk-all.h.gch
	$(COMPILE) -fPIC -DPIC -x c++-header -o gtk-all.h.gch $(srcdir)/gtk-all.h
endif

logjam_SOURCES = $(src_base)

if WITH_GTK
logjam_SOURCES += $(src_gtk)
endif

if WIN32
logjam_SOURCES += $(src_win32)
else
if WITH_GTK
logjam_SOURCES += $(src_gtkunix)
endif
if WITH_LIBSOUP
logjam_SOURCES += $(src_soup)
else
if WITH_CURL
logjam_SOURCES += $(src_curl)
else
logjam_SOURCES += $(src_libxmlnano)
endif
endif
if WITH_GTK
if WITH_DOCK
logjam_SOURCES += $(DOCKLET)
endif
endif
endif

if WITH_HTML
logjam_SOURCES += $(src_preview)
endif

if WITH_SQLITE3
logjam_SOURCES += journalstore-sqlite.c
else
logjam_SOURCES += journalstore-xml.c
endif

# logjam_LDADD=$(top_builddir)/protocol/liblivejournal/livejournal/liblivejournal.la $(top_builddir)/protocol/blogger/libblogger.la @LOGJAM_LIBS@ @NETWORK_LIBS@
logjam_LDADD=$(top_builddir)/protocol/liblivejournal/livejournal/liblivejournal.la @LOGJAM_LIBS@ @NETWORK_LIBS@

ctags:
	@version=`ctags --version | head -1`; \
	echo "$$version" | grep GNU && version=GNU; \
	echo "$$version" | grep Exuberant && version=Exuberant; \
	case "$$version" in \
		GNU) \
			ctags -dt --no-warn $(logjam_SOURCES) \
			;; \
		Exuberant) \
			ctags -R . \
			;; \
		*) \
			echo "no known version of ctags found [$$version]. sorry." \
			;; \
	esac
