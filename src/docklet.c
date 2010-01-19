/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include "jam.h"		/* jam_quit() */
#include "conf.h"
#include "eggtrayicon.h"
#include "about.h"		/* about_dlg() */
#include "menu.h"		/* menu_friends_manager() */
#include "settings.h"	/* settings_run() */

static void
docklet_destroy_cb(GtkWidget *widget) {
	app.docklet = NULL;
}

static void
docklet_menu(GtkWidget *win) {
	static GtkWidget *menu = NULL;
	GtkWidget *entry;
	GtkWidget *menuitem;
	GtkWidget *image;

	if (menu) {
		gtk_widget_destroy(menu);
	}

	menu = gtk_menu_new();

	/* About... */
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("About LogJam..."));
	image = gtk_image_new_from_stock("logjam-goat", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(about_dlg), win);
	gtk_widget_show_all(menuitem);

	/* Friends... */
	menuitem = gtk_menu_item_new_with_mnemonic(_("Friends..."));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(menu_friends_manager), win);
	gtk_widget_show_all(menuitem);

	/* Preferences... */
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("Preferences..."));
	image = gtk_image_new_from_stock(GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(settings_run), win);
	gtk_widget_show_all(menuitem);

	/* -------------- */
	menuitem = gtk_separator_menu_item_new();
	gtk_widget_show(menuitem);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	/* Quit */
	menuitem = gtk_image_menu_item_new_with_mnemonic(_("Quit"));
	image = gtk_image_new_from_stock(GTK_STOCK_QUIT, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(jam_quit), win);
	gtk_widget_show_all(menuitem);

	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

static gboolean
click_cb(GtkWidget* w, GdkEventButton *ev, GtkWidget *win) {
	/* right-clicks start context menu (note: this case is terminal) */
	if (ev->button == 3) {
		docklet_menu(win);
		return TRUE;
	}

	if (GTK_WIDGET_VISIBLE(win)) {
		gtk_widget_hide(win);
	} else {
		gtk_widget_show(win);
	}

#if 0
	/* *all* left-clicks move CF_NEW to CF_ON */
	if (cfm->state == CF_NEW) {
		cfmgr_set_state(cfm, CF_ON);
	}
	/* and double-clicks open up the browser, too */
	if (ev->type==GDK_2BUTTON_PRESS) {
		open_friends_list(GTK_WINDOW(cfi->parent));
	
		return TRUE;
	}
	
	/* this help box will only be called once in a double-click,
	 * thankfully, because the above is terminal on double-clicks. */
	if (cfm->state == CF_DISABLED) {
		jam_message(GTK_WINDOW(cfi->parent), JAM_MSG_INFO, TRUE,
				_("Check Friends"), _(cf_help));
	}
#endif

	return TRUE;
}

void
docklet_setup(GtkWidget *win) {
	EggTrayIcon *docklet;

	GtkWidget *box, *image;
	
	image = gtk_image_new();
	gtk_image_set_from_stock(GTK_IMAGE(image), "logjam-goat",
		GTK_ICON_SIZE_MENU);

	box = gtk_event_box_new();
	gtk_tooltips_set_tip(app.tooltips, box, _("LogJam"), _("LogJam"));
	gtk_container_set_border_width(GTK_CONTAINER(box), 3);
	gtk_container_add(GTK_CONTAINER(box), image);

	docklet = egg_tray_icon_new("LogJam");
	
	/* This could be used to tell the calling app that the docklet has been
	 * successfully embedded into a dock :
	 * g_signal_connect(G_OBJECT(docklet), "embedded", G_CALLBACK(cf_docklet_embedded), NULL); */
	g_signal_connect(G_OBJECT(docklet), "destroy",
			G_CALLBACK(docklet_destroy_cb), NULL);
	g_signal_connect(G_OBJECT(box), "button_press_event",
			G_CALLBACK(click_cb), win);

	gtk_container_add(GTK_CONTAINER(docklet), box);
	gtk_widget_show_all(GTK_WIDGET(docklet));

	app.docklet = docklet;
}

void
docklet_enable(GtkWidget *win, gboolean enable) {
	if ((enable == TRUE) == (app.docklet != NULL))
		return;

	if (enable) {
		docklet_setup(win);
	} else {
		gtk_widget_destroy(app.docklet);
	}
}
