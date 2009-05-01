/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include <stdlib.h> /* atoi */

#include "account.h"
#include "userlabel.h"

struct _JamUserLabel {
	GtkButton parent;

	GtkWidget *box;
	GtkWidget *servericon;
	GtkWidget *serverlabel;
	GtkWidget *usericon;
	GtkWidget *userlabel;
	GtkWidget *postingin;
	GtkWidget *journalicon;
	GtkWidget *journallabel;
};

static GtkWidgetClass *parent_class = NULL;

static void
set_label_ljuser(GtkLabel *label, const char *user) {
	char *text;

	text = g_strdup_printf(
			"<span foreground='#0000FF' weight='bold' underline='single'>%s</span>",
			user);
	gtk_label_set_markup(label, text);
	g_free(text);
}

void
jam_user_label_set_account(JamUserLabel *jul, JamAccount *acc) {
	JamHost *host = jam_account_get_host(acc);
	gtk_image_set_from_stock(GTK_IMAGE(jul->servericon),
			jam_host_get_stock_icon(host), GTK_ICON_SIZE_MENU);
	gtk_label_set_text(GTK_LABEL(jul->serverlabel), host->name);

	if (JAM_ACCOUNT_IS_LJ(acc)) {
		gtk_widget_show(jul->usericon);
		set_label_ljuser(GTK_LABEL(jul->userlabel),
				jam_account_get_username(acc));
	} else {
		gtk_widget_hide(jul->usericon);
		gtk_label_set_text(GTK_LABEL(jul->userlabel),
				jam_account_get_username(acc));
	}
}

void
jam_user_label_set_journal(JamUserLabel *jul, const char *journalname) {
	if (journalname) {
		gtk_widget_show(jul->postingin);
		gtk_widget_show(jul->journalicon);
		gtk_widget_show(jul->journallabel);
		set_label_ljuser(GTK_LABEL(jul->journallabel), journalname);
	} else {
		gtk_widget_hide(jul->postingin);
		gtk_widget_hide(jul->journalicon);
		gtk_widget_hide(jul->journallabel);
	}
}

static void
jam_user_label_init(JamUserLabel *jul) {
	GtkWidget *sep;

	gtk_button_set_relief(GTK_BUTTON(jul), GTK_RELIEF_NONE);
	GTK_WIDGET_UNSET_FLAGS(jul, GTK_CAN_FOCUS);

	jul->box = gtk_hbox_new(FALSE, 0);

	jul->servericon = gtk_image_new();
	jul->serverlabel = gtk_label_new(NULL);
	/* this string is the separator between the server and username
	 * next to the "submit" button. */
	sep = gtk_label_new(_("/"));
	jul->usericon = gtk_image_new_from_stock("logjam-ljuser",
			GTK_ICON_SIZE_MENU);
	jul->userlabel = gtk_label_new(NULL);
	jul->postingin = gtk_label_new(_(", using "));
	jul->journalicon = gtk_image_new_from_stock("logjam-ljcomm",
			GTK_ICON_SIZE_MENU);
	jul->journallabel = gtk_label_new(NULL);

	gtk_box_pack_start(GTK_BOX(jul->box), jul->servericon, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(jul->box), jul->serverlabel, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(jul->box), sep, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(jul->box), jul->usericon, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(jul->box), jul->userlabel, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(jul->box), jul->postingin, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(jul->box), jul->journalicon, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(jul->box), jul->journallabel, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(jul), jul->box);

	gtk_widget_show(jul->servericon);
	gtk_widget_show(jul->serverlabel);
	gtk_widget_show(sep);
	gtk_widget_show(jul->usericon);
	gtk_widget_show(jul->userlabel);
	gtk_widget_show(jul->box);
	gtk_widget_hide(jul->postingin);
	gtk_widget_hide(jul->journalicon);
	gtk_widget_hide(jul->journallabel);
}

static void
jam_user_label_show_all(GtkWidget *w) {
	gtk_widget_show(w);
	/* don't show children. */
}

static void
jam_user_label_class_init(GtkWidgetClass *klass) {
	parent_class = g_type_class_peek_parent(klass);
	klass->show_all = jam_user_label_show_all;
}

GType
jam_user_label_get_type(void) {
	static GType jul_type = 0;
	if (!jul_type) {
		static const GTypeInfo jul_info = {
			sizeof (GtkButtonClass),
			NULL,
			NULL,
			(GClassInitFunc) jam_user_label_class_init,
			NULL,
			NULL,
			sizeof (JamUserLabel),
			0,
			(GInstanceInitFunc) jam_user_label_init,
		};
		jul_type = g_type_register_static(GTK_TYPE_BUTTON, "JamUserLabel",
				&jul_info, 0);
	}
	return jul_type;
}

GtkWidget*
jam_user_label_new(void) {
	JamUserLabel *jul = JAM_USER_LABEL(g_object_new(jam_user_label_get_type(), NULL));
	return GTK_WIDGET(jul);
}

