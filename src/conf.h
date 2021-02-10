/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef CONF_H
#define CONF_H

#include "config.h"

#include <livejournal/livejournal.h>

#ifdef HAVE_GTK
#include "gtk-all.h"
#include "jamview.h" /* need META_COUNT */
#endif

#include "account.h"
#include "checkfriends.h"

typedef struct {
	int x, y, width, height;
	int panedpos; /* optional; for windows with panes. */
} Geometry;

/* this should match the geometry_names[] array in conf_xml.c */
typedef enum {
	GEOM_MAIN,
	GEOM_LOGIN,
	GEOM_FRIENDS,
	GEOM_FRIENDGROUPS,
	GEOM_CONSOLE,
	GEOM_MANAGER,
	GEOM_CFFLOAT,
	GEOM_OFFLINE,
	GEOM_PREVIEW,
	GEOM_COUNT
} GeometryType;

typedef struct {
	gboolean netdump;
	gboolean nofork;
	gboolean useproxy;
	gboolean useproxyauth;
#ifdef HAVE_GTK
#ifdef HAVE_GTKSPELL
	gboolean usespellcheck;
#endif
	gboolean revertusejournal;
	gboolean autosave;
	gboolean cfautostart;
	gboolean cfusemask;
	gboolean close_when_send;
	gboolean docklet;
	gboolean cffloat;
	gboolean cffloatraise;
	gboolean cffloat_decorate;
	gboolean friends_hidestats;
	gboolean allowmultipleinstances;
	gboolean smartquotes;
	gboolean showloginhistory;
	gboolean showmeta[JAM_VIEW_META_COUNT];
	gboolean start_in_dock;
	gboolean keepsaveddrafts;
#endif /* HAVE_GTK */
} Options;

typedef enum {
	POSTMODE_GUI,
	POSTMODE_CMD
} PostmodeType;

typedef struct {
	char *label;
	char *command;
} CommandList;

typedef struct {
	/* configuration file */
	GSList *hosts;
	JamHost *lasthost;

	Geometry geometries[GEOM_COUNT];

	Options options;

	gchar *uifont;
#ifdef HAVE_GTKSPELL
	gchar *spell_language;
#endif
#ifndef G_OS_WIN32
	char *spawn_command;

	char *music_command;
	gboolean music_mpris;

	char *proxy;
	char *proxyuser, *proxypass;
#endif

	LJSecurity defaultsecurity;

	gint cfuserinterval;
	gint cfthreshold;

	/* run-time settings. */
	int postmode;
} Configuration;

typedef struct {
	gchar *programname;
	gchar *conf_dir;        /* may be null, which means <home>/.logjam/ */

	gboolean cli;           /* true if there's no gui */
	gboolean quiet;

#ifdef HAVE_GTK
	GtkTooltips *tooltips;
	GSList *secmgr_list;

	GSList *quiet_dlgs;

	CFMgr *cfmgr;
	CFFloat *cf_float;

	gint autosave;          /* timeout id */

	void *remote;
	void *docklet;
#endif
} Application;

JamHost*  conf_host_by_name(Configuration *c, const char *hostname);

extern Configuration conf;
extern Application app;

int conf_verify_dir(void);
void conf_make_path(char *file, char *buf, int len);

char* conf_make_account_path(JamAccount *acc, const char *path);

void conf_verify_a_host_exists();

gboolean conf_rename_host(JamHost *host, const char *newname, GError **err);

#endif /* CONF_H */
