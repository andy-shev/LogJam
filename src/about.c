/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include <stdlib.h>

#include "icons.h"
#include "thanks.h"
#include "spawn.h"
#include "util-gtk.h"

/* magic number, defines how long the name will stay centered before scrolling off. */
#define COUNTER_DELAY 20

typedef struct {
	GtkWidget *win;

	GtkWidget *scrollerbox;
	GtkWidget *scroller;
	int curcontrib;
	int timeout;
	PangoLayout *layout;
	int x, y, counter;

	GtkWidget *linkwin;
	GdkCursor *cursor;
} AboutUI;

/* randomly permute the list of contributors. */
static void
rearrange_contribs(void) {
	int i, j;
	Contributor t;

	srand((unsigned int)time(NULL));
	for (i = 0; i < CONTRIBCOUNT; i++) {
		j = rand() % CONTRIBCOUNT;
		t = contribs[i];
		contribs[i] = contribs[j];
		contribs[j] = t;
	}
}

static void
homepage_cb(GtkWidget *win, GdkEventButton *eb, AboutUI *aui) {
	spawn_url(GTK_WINDOW(aui->win), "http://logjam.danga.com");
}

static GtkWidget*
make_title_box(AboutUI *aui) {
	GtkWidget *hbox;
	GtkWidget *image, *label;
	char *text;

	image = gtk_image_new_from_stock("logjam-goat", GTK_ICON_SIZE_DIALOG);

	label = gtk_label_new(NULL);
	text = g_strdup_printf(_("<b><big>LogJam %s</big></b>\n"
			"<small>Copyright (C) 2000-2004 Evan Martin</small>"),
			PACKAGE_VERSION);
	gtk_label_set_markup(GTK_LABEL(label), text);
	g_free(text);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);

	aui->linkwin = gtk_event_box_new();
	g_signal_connect(G_OBJECT(aui->linkwin), "button-press-event",
			G_CALLBACK(homepage_cb), aui);
	aui->cursor = gdk_cursor_new(GDK_HAND2);
	gtk_container_add(GTK_CONTAINER(aui->linkwin), label);

	hbox = gtk_hbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), aui->linkwin, TRUE, FALSE, 0);

	return hbox;
}

static gboolean
showthanks_cb(AboutUI *aui) {
	int w, h;

	if (!aui->layout || aui->y > aui->scroller->allocation.height) {
		if (aui->layout) {
			g_object_unref(G_OBJECT(aui->layout));
			aui->curcontrib = (aui->curcontrib + 1) % CONTRIBCOUNT;
			gtk_tooltips_set_tip(app.tooltips, aui->scrollerbox,
					_(contribs[aui->curcontrib].contribution), NULL);
		}

		aui->layout = gtk_widget_create_pango_layout(aui->scroller, contribs[aui->curcontrib].name);
		pango_layout_get_pixel_size(aui->layout, &w, &h);
		gtk_widget_set_size_request(aui->scroller, w, h);
		gtk_widget_queue_resize(aui->scroller);
		aui->y = -h;
	} else {
		/* advance y, except when we get to zero we cycle through counter once. */
		if (aui->y == 0) {
			aui->counter++;
		} else {
			aui->y++;
		}

		if (aui->counter == COUNTER_DELAY) {
			aui->counter = 0;
			aui->y++;
		}
	}
	if (aui->scroller->window)
		gdk_window_invalidate_rect(aui->scroller->window, NULL, FALSE);

	return TRUE; /* continue running. */
}
static void
button_cb(GtkWidget *widget, GdkEventExpose *event, AboutUI *aui) {
	/* force advance to next name. */
	aui->y = aui->scroller->allocation.height+1;
	showthanks_cb(aui);
	aui->y = 0;
	aui->counter = 0;
}
static void
expose_cb(GtkWidget *widget, GdkEventExpose *event, AboutUI *aui) {
	gtk_paint_layout(widget->style, widget->window, GTK_STATE_NORMAL, FALSE,
			&event->area, widget, "label", aui->x, aui->y, aui->layout);
}
static void
configure_cb(GtkWidget *widget, GdkEventConfigure *e, AboutUI *aui) {
	int w;
	pango_layout_get_pixel_size(aui->layout, &w, NULL);
	aui->x = (widget->allocation.width - w) / 2;
	gdk_window_invalidate_rect(widget->window, NULL, FALSE);
}

static void
destroy_cb(GtkWidget *win, AboutUI *aui) {
	if (aui->layout)
		g_object_unref(G_OBJECT(aui->layout));
	if (aui->timeout)
		g_source_remove(aui->timeout);
	if (aui->cursor)
		gdk_cursor_unref(aui->cursor);
	g_free(aui);
}

void about_dlg(GtkWindow *mainwin) {
	AboutUI *aui;
	GtkWidget *mainbox, *fh;

	aui = g_new0(AboutUI, 1);

	aui->win = gtk_dialog_new_with_buttons(_("About LogJam"),
			mainwin, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);
	g_signal_connect_swapped(G_OBJECT(aui->win), "response",
			G_CALLBACK(gtk_widget_destroy), aui->win);
	g_signal_connect(G_OBJECT(aui->win), "destroy",
			G_CALLBACK(destroy_cb), aui);

	/* make the bounding padding box */
	mainbox = gtk_vbox_new(FALSE, 5);

	gtk_box_pack_start(GTK_BOX(mainbox), make_title_box(aui), FALSE, FALSE, 0);

	fh = gtk_label_new(_(
	"This program is free software; you can redistribute it and/or modify\n"
	"it under the terms of the GNU General Public License as published by\n"
	"the Free Software Foundation; either version 2 of the License, or\n"
	"(at your option) any later version."));

	gtk_box_pack_start(GTK_BOX(mainbox), fh, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(mainbox),
			gtk_label_new(_("LogJam was made with the help of many people, including:")),
			FALSE, FALSE, 0);

	aui->scrollerbox = gtk_event_box_new();
	gtk_box_pack_start(GTK_BOX(mainbox), aui->scrollerbox, FALSE, FALSE, 0);

	aui->scroller = gtk_drawing_area_new();
	rearrange_contribs();
	showthanks_cb(aui);
	gtk_widget_add_events(aui->scroller, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(G_OBJECT(aui->scroller), "button-press-event",
			G_CALLBACK(button_cb), aui);
	g_signal_connect(G_OBJECT(aui->scroller), "expose-event",
			G_CALLBACK(expose_cb), aui);
	g_signal_connect(G_OBJECT(aui->scroller), "configure-event",
			G_CALLBACK(configure_cb), aui);
	gtk_container_add(GTK_CONTAINER(aui->scrollerbox), aui->scroller);

	aui->timeout = g_timeout_add(50, (GSourceFunc)showthanks_cb, aui);

	jam_dialog_set_contents(GTK_DIALOG(aui->win), mainbox);

	gtk_widget_realize(aui->win);
	gtk_widget_realize(aui->linkwin);
	gdk_window_set_cursor(aui->linkwin->window, aui->cursor);
	gtk_widget_show(aui->win);
	showthanks_cb(aui);
}

