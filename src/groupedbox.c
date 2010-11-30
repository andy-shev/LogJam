/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include "groupedbox.h"

/*
http://developer.gnome.org/projects/gup/hig/1.0/layout.html#window-layout-spacing
 */

static void
groupedbox_init(GroupedBox *gb) {
	GtkWidget *margin;
	GtkWidget *hbox;

	gtk_box_set_spacing(GTK_BOX(gb), 6);

	hbox = gtk_hbox_new(FALSE, 5);
	gb->vbox = gtk_vbox_new(FALSE, 6);

	margin = gtk_label_new("    ");

	gtk_box_pack_start(GTK_BOX(hbox), margin, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gb->vbox, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(gb), hbox, TRUE, TRUE, 0);

	gb->body = hbox;
}

GtkWidget*
groupedbox_new() {
	GroupedBox *gb = GROUPEDBOX(g_object_new(groupedbox_get_type(), NULL));
	return GTK_WIDGET(gb);
}

GtkWidget*
groupedbox_new_with_text(const char *text) {
	GroupedBox *gb = GROUPEDBOX(g_object_new(groupedbox_get_type(), NULL));
	groupedbox_set_header(gb, text, TRUE);
	return GTK_WIDGET(gb);
}

void
groupedbox_set_header_widget(GroupedBox *b, GtkWidget *w) {
	if (b->header)
		gtk_container_remove(GTK_CONTAINER(b), b->header);

	if (w)
		gtk_box_pack_start(GTK_BOX(b), w, FALSE, FALSE, 0);

	b->header = w;
}

void
groupedbox_set_header(GroupedBox *b, const char *title, gboolean bold) {
	GtkWidget *label = gtk_label_new(bold ? NULL : title);
	if (bold) {
		gchar *markup = g_strdup_printf("<b>%s</b>", title);
		gtk_label_set_markup(GTK_LABEL(label), markup);
		g_free(markup);
	}

	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	groupedbox_set_header_widget(b, label);
}

void
groupedbox_pack(GroupedBox *b, GtkWidget *w, gboolean expand) {
	gtk_box_pack_start(GTK_BOX(b->vbox), w, expand, expand, 0);
}

GType
groupedbox_get_type(void) {
	static GType gb_type = 0;
	if (!gb_type) {
		const GTypeInfo gb_info = {
			sizeof (GtkVBoxClass),
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			sizeof (GroupedBox),
			0,
			(GInstanceInitFunc) groupedbox_init,
		};
		gb_type = g_type_register_static(GTK_TYPE_VBOX,
				"GroupedBox", &gb_info, 0);
	}
	return gb_type;
}
