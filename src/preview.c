/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include <gtkhtml/gtkhtml.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "jam.h"
#include "icons.h"
#include "preview.h"
#include "spawn.h"
#include "../images/inline.h"

#define RESPONSE_UPDATE 1

#define HTMLPREVIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), html_preview_get_type(), HTMLPreview))
static GType html_preview_get_type(void);

static void url_requested(GtkHTML *html, const char *url, GtkHTMLStream *handle);
static void link_clicked(GtkHTML *html, const char *url, gpointer data);

static void
html_preview_init(HTMLPreview *hp) {
	gtk_html_construct((gpointer)hp);
	g_signal_connect(G_OBJECT(hp), "url_requested",
			G_CALLBACK(url_requested), NULL);
	g_signal_connect(G_OBJECT(hp), "link_clicked",
			G_CALLBACK(link_clicked), NULL);
}

GtkWidget*
html_preview_new(GetEntryFunc get_entry_f, gpointer get_entry_data) {
	HTMLPreview *hp = HTMLPREVIEW(g_object_new(html_preview_get_type(), NULL));
	hp->get_entry      = get_entry_f;
	hp->get_entry_data = get_entry_data;
	return GTK_WIDGET(hp);
}

GType
html_preview_get_type(void) {
	static GType hp_type = 0;
	if (!hp_type) {
		const GTypeInfo hp_info = {
			sizeof (GtkHTMLClass),
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			sizeof (HTMLPreview),
			0,
			(GInstanceInitFunc) html_preview_init,
		};
		hp_type = g_type_register_static(GTK_TYPE_HTML,
				"HTMLPreview", &hp_info, 0);
	}
	return hp_type;
}

static void
link_clicked(GtkHTML *html, const char *url, gpointer data) {
	spawn_url(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(html))), url);
}

static void
url_requested(GtkHTML *html, const char *url, GtkHTMLStream *handle) {
#define PROVIDE_IMAGE(name) \
	if (g_ascii_strcasecmp(url, "logjam:" #name) == 0) { \
		gtk_html_write(html, handle, \
				(char*)logjam_##name##_png, sizeof(logjam_##name##_png)); }

	PROVIDE_IMAGE(ljuser)
	else
	PROVIDE_IMAGE(ljcomm)
	else
	PROVIDE_IMAGE(protected)
	else
	PROVIDE_IMAGE(private)
	else {
		gtk_html_end(html, handle, GTK_HTML_STREAM_ERROR);
		return;
	}
	gtk_html_end(html, handle, GTK_HTML_STREAM_OK);
}

/* XXX not utf8 safe, but i think we're ok because we depend on ASCII
 * characters in HTML and usernames. */
static char*
parse_ljtag(char *src, GString *dst) {
	gboolean isuser;
	char *start, *end;

	char *p = src + 3;
	while (*p == ' ') p++;

	if (*p == 'u')
		isuser = TRUE;
	else if (*p == 'c')
		isuser = FALSE;
	else
		return src;

	/* skip forward to equals sign. */
	while (*p != '=') {
		if (*p == 0) return src;
		p++;
	}
	p++;
	while (*p == ' ') p++; /* skip spaces. */
	if (*p == '\'' || *p == '"') /* optional quote. */
		p++;
	
	start = p;
	while (*p != '\'' && *p != '"' && *p != '>' && *p != ' ' && *p != '/') {
		if (*p == 0) return src;
		p++;
	}
	end = p;

	if (*p == '\'' || *p == '"') p++;
	while (*p == ' ') p++;
	if (*p == '/') p++;
	while (*p == ' ') p++;
	if (*p != '>')
		return src;
	p++;

	g_string_append_printf(dst, "<a href='nowhere'>"
			"<img src='logjam:%s' align='bottom' border='0'/>"
			"<b>", isuser ? "ljuser" : "ljcomm");
	g_string_append_len(dst, start, end-start);
	g_string_append(dst, "</b></a>");

	return p;
}

static GString*
entry_prepare_preview(LJEntry *entry) {
	GString *str = g_string_new(NULL);
	gchar *event;
	gboolean has_time, has_security;
	
	if (!entry)
		return str;

	has_time = entry->time.tm_year > 0;
	has_security = entry->security.type != LJ_SECURITY_PUBLIC;

	if (has_security || entry->subject || has_time) {
		g_string_append(str, "<table width='100%'><tr>");
		if (has_security || entry->subject) {
			g_string_append(str, "<td align='left'>");
			if (has_security) {
				char *img;
				if (entry->security.type == LJ_SECURITY_PRIVATE)
					img = "private";
				else
					img = "protected";
				g_string_append_printf(str,
						"<img src='logjam:%s' align='bottom'/>", img);
			}
			if (entry->subject)
				g_string_append_printf(str, "<b>%s</b>", entry->subject);
			g_string_append(str, "</td>");
		}
		if (has_time) {
			g_string_append_printf(str,
					"<td align='right'>%s</td>", asctime(&entry->time));
		}
		g_string_append(str, "</tr></table><hr/><br/>");
	}

	if (entry->mood || entry->music || entry->location || entry->taglist) {
		if (entry->mood)
			g_string_append_printf(str, "<i>%s</i>: %s<br/>", _("Current Mood"), entry->mood);
		if (entry->music)
			g_string_append_printf(str, "<i>%s</i>: %s<br/>", _("Current Music"), entry->music);
		if (entry->location)
			g_string_append_printf(str, "<i>%s</i>: %s<br/>", _("Current Location"), entry->location);
		if (entry->taglist)
			g_string_append_printf(str, "<i>%s</i>: %s<br/>", _("Tags"), entry->taglist);
		g_string_append(str, "<br/>");
	}

	/* insert <br/> tags (if preformmated) and fixup <lj user=foo> tags. */
	for (event = entry->event; *event; event++) {
		if (*event == '\n' && !entry->preformatted) {
			g_string_append_len(str, "<br/>", 5);
		} else if (event[0] == '<' &&
			/* check for lj user / lj comm. */
			/* this is not good html parsing, but it should be enough.
			 * besides, the whole <lj user='foo'> tag is yucky html. :P */
			event[1] == 'l' && event[2] == 'j' && event[3] == ' ') {
			char *p = parse_ljtag(event, str);
			if (p != event) {
				event = p-1;
				continue;
			}
		} else {
			g_string_append_c(str, *event);
		}
	}

	return str;
}

void
preview_update(HTMLPreview *hp) {
	LJEntry *entry;
	GString *str;

	entry = hp->get_entry(hp->get_entry_data);
	if (!entry)
		return;
	str = entry_prepare_preview(entry);
	/* gtkhtml whines if you give it an empty string. */
	if (str->str[0] == '\0')
		g_string_append_c(str, ' ');

	gtk_html_load_from_string(GTK_HTML(hp), str->str, -1);

	g_string_free(str, TRUE);
	lj_entry_free(entry);
}

static LJEntry*
get_entry_from_jw(JamWin *jw) {
	return jam_doc_get_entry(jw->doc);
}

static void
response_cb(GtkWidget *dlg, gint response, PreviewUI *pui) {
	if (response == RESPONSE_UPDATE)
		preview_update(pui->html);
	else
		gtk_widget_destroy(dlg);
}

static void
make_window(PreviewUI *pui) {
	pui->win = gtk_dialog_new_with_buttons(_("HTML Preview"),
			GTK_WINDOW(pui->jw),
			GTK_DIALOG_DESTROY_WITH_PARENT|GTK_DIALOG_NO_SEPARATOR,
			_("_Update"), RESPONSE_UPDATE,
			GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
			NULL);
	gtk_window_set_default_size(GTK_WINDOW(pui->win), 500, 400);
	geometry_tie(GTK_WIDGET(pui->win), GEOM_PREVIEW);
	/* this reportedly breaks some wms.
	 * gtk_window_set_type_hint(GTK_WINDOW(pui->win),
			GDK_WINDOW_TYPE_HINT_UTILITY);*/
	g_signal_connect(G_OBJECT(pui->win), "response",
			G_CALLBACK(response_cb), pui);

	pui->html = HTMLPREVIEW(html_preview_new(
				(GetEntryFunc)get_entry_from_jw, pui->jw));
	preview_update(pui->html);

	jam_dialog_set_contents(GTK_DIALOG(pui->win),
			scroll_wrap(GTK_WIDGET(pui->html)));
}

void
preview_ui_show(JamWin *jw) {
	PreviewUI *pui;

	if (jw->preview) {
		pui = jw->preview;
		preview_update(pui->html);
		gtk_window_present(GTK_WINDOW(pui->win));
		return;
	}

	jw->preview = pui = g_new0(PreviewUI, 1);
	pui->jw = jw;
	make_window(pui);
	g_signal_connect_swapped(G_OBJECT(pui->win), "destroy",
			G_CALLBACK(g_free), pui);
	gtk_widget_show_all(pui->win);

	g_object_add_weak_pointer(G_OBJECT(pui->win), &jw->preview);
}

