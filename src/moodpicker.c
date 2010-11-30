/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include <stdlib.h>

#include <livejournal/livejournal.h>

#include "util-gtk.h"
#include "account.h"

typedef struct {
	LJMood *mood;
	GSList *children;
} MoodTree;

static gint
mood_tree_compare(gconstpointer a, gconstpointer b) {
	const MoodTree *mta = a, *mtb = b;
	return g_ascii_strcasecmp(mta->mood->name, mtb->mood->name);
}

static void
mood_tree_free(MoodTree *mt) {
	g_slist_foreach(mt->children, (GFunc)mood_tree_free, NULL);
	g_slist_free(mt->children);
	g_free(mt);
}

static MoodTree*
build_mood_tree(LJServer *server) {
	GSList *moodsleft, *cur;
	GHashTable *moodtrees;
	LJMood nullmood = { 0 };
	MoodTree *mtbase;

	moodtrees = g_hash_table_new(g_int_hash, g_int_equal);
	moodsleft = g_slist_copy(server->moods);

	mtbase = g_new0(MoodTree, 1);
	mtbase->mood = &nullmood;
	g_hash_table_insert(moodtrees, &mtbase->mood->id, mtbase);

	while (moodsleft) {
		LJMood *mood = NULL;
		MoodTree *mtparent = NULL, *mtnew;
		GSList *last = NULL;

		/* find mood that has a parent in the tree. */
		for (cur = moodsleft; cur; last = cur, cur = cur->next) {
			mood = cur->data;
			mtparent = g_hash_table_lookup(moodtrees, &mood->parentid);
			if (mtparent)
				break;
		}
		if (!cur) {
			if (moodsleft) {
				mood = moodsleft->data;
				g_warning("nowhere to attach mood %d (%s) to %d?\n",
						mood->id, mood->name, mood->parentid);
			}
			break;
		}

		if (last) last->next = cur->next;
		else moodsleft = cur->next;
		g_slist_free_1(cur);

		mtnew = g_new0(MoodTree, 1);
		mtnew->mood = mood;
		mtparent->children = g_slist_insert_sorted(
				mtparent->children, mtnew, mood_tree_compare);
		g_hash_table_insert(moodtrees, &mtnew->mood->id, mtnew);
	}

	g_hash_table_destroy(moodtrees);
	mtbase->mood = NULL;
	return mtbase;
}

static void
populate_mood_list(GtkTreeStore *store, MoodTree *mtbase, GtkTreeIter *parent) {
	GSList *l;
	GtkTreeIter iter;
	MoodTree *mt;

	for (l = mtbase->children; l; l = l->next) {
		mt = l->data;

		gtk_tree_store_insert(store, &iter, parent, -1);
		gtk_tree_store_set(store, &iter,
				0, mt->mood->name,
				1, mt->mood->id,
				-1);
		populate_mood_list(store, mt, &iter);
	}
}

static GtkWidget*
make_mood_picker(MoodTree *mt) {
	GtkTreeStore *store;
	GtkWidget *tree;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_INT);
	populate_mood_list(store, mt, NULL);

	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
				_("Mood"), renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(tree));

	return tree;
}

gboolean
mood_picker_run(GtkWindow *parent, JamHostLJ *host, int *moodid, char *moodtext) {
	GtkWidget *dlg, *vbox, *hbox;
	GtkWidget *textentry, *textpicker, *scroll;
	MoodTree *mt;

	mt = NULL; // XXX blogger build_mood_tree(host->server);
	build_mood_tree(NULL);

	dlg = gtk_dialog_new_with_buttons(_("Current Mood"), parent,
			GTK_DIALOG_MODAL,
			GTK_STOCK_OK,     GTK_RESPONSE_OK,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			NULL);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox),
			gtk_label_new_with_mnemonic(_("Mood _Text:")),
			FALSE, FALSE, 0);
	{
		GtkWidget *vbox;
		vbox = gtk_vbox_new(FALSE, 6);
		textentry = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(vbox), textentry, FALSE, FALSE, 0);

		textpicker = make_mood_picker(mt);
		scroll = scroll_wrap(textpicker);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
				GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

		gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
	}
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox),
			gtk_label_new_with_mnemonic(_("Mood _Icon:")),
			FALSE, FALSE, 0);
	{
		GtkWidget *vbox, *check;
		vbox = gtk_vbox_new(FALSE, 6);
		check = gtk_check_button_new_with_mnemonic(_("_Match Text"));
		gtk_box_pack_start(GTK_BOX(vbox), check, FALSE, FALSE, 0);

		/*iconpicker = make_mood_picker(mt);
		scroll = scroll_wrap(iconpicker);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
				GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);*/

		gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
	}
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	gtk_widget_show_all(vbox);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox),
			vbox, TRUE, TRUE, 0);

	gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);

	mood_tree_free(mt);

	return FALSE;
}

