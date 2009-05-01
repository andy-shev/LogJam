/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#ifdef HAVE_GTK
#include "gtk-all.h"
#else
#include "glib-all.h"
#endif
#include <errno.h>

#include <livejournal/sync.h>

#include "conf.h"
#include "journalstore.h"
#include "network.h"
#include "sync.h"

static const char* status_stages[] = {
	"Entry metadata...",
	"Entries...",
/*	N_("Comment metadata..."),
	N_("Comments..."),*/
};
typedef enum {
	STAGE_ENTRYMETA,
	STAGE_ENTRY,
/*	STAGE_COMMENTMETA,
	STAGE_COMMENT,*/
	STAGE_COUNT
} StatusStage;

typedef struct {
#ifdef HAVE_GTK
	GtkWidget *progresswin;
	GtkWidget *label, *progress;
	StatusStage stage;
#endif
	
	JamAccountLJ *account;
	JournalStore *js;
	char *lastsync;
	int synced;
	int total;

	gboolean sync_complete;
	GSList *syncitems;
} SyncStatus;

static gboolean
sync_run_internal(SyncStatus *status,
		LJRunVerbCallback sync_run_verb,
		LJSyncProgressCallback sync_progress,
		gpointer parent,
		GSList **warnings, GError **err);

/*** gtk-specific functions. ***/
#ifdef HAVE_GTK
static void
show_status_stage(SyncStatus *status) {
	GString *str = g_string_sized_new(20*STAGE_COUNT);
	int i;
	for (i = 0; i < STAGE_COUNT; i++) {
		if (i == status->stage)
			g_string_append(str, "\342\226\266 "); /* U+25B6: black arrow */
		g_string_append(str, _(status_stages[i]));
		g_string_append_c(str, '\n');
	}
	gtk_label_set_label(GTK_LABEL(status->label), str->str);
	g_string_free(str, TRUE);
}

static void
sync_init_gtk(SyncStatus *status, gpointer parent) {
	char *id;
	char *caption;

	status->progresswin = progress_window_new(GTK_WINDOW(parent), _("Synchronizing Journal"));

	status->label = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(status->label), 0, 0);

	status->stage = 0;
	show_status_stage(status);

	status->progress = gtk_progress_bar_new();

	gtk_widget_show(status->label);
	gtk_widget_show(status->progress);
	progress_window_pack(PROGRESS_WINDOW(status->progresswin), status->label);
	progress_window_pack(PROGRESS_WINDOW(status->progresswin), status->progress);
	gtk_widget_show(status->progresswin);
}

static gboolean
sync_run_verb_gtk(gpointer data, LJVerb *verb, GError **err) {
	SyncStatus *status = data;
	return net_window_run_verb(PROGRESS_WINDOW(status->progresswin), verb);
}

static void
sync_progress_gtk(gpointer data, LJSyncProgress progress, int cur, int max, const char *date) {
	SyncStatus *status = data;
	double fraction = 0.0;
	StatusStage stage;

	switch (progress) {
	case LJ_SYNC_PROGRESS_ITEMS:
		stage = STAGE_ENTRYMETA; break;
	case LJ_SYNC_PROGRESS_ENTRIES:
		stage = STAGE_ENTRY; break;
	}

	if (stage != status->stage) {
		status->stage = stage;
		show_status_stage(status);
	}
	
	if (max > 0) fraction = cur/(double)max;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(status->progress),
			fraction);
}

static void
sync_show_warnings_gtk(SyncStatus *status, GSList *warnings) {
	GtkWidget *dlg, *hbox, *image, *label, *scroll;
	GSList *l;
	GString *warnstr;

	/* we create our own version of GtkMessageDialog
	 * so we can add the scroll bar. */
	dlg = gtk_dialog_new_with_buttons(_("Warnings"),
			GTK_WINDOW(status->progresswin), GTK_DIALOG_MODAL,
			GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
			NULL);

	image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_WARNING,
			GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.0);

	warnstr = g_string_new(warnings->data);
	for (l = warnings->next; l; l = l->next) {
		g_string_append_c(warnstr, '\n');
		g_string_append(warnstr, l->data);
	}

	label = gtk_label_new(warnstr->str);
	g_string_free(warnstr, TRUE);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), 
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll),
			GTK_SHADOW_NONE);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll),
			label);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), scroll, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), hbox, TRUE, TRUE, 0);
	gtk_widget_show_all(hbox);

	gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);
}

gboolean
sync_run_gtk(JamAccountLJ *acc, gpointer parent) {
	SyncStatus st = {0}, *status = &st;
	GSList *warnings = NULL;
	GError *err = NULL;
	gboolean success;

	status->account = acc;

	sync_init_gtk(status, parent);
	success = sync_run_internal(status, sync_run_verb_gtk, sync_progress_gtk,
			parent, &warnings, &err);

	if (warnings) {
		sync_show_warnings_gtk(status, warnings);
		g_slist_foreach(warnings, (GFunc)g_free, NULL);
		g_slist_free(warnings);
	}

	if (err) {
		progress_window_show_error(PROGRESS_WINDOW(status->progresswin), err->message);
		g_error_free(err);
		success = FALSE;
	}

	if (status->progresswin)
		gtk_widget_destroy(status->progresswin);

	return success;
}

#endif /* HAVE_GTK */

/*** console-specific functions. ***/

static gboolean
sync_run_verb_cli(gpointer data, LJVerb *verb, GError **err) {
	return net_run_verb_ctx(verb, network_ctx_silent, err);
}

static void
sync_progress_cli(gpointer data, LJSyncProgress progress, int cur, int max, const char *date) {
	if (max == 0) {
		if (cur == 0 && progress == LJ_SYNC_PROGRESS_ITEMS)
			g_print(_("Up-to-date.\n"));
		return;
	}
	if (cur == 0) {
		switch (progress) {
		case LJ_SYNC_PROGRESS_ITEMS:
			g_print(_("Downloading %d sync items: "), max);
			break;
		case LJ_SYNC_PROGRESS_ENTRIES:
			g_print(_("Downloading %d entries: "), max);
			break;
		}
	} else {
		g_print("%.1f%% ", (cur*100.0)/(double)max);
		if (cur == max)
			g_print(_("done.\n"));
	}
}

gboolean
sync_run_cli(JamAccountLJ *acc) {
	SyncStatus st = {0}, *status = &st;
	GSList *warnings = NULL, *l;
	GError *err = NULL;
	char *id;
	gboolean success;

	status->account = acc;
	id = jam_account_id_strdup(JAM_ACCOUNT(status->account));
	g_print(_("Synchronizing '%s'..."), id);
	g_print("\n");
	g_free(id);

	success = sync_run_internal(status, sync_run_verb_cli, sync_progress_cli, NULL, &warnings, &err);

	if (warnings) {
		g_print(_("Warnings:\n"));
		for (l = warnings; l; l = l->next)
			g_print("\t%s\n", (char*)l->data);
		g_slist_foreach(warnings, (GFunc)g_free, NULL);
		g_slist_free(warnings);
	}
	if (err) {
		g_print(_("Error: %s\n"), err->message);
		g_error_free(err);
		success = FALSE;
	}
	return success;
}

static gboolean
put_lastsync(gpointer data, const char *lastsync, GError **err) {
	SyncStatus *status = data;
	return journal_store_put_lastsync(status->js, lastsync, err);
}

static gboolean
put_entries(gpointer data, LJEntry **entries, int count, GError **err) {
	SyncStatus *status = data;
	int i;

	if (!journal_store_put_group(status->js, entries, count, err))
		return FALSE;

	return TRUE;
}

/* through callbacks, this function manages to drive both
 * the gtk and the cli sync. */
static gboolean
sync_run_internal(SyncStatus *status,
		LJRunVerbCallback sync_run_verb,
		LJSyncProgressCallback sync_progress,
		gpointer parent,
		GSList **warnings, GError **err) {
	GError *tmperr = NULL;
	gboolean success = FALSE;
	char *lastsync = NULL;
	
	status->js = journal_store_open(JAM_ACCOUNT(status->account), TRUE, &tmperr);
	if (!status->js) goto err;

	lastsync = journal_store_get_lastsync(status->js);

	if (!lj_sync_run(jam_account_lj_get_user(status->account), NULL /* usejournal */,
			lastsync,
			put_lastsync, sync_run_verb, put_entries, sync_progress,
			status, warnings, &tmperr))
		goto err;

	success = TRUE;

err:
	if (tmperr)
		g_propagate_error(err, tmperr);

	g_free(lastsync);
	if (status->js)
		journal_store_free(status->js);

	return success;
}

gboolean
sync_run(JamAccountLJ *acc, gpointer parent) {
#ifdef HAVE_GTK
	if (!app.cli) return sync_run_gtk(acc, parent);
	else return sync_run_cli(acc);
#else
	return sync_run_cli(acc);
#endif
}

