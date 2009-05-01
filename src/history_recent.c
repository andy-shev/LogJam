/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include <stdlib.h>

#include <livejournal/getevents.h>

#include "util-gtk.h"
#include "history.h"
#include "network.h"

/* how many entries to load each time "load more..." is clicked. */
#define HISTORY_LOAD_BATCH 20

typedef struct {
	GtkWidget *win;
	GtkListStore *recentstore;
	GtkWidget *recentview;
	GtkWidget *lstats;
	GtkWidget *bloadmore;
	char *lastdate;

	JamAccount *account;
	const char *usejournal;
	GdkPixbuf *pb_friends, *pb_private;
} HistoryUI;

enum {
	COL_SECURITY,
	COL_DATE,
	COL_SUBJECT,
	COL_ITEMID,
	COL_COUNT
};

static void
recalculate_stats(HistoryUI *hui) {
	GtkTreeModel *model;
	GtkTreeIter iter;
	GdkPixbuf *pb;
	char *stats;

	int ecount, pecount, fecount;

	ecount = pecount = fecount = 0;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(hui->recentview));
	if (!gtk_tree_model_get_iter_first(model, &iter)) {
		/* no history. */
		gtk_label_set_text(GTK_LABEL(hui->lstats), "");
		return;
	}

	do {
		gtk_tree_model_get(model, &iter,
				COL_SECURITY, &pb,
				-1);
		ecount++;

		/* FIXME: gross hack. */
		if (pb == hui->pb_friends)
			fecount++;
		else if (pb == hui->pb_private)
			pecount++;

	} while (gtk_tree_model_iter_next(model, &iter));
	stats = g_strdup_printf(_("%d entries displayed.  "
			"%.1f%% public / %.1f%% friends-only / %.1f%% private."),
			ecount,
			100.0f * (ecount - pecount - fecount) / (float)ecount,
			100.0f * fecount / (float)ecount,
			100.0f * pecount / (float)ecount);
	gtk_label_set_text(GTK_LABEL(hui->lstats), stats);
	g_free(stats);
}

static int
load_recent(HistoryUI *hui, GtkWindow *parent) {
	LJGetEventsRecent *getevents;
	int i, count;
	GtkTreeIter iter;
	GdkPixbuf *pb;
	NetContext *ctx;
	
	if (!JAM_ACCOUNT_IS_LJ(hui->account)) {
		g_warning("XXX blogger: history for blogger\n");
		return -1;
	}

	getevents = lj_getevents_recent_new(
			jam_account_lj_get_user(JAM_ACCOUNT_LJ(hui->account)),
			hui->usejournal, HISTORY_LOAD_BATCH,
			hui->lastdate, TRUE, 100);
	ctx = net_ctx_gtk_new(parent, _("Loading History"));
	if (!net_run_verb_ctx((LJVerb*)getevents, ctx, NULL)) {
		lj_getevents_recent_free(getevents, TRUE);
		net_ctx_gtk_free(ctx);
		return -1;
	}

	count = getevents->entry_count;
	for (i = 0; i < count; i++) {
		LJEntry *entry = getevents->entries[i];
		char *date = lj_tm_to_ljdate(&entry->time);
		switch (entry->security.type) {
			case LJ_SECURITY_FRIENDS:
			case LJ_SECURITY_CUSTOM:
				pb = hui->pb_friends; break;
			case LJ_SECURITY_PRIVATE:
				pb = hui->pb_private; break;
			default:
				pb = NULL; /* public: no icon */
		}
		gtk_list_store_append(hui->recentstore, &iter);
		gtk_list_store_set(hui->recentstore, &iter,
				COL_SECURITY, pb,
				COL_DATE, date,
				/* because we set summary=TRUE above, the real subject
				 * is returned in event. */
				COL_SUBJECT, entry->event,
				COL_ITEMID, entry->itemid,
				-1);
		g_free(date);
	}

	if (count < HISTORY_LOAD_BATCH) {
		/* we've reached the beginning of history. */
		gtk_widget_set_sensitive(hui->bloadmore, FALSE);
	} else if (count > 0) {
		string_replace(&hui->lastdate,
				lj_tm_to_ljdate(&getevents->entries[count-1]->time));
	}

	recalculate_stats(hui);

	lj_getevents_recent_free(getevents, TRUE);
	net_ctx_gtk_free(ctx);

	return count;
}

static void
load_more_cb(GtkWidget *w, HistoryUI *hui) {
	load_recent(hui, GTK_WINDOW(hui->win));
}

static void
row_activated_cb(GtkTreeView *treeview, GtkTreePath *arg1, GtkTreeViewColumn *arg2, HistoryUI* hui) {
	gtk_dialog_response(GTK_DIALOG(hui->win), GTK_RESPONSE_OK);
}

static GtkWidget*
make_recentview(HistoryUI *hui) {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	
	hui->recentview = 
		gtk_tree_view_new_with_model(GTK_TREE_MODEL(hui->recentstore));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(hui->recentview), TRUE);
	g_object_unref(G_OBJECT(hui->recentstore));
	g_signal_connect(G_OBJECT(hui->recentview), "row-activated",
			G_CALLBACK(row_activated_cb), hui);

	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(_("Security"), renderer,
			"pixbuf", COL_SECURITY,
			NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(hui->recentview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Date"), renderer,
			"text", COL_DATE,
			NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(hui->recentview), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Subject"), renderer,
			"text", COL_SUBJECT,
			NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(hui->recentview), column);

	return scroll_wrap(hui->recentview);
}

static void
hui_destroy_cb(GtkWidget *w, HistoryUI *hui) {
	g_free(hui->lastdate);
}

void
make_dialog(HistoryUI *hui, GtkWindow *parent) {
	GtkWidget *vbox, *bbox;

	hui->win = gtk_dialog_new_with_buttons(_("History"),
			parent, GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
	g_signal_connect(G_OBJECT(hui->win), "destroy",
			G_CALLBACK(hui_destroy_cb), hui);
	gtk_window_set_default_size(GTK_WINDOW(hui->win), 500, 400);
	gtk_dialog_set_default_response(GTK_DIALOG(hui->win), GTK_RESPONSE_OK);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), make_recentview(hui), TRUE, TRUE, 0);
	hui->lstats = gtk_label_new("");
	gtk_label_set_selectable(GTK_LABEL(hui->lstats), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), hui->lstats, FALSE, FALSE, 0);

	bbox = jam_dialog_buttonbox_new();
	hui->bloadmore = jam_dialog_buttonbox_button_with_label(bbox,
			_("_Load More..."));
	g_signal_connect(G_OBJECT(hui->bloadmore), "clicked",
			G_CALLBACK(load_more_cb), hui);

	jam_dialog_set_contents_buttonbox(hui->win, vbox, bbox);
}

static LJEntry*
load_selected(HistoryUI *hui) {
	GtkTreeModel *model;
	GtkTreeIter iter;
	int itemid;

	if (!gtk_tree_selection_get_selected(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(hui->recentview)),
			&model, &iter))
		return NULL;

	gtk_tree_model_get(model, &iter,
			COL_ITEMID, &itemid,
			-1);

	return history_load_itemid(GTK_WINDOW(hui->win), hui->account, hui->usejournal, itemid);
}

LJEntry*
history_recent_dialog_run(GtkWindow *parent, JamAccount *acc, const char *usejournal) {
	HistoryUI hui_actual = {0}, *hui = &hui_actual;
	LJEntry *entry = NULL;

	hui->account = acc;
	hui->usejournal = usejournal;
	hui->recentstore = gtk_list_store_new(COL_COUNT,
			GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

	make_dialog(hui, parent);

	hui->pb_friends = gtk_widget_render_icon(hui->win, 
			"logjam-protected", GTK_ICON_SIZE_MENU, NULL);
	hui->pb_private = gtk_widget_render_icon(hui->win, 
			"logjam-private", GTK_ICON_SIZE_MENU, NULL);

	if (load_recent(hui, parent) <= 0)
		return NULL;

	while (gtk_dialog_run(GTK_DIALOG(hui->win)) == GTK_RESPONSE_OK) {
		entry = load_selected(hui);
		if (entry)
			break;
	}
	
	gtk_widget_destroy(hui->win);
	return entry;
}

