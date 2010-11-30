/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2004 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include <string.h>

#include "jamdoc.h"
#include "usejournal.h"

/* "having a shared prefix" is a pretty vague concept.
 * we try a few heuristics. */
static int
sharedprefix(char *a, char *b) {
	const int minmatch = 4;
	int i = 0;
	char *ulpos;

	/* does this journal have an underscore? go from there. */
	ulpos = strchr(b, '_');
	if (ulpos) {
		if (strncmp(a, b, ulpos-b) == 0)
			return (int)(ulpos - b);
	}

	/* or if the shared prefix is at least minmatch chars... */
	while (*a++ == *b++)
		i++;
	return (i > minmatch) ? i : 0;
}

static void
activate_cb(GtkWidget *w, JamDoc *doc) {
	const gchar *user;
	JamAccount *acc = jam_doc_get_account(doc);

	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
		return;

	user = gtk_label_get_text(GTK_LABEL(GTK_BIN(w)->child));

	if (g_ascii_strcasecmp(user, jam_account_get_username(acc)) == 0)
		user = NULL; /* post as normal user. */

	jam_doc_set_usejournal(doc, user);
}


GtkWidget*
usejournal_build_menu(const char *defaultjournal, const char *currentjournal,
		GSList *journals, gpointer doc) {
	GtkWidget *menu, *curmenu, *item, *label;
	GSList *group = NULL;
	GSList *l;
	char *journal;
	char *curmenuprefix = NULL;
	char prefix[30];

	curmenu = menu = gtk_menu_new();

	item = gtk_radio_menu_item_new_with_label(group, defaultjournal);
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
	g_signal_connect(G_OBJECT(item), "activate",
			G_CALLBACK(activate_cb), doc);

	if (currentjournal == NULL)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);

	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	for (l = journals; l != NULL; l = l->next) {
		journal = (char*)l->data;

		if (curmenuprefix) {
			/* try to match this item to the prefix. */
			if (sharedprefix(curmenuprefix, journal)) {
				/* match. */
			} else {
				curmenu = menu;
				curmenuprefix = NULL;
			}
		}
		if (!curmenuprefix && l->next) {
			/* try to see if this begins a new prefix. */
			char *nextjournal = (char*)l->next->data;
			int ofs;
			ofs = sharedprefix(journal, nextjournal);
			if (ofs) {
				/* make a new submenu for these shared-prefix journals. */
				memcpy(prefix, journal, ofs);
				prefix[ofs] = 0;

				item = gtk_menu_item_new();
				label = gtk_label_new(NULL);
				gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
				gtk_label_set_markup(GTK_LABEL(label), prefix);
				gtk_container_add(GTK_CONTAINER(item), label);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
				curmenu = gtk_menu_new();
				curmenuprefix = prefix;
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), curmenu);
				gtk_widget_show_all(item);
			}
		}
		item = gtk_radio_menu_item_new_with_label(group, journal);
		if (currentjournal && strcmp(currentjournal, journal) == 0)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
		g_signal_connect(G_OBJECT(item), "activate",
				G_CALLBACK(activate_cb), doc);
		gtk_widget_show(item);
		gtk_menu_shell_append(GTK_MENU_SHELL(curmenu), item);

		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
	}

	return menu;
}

