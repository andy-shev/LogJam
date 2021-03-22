/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include "util-gtk.h"

#include "conf.h"
#include "jam.h"
#include "spawn.h"
#include "menu.h"
#include "settings.h"

#include "throbber.h"
#include "checkfriends.h"

struct _CFFloat {
	CFMgr            *cfmgr;
	GtkWidget        *box;
	GtkWidget        *throbber;
	GtkWidget        *win;
};

static const gchar *cf_help = N_(
	"The Check Friends feature is enabled by right-clicking on this "
	"button and selecting \"Check friends view for new entries\". Once "
	"you do so, you will be notified every time one of your friends "
	"updates their journal with a new entry.\n\n"

	"Once this happens, the indicator will turn red. You can then "
	"read your friends page (as a convenience, double-clicking on "
	"the indicator will take you there). Click on the indicator once "
	"you've read the new entries to resume monitoring for newer "
	"entries.\n\n");

static void cf_dock_destroy(CFFloat *cff);

static void cf_dock_destroyed_cb(GtkWidget *widget, CFFloat *cff);
static void cf_float_destroyed_cb(GtkWidget *widget, CFFloat *cff);

static void
cf_float_update(CFFloat *cff, CFState state) {
	const gchar *tip_text = NULL;

	switch (state) {
		case CF_DISABLED:
			throbber_stop(THROBBER(cff->throbber));
			throbber_reset(THROBBER(cff->throbber));
			tip_text    = _("Check Friends disabled");
			break;
		case CF_ON:
			throbber_stop(THROBBER(cff->throbber));
			throbber_reset(THROBBER(cff->throbber));
			tip_text    = _("No new entries in your friends view");
			break;
		case CF_NEW:
			throbber_start(THROBBER(cff->throbber));
			tip_text    = _("There are new entries in your friends view");
			break;
	}

	gtk_tooltips_set_tip(app.tooltips, cff->box, tip_text, _(cf_help));
}

static void
cf_toggle_cb(GtkMenuItem *mi, CFFloat* cff) {
	CFMgr *cfm = cff->cfmgr;
	CFState state = cfmgr_get_state(cfm);

	switch (state) {
		case CF_DISABLED:
			cfmgr_set_state(cfm, CF_ON);
			break;
		case CF_ON:
		case CF_NEW:
			cfmgr_set_state(cfm, CF_DISABLED);
			break;
	}
}

static void
open_friends_list(CFMgr *cfm) {
	char *url;
	JamAccountLJ *acc;

	acc = cfmgr_get_account(cfm);
	if (!acc) return;

	url = g_strdup_printf("%s/users/%s/friends",
			jam_account_lj_get_server(acc)->url,
			jam_account_lj_get_user(acc)->username);
	spawn_url(NULL /* XXX parent */, url);
	g_free(url);
}

void
cf_context_menu(CFFloat* cff, GdkEventButton *ev) {
	CFMgr *cfm = cff->cfmgr;
	CFState state = cfmgr_get_state(cfm);

	GtkAccelGroup *accelgroup = gtk_accel_group_new();
	GtkWidget *cfmenu = gtk_menu_new();
	GtkWidget *item;

	{ /* Check friends on/off */
		item = gtk_check_menu_item_new_with_mnemonic(
			_("_Check friends view for new entries"));
		gtk_menu_shell_append(GTK_MENU_SHELL(cfmenu), item);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
			(state != CF_DISABLED));
		g_signal_connect(G_OBJECT(item), "activate",
			G_CALLBACK(cf_toggle_cb), cff);
		gtk_widget_show(item);
	}

	{ /* ---------- */
		item = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(cfmenu), item);
		gtk_widget_show(item);
	}

	{ /* Preferences... */
		item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES,
			accelgroup);
		gtk_menu_shell_append(GTK_MENU_SHELL(cfmenu), item);
		g_signal_connect_swapped(G_OBJECT(item), "activate",
			G_CALLBACK(settings_cf_run), cfm);
		gtk_widget_show(item);
	}

	{ /* open friends list in browser */
		item = gtk_image_menu_item_new_with_mnemonic(
			_("_Open friends list in browser"));
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
			gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU));
		gtk_menu_shell_append(GTK_MENU_SHELL(cfmenu), item);
		g_signal_connect_swapped(G_OBJECT(item), "activate",
			G_CALLBACK(open_friends_list), cfm);
		gtk_widget_show_all(item);
	}

	gtk_menu_popup(GTK_MENU(cfmenu), NULL, NULL, NULL, NULL,
		ev->button, ev->time);

}

static gboolean
clicked_cb(GtkWidget* w, GdkEventButton *ev, CFFloat *cff) {
	CFMgr *cfm = cff->cfmgr;
	CFState state = cfmgr_get_state(cfm);

	/* right-clicks start context menu (note: this case is terminal) */
	if (ev->button == 3) {
		cf_context_menu(cff, ev);
		return TRUE;
	}

	/* *all* left-clicks move CF_NEW to CF_ON */
	if (state == CF_NEW)
		cfmgr_set_state(cfm, CF_ON);

	if (ev->button == 2) // XXX testing
		cfmgr_set_state(cfm, CF_NEW);


	/* and double-clicks open up the browser, too */
	if (ev->type==GDK_2BUTTON_PRESS) {
		// XXX open_friends_list(GTK_WINDOW(cff->parent));

		return TRUE;
	}

	/* this help box will only be called once in a double-click,
	 * thankfully, because the above is terminal on double-clicks. */
	/*if (state == CF_DISABLED) {
		jam_message(GTK_WINDOW(cff->parent), JAM_MSG_INFO, TRUE,
				_("Check Friends"), _(cf_help));
	}*/

	return TRUE;
}

static void
cf_float_state_changed_cb(CFMgr *cfm, CFState state, CFFloat *cff) {
	cf_float_update(cff, state);

	/* autoraise if necessary */
	if (state == CF_NEW && conf.options.cffloatraise)
		gtk_window_present(GTK_WINDOW(cff->win));
}

CFFloat*
cf_float_new(CFMgr *cfm) {
	CFFloat *cff;

	cff = g_new0(CFFloat, 1);
	cff->cfmgr = cfm;

	g_signal_connect(G_OBJECT(cfm), "state_changed",
			G_CALLBACK(cf_float_state_changed_cb), cff);

	cff->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(cff->win), "destroy",
			G_CALLBACK(cf_float_destroyed_cb), cff);
	gtk_window_set_wmclass(GTK_WINDOW(cff->win),
			"logjam-checkfriends", "logjam-checkfriends");
	geometry_tie(cff->win, GEOM_CFFLOAT);

	cff->box = gtk_event_box_new();
	gtk_container_set_border_width(GTK_CONTAINER(cff->box), 3);
	g_signal_connect(G_OBJECT(cff->box), "button_press_event",
			G_CALLBACK(clicked_cb), cff);

	cff->throbber = throbber_new();
	gtk_container_add(GTK_CONTAINER(cff->box), cff->throbber);

	if (!conf.options.cffloat_decorate) {
		GtkWidget *frame = gtk_frame_new(NULL);
		gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
		gtk_container_add(GTK_CONTAINER(cff->win), frame);
		gtk_container_add(GTK_CONTAINER(frame), cff->box);
		gtk_window_set_decorated(GTK_WINDOW(cff->win), FALSE);
	} else {
		gtk_container_add(GTK_CONTAINER(cff->win), cff->box);
	}
	cf_float_update(cff, cfmgr_get_state(cfm));
	gtk_widget_show_all(cff->win);
	return cff;
}

#ifdef USE_DOCK
void
cf_update_dock(CFMgr *cfm, GtkWindow* parent) {
	if (!conf.options.docklet && app.docklet) {
		cf_dock_destroy(app.docklet);
	} else if (conf.options.docklet && !app.docklet) {
		app.docklet = cf_float_new(cfm);
	}
}
#endif

static void
cf_float_destroyed_cb(GtkWidget *widget, CFFloat *cff) {
	CFMgr *cfm = cff->cfmgr;

	g_signal_handlers_disconnect_matched(cfm,
			G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL,
			cff);
}

static void
cf_float_destroy(CFFloat *cff) {
	if (GTK_IS_WIDGET(cff->win))
		gtk_widget_destroy(cff->win);

	app.cf_float = NULL;
}

void
cf_app_update_float(void) {
	if (!conf.options.cffloat && app.cf_float) {
		cf_float_destroy(app.cf_float);
		app.cf_float = NULL;
	} else if (conf.options.cffloat && !app.cf_float) {
		app.cf_float = cf_float_new(app.cfmgr);
	}
}

/* Docklet code based on docklet plugin for Gaim, copyright Robert McQueen */
#ifdef USE_DOCK
static void
cf_dock_destroy(CFFloat *cff) {
	g_signal_handlers_disconnect_by_func(G_OBJECT(cff->win),
			G_CALLBACK(cf_dock_destroyed_cb), NULL);
	gtk_widget_destroy(GTK_WIDGET(cff->win));

	app.docklet  = NULL;
}

static void
cf_dock_destroyed_cb(GtkWidget *widget, CFFloat *cff) {
	app.docklet = NULL;
}

/*
 * Not currently used.
 */
/*
static void
cf_dock_setup(CFFloat *ind) {
	GtkWidget *docklet = app.docklet;

	g_signal_connect(G_OBJECT(docklet), "destroy",
			G_CALLBACK(cf_dock_destroyed_cb), ind);

	gtk_container_add(GTK_CONTAINER(docklet), ind->box);
	gtk_widget_show_all(GTK_WIDGET(docklet));

	ind->win = docklet;
}
*/
#endif /* USE_DOCK */

void cf_float_decorate_refresh(void) {
	/* suck a bunch of events in. */
	while (gtk_events_pending())
		gtk_main_iteration();

	if (app.cf_float) {
		gtk_window_set_decorated(GTK_WINDOW(app.cf_float->win),
				conf.options.cffloat_decorate);
		gtk_widget_hide(app.cf_float->win);
		gtk_widget_show(app.cf_float->win);
	}
}


