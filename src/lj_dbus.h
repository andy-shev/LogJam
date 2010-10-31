/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2009 Andy Shevchenko <andy.shevchenko@gmail.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef lj_dbus_h
#define lj_dbus_h

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

/* Take care about i18n strings */
#ifndef _
#ifdef GETTEXT_PACKAGE
#include <glib/gi18n-lib.h>
#else
#define _(x)	x
#endif
#endif		/* _ */

#define MPRIS_INFO_LEN	128

typedef struct _MetaInfo MetaInfo;
struct _MetaInfo {
	gchar artist[MPRIS_INFO_LEN];
	gchar album[MPRIS_INFO_LEN];
	gchar title[MPRIS_INFO_LEN];

#define	MPRIS_STATUS_PLAYING	0
#define	MPRIS_STATUS_PAUSED		1
#define	MPRIS_STATUS_STOPPED	2

	gint status;
};

typedef struct _MediaPlayer MediaPlayer;
struct _MediaPlayer {
	gchar *name;
	gchar *version;
	gchar *dest;
	DBusGProxy *proxy;
	MetaInfo info;

#define MPRIS_HINT_BAD_STATUS	1 << 0

	gint hint;

#define MPRIS_V1	1
#define MPRIS_V2	2

	gint mprisv;
};

typedef struct _JamDBus JamDBus;
struct _JamDBus {
	DBusGConnection *bus;
	GList *player;
};

void lj_dbus_close(JamDBus *jd);
JamDBus *lj_dbus_new(void);

gboolean lj_dbus_mpris_update_list(JamDBus *jd, GError **error);
gboolean lj_dbus_mpris_update_info(GList *list, GError **error);

#define MPRIS_ERROR_NOT_PLAYING		1
#define MPRIS_ERROR_NO_PLAYER		2

gchar *lj_dbus_mpris_current_music(JamDBus *jd, GError **error);

#endif /* lj_dbus_h */

