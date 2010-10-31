/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkstock.h>

#include "icons.h"
#include "../images/pixbufs.h"

static GtkStockItem logjam_stock_items[] = {
	/* XXX we shouldn't need those spaces in there for this to look good. :( */
	{ "logjam-submit", N_(" Submit "), 0, 0, "logjam" }
};

static GdkPixbuf*
inline_to_listpixbuf(const guint8 *data) {
	GdkPixbuf *pb, *spb;
	gint width, height, sourcewidth, sourceheight;

	gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
	height -= 5; /* FIXME: slightly smaller looks good, but it shouldn't. */
	pb = gdk_pixbuf_new_from_inline(-1, data, FALSE, NULL);

	/* now adjust width to be proportionate to the height for this icon. */
	sourceheight = gdk_pixbuf_get_height(pb);
	sourcewidth = gdk_pixbuf_get_width(pb);
	width = sourcewidth * height / sourceheight;

	spb = gdk_pixbuf_scale_simple(pb, width, height, GDK_INTERP_BILINEAR);
	g_object_unref(pb);
	
	return spb;
}
GdkPixbuf*
icons_rarrow_pixbuf(void) {
	return inline_to_listpixbuf(logjam_rarrow);
}
GdkPixbuf*
icons_larrow_pixbuf(void) {
	return inline_to_listpixbuf(logjam_larrow);
}
GdkPixbuf*
icons_lrarrow_pixbuf(void) {
	return inline_to_listpixbuf(logjam_lrarrow);
}

static GdkPixbuf*
add(GtkIconFactory *factory,
		const guchar *inline_data,
		const gchar *stock_id) {
	GtkIconSet *set;
	GdkPixbuf *pixbuf;

	pixbuf = gdk_pixbuf_new_from_inline(-1, inline_data, FALSE, NULL);
	set = gtk_icon_set_new_from_pixbuf(pixbuf);
	g_object_unref(G_OBJECT(pixbuf));

	gtk_icon_factory_add(factory, stock_id, set);
	gtk_icon_set_unref(set);

	return pixbuf;
}

void
icons_initialize(void) {
	GtkIconFactory *factory;
	GdkPixbuf *goat;
	GList *l;

	factory = gtk_icon_factory_new();
	goat = add(factory, logjam_goat, "logjam-goat");
	add(factory, logjam_pencil, "logjam-server");
	add(factory, logjam_ljuser, "logjam-ljuser");
	add(factory, logjam_ljcomm, "logjam-ljcomm");
	add(factory, logjam_twuser, "logjam-twuser");
	add(factory, logjam_protected, "logjam-protected");
	add(factory, logjam_private, "logjam-private");
	add(factory, logjam_blogger, "logjam-blogger");
	gtk_icon_factory_add_default(factory);

	l = g_list_append(NULL, goat);
	gtk_window_set_default_icon_list(l);
	g_list_free(l);

	gtk_stock_add_static(logjam_stock_items, G_N_ELEMENTS(logjam_stock_items));
}

#ifndef HAVE_LIBRSVG
void
icons_load_throbber(GdkPixbuf *pbs[]) {
	const guint8 *data;
	int i;
	GdkPixbuf *pb;
	gint w, h;

	gtk_icon_size_lookup(GTK_ICON_SIZE_DIALOG, &w, &h);

	for (i = 0; i < THROBBER_COUNT; i++) {
		switch (i) {
			case 0: data = logjam_throbber_1; break;
			case 1: data = logjam_throbber_2; break;
			case 2: data = logjam_throbber_3; break;
			case 3: data = logjam_throbber_4; break;
			case 4: data = logjam_throbber_5; break;
			case 5: data = logjam_throbber_6; break;
			case 6: data = logjam_throbber_7; break;
			case 7: data = logjam_throbber_8; break;
			default:
				g_warning("tried to load unknown throbber %d.", i);
				return;
		}
		pb = gdk_pixbuf_new_from_inline(-1, data, FALSE, NULL);
		pbs[i] = gdk_pixbuf_scale_simple(pb, w, h, GDK_INTERP_BILINEAR);
		g_object_unref(G_OBJECT(pb));
	}
}
#endif /* HAVE_LIBRSVG */
