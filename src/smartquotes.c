/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#define ASCII_BACKTICK           '`'
#define ASCII_SINGLEQUOTE        '\''
#define ASCII_DOUBLEQUOTE        '"'
#define UNICODE_LEFTSINGLEQUOTE  0x2018
#define UNICODE_RIGHTSINGLEQUOTE 0x2019
#define UNICODE_LEFTDOUBLEQUOTE  0x201C
#define UNICODE_RIGHTDOUBLEQUOTE 0x201D

static int
count_quotes(gunichar c) {
	switch (c) {
		case ASCII_BACKTICK:
		case ASCII_SINGLEQUOTE:
		case UNICODE_LEFTSINGLEQUOTE:
		case UNICODE_RIGHTSINGLEQUOTE:
			return 1;
		case ASCII_DOUBLEQUOTE:
		case UNICODE_LEFTDOUBLEQUOTE:
		case UNICODE_RIGHTDOUBLEQUOTE:
			return 2;
		default:
			return 0;
	}
}

#if 0
static gboolean
normalize_quotes(gunichar *c, gunichar *nextc) {
	if (!normalize_quote(c)) return FALSE;
	if (*c == 1) {
		/* check for `` or '', a doublequote. */
		if (!normalize_quote(nextc) || *nextc != 1)
			return TRUE; /* it's still a single quote. */
		if (INTERPRET_DOUBLE_SINGLEQUOTE) {
			*nextc = 0;
			*c = 2;
		}
	}
	return TRUE;
}
#endif
/*static gboolean
isquote(gunichar c) {
	return (c == ASCII_SINGLEQUOTE || c == ASCII_DOUBLEQUOTE);
}*/

static void
buffer_change_quote(GtkTextBuffer *buffer,
                    GtkTextIter *pos, GtkTextIter *nextpos,
                    gunichar oldc, gunichar newc)
{
	char buf[6];
	int len;

	if (oldc == newc) return;

	gtk_text_buffer_delete(buffer, pos, nextpos);
	len = g_unichar_to_utf8(newc, buf); buf[len] = 0;
	gtk_text_buffer_insert(buffer, pos, buf, len);
}

static void
run_smartquotes(GtkTextBuffer *buffer) {
	GtkTextIter pos, nextpos;
	gunichar lastc = 0, c = 0, nextc;
	int balance[10] = {0}; /* max 10 levels of nesting. */
	int curnesting = -1;
	gboolean insidetag = FALSE, closing;
	int quotes;

	/* this runs as the user is typing, so undo doesn't make much sense.
	gtk_text_buffer_begin_user_action(buffer);
	*/

	gtk_text_buffer_get_start_iter(buffer, &pos);
	while ((c = gtk_text_iter_get_char(&pos)) != 0) {
		nextpos = pos;
		gtk_text_iter_forward_char(&nextpos);
		nextc = gtk_text_iter_get_char(&nextpos);

		/*g_printf("ofs %d\n", gtk_text_iter_get_offset(&pos));*/
		if (c == '<')
			insidetag = TRUE;
		else if (c == '>')
			insidetag = FALSE;
		quotes = count_quotes(c);

		if (insidetag || quotes == 0) {
			lastc = c;
			gtk_text_iter_forward_char(&pos);
			continue;
		}

		closing = (curnesting >= 0 && balance[curnesting] == quotes);
		if (quotes == 1 &&
				g_unichar_isalnum(lastc) && 
				(!closing || g_unichar_isalnum(nextc))) {
			/* an apostrophe.  fix it up, but don't change nesting. */
			/*g_print("n %d apos %c\n", curnesting, (char)c);*/
			buffer_change_quote(buffer, &pos, &nextpos, c,
					UNICODE_RIGHTSINGLEQUOTE);
		} else if (closing) {
			/*g_print("n %d right %c\n", curnesting, (char)c);*/
			buffer_change_quote(buffer, &pos, &nextpos, c,
					quotes == 1 ? 
						UNICODE_RIGHTSINGLEQUOTE :
						UNICODE_RIGHTDOUBLEQUOTE);
			curnesting--;
		} else {
			/*g_print("n %d left %c\n", curnesting, (char)c);*/
			buffer_change_quote(buffer, &pos, &nextpos, c,
					quotes == 1 ? 
						UNICODE_LEFTSINGLEQUOTE :
						UNICODE_LEFTDOUBLEQUOTE);
			curnesting++;
			balance[curnesting] = quotes;
		} 
		if (curnesting >= 9) {
			g_warning("too many nested quotes.");
		}
		lastc = c;
		gtk_text_iter_forward_char(&pos);
	}

	/* gtk_text_buffer_end_user_action(buffer); */
}

#define SMARTQUOTES_KEY "logjam-smartquotes-id"
static void smartquotes_begin(GtkTextBuffer *buffer);
static void smartquotes_insert_cb(GtkTextBuffer *buffer, GtkTextIter *iter,
                                  gchar *text, gint len, gpointer user_data);
static void smartquotes_delete_cb(GtkTextBuffer *buffer,
                                  GtkTextIter *i1, GtkTextIter *i2);

static gboolean
smartquotes_idle_cb(GtkTextBuffer *buffer) {
	g_signal_handlers_block_by_func(buffer,
			smartquotes_insert_cb, SMARTQUOTES_KEY);
	g_signal_handlers_block_by_func(buffer,
			smartquotes_delete_cb, SMARTQUOTES_KEY);
	run_smartquotes(buffer);
	g_signal_handlers_unblock_by_func(buffer,
			smartquotes_insert_cb, SMARTQUOTES_KEY);
	g_signal_handlers_unblock_by_func(buffer,
			smartquotes_delete_cb, SMARTQUOTES_KEY);
	return FALSE;
}

static void
smartquotes_begin(GtkTextBuffer *buffer) {
	GObject *obj = G_OBJECT(buffer);
	guint idleid;
	idleid = GPOINTER_TO_INT(g_object_get_data(obj, SMARTQUOTES_KEY));
	if (idleid)
		g_source_remove(idleid);
	idleid = g_idle_add((GSourceFunc)smartquotes_idle_cb, buffer);
	g_object_set_data(obj, SMARTQUOTES_KEY, GINT_TO_POINTER(idleid));
}

static void
smartquotes_insert_cb(GtkTextBuffer *buffer, GtkTextIter *iter,
                      gchar *text, gint len, gpointer user_data) {
	int i;
	for (i = 0; i < len; i++) {
		if (count_quotes(text[i])) {
			smartquotes_begin(buffer);
			break;
		}
	}
}

static void
smartquotes_delete_cb(GtkTextBuffer *buffer, GtkTextIter *i1, GtkTextIter *i2) {
	gunichar c;
	GtkTextIter i = *i1;
	while (gtk_text_iter_in_range(&i, i1, i2)) {
		c = gtk_text_iter_get_char(&i);
		if (count_quotes(c)) {
			smartquotes_begin(buffer);
			break;
		}
		gtk_text_iter_forward_char(&i);
	}
}

void
smartquotes_attach(GtkTextBuffer *buffer) {
	g_signal_connect(buffer, "insert-text",
			G_CALLBACK(smartquotes_insert_cb), SMARTQUOTES_KEY);
	g_signal_connect(buffer, "delete-range",
			G_CALLBACK(smartquotes_delete_cb), SMARTQUOTES_KEY);
}

void
smartquotes_detach(GtkTextBuffer *buffer) {
	g_signal_handlers_disconnect_matched(G_OBJECT(buffer),
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, SMARTQUOTES_KEY);
}

