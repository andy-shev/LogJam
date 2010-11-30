/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include <livejournal/editfriendgroups.h>

#include "conf.h"
#include "util-gtk.h"
#include "network.h"
#include "friends.h"
#include "friendgroupedit.h"
#include "friendgroups.h"

typedef struct {
	GtkWidget *win;
	GtkWidget *groups, *inc, *exc;
	GtkListStore *lgroups, *linc, *lexc;

	JamAccountLJ *account;
	GSList *friends;
} FriendGroupsUI;

enum {
	FG_COL_NAME,
	FG_COL_FG
};
enum {
	FRIEND_COL_NAME,
	FRIEND_COL_FRIEND
};

static int
findfreegroup(FriendGroupsUI *fgui) {
	int freegroup;

	for (freegroup = 1; freegroup <= 30; freegroup++) {
		if (lj_friendgroup_from_id(jam_account_lj_get_user(fgui->account), freegroup) == NULL) {
			/* this id isn't in use! */
			return freegroup;
		}
	}
	jam_warning(GTK_WINDOW(fgui->win), _("Couldn't find free friend group(?)"));
	return -1; /* we didn't find one. */
}

static void
dlg_add_friend_group(FriendGroupsUI *fgui, LJFriendGroup *fg) {
	GtkTreeIter iter;

	gtk_list_store_append(fgui->lgroups, &iter);
	gtk_list_store_set(fgui->lgroups, &iter,
			FG_COL_NAME, fg->name,
			FG_COL_FG,   fg,
			-1);
}

static void
new_cb(GtkWidget *w, FriendGroupsUI *fgui) {
	LJUser *u;
	LJFriendGroup *fg;
	int freegroup;

	freegroup = findfreegroup(fgui);
	if (freegroup == -1) return; /* FIXME */

	fg = friend_group_edit_dlg_run(GTK_WINDOW(fgui->win), fgui->account, NULL, freegroup);
	if (fg == NULL) return; /* they cancelled. */

	/* new friend group! */
	u = jam_account_lj_get_user(fgui->account);
	u->friendgroups = g_slist_append(u->friendgroups, fg);
	dlg_add_friend_group(fgui, fg);
}

static LJFriendGroup*
get_selected_fg(FriendGroupsUI *fgui, GtkTreeIter *piter) {
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;
	LJFriendGroup *fg;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fgui->groups));
	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return NULL;
	if (piter)
		*piter = iter;

	gtk_tree_model_get(model, &iter,
			FG_COL_FG, &fg,
			-1);
	return fg;
}

static void
tree_multi_selected_cb(GtkTreeModel *source, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {
	*(gboolean*)data = TRUE;
}

static gboolean
tree_multi_selected(GtkTreeSelection *sel) {
	gboolean ret = FALSE;
	gtk_tree_selection_selected_foreach(sel, tree_multi_selected_cb, &ret);
	return ret;
}

static void
edit_cb(GtkWidget *w, FriendGroupsUI *fgui) {
	LJFriendGroup *fg;
	GtkTreeIter iter;

	fg = get_selected_fg(fgui, &iter);
	if (fg == NULL)
		return;

	fg = friend_group_edit_dlg_run(GTK_WINDOW(fgui->win), fgui->account, fg, -1);
	if (fg == NULL) return; /* they cancelled. */

	gtk_list_store_set(GTK_LIST_STORE(fgui->lgroups), &iter,
			FG_COL_NAME, fg->name,
			-1);
}

static void
remove_cb(GtkWidget *w, FriendGroupsUI *fgui) {
	NetContext *ctx;
	LJEditFriendGroups *efg;
	LJUser *u = jam_account_lj_get_user(fgui->account);
	LJFriendGroup *fg;
	GSList *l;
	guint32 gmask;
	GtkTreeIter iter;

	fg = get_selected_fg(fgui, &iter);
	if (fg == NULL)
		return;
	gmask = 1L << fg->id;

	efg = lj_editfriendgroups_new(u);
	lj_editfriendgroups_add_delete(efg, fg->id);

	/* unlink all friends from this group */
	/* (unless we do this, these users will erroneously be included in a
	 * future group that gets this doomed group's id!) */
	for (l = fgui->friends; l != NULL; l = l->next) {
		LJFriend *f = l->data;
		/* only update users in this group (good for large friends lists) */
		if (f->groupmask & gmask)
			lj_editfriendgroups_add_groupmask(efg, f->username,
					f->groupmask & ~gmask);
	}

	ctx = net_ctx_gtk_new(GTK_WINDOW(fgui->win), _("Removing Friend Group"));
	if (!net_run_verb_ctx((LJVerb*)efg, ctx, NULL)) {
		lj_editfriendgroups_free(efg);
		net_ctx_gtk_free(ctx);
		return;
	}

	/* now that it succeeded, actually update our local friends list. */
	for (l = fgui->friends; l != NULL; l = l->next) {
		LJFriend *f = l->data;
		f->groupmask &= ~gmask;
	}

	gtk_list_store_remove(fgui->lgroups, &iter);
	u->friendgroups = g_slist_remove(u->friendgroups, fg);
	g_free(fg->name);
	g_free(fg);

	lj_editfriendgroups_free(efg);
	net_ctx_gtk_free(ctx);
}

static void
populate_grouplist(FriendGroupsUI *fgui) {
	LJUser *u = jam_account_lj_get_user(fgui->account);
	GSList *lgroup;

	for (lgroup = u->friendgroups; lgroup != NULL; lgroup = lgroup->next) {
		LJFriendGroup *fg = lgroup->data;

		dlg_add_friend_group(fgui, fg);
	}
}

static void
populate_friendlists(FriendGroupsUI *fgui, LJFriendGroup *fg) {
	GtkTreeIter iter;
	GtkListStore *list;
	GSList *l;
	guint32 mask;

	gtk_list_store_clear(fgui->linc);
	gtk_list_store_clear(fgui->lexc);

	if (fg == NULL)
		return;

	mask = 1L << fg->id;
	for (l = fgui->friends; l != NULL; l = l->next) {
		LJFriend *f = l->data;

		if (!(f->conn & LJ_FRIEND_CONN_MY))
			continue;

		if (f->groupmask & mask) {
			list = fgui->linc;
		} else {
			list = fgui->lexc;
		}

		gtk_list_store_append(list, &iter);
		gtk_list_store_set(list, &iter,
				FRIEND_COL_NAME, f->username,
				FRIEND_COL_FRIEND, f,
				-1);
	}
}

static void
friendgroup_sel_cb(GtkTreeSelection *sel, FriendGroupsUI *fgui) {
	GtkTreeIter iter;
	GtkTreeModel *model;
	LJFriendGroup *fg = NULL;

	if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
		gtk_tree_model_get(model, &iter,
				FG_COL_FG, &fg,
				-1);
	}
	populate_friendlists(fgui, fg);
}

static GtkWidget*
fg_list_create(FriendGroupsUI *fgui) {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *sel;

	fgui->lgroups = gtk_list_store_new(2,
			G_TYPE_STRING, /* name */
			G_TYPE_POINTER /* fg   */);
	populate_grouplist(fgui);

	fgui->groups = gtk_tree_view_new_with_model(GTK_TREE_MODEL(fgui->lgroups));
	g_object_unref(G_OBJECT(fgui->lgroups));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Friend Groups"), renderer,
			"text", FG_COL_NAME,
			NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(fgui->groups), column);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fgui->groups));
	g_signal_connect(G_OBJECT(sel), "changed",
			G_CALLBACK(friendgroup_sel_cb), fgui);

	return scroll_wrap(fgui->groups);
}

typedef struct {
	LJEditFriendGroups *efg;
	guint32 gmask;
	gboolean add;
} FGAddRem;

static void
friend_addrem_pre_cb(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {
	FGAddRem *p = data;
	LJFriend *f;
	guint32 mask;

	gtk_tree_model_get(model, iter,
			FRIEND_COL_FRIEND, &f,
			-1);

	mask = f->groupmask;
	if (p->add)
		mask |= p->gmask;
	else
		mask &= ~p->gmask;

	lj_editfriendgroups_add_groupmask(p->efg, f->username, mask);
}

static void
friend_addrem_post_cb(GtkTreeModel *source, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {
	FGAddRem *p = data;
	LJFriend *f;

	gtk_tree_model_get(source, iter,
			FRIEND_COL_FRIEND, &f,
			-1);

	if (p->add)
		f->groupmask |= p->gmask;
	else
		f->groupmask &= ~p->gmask;
}

static void
addrem_cb(FriendGroupsUI *fgui,
		GtkTreeView *source, GtkTreeView *dest, gboolean add) {
	NetContext *ctx;
	LJEditFriendGroups *efg;
	LJFriendGroup *fg;
	guint32 gmask;
	FGAddRem rreq;
	GtkTreeSelection *sel;

	fg = get_selected_fg(fgui, NULL);
	if (!fg) return;

	sel = gtk_tree_view_get_selection(source);
	if (!tree_multi_selected(sel))
		return;

	gmask = (1L << fg->id);

	efg = lj_editfriendgroups_new(jam_account_lj_get_user(fgui->account));

	rreq.efg = efg;
	rreq.gmask = gmask;
	rreq.add = add;

	/* this foreach puts all of the new masks into the network request. */
	gtk_tree_selection_selected_foreach(sel, friend_addrem_pre_cb, &rreq);

	ctx = net_ctx_gtk_new(GTK_WINDOW(fgui->win), _("Modifying Friends"));
	if (!net_run_verb_ctx((LJVerb*)efg, ctx, NULL)) {
		lj_editfriendgroups_free(efg);
		net_ctx_gtk_free(ctx);
		return;
	}

	/* and this foreach actually puts all of the new masks
	 * into our local copy of the friends data. */
	gtk_tree_selection_selected_foreach(sel, friend_addrem_post_cb, &rreq);
	populate_friendlists(fgui, fg);
	lj_editfriendgroups_free(efg);
	net_ctx_gtk_free(ctx);
}

static void
add_cb(GtkWidget *w, FriendGroupsUI *fgui) {
	addrem_cb(fgui, GTK_TREE_VIEW(fgui->exc), GTK_TREE_VIEW(fgui->inc), TRUE);
}

static void
rem_cb(GtkWidget *w, FriendGroupsUI *fgui) {
	addrem_cb(fgui, GTK_TREE_VIEW(fgui->inc), GTK_TREE_VIEW(fgui->exc), FALSE);
}

static void
rowsel_sensitive_cb(GtkTreeSelection *sel, GtkWidget *w) {
	gtk_widget_set_sensitive(w,
		gtk_tree_selection_get_selected(sel, NULL, NULL));
}
static void
multisel_sensitive_cb(GtkTreeSelection *sel, GtkWidget *w) {
	gtk_widget_set_sensitive(w, tree_multi_selected(sel));
}

static GtkWidget*
fg_list_buttonbox(FriendGroupsUI *fgui) {
	GtkWidget *bnew, *bedit, *brem;
	GtkWidget *box;
	GtkTreeSelection *sel;

	bnew = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_tooltips_set_tip(app.tooltips, bnew, _("Add new friend group"),
			_("Use this to add a friend group. These are useful for reading "
			"only a specific protion of your friends list. You could, for "
			"example, use this to filter out high-volume communities that "
			"you like to read occasionally but not all the time. If you add "
			"a friend group here, you can later use it with custom security "
			"modes, and also check for new entries only from this group."));
	g_signal_connect(G_OBJECT(bnew), "clicked",
			G_CALLBACK(new_cb), fgui);

	bedit = gtk_button_new_from_stock(GTK_STOCK_PROPERTIES);
	gtk_tooltips_set_tip(app.tooltips, bedit, _("Edit group"),
			_("Use this to edit a friend group. You can modify its name "
			"and whether other people can see you have this group."));
	g_signal_connect(G_OBJECT(bedit), "clicked",
			G_CALLBACK(edit_cb), fgui);
	gtk_widget_set_sensitive(GTK_WIDGET(bedit), FALSE);

	brem = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	gtk_tooltips_set_tip(app.tooltips, brem, _("Remove friend group"),
			_("Use this to erase this friend group from your list of "
			"friend groups."));
	g_signal_connect(G_OBJECT(brem), "clicked",
			G_CALLBACK(remove_cb), fgui);
	gtk_widget_set_sensitive(GTK_WIDGET(brem), FALSE);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fgui->groups));
	g_signal_connect(G_OBJECT(sel), "changed",
			G_CALLBACK(rowsel_sensitive_cb), bedit);
	g_signal_connect(G_OBJECT(sel), "changed",
			G_CALLBACK(rowsel_sensitive_cb), brem);

	box = jam_dialog_buttonbox_new();
	jam_dialog_buttonbox_add(box, bnew);
	jam_dialog_buttonbox_add(box, bedit);
	jam_dialog_buttonbox_add(box, brem);
	return box;
}

static void
fg_make_friendlist(GtkWidget **pview, GtkListStore **pstore, const char *title) {
	GtkWidget *view;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *sel;

	*pstore = store = gtk_list_store_new(2,
			G_TYPE_STRING, /* name   */
			G_TYPE_POINTER /* friend */);
	*pview = view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(title, renderer,
			"text", FRIEND_COL_NAME,
			NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);
}

static GtkWidget*
make_incexc_side(FriendGroupsUI *fgui, GtkWidget *view, GtkWidget *button) {
	GtkWidget *vbox;
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), scroll_wrap(view), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
	return vbox;
}

static GtkWidget*
make_include_side(FriendGroupsUI *fgui) {
	GtkWidget *button;

	fg_make_friendlist(&fgui->inc, &fgui->linc, _("Included"));

	button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(button),
			gtk_image_new_from_stock(
					GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_MENU));
	gtk_tooltips_set_tip(app.tooltips, button,
			_("Remove friend from group"),
			_("Click here to remove the currently selected user "
			"from the list of users included in this friend "
			"group. This does not stop this user from being "
			"listed as your friend; it only removes them from "
			"this specific group."));
	gtk_widget_set_sensitive(button, FALSE);
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(rem_cb), fgui);

	g_signal_connect(G_OBJECT(
				gtk_tree_view_get_selection(GTK_TREE_VIEW(fgui->inc))),
			"changed",
			G_CALLBACK(multisel_sensitive_cb), button);

	return make_incexc_side(fgui, fgui->inc, button);
}
static GtkWidget*
make_exclude_side(FriendGroupsUI *fgui) {
	GtkWidget *button;

	fg_make_friendlist(&fgui->exc, &fgui->lexc, _("Excluded"));

	button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(button),
			gtk_image_new_from_stock(
					GTK_STOCK_GO_BACK, GTK_ICON_SIZE_MENU));
	gtk_tooltips_set_tip(app.tooltips, button, _("Add friend to group"),
			_("Click here to include this user in the currently "
			"selected friend group. If you have old protected "
			"entries with custom security set to this group, "
			"this means among other things that they will now "
			"have access to these entries."));
	gtk_widget_set_sensitive(button, FALSE);
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(add_cb), fgui);

	g_signal_connect(G_OBJECT(
				gtk_tree_view_get_selection(GTK_TREE_VIEW(fgui->exc))),
			"changed",
			G_CALLBACK(multisel_sensitive_cb), button);

	return make_incexc_side(fgui, fgui->exc, button);
}

void
friendgroups_dialog_new(GtkWindow *parent, JamAccountLJ *acc, GSList *friends) {
	FriendGroupsUI *fgui;
	GtkWidget *paned;
	GtkWidget *vbox, *hbox;

	fgui = g_new0(FriendGroupsUI, 1);
	fgui->account = acc;
	fgui->friends = friends;
	fgui->win = gtk_dialog_new_with_buttons(_("Friend Groups"),
			parent, GTK_DIALOG_MODAL,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);
	jam_win_set_size(GTK_WINDOW(fgui->win), 300, 400);
	g_signal_connect_swapped(G_OBJECT(fgui->win), "response",
			G_CALLBACK(gtk_widget_destroy), fgui->win);
	g_signal_connect_swapped(G_OBJECT(fgui->win), "destroy",
			G_CALLBACK(g_free), fgui);

	paned = gtk_vpaned_new();
	geometry_tie_full(GEOM_FRIENDGROUPS, GTK_WINDOW(fgui->win), GTK_PANED(paned));

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
	gtk_box_pack_start(GTK_BOX(vbox),
			fg_list_create(fgui), TRUE, TRUE, 0);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox),
			fg_list_buttonbox(fgui), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox),
			hbox, FALSE, FALSE, 0);
	gtk_paned_pack1(GTK_PANED(paned), vbox, TRUE, TRUE);

	fg_make_friendlist(&fgui->inc, &fgui->linc, _("Included"));
	fg_make_friendlist(&fgui->exc, &fgui->lexc, _("Excluded"));

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);

	gtk_box_pack_start(GTK_BOX(hbox),
			make_include_side(fgui), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox),
			make_exclude_side(fgui), TRUE, TRUE, 0);

	gtk_paned_pack2(GTK_PANED(paned), hbox, TRUE, TRUE);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(fgui->win)->vbox), paned, TRUE, TRUE, 0);
	gtk_widget_show_all(GTK_DIALOG(fgui->win)->vbox);

	gtk_widget_show(fgui->win);
}
