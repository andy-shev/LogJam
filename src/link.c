/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include <gdk/gdkkeysyms.h>
#include <gdk/gdktypes.h>
#include <stdio.h>
#include "util-gtk.h"
#include "conf.h"
#include "jamdoc.h"

typedef struct {
	GtkWidget *dlg;
	GtkWidget *etext, *ruser, *euser, *rurl, *eurl;
	GtkWidget *titl;
	JamDoc *doc;
	gint sel_type, clip_type;
	gchar *sel_input, *clip_input;
} LinkDialog;

static void
select_toggled(GtkToggleButton *tb, GtkWidget *focus) {
	if (gtk_toggle_button_get_active(tb))
		gtk_widget_grab_focus(focus);
}

static void
entry_focus_cb(GtkWidget *w, GdkEventFocus *e, GtkToggleButton *radio) {
	gtk_toggle_button_set_active(radio, TRUE);
}

static GtkWidget*
radio_option(GSList *g, GtkWidget **radio, GtkWidget **entry,
		const char *caption, const char *label, const char *initial) {
	GtkWidget *vbox;

	vbox = gtk_vbox_new(FALSE, 0);
	*radio = gtk_radio_button_new_with_mnemonic(g, caption);
	gtk_box_pack_start(GTK_BOX(vbox), *radio, FALSE, FALSE, 0);

	*entry = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(*entry), TRUE);
	g_signal_connect(G_OBJECT(*entry), "focus-in-event",
			G_CALLBACK(entry_focus_cb), *radio);
	if (initial)
		gtk_entry_set_text(GTK_ENTRY(*entry), initial);
	gtk_widget_set_size_request(*entry, 75, -1);
	g_signal_connect(G_OBJECT(*radio), "toggled",
			G_CALLBACK(select_toggled), *entry);
	g_signal_connect(G_OBJECT(*radio), "activate",
			G_CALLBACK(select_toggled), *entry);

	if (label) {
		GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), 
				gtk_label_new(label),
				FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), *entry, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	} else {
		gtk_box_pack_start(GTK_BOX(vbox), *entry, TRUE, TRUE, 0);
	}

	return vbox;
}

#if !GLIB_CHECK_VERSION(2,2,0)
static gboolean
g_str_has_prefix(const gchar *str, const gchar *prefix) {
	return (strncmp(str, prefix, strlen(prefix)) == 0);
}
#endif

#if 0
gchar*
link_magic(LinkRequest *lr) {
	gchar *a, *b;
	if (! ((lr->clip_type | lr->sel_type) & JAM_SELECTION_HAS_URL))
		return NULL;
	
	xml_escape(&lr->clip_input);

	/* boo for no list primitives in c */
	if (lr->clip_type & JAM_SELECTION_HAS_URL) {
		a = lr->clip_input; b = lr->sel_input;
	} else {
		b = lr->clip_input; a = lr->sel_input;
	}

	return g_strdup_printf("<a href=\"%s\">%s</a>", a, b);
}
#endif

static void
make_link_dialog(LinkDialog *ld, GtkWindow *win, gboolean livejournal) {
	GtkWidget *vbox;
	GtkWidget *subbox, *hhr;
	GSList *rgroup;

	ld->dlg = gtk_dialog_new_with_buttons(_("Make Link"),
			win, GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(ld->dlg), GTK_RESPONSE_OK);

	vbox = gtk_vbox_new(FALSE, 10);

	ld->etext = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(ld->etext), TRUE);
	subbox = labelled_box_new(_("Link _Text:"), ld->etext);
	gtk_box_pack_start(GTK_BOX(vbox), subbox, FALSE, FALSE, 0);
	ld->titl = gtk_entry_new();
	hhr = labelled_box_new(_("Link Title:"), ld->titl);
	gtk_box_pack_start(GTK_BOX(vbox), hhr, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), 
			radio_option(NULL, &ld->rurl, &ld->eurl, _("_URL:"), NULL, ""),
			FALSE, FALSE, 0);

	rgroup = gtk_radio_button_get_group(GTK_RADIO_BUTTON(ld->rurl));

	if (livejournal) {
		gtk_box_pack_start(GTK_BOX(vbox), 
				radio_option(rgroup, &ld->ruser, &ld->euser, 
					_("_LiveJournal User:"), ".../users/", ""),
				FALSE, FALSE, 0);
	}

	jam_dialog_set_contents(GTK_DIALOG(ld->dlg), vbox);
}

static gboolean
has_url(char *buf) {
	if (!buf || buf[0] == 0) return FALSE;
	/* just test a few of the obvious ones... */
	if (g_str_has_prefix(buf, "http://") ||
	    g_str_has_prefix(buf, "ftp://") ||
	    g_str_has_suffix(buf, ".com") ||
	    g_str_has_suffix(buf, ".net") ||
	    g_str_has_suffix(buf, ".org"))
		return TRUE;
	return FALSE;
}

static void
prepopulate_fields(LinkDialog *ld, char *bufsel) {
	GtkClipboard *clipboard; 
	char *clipsel, *url = NULL;

	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	clipsel = gtk_clipboard_wait_for_text(clipboard);

	if (bufsel && has_url(bufsel)) {
		url = bufsel;
	} else if (clipsel && has_url(clipsel)) {
		url = clipsel;
	}
	
	if (bufsel)
		gtk_entry_set_text(GTK_ENTRY(ld->etext), bufsel);
	if (url) {
		char *niceurl;
		if (g_str_has_prefix(url, "http://") ||
		    g_str_has_prefix(url, "ftp://")) {
			niceurl = url;
		} else {
			niceurl = g_strdup_printf("http://%s", url);
		}
		gtk_entry_set_text(GTK_ENTRY(ld->eurl), niceurl);
		if (niceurl != url)
			g_free(niceurl);
	}

	g_free(clipsel);
}

static char*
get_link(LinkDialog *ld, JamAccount *acc) {
	char *url, *user, *text, *title;
	char *link = NULL;

	url  = gtk_editable_get_chars(GTK_EDITABLE(ld->eurl),  0, -1);
	xml_escape(&url);
	user = gtk_editable_get_chars(GTK_EDITABLE(ld->euser), 0, -1);
	xml_escape(&user);
	text = gtk_editable_get_chars(GTK_EDITABLE(ld->etext), 0, -1);
	xml_escape(&text);
	title = gtk_editable_get_chars(GTK_EDITABLE(ld->titl), 0, -1);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ld->rurl))) {
		/* build a "url" link. */
		if (title && *title) {
			link = g_strdup_printf("<a href=\"%s\" title=\"%s\">%s</a>", url, title, text);
		} else {
			link = g_strdup_printf("<a href=\"%s\">%s</a>", url, text);
		}
	} else if (ld->ruser &&
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ld->ruser))) {
		/* build a "friends" link. */
		LJServer *server = jam_account_lj_get_server(JAM_ACCOUNT_LJ(acc));
		g_free(url);
		url = g_strdup(server->url);
		xml_escape(&url);
		link = g_strdup_printf("<a href=\"%s/users/%s\">%s</a>",
				url, user, text);
	} 
	g_free(url);
	g_free(user);
	g_free(text);
	g_free(title);
	
	return link;
}

void
link_dialog_run(GtkWindow *win, JamDoc *doc) {
	STACK(LinkDialog, ld);
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	char *sel = NULL;
	char *link;

	JamAccount *acc = jam_doc_get_account(doc);
	make_link_dialog(ld, win, JAM_ACCOUNT_IS_LJ(acc));

	buffer = jam_doc_get_text_buffer(doc);
	if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end))
		sel = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	prepopulate_fields(ld, sel);
	g_free(sel);

	if (gtk_dialog_run(GTK_DIALOG(ld->dlg)) != GTK_RESPONSE_OK) {
		gtk_widget_destroy(ld->dlg);
		return;
	}

	link = get_link(ld, acc);

	gtk_widget_destroy(ld->dlg);

	if (link) {
		gtk_text_buffer_begin_user_action(buffer);
		gtk_text_buffer_delete(buffer, &start, &end);
		gtk_text_buffer_insert(buffer, &start, link, -1);
		gtk_text_buffer_end_user_action(buffer);
		g_free(link);
	}
}

