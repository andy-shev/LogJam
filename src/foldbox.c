/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include "groupedbox.h"
#include "foldbox.h"

static GtkWidgetClass *parent_class = NULL;

static void
foldbox_init(FoldBox *fb) {
	GtkWidget *ebox = gtk_event_box_new();
	fb->hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(ebox), fb->hbox);
	groupedbox_set_header_widget(GROUPEDBOX(fb), ebox);

	g_signal_connect(G_OBJECT(ebox), "button_press_event",
			G_CALLBACK(foldbox_toggle_cb), fb);
}

static void
foldbox_show_all(GtkWidget *w) {
	parent_class->show_all(w);
	jam_widget_set_visible(GROUPEDBOX(w)->body, FOLDBOX(w)->unfolded);
}

static void
foldbox_size_request(GtkWidget *w, GtkRequisition *requisition) {
	parent_class->size_request(w, requisition);

	/* we still want to request as much horizontal space
	 * as we would use if we unfolded. */
	if (!FOLDBOX(w)->unfolded) {
		GtkRequisition fullrequest;

		jam_widget_set_visible(GROUPEDBOX(w)->body, TRUE);
		parent_class->size_request(w, &fullrequest);
		jam_widget_set_visible(GROUPEDBOX(w)->body, FALSE);
		requisition->width = fullrequest.width;
	}
}

static void
foldbox_class_init(GtkWidgetClass *klass) {
	parent_class = g_type_class_peek_parent(klass);
	klass->show_all = foldbox_show_all;
	klass->size_request = foldbox_size_request;
}

GtkWidget*
foldbox_new() {
	FoldBox *fb = FOLDBOX(g_object_new(FOLDBOX_TYPE, NULL));
	return GTK_WIDGET(fb);
}

GtkWidget*
foldbox_new_with_text(
		const char *text,
		gboolean unfolded,
		FoldBoxArrowLocation arrow_location) {
	FoldBox *fb = FOLDBOX(g_object_new(FOLDBOX_TYPE, NULL));
	fb->unfolded = unfolded;
	fb->arrow_location = arrow_location;
	
	foldbox_set_heading_text(fb, text);
	foldbox_refresh_arrow(fb);
	
	return GTK_WIDGET(fb);
}

void
foldbox_set_heading_text(FoldBox *fb, const char *text) {
	GString *labels = g_string_new(NULL);
	GtkWidget *label = gtk_label_new(NULL);

	g_string_printf(labels, "<b>%s</b>", text ? text : "");
	gtk_label_set_markup(GTK_LABEL(label), labels->str);
	
	if (fb->heading)
		g_object_unref(G_OBJECT(fb->heading));

	gtk_box_pack_start(GTK_BOX(fb->hbox), label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	fb->heading = label;
	g_string_free(labels, TRUE);
}	

void
foldbox_toggle_cb(GtkWidget* w, GdkEventButton *ev, FoldBox *fb) {
	fb->unfolded = !fb->unfolded;
	foldbox_refresh(fb);
}

void
foldbox_refresh(FoldBox *fb) {
	foldbox_refresh_arrow(fb);
	jam_widget_set_visible(GROUPEDBOX(fb)->body, FOLDBOX(fb)->unfolded);
}

void
foldbox_refresh_arrow(FoldBox *fb) {
	GtkWidget *a;
	
	if (! fb->arrow_location)
		return;
	
	a = fb->arrow;
	if (a) {
		gtk_arrow_set(GTK_ARROW(a),
				fb->unfolded ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT,
				GTK_SHADOW_OUT);
	} else {
		a = gtk_arrow_new(
				fb->unfolded ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT,
				GTK_SHADOW_OUT);
		gtk_box_pack_start(GTK_BOX(fb->hbox), a, FALSE, FALSE, 0);
		gtk_box_reorder_child(GTK_BOX(fb->hbox), a,
				fb->arrow_location == ARROW_LOC_START ? 0 : -1);
		fb->arrow = a;
	}
}

GType
foldbox_get_type(void) {
	static GType fb_type = 0;
	if (!fb_type) {
		const GTypeInfo fb_info = {
			sizeof (GroupedBoxClass),
			NULL,
			NULL,
			(GClassInitFunc) foldbox_class_init,
			NULL,
			NULL,
			sizeof (FoldBox),
			0,
			(GInstanceInitFunc) foldbox_init,
		};
		fb_type = g_type_register_static(TYPE_GROUPEDBOX,
				"FoldBox", &fb_info, 0);
	}
	return fb_type;
}
