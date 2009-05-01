/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2005 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "glib-all.h"
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifndef G_OS_WIN32
#include <unistd.h>
#endif

#ifndef G_OS_WIN32
#include <netinet/in.h>
#else
#include <winsock2.h>
#endif

#include <libxml/parser.h>

#include "journalstore.h"

#include "conf.h"
#include "jam_xml.h"

#define JOURNAL_STORE_INDEX_VERSION 3
#define JOURNAL_STORE_XML_VERSION 2

struct _JournalStore {
	JamAccount *account;

	char *path;
	/* the index is a flat array of itemid->time_t.
	 * it's small enough to scan through when
	 * we need to do a reverse lookup. */
	GArray *index;

	/* has the file format changed in such a way that we need to resync? */
	gboolean invalid;

	/* we cache the "current" xml doc around in memory,
	 * because we usually want to grab multiple entries from
	 * one document. */
	xmlDocPtr xml_doc; int xml_year, xml_mon; gboolean xml_dirty;
};

JamAccount*
journal_store_get_account(JournalStore *js) {
	return js->account;
}

#define index_at(idx, i) g_array_index(idx, time_t, i)
#define index_get(idx, i) ntohl(index_at(idx, i))
#define index_set(idx, i, val) index_at(idx, i) = htonl(val)

static gboolean
index_load(JournalStore *js, GError **err) {
	char *path;
	struct stat statbuf;
	FILE *f;
	int itemcount;
	int ret;
	guint32 ver;

	path = g_build_filename(js->path, "index", NULL);

	if (stat(path, &statbuf) < 0 && errno == ENOENT) {
		g_free(path);
		return TRUE;
	}

	f = fopen(path, "rb");
	g_free(path);
	if (f == NULL) {
		g_print("XXX index fopen: %s\n", g_strerror(errno));
		return FALSE;
	}

	itemcount = statbuf.st_size / sizeof(time_t);
	g_array_set_size(js->index, itemcount);
	ret = (int)fread(js->index->data, sizeof(time_t), itemcount, f);
	if (ret < itemcount) {
		g_print("XXX index fread read too little\n");
		return FALSE;
	}

	ver = index_get(js->index, 0);
	if ((ver & 0xFF000000) != 0) {
		/* byte order is messed up in older versions.  fix it here.  yuck. */
		int i;
		for (i = 0; i < itemcount; i++) {
			guint32 v = index_at(js->index, i);
			index_at(js->index, i) =
				((v & 0x000000FF) << 24) |
				((v & 0x0000FF00) <<  8) |
			    ((v & 0x00FF0000) >>  8) |
				((v & 0xFF000000) >> 24);
		}
		ver = index_get(js->index, 0);
	}
		
	if (ver < JOURNAL_STORE_INDEX_VERSION) {
		/* file format somehow changed.  clear the index. */
		g_array_set_size(js->index, 1);
		index_set(js->index, 0, JOURNAL_STORE_INDEX_VERSION);
		js->invalid = TRUE;
	}

	return TRUE;
}

static gboolean
index_write(const char *storepath, GArray *idx, GError **err) {
	char *path;
	FILE *f;
	size_t wrote;

	path = g_build_filename(storepath, "index", NULL);

	f = fopen(path, "wb");
	g_free(path);
	if (f == NULL) {
		g_set_error(err, 0, 0, _("Error opening index: %s"),
				g_strerror(errno));
		return FALSE;
	}

	wrote = fwrite(idx->data, sizeof(time_t), idx->len, f);
	if (wrote < idx->len) {
		g_set_error(err, 0, 0, _("Error writing index: %s"),
				g_strerror(errno));
		fclose(f);
		return FALSE;
	}
	fclose(f);

	return TRUE;
}
static gboolean
index_save(JournalStore *js, GError **err) {
	return index_write(js->path, js->index, err);
}

static void
time_to_docidx(const time_t *entrytime, int *year, int *mon, int *day) {
	struct tm *lt;
	if (entrytime) {
		lt = gmtime(entrytime);
		*year = lt->tm_year+1900;
		*mon  = lt->tm_mon+1;
		*day  = lt->tm_mday;
	} else {
		*year = *mon = *day = 0;
	}
}
static char*
docidx_to_str(char *base, int year, int mon) {
	return g_strdup_printf("%s/%d/%02d.xml", base, year, mon);
}

static void
delete_unused_whitespace_r(xmlNodePtr node) {
	xmlNodePtr next;

	/* whitespace is significant within many nodes, like the event,
	 * but we only get isolated pure-whitespace nodes when whitespace
	 * is used alongside nodes.  all journal content is escaped, so
	 * all nodes that contain both whitespace and nodes should be
	 * stripped.  */

	for (node = node->xmlChildrenNode; node; node = next) {
		next = node->next;
		if (xmlIsBlankNode(node)) {
			xmlUnlinkNode(node);
			xmlFreeNode(node);
		} else {
			delete_unused_whitespace_r(node);
		}
	}
}

static void
delete_unused_whitespace(xmlDocPtr doc) {
	delete_unused_whitespace_r(xmlDocGetRootElement(doc));
}

static xmlDocPtr
make_new_doc(int year, int mon) {
	xmlDocPtr doc;
	xmlNodePtr node;
	jam_xmlNewDoc(&doc, &node, "entrymonth");
	jam_xmlSetIntProp(node, "version", JOURNAL_STORE_XML_VERSION);
	jam_xmlSetIntProp(node, "year", year);
	jam_xmlSetIntProp(node, "month", mon);
	return doc;
}

static gboolean
switch_xml_file(JournalStore *js, int year, int mon, GError **err) {
	char *path;
	struct stat statbuf;
	xmlDocPtr doc = NULL;

	/* are we already there? */
	if (year == js->xml_year && mon == js->xml_mon) {
		return TRUE;
	}

	/* otherwise, switch to this file. 
	 * XXX protective locking would be good. */
	/* first write out the old file, if we have one. */
	if (js->xml_year && js->xml_dirty) {
		char *tmppath;
		path = docidx_to_str(js->path, js->xml_year, js->xml_mon);
		if (!verify_path(path, FALSE, err)) {
			g_free(path);
			return FALSE;
		}
		/* write to a temp file and then rename,
		 * to avoid losing data if we die mid-write. */
		tmppath = g_strconcat(path, ".tmp", NULL);
		if (xmlSaveFormatFile(tmppath, js->xml_doc, TRUE) < 0) {
			g_set_error(err, 0, 0, _("Error writing journal xml file to %s: %s"),
					tmppath, g_strerror(errno));
			g_free(tmppath); g_free(path);
			return FALSE;
		}
		if (rename(tmppath, path) < 0) {
			g_set_error(err, 0, 0, _("Error renaming journal xml file %s to %s: %s"),
					tmppath, path, g_strerror(errno));
			g_free(tmppath); g_free(path);
			return FALSE;
		}
		g_free(tmppath);
		g_free(path);
	}

	/* then switch to the new file, if we have one. */
	if (year) {
		path = docidx_to_str(js->path, year, mon);
		if (stat(path, &statbuf) < 0 && errno == ENOENT) {
			doc = make_new_doc(year, mon);
		} else {
			int ver;
			doc = xmlParseFile(path);

			if (!jam_xmlGetIntProp(xmlDocGetRootElement(doc), "version", &ver))
				ver = 0;

			if (ver < JOURNAL_STORE_XML_VERSION) {
				/* out of date document. */
				xmlFreeDoc(doc);
				doc = make_new_doc(year, mon);
			} else {
				/* if there is any whitespace in nodes where we don't
				 * care about whitespace, libxml thinks that the whitespace
				 * was important and won't reformat it correctly.
				 * so we need to delete all the whitespace on load. */
				delete_unused_whitespace(doc);

				if (!doc) {
					g_set_error(err, 0, 0,
							_("Error parsing journal XML file %s"), path);
					g_free(path);
					return FALSE;
				}
			}
		}
		g_free(path);
		js->xml_dirty = FALSE;
	}
	js->xml_doc = doc;
	js->xml_year = year;
	js->xml_mon = mon;

	return TRUE;
}

static gboolean
switch_xml_file_from_time(JournalStore *js, time_t *entrytime, GError **err) {
	int year, mon, day;
	time_to_docidx(entrytime, &year, &mon, &day);
	return switch_xml_file(js, year, mon, err);
}
 
time_t
journal_store_lookup_entry_time(JournalStore *js, int itemid) {
	if (itemid >= (int)js->index->len)
		return 0;
	return index_get(js->index, itemid);
}

static xmlNodePtr
make_day_node(xmlDocPtr doc, int day) {
	xmlNodePtr newnode;
	newnode = xmlNewDocNode(doc, NULL, BAD_CAST "day", NULL);
	jam_xmlSetIntProp(newnode, "number", day);
	return newnode;
}

static xmlNodePtr
find_day(xmlDocPtr doc, int day, gboolean create) {
	xmlNodePtr root, node, newnode;
	xmlChar *eday;
	int fday;

	root = xmlDocGetRootElement(doc);

	for (node = root->xmlChildrenNode; node; node = node->next) {
		if ((eday = xmlGetProp(node, BAD_CAST "number")) != NULL) {
			fday = atoi((char*)eday);
			xmlFree(eday);
			if (fday == day) {
				return node; /* found it. */
			} else if (fday > day) {
				/* we didn't find it, but we know where to insert it. */
				if (create) {
					newnode = make_day_node(doc, day);
					xmlAddPrevSibling(node, newnode);
					return newnode;
				}
				return NULL;
			}
		}
	}
	if (create) {
		/* we're either the first day inserted or last day for the month. */
		newnode = make_day_node(doc, day);
		xmlAddChild(root, newnode);
		return newnode;
	}
	return NULL;
}

static xmlNodePtr
find_entry(xmlNodePtr day, int itemid) {
	xmlNodePtr node;
	xmlChar *eitemid;
	for (node = day->xmlChildrenNode; node; node = node->next) {
		if ((eitemid = xmlGetProp(node, BAD_CAST "itemid")) != NULL) {
			if (atoi((char*)eitemid) == itemid) {
				xmlFree(eitemid);
				break;
			}
			xmlFree(eitemid);
		}
	}
	return node;
}

static gboolean
remove_old(JournalStore *js, int itemid, GError **err) {
	time_t entrytime;
	int year, mon, day;
	char *path = NULL;
	struct stat statbuf;
	xmlDocPtr doc = NULL;
	xmlNodePtr nday, node;

	entrytime = journal_store_lookup_entry_time(js, itemid);
	if (entrytime == 0)
		return TRUE; /* this entry isn't in the index. */

	/* are we already there? */
	time_to_docidx(&entrytime, &year, &mon, &day);
	if (year == js->xml_year && mon == js->xml_mon) {
		/* remove this from the in-memory doc. */
		doc = js->xml_doc;
	} else {
		path = docidx_to_str(js->path, year, mon);
		if (!stat(path, &statbuf) && errno == ENOENT) {
			/* no document means there's nothing to delete. */
			g_free(path);
			return TRUE;
		}
		doc = xmlParseFile(path);
	}

	nday = find_day(doc, day, FALSE);
	if (nday) {
		/* find the entry node and remove it. */
		node = find_entry(nday, itemid);
		if (node) {
			xmlUnlinkNode(node);
			xmlFreeNode(node);
		}

		/* and delete day if it's empty. */
		if (nday->xmlChildrenNode == NULL) {
			xmlUnlinkNode(nday);
			xmlFreeNode(nday);
		}
	}

	/* if we deleted from somewhere other than the current file,
	 * we want to save it out immediately. */
	if (path) {
		xmlSaveFormatFile(path, doc, TRUE);
		xmlFreeDoc(doc);
		g_free(path);
	} else {
		js->xml_dirty = TRUE;
	}

	return TRUE;
}

gboolean
journal_store_put(JournalStore *js, LJEntry *entry, GError **err) {
	time_t entrytime;
	xmlNodePtr node, newnode;

	entrytime = lj_timegm(&entry->time);

	if (!switch_xml_file_from_time(js, &entrytime, err))
		return FALSE;

	if (!remove_old(js, entry->itemid, err))
		return FALSE;

	/* write main xml. */
	node = find_day(js->xml_doc, entry->time.tm_mday, TRUE);
	newnode = lj_entry_to_xml_node(entry, js->xml_doc);
	/* XXX sort here. */
	xmlAddChild(node, newnode);
	js->xml_dirty = TRUE;

	/* write index. */
	if (entry->itemid+1 > (int)js->index->len)
		g_array_set_size(js->index, entry->itemid+1);
	index_set(js->index, entry->itemid, entrytime);

	return TRUE;
}

gboolean
journal_store_put_group(JournalStore *js, LJEntry **entries, int c, GError **err) {
	int i;

	for (i = 0; i < c; i++) {
		if (!journal_store_put(js, entries[i], err))
			return FALSE;
	}

	if (!switch_xml_file_from_time(js, NULL, err))
		return FALSE;
	if (!index_save(js, err))
		return FALSE;
	return TRUE;
}


void
journal_store_free(JournalStore *js) {
	if (js->index) {
		index_save(js, NULL);
		g_array_free(js->index, TRUE);
	}
	g_free(js->path);
	g_free(js);
}

guint32
journal_store_get_month_entries(JournalStore *js, int year, int mon) {
	guint32 days;
	xmlNodePtr nday;
	int day;
	GError *err = NULL;

	days = 0;
	if (!switch_xml_file(js, year, mon, &err)) {
		g_warning("journalstore couldn't switch files: %s\n", err->message);
		g_error_free(err);
		return 0;
	}
	nday = xmlDocGetRootElement(js->xml_doc)->xmlChildrenNode;
	for (; nday; nday = nday->next) {
		if (jam_xmlGetIntProp(nday, "number", &day))
			days |= 1 << day;
	}
	return days;
}

static void
call_summarycb(JournalStore *js, xmlNodePtr nentry, 
		JournalStoreSummaryCallback cb_func, gpointer cb_data) {
	xmlNodePtr nchild;
	xmlChar *sitemid;
	xmlChar *event = NULL;
	xmlChar *subject = NULL;
	struct tm etm;
	int itemid = 0;
	const char *summary;
	LJSecurity sec = {0};

	for (nchild = nentry->xmlChildrenNode; nchild; nchild = nchild->next) {
		if (xmlStrcmp(nchild->name, BAD_CAST "event") == 0) {
			event = xmlNodeListGetString(js->xml_doc,
					nchild->xmlChildrenNode, TRUE);
			sitemid = xmlGetProp(nentry, BAD_CAST "itemid");
			itemid = 0;
			if (sitemid) {
				itemid = atoi((char*)sitemid);
				xmlFree(sitemid);
			}
		} else if (xmlStrcmp(nchild->name, BAD_CAST "time") == 0) {
			lj_ljdate_to_tm((const char*)XML_GET_CONTENT(nchild->xmlChildrenNode), &etm);
		} else if (xmlStrcmp(nchild->name, BAD_CAST "subject") == 0) {
			subject = xmlNodeListGetString(js->xml_doc,
					nchild->xmlChildrenNode, TRUE);
		} else if (xmlStrcmp(nchild->name, BAD_CAST "security") == 0) {
			xmlChar *type = NULL, *mask = NULL;
			type = xmlGetProp(nchild, BAD_CAST "type");
			mask = xmlGetProp(nchild, BAD_CAST "mask");
			lj_security_from_strings(&sec, (char*)type, (char*)mask);
			if (type) xmlFree(type);
			if (mask) xmlFree(mask);
		}
	}

	summary = lj_get_summary((char*)subject, (char*)event);
	cb_func(itemid, lj_timegm(&etm), summary, &sec, cb_data);
	xmlFree(event);
	if (subject) {
		xmlFree(subject);
		subject = NULL;
	}
}

gboolean
journal_store_get_day_entries(JournalStore *js, int year, int mon, int day,
		JournalStoreSummaryCallback cb_func, gpointer cb_data) {
	xmlNodePtr nday, nentry;
	
	switch_xml_file(js, year, mon, NULL);
	nday = find_day(js->xml_doc, day, FALSE);

	if (!nday) /* no entries today. */
		return TRUE;

	for (nentry = nday->xmlChildrenNode; nentry; nentry = nentry->next)
		call_summarycb(js, nentry, cb_func, cb_data);

	return TRUE;
}

typedef struct {
	JournalStoreScanCallback scan_cb;
	gpointer scan_data;
	JournalStoreSummaryCallback summary_cb;
	gpointer summary_data;
	int matchcount;
} Scan;

static gboolean
match_node(JournalStore *js, xmlNodePtr node, const Scan *scan) {
	xmlChar *content;
	gboolean found;

	content = xmlNodeListGetString(js->xml_doc, node->xmlChildrenNode, TRUE);
	found = scan->scan_cb((const char*)content, scan->scan_data);
	xmlFree(content);
	return found;
}

static gboolean
match_entry(JournalStore *js, xmlNodePtr nentry, const Scan *scan) {
	xmlNodePtr nchild;
	gboolean matched;

	nchild = nentry->xmlChildrenNode;
	matched = FALSE;
	for ( ; nchild; nchild = nchild->next) {
		if (xmlStrcmp(nchild->name, BAD_CAST "event") == 0)
			matched = matched || match_node(js, nchild, scan);
		else if (xmlStrcmp(nchild->name, BAD_CAST "subject") == 0)
			matched = matched || match_node(js, nchild, scan);
	}
	return matched;
}

static gboolean
scan_month(JournalStore *js, int year, int month, Scan *scan) {
	xmlNodePtr nday, nentry;
	if (!switch_xml_file(js, year, month, NULL))
		return FALSE;

	nday = xmlDocGetRootElement(js->xml_doc)->xmlChildrenNode;
	for (; nday; nday = nday->next) {
		for (nentry = nday->xmlChildrenNode; nentry; nentry = nentry->next) {
			if (match_entry(js, nentry, scan)) {
				call_summarycb(js, nentry,
						scan->summary_cb, scan->summary_data);
				if (++scan->matchcount == MAX_MATCHES)
					return FALSE;
			}
		}
	}
	return TRUE;
}

gboolean
journal_store_scan(JournalStore *js,
                   JournalStoreScanCallback scan_cb, gpointer scan_data,
                   JournalStoreSummaryCallback cb_func, gpointer cb_data) {
	GDir *journaldir, *yeardir;
	const char *yearname;
	char *yearpath;
	int year, month;
	Scan scan = {
		scan_cb, scan_data,
		cb_func, cb_data,
		0
	};

	journaldir = g_dir_open(js->path, 0, NULL);
	if (!journaldir)
		return FALSE;

	yearname = g_dir_read_name(journaldir);
	while (yearname && scan.matchcount < MAX_MATCHES) {
		yearpath = g_build_filename(js->path, yearname, NULL);
		yeardir = g_dir_open(yearpath, 0, NULL);
		g_free(yearpath);

		if (yeardir) {
			year = atoi(yearname);
			for (month = 1; month <= 12; month++) {
				if (!scan_month(js, year, month, &scan))
					break;
			}
			g_dir_close(yeardir);
		}
		yearname = g_dir_read_name(journaldir);
	}
	g_dir_close(journaldir);

	return TRUE;
}


gboolean
journal_store_find_relative_by_time(JournalStore *js, time_t when,
		int *ritemid, int dir, GError *err) {
	time_t candidate;
	gint i;

	int fitemid = 0;
	time_t ftime = 0;

	/* XXX need to handle items with same date. */
	if (dir < 0) {
		for (i = js->index->len-1; i >= 1; i--) {
			if (index_get(js->index, i) == when)
				continue; /* skip self */
			candidate = journal_store_lookup_entry_time(js, i);
			if (candidate < when && candidate > ftime) {
				ftime = candidate;
				fitemid = i;
			}
		}
	} else if (dir > 0) {
		for (i = 1; i < (int)js->index->len; i++) {
			if (index_get(js->index, i) == when)
				continue; /* skip self */
			candidate = journal_store_lookup_entry_time(js, i);
			if (candidate > when) {
				if (!ftime || (ftime && candidate < ftime)) {
					ftime = candidate;
					fitemid = i;
				}
			}
		}
	}
	
	if (fitemid) {
		*ritemid = fitemid;
		return TRUE;
	}
	return FALSE;
}

LJEntry *
journal_store_get_entry(JournalStore *js, int itemid) {
	time_t entrytime;
	xmlNodePtr nday, nentry;
	struct tm *etm;
	GError *err = NULL;

	entrytime = journal_store_lookup_entry_time(js, itemid);
	if (!switch_xml_file_from_time(js, &entrytime, &err)) {
		g_warning("journalstore couldn't switch files: %s\n", err->message);
		g_error_free(err);
		return NULL;
	}

	etm = gmtime(&entrytime);
	nday = find_day(js->xml_doc, etm->tm_mday, FALSE);
	if (nday) {
		nentry = find_entry(nday, itemid);
		if (nentry)
			return lj_entry_new_from_xml_node(js->xml_doc, nentry);
	}
	return NULL;
}

int
journal_store_get_latest_id(JournalStore *js) {
	int itemid;
	for (itemid = js->index->len-1; itemid; itemid--)
		if (journal_store_lookup_entry_time(js, itemid))
			return itemid;
	return 0; /* no non-deleted messages in store */
}

int
journal_store_get_count(JournalStore *js) {
	int itemid, count = 0;
	for (itemid = 0; itemid < (int)js->index->len; itemid++)
		if (index_at(js->index, itemid) != 0)
			count++;
	return count;
}

gboolean
journal_store_get_invalid(JournalStore *js) {
	return js->invalid;
}

static char *
journal_store_make_path(JamAccount *acc) {
	return conf_make_account_path(acc, "journal");
}

JournalStore*
journal_store_open(JamAccount *acc, gboolean create, GError **err) {
	JournalStore *js;
	struct stat statbuf;

	js = g_new0(JournalStore, 1);
	js->account = acc;
	js->path = journal_store_make_path(acc);
	js->index = g_array_new(FALSE, TRUE, sizeof(time_t));

	if (!create && !g_file_test(js->path, G_FILE_TEST_EXISTS)) {
		g_set_error(err, 0, 0, _("No offline copy of this journal."));
		goto err;
	}

	/* need at least one slot for the version.
	 * if the on-disk index is an older version,
	 * loading the index will overwrite this version anyway. */
	g_array_set_size(js->index, 1);
	index_set(js->index, 0, JOURNAL_STORE_INDEX_VERSION);

	if (!verify_path(js->path, TRUE, err))
		goto err;

	if (!index_load(js, err))
		goto err;

	return js;

err:
	journal_store_free(js);
	return NULL;
}

static void
reindex_month(char *storepath, int year, int mon, GArray *idx) {
	char *xmlpath;
	xmlDocPtr doc;
	xmlNodePtr nday, nentry, nchild;
	struct tm etm;

	xmlpath = docidx_to_str(storepath, year, mon);
	doc = xmlParseFile(xmlpath);
	if (!doc)
		return;

	nday = xmlDocGetRootElement(doc)->xmlChildrenNode;
	for (; nday; nday = nday->next) {
		for (nentry = nday->xmlChildrenNode; nentry; nentry = nentry->next) {
			xmlChar *sitemid;
			int itemid = 0;

			if ((sitemid = xmlGetProp(nentry, BAD_CAST "itemid")) != NULL) {
				itemid = atoi((char*)sitemid);
				xmlFree(sitemid);
			}
			for (nchild = nentry->xmlChildrenNode; nchild; nchild = nchild->next) {
				if (xmlStrcmp(nchild->name, BAD_CAST "time") == 0) {
					lj_ljdate_to_tm((const char*)XML_GET_CONTENT(nchild->xmlChildrenNode),
							&etm);
					if (itemid > 0) {
						if (itemid+1 > (int)idx->len)
							g_array_set_size(idx, itemid+1);
						index_set(idx, itemid, lj_timegm(&etm));
					}
					break;
				}
			}
		}
	}
	xmlFreeDoc(doc);
}

gboolean
journal_store_reindex(JamAccount *acc, GError **err) {
	char *storepath;
	GDir *journaldir, *yeardir;
	const char *yearname, *monthname;
	char *yearpath;
	int year, month;
	GArray *index;
	gboolean ret;

	storepath = journal_store_make_path(acc);

	/* ick, duplication of the store code. */
	journaldir = g_dir_open(storepath, 0, NULL);
	if (!journaldir)
		return FALSE;

	index = g_array_new(FALSE, TRUE, sizeof(time_t));
	g_array_set_size(index, 1);
	index_set(index, 0, JOURNAL_STORE_INDEX_VERSION);

	yearname = g_dir_read_name(journaldir);
	while (yearname) {
		yearpath = g_build_filename(storepath, yearname, NULL);
		yeardir = g_dir_open(yearpath, 0, NULL);
		g_free(yearpath);

		if (yeardir) {
			year = atoi(yearname);

			monthname = g_dir_read_name(yeardir);
			while (monthname) {
				month = atoi(monthname);
				reindex_month(storepath, year, month, index);
				monthname = g_dir_read_name(yeardir);
			}
			g_dir_close(yeardir);
		}
		yearname = g_dir_read_name(journaldir);
	}
	g_dir_close(journaldir);

	ret = index_write(storepath, index, err);
	g_free(storepath);
	g_array_free(index, TRUE);

	return ret;
}

