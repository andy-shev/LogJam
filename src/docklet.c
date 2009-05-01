/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include "conf.h"
#include "eggtrayicon.h"

static void
docklet_destroy_cb(GtkWidget *widget) {
	app.docklet = NULL;
}

static gboolean
click_cb(GtkWidget* w, GdkEventButton *ev, GtkWidget *win) {
	/* right-clicks start context menu (note: this case is terminal) */
	if (ev->button == 3) {
		//cf_context_menu(cfi, ev);
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
