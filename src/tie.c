/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include "util-gtk.h"
#include "util.h"

static void
tie_toggle_cb(GtkToggleButton *toggle, gboolean *data) {
	*data = gtk_toggle_button_get_active(toggle);
}

GtkWidget*
tie_toggle(GtkToggleButton *toggle, gboolean *data) {
	gtk_toggle_button_set_active(toggle, *data);
	g_signal_connect(G_OBJECT(toggle), "toggled",
			G_CALLBACK(tie_toggle_cb), data);
	return GTK_WIDGET(toggle);
}

static void
tie_text_cb(GtkEditable *e, char **data) {
	string_replace(data, gtk_editable_get_chars(e, 0, -1));
}

void
tie_text(GtkEntry *entry, char **data) {
	if (*data)
		gtk_entry_set_text(entry, *data);
	g_signal_connect(G_OBJECT(entry), "changed",
			G_CALLBACK(tie_text_cb), data);
}

void
tie_combo(GtkCombo *combo, char **data) {
	tie_text(GTK_ENTRY(combo->entry), data);
}


