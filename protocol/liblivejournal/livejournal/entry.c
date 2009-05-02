/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <config.h>

#include <glib.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifndef G_OS_WIN32
#include <unistd.h>
#include <sys/wait.h>
#endif

#ifdef HAVE_LIBXML
#include <libxml/tree.h>
#endif

#include <sys/types.h>

/* FIXME: translate. */
#define _(x) x

#include "entry.h"

//static LJEntry* entry_from_user_editor(const char *filename, GError **err);
static gboolean lj_entry_load(LJEntry *entry, gchar *data, gsize len,
           LJEntryFileType type, LJEntryFileType *typeret, GError **err);

LJEntry *
lj_entry_new(void) {
	LJEntry *entry = g_new0(LJEntry, 1);
	return entry;
}
LJEntry *
lj_entry_copy(LJEntry *e) {
	LJEntry *newe = lj_entry_new();
	memcpy(newe, e, sizeof(LJEntry));
	if (e->subject)
		newe->subject = g_strdup(e->subject);
	if (e->event)
		newe->event = g_strdup(e->event);
	if (e->mood)
		newe->mood = g_strdup(e->mood);
	if (e->music)
		newe->music = g_strdup(e->music);
	if (e->location)
		newe->location = g_strdup(e->location);
	if (e->taglist)
		newe->taglist = g_strdup(e->taglist);
	if (e->pickeyword)
		newe->pickeyword = g_strdup(e->pickeyword);
	return newe;
}
void
lj_entry_free(LJEntry *e) {
	g_free(e->subject);
	g_free(e->event);
	g_free(e->mood);
	g_free(e->music);
	g_free(e->location);
	g_free(e->taglist);
	g_free(e->pickeyword);
	g_free(e);
}

const char*
lj_get_summary(const char *subject, const char *event) {
#define SUMMARY_LENGTH 50
	/* SUMMARY_LENGTH is in chars, not bytes.
	 * UTF-8 chars can be up to 6 bytes, and then 
	 * we need a bit more space to hold the "...". */

	static char buf[SUMMARY_LENGTH*6 + 5];

	if (subject && subject[0])
		return subject;

	if (event) {
		int i;
		g_utf8_strncpy(buf, event, SUMMARY_LENGTH);

		/* stop at the first newline. */
		for (i = 0; buf[i]; i++) {
			if (buf[i] == '\n') {
				buf[i] = 0;
				break; /* start "..." from this point. */
			}
		}

		if (g_utf8_strlen(event, SUMMARY_LENGTH) < SUMMARY_LENGTH)
			return buf;

		if (i == 0 || buf[i-1] != '.')
			buf[i++] = '.';
		buf[i++] = '.';
		buf[i++] = '.';
		buf[i++] = 0;
		return buf;
	}
	return NULL;
}

const char*
lj_entry_make_summary(LJEntry *entry) {
	return lj_get_summary(entry->subject, entry->event);
}

void
lj_entry_set_request_fields(LJEntry *entry, LJRequest *request) {
	struct tm *ptm = &entry->time;

	/* basic information */
	if (entry->itemid)
		lj_request_add_int(request, "itemid", entry->itemid);

	lj_request_add(request, "subject", entry->subject ? entry->subject : "");
	lj_request_add(request, "event",   entry->event);

	if (!ptm->tm_year) {
		time_t curtime_time_t = time(NULL);
		ptm = localtime(&curtime_time_t);
	}
	lj_request_add_int(request, "year", ptm->tm_year+1900);
	lj_request_add_int(request, "mon",  ptm->tm_mon+1);
	lj_request_add_int(request, "day",  ptm->tm_mday);
	lj_request_add_int(request, "hour", ptm->tm_hour);
	lj_request_add_int(request, "min",  ptm->tm_min);

	/* metadata */
/* http://www.livejournal.com/admin/schema/?mode=viewdata&table=logproplist */
	lj_request_add(request, "prop_current_mood", entry->mood ? entry->mood : "");
	if (entry->moodid)
		lj_request_add_int(request, "prop_current_moodid", entry->moodid);
	else
		lj_request_add(request, "prop_current_moodid", "");
	lj_request_add(request,
			"prop_current_location", entry->location ? entry->location : "");
	lj_request_add(request, 
			"prop_current_music", entry->music ? entry->music : "");
	lj_request_add(request, 
			"prop_taglist", entry->taglist ? entry->taglist : "");
	lj_request_add(request, 
			"prop_picture_keyword", entry->pickeyword ? entry->pickeyword : "");
	lj_request_add_int(request, "prop_opt_preformatted", entry->preformatted);
	lj_request_add_int(request, "prop_opt_nocomments", entry->comments == LJ_COMMENTS_DISABLE);
	lj_request_add_int(request, "prop_opt_noemail", entry->comments == LJ_COMMENTS_NOEMAIL);
	lj_request_add_int(request, "prop_opt_backdated", entry->backdated);

	lj_security_append_to_request(&entry->security, request);
}

static gboolean
verify_utf8(char **str) {
	if (*str == NULL)
		return TRUE;  /* NULL string is valid utf-8.  :) */

	if (!g_utf8_validate(*str, -1, NULL)) {
		char *newstr;
		/* it's bad utf-8.  :(
		 * try replacing bad stuff with "."
		 * we can assume the source encoding wasn't utf-8,
		 * because if they're posting utf-8
		 * they have no excuse for bad characters. */
		newstr = g_convert_with_fallback(*str, -1, "UTF-8", "ISO-8859-1",
				".", NULL, NULL, NULL);
		if (newstr) {
			g_free(*str);
			*str = newstr;
		}
		return FALSE;
	}
	return TRUE;
}

static gboolean
lj_entry_load_metadata(LJEntry *entry,
                       const char *key, const char *value,
                       GError **err) {
	if (strcmp(key, "current_mood") == 0) {
		entry->mood = g_strdup(value);
		if (!verify_utf8(&entry->mood)) {
			g_set_error(err, 0, 0, "Bad UTF-8 in current_mood");
			return FALSE;
		}
	} else if (strcmp(key, "current_moodid") == 0) {
		entry->moodid = atoi(value);
	} else if (strcmp(key, "current_music") == 0) {
		entry->music = g_strdup(value);
		if (!verify_utf8(&entry->music)) {
			g_set_error(err, 0, 0, "Bad UTF-8 in current_music");
			return FALSE;
		}
	} else if (strcmp(key, "current_location") == 0) {
		entry->location = g_strdup(value);
		if (!verify_utf8(&entry->location)) {
			g_set_error(err, 0, 0, "Bad UTF-8 in current_location");
			return FALSE;
		}
	} else if (strcmp(key, "taglist") == 0) {
		entry->taglist = g_strdup(value);
		if (!verify_utf8(&entry->taglist)) {
			g_set_error(err, 0, 0, "Bad UTF-8 in taglist");
			return FALSE;
		}
	} else if (strcmp(key, "picture_keyword") == 0) {
		entry->pickeyword = g_strdup(value);
		if (!verify_utf8(&entry->pickeyword)) {
			g_set_error(err, 0, 0, "Bad UTF-8 in picture_keyword");
			return FALSE;
		}
	} else if (strcmp(key, "opt_preformatted") == 0) {
		entry->preformatted = (value && value[0] == '1');
	} else if (strcmp(key, "opt_nocomments") == 0) {
		entry->comments = LJ_COMMENTS_DISABLE;
	} else if (strcmp(key, "opt_noemail") == 0) {
		entry->comments = LJ_COMMENTS_NOEMAIL;
	} else if (strcmp(key, "opt_backdated") == 0) {
		entry->backdated = (value && value[0] == '1');
	}/* else {
		g_set_error(err, 0, 0, "Unknown metadata '%s'.", key);
		return FALSE;
	}*/
	return TRUE;
}

#ifdef HAVE_LIBXML
#define XML_ENTRY_META_GET(A)                                          \
    if ((!xmlStrcmp(cur->name, BAD_CAST #A))) {	                       \
        entry->A = (char*)xmlNodeListGetString(doc, cur->xmlChildrenNode, 1); \
    }

#define XML_ENTRY_META_SET(A)                     \
	if (entry->A)                                 \
		xmlNewTextChild(root, NULL, BAD_CAST #A, BAD_CAST entry->A);

void*
lj_entry_to_xml_node(LJEntry *entry, void* doc) {
	xmlNodePtr root, node;
	root = xmlNewDocNode(doc, NULL, BAD_CAST "entry", NULL);

	/* do these in the order the fields are specified in livejournal.h,
	 * so we don't miss anything. */
	if (entry->itemid) {
		char buf[10];
		g_snprintf(buf, 10, "%d", entry->itemid);
		xmlSetProp(root, BAD_CAST "itemid", BAD_CAST buf);
	}
	XML_ENTRY_META_SET(subject);
	XML_ENTRY_META_SET(event);
	if (entry->mood || entry->moodid) {
		if (entry->mood)
			node = xmlNewTextChild(root, NULL, BAD_CAST "mood", BAD_CAST entry->mood);
		else
			node = xmlNewChild(root, NULL, BAD_CAST "mood", NULL);

		if (entry->moodid) {
			char buf[10];
			g_snprintf(buf, 10, "%d", entry->itemid);
			xmlSetProp(node, BAD_CAST "itemid", BAD_CAST buf);
		}
	}
	XML_ENTRY_META_SET(music);
	XML_ENTRY_META_SET(location);
	XML_ENTRY_META_SET(taglist);
	XML_ENTRY_META_SET(pickeyword);
	if (entry->preformatted)
		xmlNewChild(root, NULL, BAD_CAST "preformatted", NULL);
	if (entry->backdated)
		xmlNewChild(root, NULL, BAD_CAST "backdated", NULL);
	if (entry->comments) {
		char *type = NULL;
		node = xmlNewChild(root, NULL, BAD_CAST "comments", NULL);
		switch (entry->comments) {
			case LJ_COMMENTS_DEFAULT: break;
			case LJ_COMMENTS_NOEMAIL: type = "noemail"; break;
			case LJ_COMMENTS_DISABLE: type = "disable"; break;
		}
		xmlSetProp(node, BAD_CAST "type", BAD_CAST type);
	}
	if (entry->time.tm_year) {
		char *ljdate = lj_tm_to_ljdate(&entry->time);
		xmlNewTextChild(root, NULL, BAD_CAST "time", BAD_CAST ljdate);
		g_free(ljdate);
	}
	if (entry->security.type != LJ_SECURITY_PUBLIC) {
		xmlNodePtr node;
		char *type = NULL, *mask = NULL;
		lj_security_to_strings(&entry->security, &type, &mask);
		if (type) {
			node = xmlNewChild(root, NULL, BAD_CAST "security", NULL);
			xmlSetProp(node, BAD_CAST "type", BAD_CAST type);
			if (mask) {
				xmlSetProp(node, BAD_CAST "allowmask", BAD_CAST mask);
				g_free(mask);
			}
			g_free(type);
		}
	}

	return root;
}

static void*
lj_entry_to_xml(LJEntry *entry) {
	xmlDocPtr doc;
	xmlNodePtr root;

	doc = xmlNewDoc(BAD_CAST "1.0");
	root = lj_entry_to_xml_node(entry, doc);
	xmlDocSetRootElement(doc, root);

	return doc;
}
#endif /* HAVE_LIBXML */

LJEntry*
lj_entry_new_from_file(FILE *f, LJEntryFileType type,
                       LJEntryFileType *typeret, GError **err) {
	GString *str = g_string_new(NULL);
	char buf[1024];
	int len;
	LJEntry *entry;

	while ((len = (int)fread(buf, 1, 1024, f)) > 0)
		g_string_append_len(str, buf, len);

	entry = lj_entry_new();
	if (!lj_entry_load(entry, str->str, str->len, type, typeret, err)) {
		lj_entry_free(entry);
		entry = NULL;
	}

	g_string_free(str, TRUE);
	return entry;
}

LJEntry*
lj_entry_new_from_filename(const char *filename, LJEntryFileType type,
                           LJEntryFileType *typeret, GError **err) {
	LJEntry *entry;
	FILE *f;

	f = fopen(filename, "r");
	if (!f) {
		g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno),
				_("Error reading file '%s': %s"), filename, g_strerror(errno));
		return NULL;
	}
		
	entry = lj_entry_new_from_file(f, type, typeret, err);
	fclose(f);

	return entry;
}

static LJEntry*
lj_entry_new_from_result(LJResult *result, int i) {
	LJEntry *entry;
	char *itemid, *anum;

	itemid = lj_result_getf(result, "events_%d_itemid", i);
	if (!itemid)
		return NULL;

	entry = lj_entry_new();
	entry->itemid = atoi(itemid);
	anum = lj_result_getf(result, "events_%d_anum", i);
	if (anum) entry->anum = atoi(anum);
	lj_ljdate_to_tm(lj_result_getf(result, "events_%d_eventtime", i), &entry->time);

	entry->event = lj_urldecode(lj_result_getf(result, "events_%d_event", i));
	if (!verify_utf8(&entry->event))
		g_warning("Bad UTF-8 in event of itemid %d.\n", entry->itemid); 
	/* http://www.livejournal.com/community/logjam/113710.html */
	if (!entry->event)
		entry->event = g_strdup("");

	entry->subject = g_strdup(lj_result_getf(result, "events_%d_subject", i));
	if (!verify_utf8(&entry->subject))
		g_warning("Bad UTF-8 in subject of itemid %d.\n", entry->itemid); 

	lj_security_from_strings(&entry->security, 
			lj_result_getf(result, "events_%d_security", i),
			lj_result_getf(result, "events_%d_allowmask", i));

	return entry;
}

LJEntry *
lj_entry_new_single_from_result(LJResult *result, GError **err) {
	LJEntry *entry;
	int i, propcount;
	char *pname, *pvalue;
	GString *errs = NULL;
	GError *tmperr;

	entry = lj_entry_new_from_result(result, 1);

	/* if we retrieved more than one entry, this is definitely wrong. */
	propcount = lj_result_get_int(result, "prop_count");
	for (i = 1; i <= propcount; i++) {
		pname  = lj_result_getf(result, "prop_%d_name",  i);
		pvalue = lj_result_getf(result, "prop_%d_value", i);
		if (!lj_entry_load_metadata(entry, pname, pvalue, &tmperr)) {
			if (!errs) {
				errs = g_string_new(tmperr->message);
			} else {
				g_string_append_c(errs, ';');
				g_string_append(errs, tmperr->message);
			}
			g_error_free(tmperr);
		}
	}

	if (errs) {
		g_set_error(err, 0, 0, errs->str);
		g_string_free(errs, TRUE);
	}

	return entry;
}

LJEntry**
lj_entries_new_from_result(LJResult *result, int count,
                           GSList **warnings) {
	LJEntry **entries;
	GHashTable *entryhash;
	LJEntry *entry = NULL;
	int i, itemid;
	char *pname, *pvalue;
	GError *tmperr = NULL;

	entries = g_new0(LJEntry*, count);
	entryhash = g_hash_table_new(g_int_hash, g_int_equal);

	for (i = 0; i < count; i++) {
		entry = lj_entry_new_from_result(result, i+1);
		g_hash_table_insert(entryhash, &entry->itemid, entry);
		entries[i] = entry;
	}

	count = lj_result_get_int(result, "prop_count");
	for (i = 1; i <= count; i++) {
		itemid = lj_result_getf_int(result, "prop_%d_itemid", i);
		pname  = lj_result_getf(result,     "prop_%d_name",   i);
		pvalue = lj_result_getf(result,     "prop_%d_value",  i);
		entry = g_hash_table_lookup(entryhash, &itemid);

		if (!lj_entry_load_metadata(entry, pname, pvalue, &tmperr)) {
			if (warnings)
				*warnings = g_slist_append(*warnings,
						g_strdup_printf("Entry %d: %s", itemid, tmperr->message));
			g_error_free(tmperr);
			tmperr = NULL;
		}
	}
	g_hash_table_destroy(entryhash);

	return entries;
}

#ifdef HAVE_LIBXML
static void
lj_entry_load_from_xml_node(LJEntry *entry, xmlDocPtr doc, xmlNodePtr node) {
	xmlChar *itemid;
	xmlNodePtr cur;

	if ((itemid = xmlGetProp(node, BAD_CAST "itemid")) != NULL) {
		entry->itemid = atoi((char*)itemid);
		xmlFree(itemid);
	}
	cur = node->xmlChildrenNode;
	while (cur != NULL) {
		XML_ENTRY_META_GET(subject)
		else XML_ENTRY_META_GET(event)
		else if (xmlStrcmp(cur->name, BAD_CAST "mood") == 0) {
			xmlChar *id;
			entry->mood = (char*)xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (entry->mood && strlen(entry->mood) == 0) {
				g_free(entry->mood); entry->mood = NULL;
			}
			if ((id = xmlGetProp(cur, BAD_CAST "type")) != NULL) {
				entry->moodid = atoi((char*)id);
				xmlFree(id);
			}
		}
		else XML_ENTRY_META_GET(music)
		else XML_ENTRY_META_GET(location)
		else XML_ENTRY_META_GET(taglist)
		else XML_ENTRY_META_GET(pickeyword)
		else if (xmlStrcmp(cur->name, BAD_CAST "preformatted") == 0) {
			entry->preformatted = TRUE;
		} else if (xmlStrcmp(cur->name, BAD_CAST "backdated") == 0) {
			entry->backdated = TRUE;
		} else if (xmlStrcmp(cur->name, BAD_CAST "comments") == 0) {
			xmlChar *type;
			if ((type = xmlGetProp(cur, BAD_CAST "type")) != NULL) {
				if (xmlStrcmp(type, BAD_CAST "noemail") == 0) {
					entry->comments = LJ_COMMENTS_NOEMAIL;
				} else if (xmlStrcmp(type, BAD_CAST "disable") == 0) {
					entry->comments = LJ_COMMENTS_DISABLE;
				}
				xmlFree(type);
			}
		} else if (xmlStrcmp(cur->name, BAD_CAST "time") == 0) {
			xmlChar *date = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			lj_ljdate_to_tm((char*)date, &entry->time);
			g_free(date);
		} else if (xmlStrcmp(cur->name, BAD_CAST "security") == 0) {
			xmlChar *type, *mask;
			type = xmlGetProp(cur, BAD_CAST "type");
			mask = xmlGetProp(cur, BAD_CAST "allowmask");
			lj_security_from_strings(&entry->security,
			                         (char*)type, (char*)mask);
			xmlFree(type);
			xmlFree(mask);
		}

		cur = cur->next;
	}
	/* http://www.livejournal.com/community/logjam/113710.html */
	if (!entry->subject)
		entry->subject = g_strdup("");
	if (!entry->event)
		entry->event = g_strdup("");
}

LJEntry*
lj_entry_new_from_xml_node(void* doc, void* node) {
	LJEntry *entry;
	entry = lj_entry_new();
	lj_entry_load_from_xml_node(entry, doc, node);
	return entry;
}

static gboolean
lj_entry_load_from_xml(LJEntry *entry, const char *data, int len, GError **err) {
	xmlNodePtr cur;
	xmlDocPtr  doc = NULL;
	xmlParserCtxtPtr ctxt;

	ctxt = xmlCreatePushParserCtxt(NULL, NULL,
				data, 4,
				NULL /* XXX why does this want a filename? */);
	/* suppress error messages */
	ctxt->sax->warning = NULL;
	ctxt->sax->error   = NULL;

	xmlParseChunk(ctxt, data+4, len-4, 0);
	xmlParseChunk(ctxt, data, 0, 1);
	if (!ctxt->errNo)
		doc = ctxt->myDoc;

	xmlFreeParserCtxt(ctxt);

	if (!doc) {
		/* XXX better error message. */
		g_set_error(err, 0, 0, "Error parsing XML");
		return FALSE;
	}

	cur = xmlDocGetRootElement(doc);
	lj_entry_load_from_xml_node(entry, doc, cur);
	xmlFreeDoc(doc);
	return TRUE;
}
#endif /* HAVE_LIBXML */

static gboolean
rfc822_get_keyval(const char **bufp, char *key, char *val) {
	const char *buf = *bufp;
	const char *p = buf;

	/* move p up to the colon after the key */
	while (*p != ':' && (p-buf < 100)) {
		if (*p == 0) return FALSE;
		if (*p == '\n') return FALSE;
		p++;
	}
	memcpy(key, buf, p-buf);
	key[p-buf] = 0;

	/* move p up to the head of the value */
	p++;
	while (*p == ' ') {
		if (*p == 0) return FALSE;
		p++;
	}
	/* scoot buf up to the newline, so the
	 * value lies between p and buf */
	buf = p;
	while (*buf != 0 && *buf != '\n' && (buf-p < 1000))
		buf++;
	memcpy(val, p, buf-p);
	val[buf-p] = 0;

	if (*buf != 0)
		*bufp = buf + 1;
	else
		*bufp = buf;
	return TRUE;
}

static gboolean
rfc822_load_entry(const char *key, const char *val, LJEntry *entry) {
	/* we check val[0] below so we don't load up a bunch of empty fields. */
#define RFC822_GET(A)                        \
    if (g_ascii_strcasecmp(key, #A) == 0) {  \
        if (entry && val[0]) entry->A = g_strdup(val); \
    }

	RFC822_GET(subject)
	else RFC822_GET(mood) /* XXX id */
	else RFC822_GET(music)
	else RFC822_GET(location)
	else RFC822_GET(taglist)
	else RFC822_GET(pickeyword)
	else if (g_ascii_strcasecmp(key, "time") == 0) {
		if (entry) lj_ljdate_to_tm(val, &entry->time);
	}
	else if (g_ascii_strcasecmp(key, "backdated") == 0) {
		if (entry && val[0]) {
			if (g_ascii_strcasecmp(val, "yes") == 0)
				entry->backdated = TRUE;
		}
	}
	else return FALSE;

	return TRUE;
}

static gboolean
probe_rfc822(const char *data) {
	char key[1024], val[1024];

	/* if we don't get a keyval on the first line,
	 * we know we're not reading an rfc822 file. */
	if (!rfc822_get_keyval(&data, key, val))
		return FALSE;
	/* if it's not a valid keyval,
	 * we know we're not reading an rfc822 file. */
	if (!rfc822_load_entry(key, val, NULL))
		return FALSE;

	return TRUE;
}

static void
lj_entry_load_from_rfc822(LJEntry *entry, const char *data, int len) {
	const char *buf = data;
	char key[1024], val[1024];

	while (rfc822_get_keyval(&buf, key, val)) {
		rfc822_load_entry(key, val, entry);
	}

	/* buf is only advanced when we successfully get a keyval,
	 * so the first time we don't we're at the text of the entry. */
	while (*buf == '\n')
		buf++;
	entry->event = g_strdup(buf);
}

static void
lj_entry_load_from_plain(LJEntry *entry, const char *data, int len) {
	entry->event = g_strdup(data);
}

static gboolean
lj_entry_load_from_autodetect(LJEntry *entry, LJEntryFileType *typeret, const char *data, int len, GError **err) {
#ifdef HAVE_LIBXML
	if (lj_entry_load_from_xml(entry, data, len, err)) {
		*typeret = LJ_ENTRY_FILE_XML;
		return TRUE;
	}
#endif /* HAVE_LIBXML */
	g_clear_error(err);
	if (probe_rfc822(data)) {
		lj_entry_load_from_rfc822(entry, data, len);
		*typeret = LJ_ENTRY_FILE_RFC822;
		return TRUE;
	}

	if (typeret)
		*typeret = LJ_ENTRY_FILE_PLAIN;
	lj_entry_load_from_plain(entry, data, len);
	return TRUE;
}

static gboolean
lj_entry_load(LJEntry *entry, gchar *data, gsize len,
           LJEntryFileType type, LJEntryFileType *typeret, GError **err) {
	switch (type) {
#ifdef HAVE_LIBXML
		case LJ_ENTRY_FILE_XML:
			return lj_entry_load_from_xml(entry, data, len, err);
#endif /* HAVE_LIBXML */
		case LJ_ENTRY_FILE_RFC822:
			lj_entry_load_from_rfc822(entry, data, len);
			return TRUE;
		case LJ_ENTRY_FILE_PLAIN:
			lj_entry_load_from_plain(entry, data, len);
			return TRUE;
		case LJ_ENTRY_FILE_AUTODETECT:
		default:
			return lj_entry_load_from_autodetect(entry, typeret, data, len, err);
	}
}

#ifdef HAVE_LIBXML
gboolean
lj_entry_to_xml_file(LJEntry *entry, const char *filename, GError **err) {
	int ret;
	xmlDocPtr doc;
	
	doc = lj_entry_to_xml(entry);
	ret = xmlSaveFormatFileEnc(filename, doc, "utf-8", TRUE);
	xmlFreeDoc(doc);
	
	if (ret < 0) {
		/* XXX um, is there no way to get a more reasonable error message
		 * out of libxml? */
		g_set_error(err, 0, 0, "Unable to save to file '%s'.", filename);
		return FALSE;
	}
	return TRUE;
}
#endif /* HAVE_LIBXML */

/* the do/while kludge is to allow this macro to come in a blockless if */
#define _FILE_ERROR_THROW(err, message)                          \
	{                                                            \
		g_set_error(err, G_FILE_ERROR,                           \
				g_file_error_from_errno(errno), message);       \
		return 0; /* works both as NULL and as FALSE, luckily */ \
	}

static void
append_field(GString *str, const char *name, const char *value, gboolean includeempty) {
	if (value || includeempty)
		g_string_append_printf(str, "%s: %s\n", name, value ? value : "");
}

GString*
lj_entry_to_rfc822(LJEntry *entry, gboolean includeempty) {
	GString *str = g_string_new(NULL);

	if (entry->time.tm_year) {
		char *ljdate;
		ljdate = lj_tm_to_ljdate(&entry->time);
		g_string_printf(str, "Time: %s\n", ljdate);
		g_free(ljdate);
	}
	
	/* XXX: add all other metadata fields */
	append_field(str, "Subject", entry->subject, includeempty);
	append_field(str, "Mood", entry->mood, includeempty);
	append_field(str, "Music", entry->music, includeempty);
	append_field(str, "Location", entry->location, includeempty);
	append_field(str, "TagList", entry->taglist, includeempty);
	append_field(str, "PicKeyword", entry->pickeyword, includeempty);
	g_string_append(str, "\n");
	if (entry->event)
		g_string_append(str, entry->event);

	return str;
}

gboolean
lj_entry_to_rfc822_file(LJEntry *entry, const char *filename, gboolean includeempty, GError **err) {
	GString *str;
	FILE *f = fopen(filename, "wb");
	if (!f)
		_FILE_ERROR_THROW(err, _("can't open rfc822 file"));

	str = lj_entry_to_rfc822(entry, includeempty);
	fprintf(f, "%s", str->str);
	g_string_free(str, TRUE);
	
	if (fclose(f) != 0)
		_FILE_ERROR_THROW(err, _("can't close rfc822 file"));
	
	return TRUE;
}

static LJEntry* lj_entry_from_user_editor(const char *filename, GError **err);

/* Note how unlike the entry_from_* functions, this acts on an out
 * variable containing an *existing* LJEntry. see entry_from_user_editor
 * for the lower-level service of forking off an editor on a file and
 * constructing an LJEntry as a result. */
gboolean
lj_entry_edit_with_usereditor(LJEntry *entry, const char *basepath, GError **err) {
	gboolean ret = FALSE;
#ifndef G_OS_WIN32
	char *filename = NULL;
	LJEntry *tmp_entry = NULL;
	gint editfh;
	
	filename = g_build_filename(basepath, "logjam-edit-XXXXXX", NULL);
	editfh = g_mkstemp(filename);
	if (!editfh) {
		g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno),
				_("Can't make temp file: %s"), filename);
		goto err;
	}

	/* we're about to open the same file again */
	if (close(editfh) != 0) {
		g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno),
				_("Can't close temp file: %s"), filename);
		goto err;
	}

	if (!lj_entry_to_rfc822_file(entry, filename, TRUE, err)) {
		goto err; /* err has already been set */
	}

	tmp_entry = lj_entry_from_user_editor(filename, err);
	if (!tmp_entry) {
		goto err; /* err has already been set */
	}
	memcpy(entry, tmp_entry, sizeof(LJEntry)); /* the ol' switcheroo */
	g_free(tmp_entry);                       /* and _not_ entry_free */

	unlink(filename);
	ret = TRUE;

err:
	g_free(filename);
#endif
	return ret;
}

static LJEntry*
lj_entry_from_user_editor(const char *filename, GError **err) {
#ifndef G_OS_WIN32
	gint   pid;
	LJEntry *entry;
	
	/* g_spawn* would do no good: it disassociates the tty. viva fork! */
	pid = fork();
	if (pid < 0) {                 /* fork error */
		g_set_error(err, G_SPAWN_ERROR, G_SPAWN_ERROR_FORK,
				g_strerror(errno));
		return NULL;
	}

	if (pid == 0) {                /* child */
		gchar *editor =
			(getenv("VISUAL") ? getenv("VISUAL") :
			 getenv("EDITOR") ? getenv("EDITOR") : "vi");
		execlp(editor, editor, filename, NULL);
		_exit(0);
	}

	/* parent */
	if (wait(NULL) != pid) {
		g_set_error(err, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED,
				g_strerror(errno));
		return NULL;
	}

	if (!(entry = lj_entry_new_from_filename(filename, LJ_ENTRY_FILE_RFC822, NULL, err))) {
		return NULL; /* err has already been already set; propagate it */
	}
	
	return entry;
	
#else
	g_warning("external editor option not supported on Win32 yet. Sorry.\n");
	return NULL;
#endif /* ndef G_OS_WIN32 */
}

gint
lj_entry_get_itemid(LJEntry *e) {
	return e->itemid;
}

gint
lj_entry_get_anum(LJEntry *e) {
    return e->anum;
}

void
lj_entry_get_time(LJEntry *e, struct tm *time) {
	memcpy(time, &e->time, sizeof(struct tm));
}
