/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <livejournal/livejournal.h>
#include <livejournal/getfriends.h>
#include <livejournal/editfriends.h>

#include "network.h"
#include "conf.h"
#include "util-gtk.h"
#include "spawn.h"
#include "account.h"
#include "friends.h"
#include "friendedit.h"
#include "friendgroups.h"

#include "icons.h"

#include "friends.h"

typedef struct {
	GtkWindow  win; /* parent */
	GtkWidget *friendview, *statspanel, *statslabel, *statscomm;
	GtkItemFactory *itemfactory;
	GtkWidget *omconn, *omtype;

	GdkPixbuf *pb_user, *pb_comm, *pb_larrow, *pb_rarrow, *pb_lrarrow;

	JamAccountLJ *account;
	GSList *friends;
	LJFriendType filter_type;
	int filter_conn;
} JamFriendsUI;

enum {
	FRIEND_COL_LINK,
	FRIEND_COL_USERNAME,
	FRIEND_COL_FULLNAME,
	FRIEND_COL_COUNT
};

enum {
	ACTION_EDIT_MENU=1,
	ACTION_ADD,
	ACTION_EDIT,
	ACTION_REMOVE,
	ACTION_JOURNAL_VIEW,
	ACTION_USERINFO_VIEW,
	ACTION_HIDE_STATISTICS
};

#define FRIENDSUI(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), friendsui_get_type(), JamFriendsUI))

static GType
friendsui_get_type(void) {
	static GType new_type = 0;
	if (!new_type) {
		const GTypeInfo new_info = {
			sizeof (GtkWindowClass),
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			sizeof (JamFriendsUI),
			0,
			NULL
		};
		new_type = g_type_register_static(GTK_TYPE_WINDOW,
				"JamFriendsUI", &new_info, 0);
	}
	return new_type;
}

static GtkWidget*
friendsui_new(void) {
	JamFriendsUI *fui = FRIENDSUI(g_object_new(friendsui_get_type(), NULL));
	return GTK_WIDGET(fui);
}

static gboolean request_remove_friend(JamFriendsUI *fui, const char *username);

static void
recalculate_stats(JamFriendsUI *fui) {
	GSList *l;
	LJFriend *f;
	char buf[1024];
	gboolean includecomm;

	int count, fmcount, focount;
	count = fmcount = focount = 0;

	includecomm = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fui->statscomm));

	for (l = fui->friends; l; l = l->next) {
		f = l->data;

		if (!includecomm && f->type == LJ_FRIEND_TYPE_COMMUNITY)
			continue;
		
		switch (f->conn) {
			case LJ_FRIEND_CONN_MY:
				fmcount++; break;
			case LJ_FRIEND_CONN_OF:
				focount++; break;
			default:
				break;
		}
		count++;
	}

	g_snprintf(buf, 1024, _("Total Connections: %d\n"
			"\n"
			"Inclusive:\n"
			"Friends: %d\n"
			"Friend Of: %d\n"
			"Ratio: %d%%\n"
			"\n"
			"Exclusive:\n"
			"Friends: %d\n"
			"Friend Of: %d\n"
			"Friend Both: %d\n"),
			count,
			count-focount, count-fmcount,
			((count-fmcount) != 0) ? ((count-focount)*100)/(count-fmcount) : 0,
			fmcount, focount, count-fmcount-focount);
	gtk_label_set_markup(GTK_LABEL(fui->statslabel), buf);
}

static LJFriend*
friend_exists(JamFriendsUI *fui, char *username) {
	GSList *l;
	LJFriend *f;

	for (l = fui->friends; l; l = l->next) {
		f = l->data;
		if (strcmp(f->username, username) == 0)
			return f;
	}

	return NULL;
}

static gint
friend_compare(gconstpointer a, gconstpointer b) {
	return g_ascii_strcasecmp(((LJFriend*)a)->username, ((LJFriend*)b)->username);
}

static void
friends_hash_list_cb(gpointer key, LJFriend *f, GSList **list) {
	*list = g_slist_insert_sorted(*list, f, friend_compare);
}

static gboolean
load_friends(GtkWindow *parent, JamAccountLJ *acc, GSList **l) {
	LJGetFriends *getfriends;
	NetContext *ctx;
	
	ctx = net_ctx_gtk_new(parent, _("Loading Friends"));
	getfriends = lj_getfriends_new(jam_account_lj_get_user(acc));
	if (!net_run_verb_ctx((LJVerb*)getfriends, ctx, NULL)) { 
		lj_getfriends_free(getfriends, TRUE);
		net_ctx_gtk_free(ctx);
		return FALSE;
	}

	g_hash_table_foreach(getfriends->friends, (GHFunc)friends_hash_list_cb, l);

	lj_getfriends_free(getfriends, FALSE);
	net_ctx_gtk_free(ctx);

	return TRUE;
}

static void
populate_model(JamFriendsUI *fui) {
	GtkListStore *liststore;
	GSList *l;
	LJFriend *f;
	GtkTreeIter iter;
	
	liststore = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(fui->friendview)));
	gtk_list_store_clear(liststore);
	
	for (l = fui->friends; l; l = l->next) {
		f = l->data;

		if (fui->filter_conn && !(fui->filter_conn == f->conn))
			continue;

		if (fui->filter_type && !(fui->filter_type == f->type))
			continue;

		gtk_list_store_append(liststore, &iter);
		gtk_list_store_set(liststore, &iter,
				0, f,
				-1);
	}
}

static void close_cb         (JamFriendsUI *fui);
static void friend_add_cb    (JamFriendsUI *fui);
static void friend_edit_cb   (JamFriendsUI *fui);
static void friend_remove_cb (JamFriendsUI *fui);
static void friend_journal_view_cb(JamFriendsUI *fui);
static void friend_userinfo_view_cb(JamFriendsUI *fui);
static void friendgroups_cb  (JamFriendsUI *fui);
static void stats_cb         (JamFriendsUI *fui,
                              gint action, GtkCheckMenuItem *item);
static void export_cb        (JamFriendsUI *fui);

static GtkWidget*
make_menu(JamFriendsUI *fui) {
	GtkWidget *bar;
	GtkAccelGroup *accelgroup = NULL;
	GtkItemFactory *item_factory = NULL;

static GtkItemFactoryEntry menu_items[] = {
	{ N_("/_Friends"),               NULL, NULL, 0, "<Branch>" },
	{ N_("/Friends/_Friend Groups..."), NULL, friendgroups_cb, 0, NULL },
	{ N_("/Friends/sep"),              NULL, NULL, 0, "<Separator>" },
	{ N_("/Friends/_Close"),     NULL, close_cb, 0,
	                           "<StockItem>", GTK_STOCK_CLOSE },

	{ N_("/_Edit"),                 NULL, NULL, ACTION_EDIT_MENU, "<Branch>" },
	{ N_("/Edit/_Add Friend..."),   NULL, friend_add_cb, ACTION_ADD,
	                             "<StockItem>", GTK_STOCK_ADD },
	{ N_("/Edit/_Edit Friend..."),  NULL, friend_edit_cb, ACTION_EDIT,
	                             "<StockItem>", GTK_STOCK_PROPERTIES },
	{ N_("/Edit/_Remove Friend"),   NULL, friend_remove_cb, ACTION_REMOVE,
	                             "<StockItem>", GTK_STOCK_REMOVE },
	{ N_("/Edit/sep"),              NULL, NULL, 0, "<Separator>" },
	{ N_("/Edit/Open _Journal"),    NULL, friend_journal_view_cb, ACTION_JOURNAL_VIEW,
	                             "<StockItem>", GTK_STOCK_JUMP_TO },
	{ N_("/Edit/Open _User Info"),  NULL, friend_userinfo_view_cb, ACTION_USERINFO_VIEW,
	                             "<StockItem>", GTK_STOCK_JUMP_TO },

	{ N_("/_Tools"),                 NULL, NULL, 0, "<Branch>" },
	{ N_("/Tools/Hide _Statistics"), NULL, stats_cb, ACTION_HIDE_STATISTICS, "<CheckItem>" },
	{ N_("/Tools/Export..."),        NULL, export_cb, 0, NULL },
};
	int itemcount = sizeof(menu_items) / sizeof(menu_items[0]);

	accelgroup = gtk_accel_group_new();
	item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accelgroup);
	gtk_item_factory_set_translate_func(item_factory, gettext_translate_func, NULL, NULL);
	gtk_item_factory_create_items(item_factory, itemcount, menu_items, fui);
	gtk_window_add_accel_group(GTK_WINDOW(fui), accelgroup);

	bar = gtk_item_factory_get_widget(item_factory, "<main>");
	fui->itemfactory = item_factory;

	return bar;
}

static void
close_cb(JamFriendsUI *fui) {
	gtk_widget_destroy(GTK_WIDGET(fui));
}

static void
update_edit_menu(JamFriendsUI *fui, GtkTreeSelection *sel) {
	GtkTreeModel *model;
	GtkTreeIter iter;
	LJFriend *f = NULL;
	gboolean issel, active;

	if (sel == NULL)
		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fui->friendview));

	issel = gtk_tree_selection_get_selected(sel, &model, &iter);

	if (issel) 
		gtk_tree_model_get(model, &iter,
				0, &f,
				-1);

	active = issel && f->conn & LJ_FRIEND_CONN_MY;
	gtk_widget_set_sensitive(
			gtk_item_factory_get_widget_by_action(fui->itemfactory,
				ACTION_EDIT),
			active);
	gtk_widget_set_sensitive(
			gtk_item_factory_get_widget_by_action(fui->itemfactory,
				ACTION_REMOVE),
			active);
	gtk_widget_set_sensitive(
			gtk_item_factory_get_widget_by_action(fui->itemfactory,
				ACTION_JOURNAL_VIEW),
			issel);
	gtk_widget_set_sensitive(
			gtk_item_factory_get_widget_by_action(fui->itemfactory,
				ACTION_USERINFO_VIEW),
			issel);
}

static void 
friend_add_cb(JamFriendsUI *fui) {
	GtkTreeSelection *sel;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;

	LJFriend *f = NULL, *newf;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fui->friendview));

	if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
		gtk_tree_model_get(model, &iter,
				0, &f,
				-1);

		if (f->conn & LJ_FRIEND_CONN_MY) {
			/* we can't add friends we already have... */
			f = NULL;
		}
	}

	newf = friend_edit_dlg_run(GTK_WINDOW(fui), fui->account, FALSE, f);
	if (!newf) return;

	if (f != newf) {
		/* they didn't edit the friend they might have clicked on,
		 * but they could have typed an existing friend's name in. */
		f = friend_exists(fui, newf->username);
		if (f) {
			f->foreground = newf->foreground;
			f->background = newf->background;
			lj_friend_free(newf);
			newf = f;
		}
	}

	/* did they modify an existing friend, or add a new one? */
	if (f == newf) {
		GtkTreePath *path;

		f->conn |= LJ_FRIEND_CONN_MY;

		/* now we need to signal to the tree model
		 * that an element has changed. */
		gtk_tree_model_get_iter_first(model, &iter);
		do {
			gtk_tree_model_get(model, &iter, 0, &newf, -1);
		} while (newf != f && gtk_tree_model_iter_next(model, &iter));

		/* it's possible they modified a friend
		 * who is not in the current view,
		 * so we only should modify the model
		 * if we found that friend. */
		if (newf == f) {
			path = gtk_tree_model_get_path(model, &iter);
			gtk_tree_model_row_changed(model, path, &iter);
			gtk_tree_path_free(path);
		}
	} else {
		GtkListStore *liststore = GTK_LIST_STORE(model);
		GtkTreeIter iter;

		/* if the user adds themself, the connection is immediately two-way. */
		if (g_ascii_strcasecmp(newf->username,
					jam_account_get_username(JAM_ACCOUNT(fui->account))) == 0)
			newf->conn = LJ_FRIEND_CONN_BOTH;

		fui->friends = g_slist_insert_sorted(fui->friends, newf, friend_compare);

		gtk_list_store_append(liststore, &iter);
		gtk_list_store_set(liststore, &iter,
				0, newf,
				-1);
	}
	update_edit_menu(fui, sel);
	recalculate_stats(fui);
}

static void 
friend_edit_cb(JamFriendsUI *fui) {
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;

	LJFriend *f;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fui->friendview));

	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;

	gtk_tree_model_get(model, &iter,
			0, &f,
			-1);

	f = friend_edit_dlg_run(GTK_WINDOW(fui), fui->account, TRUE, f);
	if (!f) return;

	f->conn |= LJ_FRIEND_CONN_MY;

	path = gtk_tree_model_get_path(model, &iter);
	gtk_tree_model_row_changed(model, path, &iter);
	gtk_tree_path_free(path);

	update_edit_menu(fui, sel);
	recalculate_stats(fui);
}

static void 
friend_remove_cb(JamFriendsUI *fui) {
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;

	LJFriend *f;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fui->friendview));

	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;

	gtk_tree_model_get(model, &iter,
			0, &f,
			-1);

	if (!request_remove_friend(fui, f->username))
		return;

	f->conn &= ~LJ_FRIEND_CONN_MY;
	
	/* if the user removes themself, the connection is completely gone. */
	if (g_ascii_strcasecmp(f->username,
				jam_account_get_username(JAM_ACCOUNT(fui->account))) == 0)
		f->conn = 0;

	if (f->conn == 0) {
		/* delete from our list. */
		gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
		fui->friends = g_slist_remove(fui->friends, f);
		lj_friend_free(f);
	} else {
		/* we need to tell the view that we changed something. */
		GtkTreePath *path;

		path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_model_row_changed(model, path, &iter);
		gtk_tree_path_free(path);
	}

	update_edit_menu(fui, sel);
	recalculate_stats(fui);
}

static void
friendgroups_cb(JamFriendsUI *fui) {
	friendgroups_dialog_new(GTK_WINDOW(fui), fui->account, fui->friends);
}

static void
stats_cb(JamFriendsUI *fui, gint action, GtkCheckMenuItem *item) {
	conf.options.friends_hidestats = gtk_check_menu_item_get_active(item);
	jam_widget_set_visible(fui->statspanel, !conf.options.friends_hidestats);
}

static void 
row_selected(GtkTreeSelection *sel, JamFriendsUI *fui) {
	update_edit_menu(fui, sel);
}

static gboolean
request_remove_friend(JamFriendsUI *fui, const char *username) {
	NetContext *ctx;
	LJEditFriends *ef;
	gboolean ret;

	ef = lj_editfriends_new(jam_account_lj_get_user(fui->account));
	lj_editfriends_add_delete(ef, username);
	ctx = net_ctx_gtk_new(GTK_WINDOW(fui), _("Deleting Friend"));
	ret = net_run_verb_ctx((LJVerb*)ef, ctx, NULL);
	lj_editfriends_free(ef);
	net_ctx_gtk_free(ctx);

	return ret;
}

static void
link_data_func(GtkTreeViewColumn *tree_column, 
               GtkCellRenderer   *cell, 
               GtkTreeModel      *model, 
               GtkTreeIter       *iter, 
               gpointer           data)
{
	JamFriendsUI *fui = data;
	LJFriend *friend;
	gtk_tree_model_get(model, iter, 
			0, &friend,
			-1);
	switch (friend->conn) {
		case LJ_FRIEND_CONN_MY:
			g_object_set(cell,
					"pixbuf", fui->pb_rarrow,
					NULL);
			break;
		case LJ_FRIEND_CONN_OF:
			g_object_set(cell,
					"pixbuf", fui->pb_larrow,
					NULL);
			break;
		case LJ_FRIEND_CONN_BOTH:
			g_object_set(cell,
					"pixbuf", fui->pb_lrarrow,
					NULL);
			break;
	}
}

static gint
link_sort_func(GtkTreeModel *model,
               GtkTreeIter  *a,
               GtkTreeIter  *b,
               gpointer      data)
{
	LJFriend *fa, *fb;
	gtk_tree_model_get(model, a, 
			0, &fa,
			-1);
	gtk_tree_model_get(model, b, 
			0, &fb,
			-1);
	if (fa->conn < fb->conn)
		return -1;
	if (fa->conn > fb->conn)
		return 1;
	return 0;
}

static void
user_pixbuf_data_func(GtkTreeViewColumn *tree_column, 
                      GtkCellRenderer   *cell, 
                      GtkTreeModel      *model, 
                      GtkTreeIter       *iter, 
                      gpointer           data)
{
	JamFriendsUI *fui = data;
	LJFriend *friend;
	gtk_tree_model_get(model, iter, 
			0, &friend,
			-1);
	switch (friend->type) {
		case LJ_FRIEND_TYPE_COMMUNITY:
			g_object_set(cell,
					"pixbuf", fui->pb_comm,
					NULL);
			break;
		default:
			g_object_set(cell,
					"pixbuf", fui->pb_user,
					NULL);
	}
}

static void
rgb_to_gdkcolor(guint32 color, GdkColor *gdkcolor) {
	/* gdkcolors are 16 bits per channel. */
	gdkcolor->red   = (color & 0x00FF0000) >> 8;
	gdkcolor->green = (color & 0x0000FF00);
	gdkcolor->blue  = (color & 0x000000FF) << 8;
}

static void
username_data_func(GtkTreeViewColumn *tree_column, 
                    GtkCellRenderer   *cell, 
                    GtkTreeModel      *model, 
                    GtkTreeIter       *iter, 
                    gpointer           data)
{
	LJFriend *friend;
	GdkColor fg = {0}, bg = {0};

	gtk_tree_model_get(model, iter, 
			0, &friend,
			-1);

	rgb_to_gdkcolor(friend->foreground, &fg);
	rgb_to_gdkcolor(friend->background, &bg);

	g_object_set(cell,
			"text", friend->username,
			"foreground-gdk", &fg,
			"background-gdk", &bg,
			NULL);
}

static gint
username_sort_func(GtkTreeModel *model,
                   GtkTreeIter  *a,
                   GtkTreeIter  *b,
                   gpointer      data)
{
	LJFriend *fa, *fb;
	gtk_tree_model_get(model, a, 
			0, &fa,
			-1);
	gtk_tree_model_get(model, b, 
			0, &fb,
			-1);
	return g_ascii_strcasecmp(fa->username, fb->username);
}

static void
fullname_data_func(GtkTreeViewColumn *tree_column, 
                   GtkCellRenderer   *cell, 
                   GtkTreeModel      *model, 
                   GtkTreeIter       *iter, 
                   gpointer           data)
{
	LJFriend *friend;
	gtk_tree_model_get(model, iter, 
			0, &friend,
			-1);
	g_object_set(cell,
			"text", friend->fullname,
			"style", PANGO_STYLE_ITALIC,
			NULL);
}

static gint
fullname_sort_func(GtkTreeModel *model,
               GtkTreeIter  *a,
               GtkTreeIter  *b,
               gpointer      data)
{
	LJFriend *fa, *fb;
	gtk_tree_model_get(model, a, 
			0, &fa,
			-1);
	gtk_tree_model_get(model, b, 
			0, &fb,
			-1);
	return g_ascii_strcasecmp(fa->fullname, fb->fullname);
}

static gchar*
selected_username_get(JamFriendsUI *fui) {
	GtkWidget *view = fui->friendview;
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;

	LJFriend *f = NULL;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return NULL;

	gtk_tree_model_get(model, &iter, 0, &f, -1); 
	
	return f ?f->username : NULL;
}

static void
friend_journal_view_cb(JamFriendsUI *fui) {
	gchar *friendname = selected_username_get(fui);
	gchar url[2000];

	g_snprintf(url, 2000, "%s/users/%s/",
			jam_account_lj_get_server(fui->account)->url, friendname);
	spawn_url(GTK_WINDOW(fui), url);
}

static void
friend_userinfo_view_cb(JamFriendsUI *fui) {
	gchar *friendname = selected_username_get(fui);
	gchar url[2000];

	g_snprintf(url, 2000, "%s/userinfo.bml?user=%s",
			jam_account_lj_get_server(fui->account)->url, friendname);
	spawn_url(GTK_WINDOW(fui), url);
}

static gboolean
button_press_cb(GtkTreeView *view, GdkEventButton *e, JamFriendsUI *fui) {
	GtkWidget *frmenu;

	if (e->button != 3)
		return FALSE;
	
	frmenu = gtk_item_factory_get_widget_by_action(fui->itemfactory, ACTION_EDIT_MENU);
	gtk_menu_popup(GTK_MENU(frmenu), NULL, NULL, NULL, NULL,
			3, e->time);

	return FALSE; /* we must let this event through.
					 see the FIXME where this signal is hooked up. */
}

static gboolean
search_equal_cb(GtkTreeModel *model, gint column, const gchar *key,
		GtkTreeIter *iter, gpointer data) {
	LJFriend *f;
	gtk_tree_model_get(model, iter,
			0, &f,
			-1);
	return g_ascii_strcasecmp(key, f->username) > 0;
}

static GtkWidget*
friends_list_create(JamFriendsUI *fui) {
	GtkWidget *view;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;
	GtkTreeModel *friendstore;
	GtkTreeSelection *sel;

	friendstore = GTK_TREE_MODEL(gtk_list_store_new(1, G_TYPE_POINTER));
	fui->friendview = view = gtk_tree_view_new_with_model(friendstore);
	g_object_unref(G_OBJECT(friendstore));

	populate_model(fui);

	/* HACK: should be connect_after, because we want the list item
	 * to be selected first.  due to a gtk bug(?) a connect_after'd event never
	 * is called.  it works out ok in this case anyway because the row_selected
	 * event dynamically enables/disables the menu items, but that's weird.
	 *
	 * This prompts some kludgery for handling the context menu correctly.
	 */
	g_signal_connect(G_OBJECT(view), "button-press-event",
			G_CALLBACK(button_press_cb), fui);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	update_edit_menu(fui, sel);
	g_signal_connect(G_OBJECT(sel), "changed",
			G_CALLBACK(row_selected), fui);

	fui->pb_user = gtk_widget_render_icon(view, 
			"logjam-ljuser", GTK_ICON_SIZE_MENU, NULL);
	fui->pb_comm = gtk_widget_render_icon(view, 
			"logjam-ljcomm", GTK_ICON_SIZE_MENU, NULL);
	fui->pb_larrow = icons_larrow_pixbuf();
	fui->pb_rarrow = icons_rarrow_pixbuf();
	fui->pb_lrarrow = icons_lrarrow_pixbuf();

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Link"));

	cell_renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, cell_renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func(column, cell_renderer,
			link_data_func, fui, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(friendstore),
			FRIEND_COL_LINK,
			link_sort_func, NULL, NULL);
	gtk_tree_view_column_set_sort_column_id(column, FRIEND_COL_LINK);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("User"));

	cell_renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, cell_renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func(column, cell_renderer,
			user_pixbuf_data_func, fui, NULL);

	cell_renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, cell_renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func(column, cell_renderer,
			username_data_func, NULL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(friendstore),
			FRIEND_COL_USERNAME,
			username_sort_func, NULL, NULL);
	gtk_tree_view_column_set_sort_column_id(column, FRIEND_COL_USERNAME);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Full Name"));

	cell_renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, cell_renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func(column, cell_renderer,
			fullname_data_func, fui, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(friendstore),
			FRIEND_COL_FULLNAME,
			fullname_sort_func, NULL, NULL);
	gtk_tree_view_column_set_sort_column_id(column, FRIEND_COL_FULLNAME);

	gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(friendstore),
			username_sort_func, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(friendstore),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(view),
			search_equal_cb, NULL, NULL);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), FRIEND_COL_USERNAME);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), TRUE);
	
	return scroll_wrap(view);
}

static GtkWidget*
make_stats_panel(JamFriendsUI *fui) {
	GtkWidget *vbox, *label;

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Statistics</b>"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	label = gtk_label_new(NULL);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	fui->statscomm = gtk_check_button_new_with_label(_("Include Communities"));
	g_signal_connect_swapped(G_OBJECT(fui->statscomm), "toggled",
			G_CALLBACK(recalculate_stats), fui);
	gtk_box_pack_start(GTK_BOX(vbox), fui->statscomm, FALSE, FALSE, 0);

	fui->statspanel = vbox;
	fui->statslabel = label;
	recalculate_stats(fui);

	return vbox;
}

static void
friendsui_destroy_cb(GtkWidget *w, JamFriendsUI *fui) {
	g_slist_foreach(fui->friends, (GFunc)lj_friend_free, NULL);
	g_slist_free(fui->friends);

	UNREF_AND_NULL(fui->pb_user);
	UNREF_AND_NULL(fui->pb_comm);
	UNREF_AND_NULL(fui->pb_larrow);
	UNREF_AND_NULL(fui->pb_rarrow);
	UNREF_AND_NULL(fui->pb_lrarrow);
}

static GtkWidget*
make_menu_item(GdkPixbuf *pb, const char *label) {
	GtkWidget *hbox, *item;

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox), 
			gtk_image_new_from_pixbuf(pb), 
			FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), 
			gtk_label_new(label), FALSE, FALSE, 0);

	item = gtk_menu_item_new();
	gtk_container_add(GTK_CONTAINER(item), hbox);
	return item;
}

static void
filter_type_cb(GtkOptionMenu *om, JamFriendsUI *fui) {
	/* yuck, hard-coded magic numbers. */
	switch (gtk_option_menu_get_history(om)) {
		case 0: /* reset */
			fui->filter_type = 0;
			break;
		case 2: 
			fui->filter_type = LJ_FRIEND_TYPE_USER;
			break;
		case 3: 
			fui->filter_type = LJ_FRIEND_TYPE_COMMUNITY;
			break;
	}
	populate_model(fui);
}

static void
filter_conn_cb(GtkOptionMenu *om, JamFriendsUI *fui) {
	/* yuck, hard-coded magic numbers. */
	switch (gtk_option_menu_get_history(om)) {
		case 0: /* reset */
			fui->filter_conn = 0;
			break;
		case 2: 
			fui->filter_conn = LJ_FRIEND_CONN_BOTH;
			break;
		case 3: 
			fui->filter_conn = LJ_FRIEND_CONN_MY;
			break;
		case 4: 
			fui->filter_conn = LJ_FRIEND_CONN_OF;
			break;
	}
	populate_model(fui);
}

static GtkWidget*
make_filter_box(JamFriendsUI *fui) {
	GtkWidget *box, *menu, *item;;

	fui->omtype = gtk_option_menu_new();
	menu = gtk_menu_new();
	item = make_menu_item(NULL, _("All Journal Types"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_menu_item_new());
	item = make_menu_item(fui->pb_user, _("Users"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	item = make_menu_item(fui->pb_comm, _("Communitites"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(fui->omtype), menu);
	g_signal_connect(G_OBJECT(fui->omtype), "changed",
			G_CALLBACK(filter_type_cb), fui);
	
	fui->omconn = gtk_option_menu_new();
	menu = gtk_menu_new();
	item = make_menu_item(NULL, _("All Connections"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_menu_item_new());
	item = make_menu_item(fui->pb_lrarrow, _("Two-way Friends"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	item = make_menu_item(fui->pb_rarrow, _("One-way Friends"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	item = make_menu_item(fui->pb_larrow, _("One-way Friendofs"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(fui->omconn), menu);
	g_signal_connect(G_OBJECT(fui->omconn), "changed",
			G_CALLBACK(filter_conn_cb), fui);

	box = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), gtk_label_new(_("Show:")), FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), fui->omtype, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), fui->omconn, FALSE, FALSE, 5);

	return box;
}

void 
friends_manager_show(GtkWindow *mainwin, JamAccountLJ *acc) {
	JamFriendsUI *fui;
	GtkWidget *mainbox, *hbox;
	GSList *friends = NULL;

	if (!load_friends(mainwin, acc, &friends))
		return;

	fui = FRIENDSUI(friendsui_new());
	fui->account = acc;
	fui->friends = friends;
	gtk_window_set_title(GTK_WINDOW(fui), _("Friends Manager"));
	gtk_window_set_default_size(GTK_WINDOW(fui), 400, 300);
	geometry_tie(GTK_WIDGET(fui), GEOM_FRIENDS);
	g_signal_connect(G_OBJECT(fui), "destroy",
			G_CALLBACK(friendsui_destroy_cb), fui);

	mainbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(mainbox), make_menu(fui), FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), friends_list_create(fui), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), make_stats_panel(fui), FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(mainbox), hbox, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(mainbox), make_filter_box(fui), FALSE, FALSE, 5);

	gtk_widget_show_all(mainbox);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
				gtk_item_factory_get_widget_by_action(fui->itemfactory, ACTION_HIDE_STATISTICS)),
			conf.options.friends_hidestats);

	gtk_container_add(GTK_CONTAINER(fui), mainbox);

	gtk_widget_show(GTK_WIDGET(fui));
}

static void
export_do(JamFriendsUI *fui, GtkFileChooser *fsel) {
	const char *filename = gtk_file_chooser_get_filename(fsel);
	FILE *fout;
	LJFriend *f;
	GSList *l;

	if ((fout = fopen(filename, "w")) == NULL) {
		jam_warning(GTK_WINDOW(fui), _("Unable to open %s: %s\n"), 
				filename, g_strerror(errno));
		return;
	}
	for (l = fui->friends; l != NULL; l = l->next) {
		f = l->data;

		fprintf(fout, "%c-%c %s\n", 
				(f->conn & LJ_FRIEND_CONN_OF ? '<' : ' '),
				(f->conn & LJ_FRIEND_CONN_MY ? '>' : ' '),
				f->username);
	}

	fclose(fout);
}

static void
suggest_cb(GtkWidget *w, GtkFileChooser *fsel) {
	char buf[50];
	time_t curtime;
	struct tm *date;
	
	time(&curtime);
	date = localtime(&curtime);

	sprintf(buf, "friends.%04d-%02d-%02d", 
			date->tm_year+1900, date->tm_mon+1, date->tm_mday);

	gtk_file_chooser_set_filename(fsel, buf);
}

static void
export_cb(JamFriendsUI *fui) {
	GtkWidget *fsel;
	GtkWidget *bstamp;

	fsel = gtk_file_chooser_dialog_new(_("Select output file"), GTK_WINDOW(fui),
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);

	bstamp = gtk_button_new_with_label(_("  Suggest Filename  "));

	gtk_widget_show(bstamp);
	gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(fsel), bstamp);
	g_signal_connect(G_OBJECT(bstamp), "clicked",
			G_CALLBACK(suggest_cb), 
			GTK_FILE_CHOOSER(fsel));

	if (gtk_dialog_run(GTK_DIALOG(fsel)) == GTK_RESPONSE_ACCEPT)
		export_do(fui, GTK_FILE_CHOOSER(fsel));
	gtk_widget_destroy(fsel);
}

