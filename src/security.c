/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include <stdlib.h>

#include "security.h"
#include "conf.h"
#include "network.h"
#include "icons.h"
#include "util-gtk.h"

struct _SecMgr {
	GtkOptionMenu omenu;
	LJSecurity security;
	GtkWidget *custom;
	JamAccountLJ *account;
};

static void secmgr_init(SecMgr *sm);

static void security_changed(SecMgr *sm);
static void secmgr_destroyed_cb(GObject* sm);

static const LJSecurity security_private = { LJ_SECURITY_PRIVATE, 0 };

/* gtk stuff */
GType
secmgr_get_type(void) {
	static GType sm_type = 0;
	if (!sm_type) {
		const GTypeInfo sm_info = {
			sizeof (GtkOptionMenuClass),
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			sizeof (SecMgr),
			0,
			(GInstanceInitFunc) secmgr_init,
		};
		sm_type = g_type_register_static(GTK_TYPE_OPTION_MENU,
				"SecMgr", &sm_info, 0);
	}
	return sm_type;
}

static GtkWidget*
add_menu_item(GtkMenuShell *ms, const gchar *id, const gchar *text) {
	GtkWidget *hbox;
	GtkWidget *item;

	hbox = gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox),
			gtk_image_new_from_stock(id, GTK_ICON_SIZE_MENU),
			FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox),
			gtk_label_new(text), FALSE, FALSE, 0);

	item = gtk_menu_item_new();
	gtk_container_add(GTK_CONTAINER(item), hbox);
	gtk_menu_shell_append(ms, item);
	return item;
}

static void
secmgr_init(SecMgr *sm) {
	GtkWidget *menu;
	GtkMenuShell *ms;

	menu = gtk_menu_new();
	ms = GTK_MENU_SHELL(menu);

	add_menu_item(ms, "logjam-ljuser", _("Public"));
	add_menu_item(ms, "logjam-protected", _("Friends"));
	add_menu_item(ms, "logjam-private", _("Private"));
	sm->custom = add_menu_item(ms, NULL, _("Custom..."));
	gtk_widget_set_sensitive(sm->custom, FALSE);

	gtk_widget_show_all(menu);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(sm), menu);

	gtk_option_menu_set_history(GTK_OPTION_MENU(sm), sm->security.type);

	g_signal_connect(G_OBJECT(sm), "changed",
			G_CALLBACK(security_changed), NULL);

	/* register secmgr */
	app.secmgr_list = g_slist_append(app.secmgr_list, sm);
	/* g_signal_connect(G_OBJECT(sm), "destroy",
			G_CALLBACK(g_slist_remove), sm);

			wouldn't this have been nice? unfortunately, the return value of
			g_slist_remove must be kept, so we have to wrap this in a function.
	 */
	g_signal_connect(G_OBJECT(sm), "destroy",
			G_CALLBACK(secmgr_destroyed_cb), NULL);
}

static void
secmgr_destroyed_cb(GObject* sm) {
	app.secmgr_list = g_slist_remove(app.secmgr_list, sm);
}

GtkWidget*
secmgr_new(gboolean withcustom) {
	SecMgr *sm = SECMGR(g_object_new(secmgr_get_type(), NULL));
	if (!withcustom)
		gtk_widget_destroy(sm->custom);
	return GTK_WIDGET(sm);
}

void
secmgr_set_account(SecMgr *sm, JamAccountLJ *account) {
	gtk_widget_set_sensitive(sm->custom, account != NULL);
	if (account != sm->account) {
		LJSecurity oldsec;

		sm->account = account;

		secmgr_security_get(sm, &oldsec);
		if (oldsec.type == LJ_SECURITY_CUSTOM)
			secmgr_security_set(sm, &security_private);
	}
}

void
secmgr_security_set(SecMgr *secmgr, const LJSecurity *security) {
	secmgr->security = *security;
	gtk_option_menu_set_history(GTK_OPTION_MENU(secmgr), secmgr->security.type);
}
void
secmgr_security_get(SecMgr *secmgr, LJSecurity *security) {
	*security = secmgr->security;
}

/* a version of secmgr_security_set that avoids emitting the "changed"
 * signal. useful for programmatic adjustments to a SecMgr (such as applying
 * a user's changes from the Settings dialog), in order to avoid running the
 * custom security dialog again. It should *only* be used in cases where
 * the "changed" handler has already dealt with this change, or the input is
 * known to be good for some other reason. */
void
secmgr_security_set_force(SecMgr *secmgr, const LJSecurity *security) {
	g_signal_handlers_block_by_func(G_OBJECT(secmgr),
			security_changed, NULL);
	secmgr_security_set(secmgr, security);
	g_signal_handlers_unblock_by_func(G_OBJECT(secmgr),
			security_changed, NULL);
}

void
security_changed(SecMgr *sm) {
	sm->security.type = gtk_option_menu_get_history(GTK_OPTION_MENU(sm));
	if (sm->security.type == LJ_SECURITY_CUSTOM) {
		sm->security.allowmask =
			custom_security_dlg_run(
					GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(sm))),
					sm->security.allowmask,
					sm->account);
	} else {
		sm->security.allowmask = 0; /* reset it. */
	}

	if (sm->security.type == LJ_SECURITY_CUSTOM
			&& sm->security.allowmask == 0) {
		secmgr_security_set(sm, &security_private);
	}
}

enum {
	COL_INCLUDED,
	COL_NAME,
	COL_FG
};

static void
included_toggle_cb(GtkCellRendererToggle *cell,
                   gchar *path_str, gpointer data) {
	GtkTreeModel *model = data;
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
	gboolean included;

	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter,
			COL_INCLUDED, &included,
			-1);

	included = !included;

	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			COL_INCLUDED, included,
			-1);
	gtk_tree_path_free(path);
}

static GtkTreeModel*
build_model(guint32 mask, LJUser *user) {
	GtkListStore *store;
	GSList *l;
	LJFriendGroup *fg;
	GtkTreeIter iter;

	store = gtk_list_store_new(3,
			G_TYPE_BOOLEAN, /* included?   */
			G_TYPE_STRING,  /* name        */
			G_TYPE_POINTER  /* friendgroup */);
	for (l = user->friendgroups; l != NULL; l = l->next) {
		fg = (LJFriendGroup*)l->data;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				COL_INCLUDED, ((1L << fg->id) & mask) != 0,
				COL_NAME,     fg->name,
				COL_FG,       fg,
				-1);
	}
	return GTK_TREE_MODEL(store);
}

static GtkWidget*
build_view(GtkTreeModel *model) {
	GtkWidget *view;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	view = gtk_tree_view_new_with_model(model);
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Included Groups"));

	renderer = gtk_cell_renderer_toggle_new();
	g_signal_connect(G_OBJECT(renderer), "toggled",
			G_CALLBACK(included_toggle_cb), model);
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer,
			"active", COL_INCLUDED);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer,
			"text", COL_NAME);

	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
	return view;
}

static guint32
get_new_mask(GtkTreeModel *model) {
	GtkTreeIter iter;
	gboolean included;
	LJFriendGroup *fg;
	guint32 newmask = 0;

	if (gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			gtk_tree_model_get(model, &iter,
					COL_INCLUDED, &included,
					COL_FG, &fg,
					-1);
			if (included)
				newmask |= (1L << fg->id);
		} while (gtk_tree_model_iter_next(model, &iter));
	}
	return newmask;
}

guint32
custom_security_dlg_run(GtkWindow *parent, guint32 mask, JamAccountLJ *acc) {
	GtkWidget *dlg;
	GtkTreeModel *model;
	GtkWidget *view;
	LJUser *user = jam_account_lj_get_user(acc);

	dlg = gtk_dialog_new_with_buttons(_("Select Friend Groups"),
			parent, GTK_DIALOG_MODAL,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);
	jam_win_set_size(GTK_WINDOW(dlg), 300, -1);

	model = build_model(mask, user);
	view = build_view(model);

	jam_dialog_set_contents(GTK_DIALOG(dlg), scroll_wrap(view));

	gtk_dialog_run(GTK_DIALOG(dlg));
	mask = get_new_mask(model);
	g_object_unref(G_OBJECT(model));
	gtk_widget_destroy(dlg);
	return mask;
}

