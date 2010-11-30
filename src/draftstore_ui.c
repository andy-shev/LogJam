/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include "draftstore.h"
#include "util-gtk.h"

enum {
	COL_DATE,
	COL_SUBJECT,
	COL_ITEMID,
	COL_COUNT
};

typedef struct {
	DraftStore *ds;
	GtkWidget *win;
	GtkListStore *store;
	GtkWidget *view;
	/*GtkWidget *lstats;*/
	/*GdkPixbuf *pb_friends, *pb_private;*/
} DraftStoreUI;

static void
row_activated_cb(GtkTreeView *treeview, GtkTreePath *arg1,
		GtkTreeViewColumn *arg2, DraftStoreUI* dsui) {
	gtk_dialog_response(GTK_DIALOG(dsui->win), GTK_RESPONSE_OK);
}

static GtkWidget*
make_view(DraftStoreUI *dsui) {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	dsui->view =
		gtk_tree_view_new_with_model(GTK_TREE_MODEL(dsui->store));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(dsui->view), TRUE);
	g_object_unref(G_OBJECT(dsui->store));
	g_signal_connect(G_OBJECT(dsui->view), "row-activated",
			G_CALLBACK(row_activated_cb), dsui);

	/*renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(_("Security"), renderer,
			"pixbuf", COL_SECURITY,
			NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(dsui->view), column);*/

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Date"), renderer,
			"text", COL_DATE,
			NULL);
	gtk_tree_view_column_set_sort_column_id(column, COL_DATE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(dsui->view), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Subject"), renderer,
			"text", COL_SUBJECT,
			NULL);
	gtk_tree_view_column_set_sort_column_id(column, COL_SUBJECT);
	gtk_tree_view_append_column(GTK_TREE_VIEW(dsui->view), column);

	return scroll_wrap(dsui->view);
}

static void
delete_cb(GtkWidget *w, DraftStoreUI *dsui) {
	GtkTreeModel *model;
	GtkTreeIter iter;
	int itemid;

	if (!gtk_tree_selection_get_selected(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(dsui->view)),
			&model, &iter))
		return;

	if (!jam_confirm(GTK_WINDOW(dsui->win), _("Delete"), _("Delete entry?")))
		return;

	gtk_tree_model_get(model, &iter,
			COL_ITEMID, &itemid,
			-1);

	gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

	draft_store_remove_entry(dsui->ds, itemid, NULL);
}

static void
dsui_destroy_cb(GtkWidget *w, DraftStoreUI *dsui) {
	/*gdk_pixbuf_unref(dsui->pb_friends);
	gdk_pixbuf_unref(dsui->pb_private);*/
}

static void
make_dialog(DraftStoreUI *dsui, GtkWindow *parent) {
	GtkWidget *vbox, *bbox, *button;

	dsui->win = gtk_dialog_new_with_buttons(_("Entries"),
			parent, GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
	g_signal_connect(G_OBJECT(dsui->win), "destroy",
			G_CALLBACK(dsui_destroy_cb), dsui);
	gtk_window_set_default_size(GTK_WINDOW(dsui->win), 500, 400);
	gtk_dialog_set_default_response(GTK_DIALOG(dsui->win), GTK_RESPONSE_OK);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), make_view(dsui), TRUE, TRUE, 0);
	/*dsui->lstats = gtk_label_new("");
	gtk_label_set_selectable(GTK_LABEL(dsui->lstats), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), dsui->lstats, FALSE, FALSE, 0);*/

	bbox = jam_dialog_buttonbox_new();
	button = jam_dialog_buttonbox_button_from_stock(bbox, GTK_STOCK_DELETE);
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(delete_cb), dsui);

	jam_dialog_set_contents_buttonbox(dsui->win, vbox, bbox);
}

static void
load_item_cb(DraftStore *ec, LJEntry *entry, gpointer data) {
	DraftStoreUI *dsui = data;
	GtkTreeIter iter;
	/*GdkPixbuf *pb;*/
	char *date;

#if 0
	switch (entry->security.type) {
		case SECURITY_FRIENDS:
		case SECURITY_CUSTOM:
			pb = dsui->pb_friends; break;
		case SECURITY_PRIVATE:
			pb = dsui->pb_private; break;
		default:
			pb = NULL; /* public: no icon */
	}
#endif
	date = lj_tm_to_ljdate_noseconds(&entry->time);
	gtk_list_store_append(dsui->store, &iter);
	gtk_list_store_set(dsui->store, &iter,
			/*COL_SECURITY, pb,*/
			COL_DATE, date,
			COL_SUBJECT, entry->subject,
			COL_ITEMID, entry->itemid,
			-1);
	g_free(date);
}
static gboolean
load_items(DraftStoreUI *dsui) {
	LJEntry *entry;
	entry = lj_entry_new();
	draft_store_each_header(dsui->ds, entry, load_item_cb, dsui);
	lj_entry_free(entry);
	return TRUE;
}

static LJEntry*
load_selected(DraftStoreUI *dsui) {
	GtkTreeModel *model;
	GtkTreeIter iter;
	int itemid;

	if (!gtk_tree_selection_get_selected(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(dsui->view)),
			&model, &iter))
		return NULL;
	gtk_tree_model_get(model, &iter,
			COL_ITEMID, &itemid,
			-1);

	return draft_store_get_entry(dsui->ds, itemid, NULL);
}

LJEntry*
draft_store_ui_select(DraftStore *ds, GtkWindow *parent) {
	DraftStoreUI dsui_actual = {0}, *dsui = &dsui_actual;
	LJEntry *entry = NULL;

	dsui->ds = ds;
	dsui->store = gtk_list_store_new(COL_COUNT,
			/*GDK_TYPE_PIXBUF, */G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

	make_dialog(dsui, parent);

	/*dsui->pb_friends = gtk_widget_render_icon(dsui->win,
			"logjam-protected", GTK_ICON_SIZE_MENU, NULL);
	dsui->pb_private = gtk_widget_render_icon(dsui->win,
			"logjam-private", GTK_ICON_SIZE_MENU, NULL);*/

	if (!load_items(dsui))
		return NULL;

	if (gtk_dialog_run(GTK_DIALOG(dsui->win)) == GTK_RESPONSE_OK)
		entry = load_selected(dsui);
	gtk_widget_destroy(dsui->win);
	return entry;
}

