/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "glib-all.h"

#include <livejournal/checkfriends.h>

#include <stdio.h>
#include <stdlib.h> /* atoi */
#ifndef G_OS_WIN32
#include <unistd.h> /* unlink */
#else
#include <io.h>
#endif
#include <errno.h>

#include "checkfriends.h"
#include "account.h"
#include "conf.h"
#include "network.h"

enum {
	ACCOUNT_CHANGED,
	STATE_CHANGED,
	LAST_SIGNAL
};

struct _CFMgr {
	GObject parent;
	JamAccountLJ *account;        /* for whom the bell tolls^W^W^W to check */

	gint         server_interval; /* minimum poll interval dictated by server */
	gint         user_interval;   /* poll interval requested by user */
	guint32      mask;            /* mask of friends groups to check for */
	CFState      state;           /* activity status of this cf manager */
	gint         newcount;        /* number of NEW hits; for thresholding */
	gchar       *lastupdate;      /* opaque server-owned lastupdate tag */
	gint         errors;          /* number of (network?) errors withstood */

#ifdef HAVE_GTK
	guint        timeout;         /* handle on glib timeout for next check */
#endif
};

struct _CFMgrClass {
	GObjectClass parent_class;
	void (*account_changed)(CFMgr *cfm, JamAccount *acc);
	void (*state_changed)  (CFMgr *cfm, CFState state);
};

static guint signals[LAST_SIGNAL] = { 0 };

static const gint CF_MAXERRORS = 3;
static const gint CF_AUTOSCHED = -1;
static const gint CF_MAX_THRESHOLD = 7;

static int do_checkfriends(CFMgr *cfm, NetContext *ctx);
static void reschedule(CFMgr *cfm, gint interval);

static void cfmgr_init(CFMgr *cfm);
#if 0
void
cfmgr_free(CFMgr *cfm) {
#ifdef HAVE_GTK
	/* and destroy it */
	if (cfm->timeout)
		gtk_timeout_remove(cfm->timeout); /* is this safe *enough*? */

	/* FIXME: we chicken out on actually freeing the indicators.
	 * there's no actual cost now because they'd been passed to the new
	 * cfmgr with the inherit hack. They're not GOjects yet, anyway */
	//g_slist_foreach(cfm->indicators, (GFunc)g_object_unref, NULL);
	g_slist_free(cfm->indicators);
#endif /* HAVE_GTK */

	g_free(cfm->lastupdate);
}
#endif

static void
cfmgr_class_init(gpointer klass, gpointer class_data) {
	signals[ACCOUNT_CHANGED] = g_signal_new("account_changed",
			LOGJAM_TYPE_CFMGR,
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(CFMgrClass, account_changed),
			NULL, NULL,
			g_cclosure_marshal_VOID__POINTER,
			G_TYPE_NONE, 1, G_TYPE_POINTER);

	signals[STATE_CHANGED] = g_signal_new("state_changed",
			LOGJAM_TYPE_CFMGR,
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(CFMgrClass, state_changed),
			NULL, NULL,
			g_cclosure_marshal_VOID__INT,
			G_TYPE_NONE, 1, G_TYPE_INT);
}

void
cfmgr_set_account(CFMgr *cfm, JamAccount *acc) {
	cfmgr_set_state(cfm, CF_DISABLED);
	cfmgr_init(cfm);

	if (JAM_ACCOUNT_IS_LJ(acc)) {
		cfm->account = JAM_ACCOUNT_LJ(acc);
		cfmgr_set_mask(cfm, jam_account_lj_get_cfmask(JAM_ACCOUNT_LJ(acc)));
#ifdef HAVE_GTK
		if (conf.options.cfautostart && jam_account_lj_get_checkfriends(acc))
			cfmgr_set_state(cfm, CF_ON);
#endif /* HAVE_GTK */
	} else {
		cfm->account = NULL;
	}
	g_signal_emit_by_name(cfm, "account_changed", acc);
}

static gboolean
cf_timeout_cb(CFMgr *mgr) {
	if (do_checkfriends(mgr, network_ctx_silent) == 0)
		reschedule(mgr, CF_AUTOSCHED);
	return FALSE;
}

/* install a timeout for the next checkfriends call according to the
 * most recent interval information */
static void
reschedule(CFMgr *cfm, gint interval) {
#ifdef HAVE_GTK
	if (interval == CF_AUTOSCHED)
		interval = MAX(cfm->server_interval, cfm->user_interval) * 1000;

	cfm->timeout = g_timeout_add(
			interval,
			(GSourceFunc)cf_timeout_cb,
			(gpointer)cfm);
#endif
}

void
cfmgr_set_state(CFMgr *cfm, CFState state) {
	cfm->state = state;

	switch (state) {
		case CF_DISABLED:
#ifdef HAVE_GTK
			if (cfm->timeout)
				gtk_timeout_remove(cfm->timeout);
			cfm->timeout = 0;
#endif /* HAVE_GTK */
			break;

		case CF_ON:
			/* every startup gets a clean slate */
			cfm->errors   = 0;
			cfm->newcount = 0;

			/* Start checking friends right away when we're turned on.
			 *
			 * We schedule an almost-immediate checkfriends instead
			 * of calling it directly to improve UI responsiveness */
			reschedule(cfm, 0);
			break;

		case CF_NEW:
			break;
	}
	g_signal_emit_by_name(cfm, "state_changed", cfm->state);
}

CFState
cfmgr_get_state(CFMgr *cfm) {
	return cfm->state;
}

JamAccountLJ*
cfmgr_get_account(CFMgr *cfm) {
	if (!cfm->account) return NULL;
	return JAM_ACCOUNT_LJ(cfm->account);
}

void
cfmgr_set_mask(CFMgr *cfm, guint32 mask) {
#ifdef HAVE_GTK
	if (conf.options.cfusemask == FALSE)
		mask = 0;
#endif /* HAVE_GTK */
	if (cfm->mask != mask) {
		cfm->mask  = mask;              /* install new mask */
		string_replace(&cfm->lastupdate, g_strdup(""));
		                  /* invalidate monitor information */
	}
}

static void
cfmgr_init(CFMgr *cfm) {
	cfm->account         = NULL;
	cfm->state           = CF_DISABLED;
	cfm->server_interval = 45; /* just a default quicky overridden */
	cfm->user_interval   = conf.cfuserinterval ? conf.cfuserinterval :
							   cfm->server_interval;
	cfm->mask            = 0; /* set by cfmgr_mask_set */
	cfm->newcount        = 0; /* approximate number of new friend entries */
	cfm->errors          = 0;

	string_replace(&cfm->lastupdate, g_strdup(""));

#ifdef HAVE_GTK
	cfm->timeout         = 0; /* will be installed upon first "CF_ON" */
#endif /* HAVE_GTK */
}

CFMgr*
cfmgr_new(JamAccount *acc) {
	CFMgr *cfm = LOGJAM_CFMGR(g_object_new(cfmgr_get_type(), NULL));
	cfmgr_set_account(cfm, acc);
	return cfm;
}

GType
cfmgr_get_type(void) {
	static GType cfm_type = 0;
	if (!cfm_type) {
		const GTypeInfo cfm_info = {
			sizeof (CFMgrClass),
			NULL,
			NULL,
			(GClassInitFunc) cfmgr_class_init,
			NULL,
			NULL,
			sizeof (CFMgr),
			0,
			(GInstanceInitFunc) cfmgr_init,
		};
		cfm_type = g_type_register_static(G_TYPE_OBJECT,
				"CFMgr", &cfm_info, 0);
	}
	return cfm_type;
}


void
cf_threshold_normalize(gint *threshold) {
	if (*threshold < 1)
		*threshold = 1;
	if (*threshold > CF_MAX_THRESHOLD)
		*threshold = CF_MAX_THRESHOLD;
}


/* returns -1 on error,
 *          0 on success, but no new posts,
 *     and  1 on success with new posts.
 */
static int
do_checkfriends(CFMgr *cfm, NetContext *ctx) {
	LJCheckFriends *cf;

	cf = lj_checkfriends_new(jam_account_lj_get_user(cfm->account),
			cfm->lastupdate);

	if (cfm->mask)
		lj_checkfriends_set_mask(cf, cfm->mask);

	if (!net_run_verb_ctx((LJVerb*)cf, ctx, NULL)) {
		/* if the request fails, we only stop polling after several attempts */
		lj_checkfriends_free(cf);

		cfm->errors++;
		if (cfm->errors > CF_MAXERRORS) {
#if 0
			/* the transient parent window of this notification is NULL, but
			 * it's worth thinking about a better general user notification
			 * strategy, that finds the current screen, etc. */
			jam_message(NULL, JAM_MSG_WARNING, TRUE, NULL,
					_("Too many network errors; checking friends disabled."));
#endif
			cfmgr_set_state(cfm, CF_DISABLED);
			return -1;
		}
		return 0;
	}

	/* we succeeded.  reset the error count. */
	cfm->errors = 0;

	string_replace(&cfm->lastupdate, g_strdup(cf->lastupdate));
	cfm->server_interval = cf->interval;

	lj_checkfriends_free(cf);

	if (cf->newposts == 0)
		return 0;

	cfm->newcount++;
	if (cfm->newcount >= conf.cfthreshold)
		cfmgr_set_state(cfm, CF_NEW);
	return 1;
}


static void
cf_cli_make_path(JamAccountLJ *acc, char *path) {
	gchar file[1024];
	char *id = jam_account_id_strdup(JAM_ACCOUNT(acc));
	g_snprintf(file, 1024, "checkfriends-%s", id);
	g_free(id);
	conf_make_path(file, path, 1024);
}

void
checkfriends_cli_purge(JamAccountLJ *acc) {
	gchar path[1024];

	cf_cli_make_path(acc, path);

	if (g_file_test(path, G_FILE_TEST_EXISTS)) {
		if (!unlink(path)) return;

		g_printerr(_("Can't unlink %s: %s."), path, g_strerror(errno));
	}
}

static time_t
cf_cli_conf_read(CFMgr *cfm, JamAccountLJ *acc) {
	time_t lasttry = 0;
	gchar *cfconfdata;
	gchar **parseddata;
	gchar path[1024];

	cf_cli_make_path(acc, path);

	if (g_file_get_contents(path, &cfconfdata, NULL, NULL)) {
		parseddata = g_strsplit(cfconfdata, "|", 5);
		cfm->lastupdate      = parseddata[0];
		cfm->server_interval = atoi(parseddata[1]); g_free(parseddata[1]);
		cfm->errors          = atoi(parseddata[2]); g_free(parseddata[2]);
		cfm->state           = atoi(parseddata[3]); g_free(parseddata[3]);
		lasttry              = atoi(parseddata[4]); g_free(parseddata[4]);
		g_free(cfconfdata);
	} else {
		cfm->state = CF_ON;
	}
	return lasttry;
}

static gboolean
cf_cli_conf_write(CFMgr *cfm, JamAccountLJ *acc) {
	FILE *f;
	gchar path[1024];

	cf_cli_make_path(acc, path);

	f = fopen(path, "w");
	if (!f) {
		g_printerr(_("Error opening %s for write: %s."),
				path, g_strerror(errno));
		return FALSE;
	}

	fprintf(f, "%s|%d|%d|%d|%ld",
	           cfm->lastupdate ? cfm->lastupdate : "",
	           cfm->server_interval,
	           cfm->errors,
	           cfm->state,
	           time(NULL));
	fclose(f);
	return TRUE;
}

/* gui-less version of checkfriends.
 *
 * returns TRUE  when new friends entries have been detected
 *         FALSE when no such entries exist (or when something has prevented
 *                                           the check)
 *
 * keeps track of its own persistent information with cf_cli_conf_*(). */
gboolean
checkfriends_cli(JamAccountLJ *acc) {
	CFMgr *cfm = cfmgr_new(JAM_ACCOUNT(acc));
	time_t now = time(NULL);
	time_t then = cf_cli_conf_read(cfm, acc);

	/* don't even approach the server in some cases.
	 * report the reason to the user unless we're in quiet mode */

	if (cfm->errors > CF_MAXERRORS) {
		if (!app.quiet)
			g_printerr(_("Maximum error count reached in contacting server.\n"
					"Run \"%s --checkfriends=purge\" and try again.\n"),
			app.programname);
		return FALSE;
	}
	if (now - then < cfm->server_interval) {
		if (!app.quiet)
			g_printerr(_("Request rate exceeded. Slow down.\n"));
		return FALSE;
	}
	if (cfm->state == CF_NEW) {
		if (!app.quiet)
				g_printerr(_("Read your friends page, then run\n"
					"\"%s checkfriends purge\".\n"), app.programname);
		return TRUE;
	}

	do_checkfriends(cfm, network_ctx_cmdline);

	cf_cli_conf_write(cfm, acc);

	return (cfm->state == CF_NEW);
}


