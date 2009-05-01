/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <config.h>

#include "gtk-all.h"

#define UNICODE_LEFTSINGLEQUOTE 0x2018
#define UNICODE_RIGHTSINGLEQUOTE 0x2019
#define UNICODE_LEFTDOUBLEQUOTE 0x201C
#define UNICODE_RIGHTDOUBLEQUOTE 0x201D

#define UNICODE_ENDASH 0x2013
#define UNICODE_EMDASH 0x2014

#define UNICODE_ELLIPSIS 0x2026

typedef struct {
	GString *dst;
	char *p;

	int in_em : 1;
	int in_strong : 1;
	int in_ul : 1;

	int indent;

	GError **err;
} StructuredTextParser;

typedef struct {
	char character;
	gboolean (*parse)(StructuredTextParser *parser);
} MetaChar;

static gboolean stp_backtick(StructuredTextParser *parser);
static gboolean stp_apostrophe(StructuredTextParser *parser);
static gboolean stp_asterisk(StructuredTextParser *parser);
static gboolean stp_underscore(StructuredTextParser *parser);
static gboolean stp_period(StructuredTextParser *parser);
static gboolean stp_hyphen(StructuredTextParser *parser);
static gboolean stp_html(StructuredTextParser *parser);
static gboolean stp_at(StructuredTextParser *parser);

static MetaChar structured_characters[] = {
	{ '`', stp_backtick },
	{ '\'', stp_apostrophe },
	{ '*', stp_asterisk },
	{ '_', stp_underscore },
	{ '-', stp_hyphen },
	{ '.', stp_period },
	{ '&', stp_html },
	{ '<', stp_html },
	{ '>', stp_html },
	{ '@', stp_at },
	{ 0 }
};

static gboolean
stp_backtick(StructuredTextParser *parser) {
	if (parser->p[1] == '`') {
		g_string_append_unichar(parser->dst, UNICODE_LEFTDOUBLEQUOTE);
		parser->p++;
	} else {
		g_string_append_unichar(parser->dst, UNICODE_LEFTSINGLEQUOTE);
	}
	parser->p++;
	return TRUE;
}
static gboolean
stp_apostrophe(StructuredTextParser *parser) {
	if (parser->p[1] == '\'') {
		g_string_append_unichar(parser->dst, UNICODE_RIGHTDOUBLEQUOTE);
		parser->p++;
	} else {
	/* this is the correct character to use for apostrophes,
	 * too; see Unicode Technical Report #8 section 4.6.
	 * http://www.unicode.org/unicode/reports/tr8/#Apostrophe%20Semantics%20Errata
	 */
		g_string_append_unichar(parser->dst, UNICODE_RIGHTSINGLEQUOTE);
	}
	parser->p++;
	return TRUE;
}
static gboolean
stp_asterisk(StructuredTextParser *parser) {
	if (parser->p[1] == '*') {
		g_string_append_printf(parser->dst, "<%sstrong>",
				parser->in_strong ? "/" : "");
		parser->in_strong = !parser->in_strong;
		parser->p++;
	} else {
		g_string_append_printf(parser->dst, "<%sem>",
				parser->in_em ? "/" : "");
		parser->in_em = !parser->in_em;
	}
	parser->p++;
	return TRUE;
}

static gboolean
stp_underscore(StructuredTextParser *parser) {
	g_string_append_printf(parser->dst, "<%su>", parser->in_ul ? "/" : "");
	parser->in_ul = !parser->in_ul;
	parser->p++;
	return TRUE;
}

static gboolean
stp_period(StructuredTextParser *parser) {
	if (parser->p[1] == '.' && parser->p[2] == '.') {
		g_string_append_unichar(parser->dst, UNICODE_ELLIPSIS);
		parser->p += 3;
	} else {
		g_string_append_c(parser->dst, '.');
		parser->p++;
	}
	return TRUE;
}

static gboolean
stp_hyphen(StructuredTextParser *parser) {
	if (parser->p[1] == '-') {
		if (parser->p[2] == '-') {
			g_string_append_unichar(parser->dst, UNICODE_EMDASH);
			parser->p++;
		} else {
			g_string_append_unichar(parser->dst, UNICODE_ENDASH);
		}
		parser->p++;
	} else {
		/* XXX do we want U+002D HYPHEN-MINUS or
		 *                U+2010 HYPHEN or
		 *                U+2212 MINUS SIGN  ?
		 */
		g_string_append_c(parser->dst, '-');
	}
	parser->p++;
	return TRUE;
}

static gboolean
stp_html(StructuredTextParser *parser) {
	switch (parser->p[0]) {
		case '&':
			g_string_append(parser->dst, "&amp;"); break;
		case '<':
			g_string_append(parser->dst, "&lt;"); break;
		case '>':
			g_string_append(parser->dst, "&gt;"); break;
	}
	parser->p++;
	return TRUE;
}

/* @: verbatim. */
static gboolean
stp_at(StructuredTextParser *parser) {
	gunichar search;
	char *p;

	parser->p++; /* skip @. */
	if (parser->p[0] == '@') {
		parser->p++; /* skip second @. */
		search = g_utf8_get_char(parser->p);
		parser->p = g_utf8_next_char(parser->p);
	} else {
		search = '@';
	}

	p = parser->p;
	while (*p && g_utf8_get_char(p) != search) {
		p = g_utf8_next_char(p);
	}
	g_string_append(parser->dst, "<st:verbatim>");
	g_string_append_len(parser->dst, parser->p, p-parser->p);
	g_string_append(parser->dst, "</st:verbatim>");

	if (*p)
		parser->p = g_utf8_next_char(p);
	else
		parser->p = p;
	return TRUE;
}

static gboolean
structuredtext_to_html_paragraph(StructuredTextParser *parser) {
	MetaChar *mc;
	char *base;

	base = parser->p;

	while (*parser->p) {
		for (mc = structured_characters; mc->character; mc++) {
			if (*parser->p == mc->character) {
				g_string_append_len(parser->dst, base, parser->p - base);
				if (!mc->parse(parser))
					return FALSE;
				base = parser->p;
				break;
			}
		}
		if (parser->p[0] == '\n') {
			if (parser->p[1] == '\n')
				break;
			*parser->p = ' ';
		}
		/* if there are any tabs here, it's a new block.
		 * we already filtered out useless tabs. */
		if (parser->p[0] == '\t')
			break;

		if (*parser->p)
			parser->p = g_utf8_next_char(parser->p);
	}
	g_string_append_len(parser->dst, base, parser->p - base);

	return TRUE;
}

typedef enum {
	PARAGRAPH_NORMAL,
	PARAGRAPH_UL,
	PARAGRAPH_BLOCKQUOTE
} ParagraphType;

void
structuredtext_section_to_html(StructuredTextParser *parser, int indent, gboolean breakli) {
	char *p;
	ParagraphType pt;

	while (*parser->p != 0) {
		/* skip whitespace. */
		while (*parser->p == '\n')
			parser->p = g_utf8_next_char(parser->p);
		if (*parser->p == 0) break;

		/* now parse the paragraph.
		 * don't modify the parser position in case
		 * we're in the wrong context to parse it here. */
		p = parser->p;

		if (indent == -1) {
			/* skip indentation. */
			indent = 0;
			while (*p == '\t') {
				indent++;
				p = g_utf8_next_char(p);
			}
			if (*p == 0) break;
		}

		if (indent < parser->indent) {
			/* this paragraph not on the current level. */
			return;
		}

		/* determine paragraph type. */
		if (p[0] == '*' && p[1] == ' ') {
			pt = PARAGRAPH_UL;
			if (indent == parser->indent && breakli) return;
		} else if (indent > parser->indent) {
			pt = PARAGRAPH_BLOCKQUOTE;
		} else {
			pt = PARAGRAPH_NORMAL;
		}

		parser->p = p;

		if (indent > parser->indent) {
			int oldindent = parser->indent;
			parser->indent = indent;
			if (pt == PARAGRAPH_UL) {
				g_string_append(parser->dst, "<ul>");
				structuredtext_section_to_html(parser, indent, FALSE);
				g_string_append(parser->dst, "</ul>");
			} else {
				g_string_append(parser->dst, "<blockquote>");
				structuredtext_section_to_html(parser, indent, FALSE);
				g_string_append(parser->dst, "</blockquote>");
			}
			parser->indent = oldindent;
		} else if (pt == PARAGRAPH_UL) {
			parser->p += 2;
			g_string_append(parser->dst, "<li>");
			structuredtext_section_to_html(parser, indent, TRUE);
			g_string_append(parser->dst, "</li>");
		} else {
			g_string_append(parser->dst, "<p>");
			structuredtext_to_html_paragraph(parser);
			g_string_append(parser->dst, "</p>");
		}

		indent = -1;
	}
}

char*
structuredtext_to_html(char *src, GError **err) {
	GString *str = g_string_new(NULL);
	StructuredTextParser parser_ = {0}, *parser = &parser_;
	char *ret;
	char *s, *e, *p;

	parser->p = src;
	parser->dst = str;
	parser->err = err;

	/* fixup all stretches of whitespace lines:
	 * \n \t \n
	 * into straight blocks of \n. */
	for (s = src; *s; s++) {
		if (*s == '\n') {
			for (e = s+1; *e == ' ' || *e == '\t'; e++)
				;
			if (*e == '\n') {
				for (p = s; p < e; p++)
					*p = '\n';
			}
		}
	}

	structuredtext_section_to_html(parser, -1, FALSE);

	ret = str->str;
	g_string_free(str, FALSE);
	return ret;
}

typedef struct {
	GString *dst;
	gboolean in_verbatim : 1;
	/* indent tracks the logical indentation we should be at
	 * (for example, a list within a list is two levels),
	 * while cindent (current indent) tracks the actual number of tabs we've
	 * inserted for the current block. */
	int indent;
	int cindent;
} HTMLParser;

static void
passthrough(GString *str,
	const gchar *element_name,
	const gchar **attribute_names,
	const gchar **attribute_values,
	gboolean open,
	gboolean close)
{
	int i;
	char *esc;
	g_string_append_c(str, '<');
	if (!open && close)
		g_string_append_c(str, '/');
	g_string_append(str, element_name);
	if (attribute_names) {
		for (i = 0; attribute_names[i]; i++) {
			esc = g_markup_escape_text(attribute_values[i], -1);
			g_string_append_printf(str, " %s=\"%s\"",
					attribute_names[i], esc);
			g_free(esc);
		}
	}
	if (open && close)
		g_string_append_c(str, '/');
	g_string_append_c(str, '>');
}

void st_start_element(GMarkupParseContext *context,
	const gchar *element_name,
	const gchar **attribute_names,
	const gchar **attribute_values,
	gpointer user_data,
	GError **error)
{
	HTMLParser *parser = user_data;
	if (g_ascii_strcasecmp(element_name, "strong") == 0)
		g_string_append(parser->dst, "**");
	else if (g_ascii_strcasecmp(element_name, "em") == 0)
		g_string_append_c(parser->dst, '*');
	else if (g_ascii_strcasecmp(element_name, "u") == 0)
		g_string_append_c(parser->dst, '_');
	else if (g_ascii_strcasecmp(element_name, "p") == 0) {
		for (; parser->cindent < parser->indent; parser->cindent++) 
			g_string_append_c(parser->dst, '\t');
	}
	else if (g_ascii_strcasecmp(element_name, "blockquote") == 0)
		parser->indent++;
	else if (g_ascii_strcasecmp(element_name, "ul") == 0)
		parser->indent++;
	else if (g_ascii_strcasecmp(element_name, "li") == 0) {
		if (parser->cindent > 0) {
			g_string_append_c(parser->dst, '\n');
			parser->cindent = 0;
		}
		for (; parser->cindent < parser->indent; parser->cindent++) 
			g_string_append_c(parser->dst, '\t');
		g_string_append(parser->dst, "* ");
	}
	else if (g_ascii_strcasecmp(element_name, "hr") == 0)
		g_string_append(parser->dst, "----");
	else if (g_ascii_strcasecmp(element_name, "lj") == 0)
		passthrough(parser->dst, element_name, attribute_names, attribute_values, TRUE, TRUE);
	else if (g_ascii_strcasecmp(element_name, "entry") == 0)
		;
	else if (g_ascii_strcasecmp(element_name, "st:verbatim") == 0) {
		parser->in_verbatim = 1;
		g_string_append_c(parser->dst, '@');
	} else {
		if (parser->in_verbatim) {
			passthrough(parser->dst, element_name, attribute_names, attribute_values, TRUE, FALSE);
		} else {
			g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
					_("Unknown element: '%s'"), element_name);
		}
	}
}
void st_end_element(GMarkupParseContext *context,
	const gchar *element_name,
	gpointer user_data,
	GError **error)
{
	HTMLParser *parser = user_data;
	if (g_ascii_strcasecmp(element_name, "strong") == 0)
		g_string_append_len(parser->dst, "**", 2);
	else if (g_ascii_strcasecmp(element_name, "em") == 0)
		g_string_append_c(parser->dst, '*');
	else if (g_ascii_strcasecmp(element_name, "u") == 0)
		g_string_append_c(parser->dst, '_');
	else if (g_ascii_strcasecmp(element_name, "p") == 0) {
		g_string_append_len(parser->dst, "\n\n", 2);
		parser->cindent = 0;
	}
	else if (g_ascii_strcasecmp(element_name, "blockquote") == 0)
		parser->indent--;
	else if (g_ascii_strcasecmp(element_name, "ul") == 0)
		parser->indent--;
	else if (g_ascii_strcasecmp(element_name, "li") == 0)
		;
	else if (g_ascii_strcasecmp(element_name, "lj") == 0)
		;
	else if (g_ascii_strcasecmp(element_name, "hr") == 0)
		;
	else if (g_ascii_strcasecmp(element_name, "entry") == 0)
		;
	else if (g_ascii_strcasecmp(element_name, "st:verbatim") == 0) {
		parser->in_verbatim = 0;
		g_string_append_c(parser->dst, '@');
	} else {
		if (parser->in_verbatim)
			passthrough(parser->dst, element_name, NULL, NULL, FALSE, TRUE);
		else
			g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
				_("Unknown element: '%s'"), element_name);
	}
}
void st_text(GMarkupParseContext *context,
	const gchar *text,
	gsize text_len,
	gpointer user_data,
	GError **error)
{
	HTMLParser *parser = user_data;
	const char *p;
	gunichar c;

	if (parser->in_verbatim) {
		g_string_append_len(parser->dst, text, text_len);
		return;
	}
	for (p = text; *p; p = g_utf8_next_char(p)) {
		c = g_utf8_get_char(p);
		switch (c) {
			case UNICODE_LEFTSINGLEQUOTE:
				g_string_append_c(parser->dst, '`'); break;
			case UNICODE_RIGHTSINGLEQUOTE:
				g_string_append_c(parser->dst, '\''); break;
			case UNICODE_LEFTDOUBLEQUOTE:
				g_string_append(parser->dst, "``"); break;
			case UNICODE_RIGHTDOUBLEQUOTE:
				g_string_append(parser->dst, "\'\'"); break;
			case UNICODE_ENDASH:
				g_string_append(parser->dst, "--"); break;
			case UNICODE_EMDASH:
				g_string_append(parser->dst, "---"); break;
			case UNICODE_ELLIPSIS:
				g_string_append(parser->dst, "..."); break;
			case '\n':
				parser->cindent = 0;
				/* fall through. */
			default:
				g_string_append_unichar(parser->dst, c);
		}
	}
}
/*void st_error(GMarkupParseContext *context,
	GError *error,
	gpointer user_data)
{
}*/

char*
html_to_structuredtext(char *src, GError **err) {
	GMarkupParser parser = {
		st_start_element,
		st_end_element,
		st_text,
		NULL,
		NULL
	};
	GMarkupParseContext *context;
	char *str = g_strdup_printf("<entry>%s</entry>", src);
	HTMLParser htmlparser_act = {0}, *htmlparser = &htmlparser_act;

	htmlparser->dst = g_string_new(NULL);

	context = g_markup_parse_context_new(&parser, 0, htmlparser, NULL);
	if (!g_markup_parse_context_parse(context, str, strlen(str), err)) {
		g_free(str);
		g_markup_parse_context_free(context);
		g_string_free(htmlparser->dst, TRUE);
		return NULL;
	}
	g_free(str);
	g_markup_parse_context_free(context);

	str = htmlparser->dst->str;
	g_string_free(htmlparser->dst, FALSE);
	return str;
}

