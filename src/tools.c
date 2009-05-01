/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include <libxml/parser.h>
#include <string.h>

#include "tools.h"
#include "util-gtk.h"

typedef struct {
	gunichar c;
	char *escaped;
} htmlescape;
htmlescape htmlescapes[] = {
	{ '<', "&lt;" },
	{ '>', "&gt;" },
	{ '&', "&amp;" },
	/* FIXME: add more entities here?
	 * the above are all that are really necessary, though. */
	{ 0, 0 }
};

void
tools_html_escape(GtkWindow *win, JamDoc *doc) {
	GtkTextBuffer *buffer;
	GtkTextIter start, end, pos, nextpos;
	htmlescape *esc;
	gunichar c;
	int len;

	buffer = jam_doc_get_text_buffer(doc);

	if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
		/* no selection; give them some help. */
		jam_messagebox(win, _("HTML Escape Help"),
				_("Some characters (such as '<' and '&') are reserved in HTML, and may not display correctly in your post.\n"
				"To properly escape these characters, first select a block of text and try this command again.\n"));
		return;
	}

	gtk_text_buffer_begin_user_action(buffer); /* atomic, in terms of undo */

	for (pos = start; 
			gtk_text_iter_in_range(&pos, &start, &end);
			gtk_text_iter_forward_char(&pos)) {
		c = gtk_text_iter_get_char(&pos);
		for (esc = htmlescapes; esc->c != 0; esc++) {
			if (esc->c == c) break;
		}
		/* if this character isn't in the to-escape list, continue. */
		if (esc->c == 0) continue;

		nextpos = pos;
		gtk_text_iter_forward_char(&nextpos);
		gtk_text_buffer_delete(buffer, &pos, &nextpos);
		len = (int)strlen(esc->escaped);
		gtk_text_buffer_insert(buffer, &pos, esc->escaped, len);

		/* to counteract the pos++ in the for loop */
		gtk_text_iter_backward_char(&pos);
		
		/* changing the buffer invalidates this iterator */
		gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
	}

	gtk_text_buffer_move_mark_by_name(buffer, "insert", &end);
	gtk_text_buffer_move_mark_by_name(buffer, "selection_bound", &end);

	gtk_text_buffer_end_user_action(buffer);
}

void
tools_remove_linebreaks(GtkWindow *win, JamDoc *doc) {
	GtkTextBuffer *buffer;
	GtkTextIter start, end, pos, nextpos;
	gint nlrun = 0;
	gboolean leading = TRUE;

	buffer = jam_doc_get_text_buffer(doc);

	if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
		/* no selection; give them some help. */
		jam_messagebox(win, _("Linebreak Removal Help"),
				_("When you paste text into LogJam, linebreaks from the original are preserved, and will normally show up in your post on the server. You can have LogJam remove extraneous linebreaks from your entry, but you must make a specific selection of the text you wish this to happen on first.\n\nNote that you can also use the \"don't autoformat\" option on your post."));
		return;
	}
 
	gtk_text_buffer_begin_user_action(buffer); /* atomic, in terms of undo */

	for (pos = start; 
			gtk_text_iter_in_range(&pos, &start, &end);
			gtk_text_iter_forward_char(&pos)) {
		
		/* we remove linebreaks as we see them; but later we will
		 * make up for our deletions by inserting spaces or \n\ns
		 * as appropriate. */
		if (gtk_text_iter_ends_line(&pos)) {
			if (!leading) /* leading linebreaks should just be removed */
				nlrun++;
			nextpos = pos;
			gtk_text_iter_forward_line(&nextpos);
			gtk_text_buffer_delete(buffer, &pos, &nextpos);

			gtk_text_iter_backward_char(&pos);

			gtk_text_buffer_get_selection_bounds(buffer, &start, &end);

			continue;
		}
		
		leading = FALSE;
		
		/* make up for what we removed, according to how long a run
		 * it was. */
		if (nlrun == 1) { /* line separators turn into a space */
			nlrun = 0;
			gtk_text_buffer_insert(buffer, &pos, " ", 1);
			gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
		} else if (nlrun > 1) { /* paragraph breaks are normalized */
			nlrun = 0;
			gtk_text_buffer_insert(buffer, &pos, "\n\n", 2);
			
			/* this is safe, since we've just added two characters. */
			gtk_text_iter_forward_chars(&pos, 2);

			gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
		}
	}

	gtk_text_buffer_move_mark_by_name(buffer, "insert", &end);
	gtk_text_buffer_move_mark_by_name(buffer, "selection_bound", &end);
	
	gtk_text_buffer_end_user_action(buffer);
}

void
tools_insert_file(GtkWindow *win, JamDoc *doc) {
	GtkWidget *filesel;
	GtkWidget *hbox, *label, *combo;
	GList *strings = NULL;
	const char *localeenc;

	filesel = gtk_file_chooser_dialog_new(_("Select File"), win,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);
	label = gtk_label_new(NULL);
	gtk_label_set_text_with_mnemonic(GTK_LABEL(label),
			_("File _encoding:"));
	combo = gtk_combo_new();

	/* if they're in a non-UTF-8 locale, default to the locale encoding. */
	if (!g_get_charset(&localeenc))
		strings = g_list_append(strings, (char*)localeenc);
	/* what else should go in this list other than UTF-8? */
	strings = g_list_append(strings, "UTF-8");
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), strings);

	gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_COMBO(combo)->entry);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(filesel)->vbox),
			hbox, FALSE, FALSE, 0);
	gtk_widget_show_all(hbox);

	while (gtk_dialog_run(GTK_DIALOG(filesel)) == GTK_RESPONSE_ACCEPT) {
		const gchar *filename;
		const gchar *encoding;
		GError *err = NULL;

		filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(filesel));
		encoding = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry));

		if (!jam_doc_insert_file(doc, filename, encoding, &err)) {
			jam_warning(GTK_WINDOW(filesel),
					_("Error loading file: %s"), err->message);
			g_error_free(err);
		} else {
			break;
		}
	}
	gtk_widget_destroy(filesel);
}

void
tools_insert_command_output(GtkWindow *win, JamDoc *doc) {
	/* FIXME: use combo with a history drop-down, save the history */
	GtkWidget *cmd_dlg, *entry, *box;

	cmd_dlg = gtk_dialog_new_with_buttons(_("Insert Command Output"),
			win, GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
	gtk_window_set_transient_for(GTK_WINDOW(cmd_dlg), win);

	entry = gtk_entry_new();
	
	box = labelled_box_new(_("_Command:"), entry);
	jam_dialog_set_contents(GTK_DIALOG(cmd_dlg), box);
	gtk_widget_grab_focus(entry);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_dialog_set_default_response(GTK_DIALOG(cmd_dlg), GTK_RESPONSE_OK);

	while (gtk_dialog_run(GTK_DIALOG(cmd_dlg)) == GTK_RESPONSE_OK) {
		const gchar *command;
		const gchar *encoding = 0;
		GError *err = NULL;

		command = gtk_entry_get_text(GTK_ENTRY(entry));
		if (!jam_doc_insert_command_output(doc,
				command, encoding, &err, win) && (err)) {
			jam_warning(GTK_WINDOW(cmd_dlg),
				_("Error getting command output: %s"),
				err->message);
			if (err->code != 127)
				break;
			g_error_free(err);
		} else {
			break;
		}
	}
	gtk_widget_destroy(cmd_dlg);
}

/* it appears there's no way to attach a pointer to a libXML
 * parser context, which means we need to use a global variable
 * so we can save the error message it tries to print to stdout
 * and display it in a dialog instead.  as another hack, we need
 * to print that text in a monospaced font because it highlights
 * the invalid character position...
 *
 * the ideal solution would be for libxml to use GError to report
 * errors and then I wouldn't need any of this.
 */ 
static GString *xml_error_context_hack;
void
xmlErrFunc(void *ctx, const char *msg, ...) {
	char *s;
	va_list ap;
	va_start(ap, msg);
	s = g_strdup_vprintf(msg, ap);
	g_string_append(xml_error_context_hack, s);
	g_free(s);
	va_end(ap);
}
static void
xml_error_dialog(GtkWindow *parent, GString *errstr) {
	GtkWidget *dlg, *box, *label;

	dlg = gtk_dialog_new_with_buttons(_("XML Parse Error"),
			parent, GTK_DIALOG_MODAL,
			GTK_STOCK_OK, GTK_STOCK_CANCEL, NULL);

	box = gtk_vbox_new(FALSE, 5);

	gtk_box_pack_start(GTK_BOX(box),
			gtk_label_new(_("Error parsing XML:")),
			FALSE, FALSE, 0);

	label = gtk_label_new(errstr->str);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	jam_widget_set_font(label, "monospace");
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);

	jam_dialog_set_contents(GTK_DIALOG(dlg), box);

	gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);
}

static gboolean
validate_xml(GtkWindow *parent, char *text) {
	xmlParserCtxtPtr ctxt = xmlCreateDocParserCtxt(BAD_CAST text);
	
	xml_error_context_hack = g_string_new(NULL);
	xmlSetGenericErrorFunc(ctxt, xmlErrFunc);
	if (xmlParseDocument(ctxt) < 0) {
		xml_error_dialog(parent, xml_error_context_hack);
		xmlFreeParserCtxt(ctxt);
		g_string_free(xml_error_context_hack, TRUE);
		return FALSE;
	}
	initGenericErrorDefaultFunc(NULL);
	xmlFreeParserCtxt(ctxt);
	g_string_free(xml_error_context_hack, TRUE);
	return TRUE;
}

void
tools_validate_xml(GtkWindow *win, JamDoc *doc) {
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	char *entry, *str;

	buffer = jam_doc_get_text_buffer(doc);

	/* default to the selection, but fall back to the entire entry. */
	if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end))
		gtk_text_buffer_get_bounds(buffer, &start, &end);

	entry = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);
	str = g_strdup_printf("<entry>%s</entry>", entry);

	if (validate_xml(win, str)) {
		jam_messagebox(win, _("XML Validation"),
				_("Document is well-formed XML."));
	}

	g_free(entry);
	g_free(str);
}

void
tools_ljcut(GtkWindow *win, JamDoc *doc) {
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	GtkWidget *dlg, *vbox, *hbox, *label, *entry;
	char *text;

	dlg = gtk_dialog_new_with_buttons(_("LJ-Cut"), win,
			GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK,     GTK_RESPONSE_OK,
			NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

	entry = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	hbox = labelled_box_new(_("Cut c_aption:"), entry);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label),
			_("<small>If left empty, the LiveJournal default "
				"will be used.</small>"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	jam_dialog_set_contents(GTK_DIALOG(dlg), vbox);

	if (gtk_dialog_run(GTK_DIALOG(dlg)) != GTK_RESPONSE_OK) {
		gtk_widget_destroy(dlg);
		return;
	}
	text = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	gtk_widget_destroy(dlg);
	if (text[0] == 0) {
		g_free(text);
		text = NULL;
	}
	xml_escape(&text);
	
	buffer = jam_doc_get_text_buffer(doc);

	gtk_text_buffer_begin_user_action(buffer); /* start undo action */
	if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
		if (text) {
			gtk_text_buffer_insert_at_cursor(buffer, "<lj-cut text=\"", -1);
			gtk_text_buffer_insert_at_cursor(buffer, text, -1);
			gtk_text_buffer_insert_at_cursor(buffer, "\">", -1);
		} else {
			gtk_text_buffer_insert_at_cursor(buffer, "<lj-cut>", -1);
		}
	} else {
		if (text) {
			gtk_text_buffer_insert(buffer, &start, "<lj-cut text=\"", -1);
			gtk_text_buffer_insert(buffer, &start, text, -1);
			gtk_text_buffer_insert(buffer, &start, "\">", -1);
		} else {
			gtk_text_buffer_insert(buffer, &start, "<lj-cut>", -1);
		}
		gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
		gtk_text_buffer_insert(buffer, &end, "</lj-cut>", -1);
		gtk_text_buffer_move_mark_by_name(buffer, "insert", &end);
		gtk_text_buffer_move_mark_by_name(buffer, "selection_bound", &end);
	}
	g_free(text);

	gtk_text_buffer_end_user_action(buffer);
}

