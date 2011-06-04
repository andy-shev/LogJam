/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2005 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"
#include "gtk-all.h"

#include <string.h>
#include "html_markup.h"


void
html_mark_tag(JamDoc *doc, const char* tag, ...) {
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	GtkTextMark *ins;
	char * startTag, * endTag;
	int tagLen;
	va_list ap;

	if (!strcmp(tag, "span")) {
		va_start(ap, tag);
		startTag = g_strdup_printf("<%s %s>", tag, va_arg(ap, const char *));
		va_end(ap);
	} else {
		startTag = g_strdup_printf("<%s>", tag);
	}
	endTag   = g_strdup_printf("</%s>", tag);
	tagLen = (int)strlen(endTag);

	buffer = jam_doc_get_text_buffer(doc);

	gtk_text_buffer_begin_user_action(buffer); /* start undo action */

	if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
		gtk_text_buffer_insert_at_cursor(buffer, startTag, -1);
		gtk_text_buffer_insert_at_cursor(buffer, endTag, -1);

		ins = gtk_text_buffer_get_mark(buffer, "insert");
		gtk_text_buffer_get_iter_at_mark(buffer, &start, ins);
		gtk_text_iter_backward_chars(&start, tagLen);
		gtk_text_buffer_move_mark_by_name(buffer, "insert", &start);
		gtk_text_buffer_move_mark_by_name(buffer, "selection_bound", &start);
	} else {
		gtk_text_buffer_insert(buffer, &start, startTag, -1);
		gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
		gtk_text_buffer_insert(buffer, &end, endTag, -1);
		gtk_text_buffer_move_mark_by_name(buffer, "insert", &end);
		gtk_text_buffer_move_mark_by_name(buffer, "selection_bound", &end);
	}

	g_free(startTag);
	g_free(endTag);
	gtk_text_buffer_end_user_action(buffer);
}


void
html_mark_bold(JamDoc *doc) {
	html_mark_tag(doc, "b");
}

void
html_mark_italic(JamDoc *doc) {
	html_mark_tag(doc, "i");
}

void
html_mark_underline(JamDoc *doc) {
	html_mark_tag(doc, "u");
}

void
html_mark_strikeout(JamDoc *doc) {
	html_mark_tag(doc, "s");
}

void
html_mark_monospaced(JamDoc *doc) {
	html_mark_tag(doc, "tt");
}

void
html_mark_blockquote(JamDoc *doc) {
	html_mark_tag(doc, "blockquote");
}
