/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#ifdef HAVE_GTK
#include "gtk-all.h"
#else
#include "glib-all.h"
#endif

#ifndef G_OS_WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include "draftstore.h"
#include "conf.h"

struct _DraftStore {
	char *path;
};

gboolean
draft_store_each_header(DraftStore *ds, LJEntry *entry,
                        DraftStoreHeaderFunc func, gpointer data) {
	GDir *dir;
	const char *filename;
	char *path;
	LJEntry *e;

	dir = g_dir_open(ds->path, 0, NULL);
	if (!dir)
		return FALSE;

	for (filename = g_dir_read_name(dir);
			filename;
			filename = g_dir_read_name(dir)) {
		path = g_build_filename(ds->path, filename, NULL);
		e = lj_entry_new_from_filename(path, LJ_ENTRY_FILE_XML, NULL, NULL);
		if (e && e->time.tm_year == 0) {
			struct stat statbuf;
			stat(path, &statbuf);
			e->time = *localtime(&statbuf.st_mtime);
		}
		g_free(path);
		if (!e)
			continue;
		entry->itemid = e->itemid;
		entry->subject = e->subject; e->subject = NULL;
		entry->security = e->security;
		entry->time = e->time;
		lj_entry_free(e);
		func(ds, entry, data);
		g_free(entry->subject);
	}
	entry->subject = NULL;

	g_dir_close(dir);
	return TRUE;
}

static char*
draft_store_make_path(DraftStore *ds, int itemid) {
	char *filename, *path;
	filename = g_strdup_printf("%d", -itemid);
	path = g_build_filename(ds->path, filename, NULL);
	g_free(filename);
	return path;
}

LJEntry*
draft_store_get_entry(DraftStore *ds, int itemid, GError **err) {
	char *path;
	LJEntry *e;
	path = draft_store_make_path(ds, itemid);
	e = lj_entry_new_from_filename(path, LJ_ENTRY_FILE_XML, NULL, NULL);
	g_free(path);
	return e;
}

gboolean
draft_store_put_entry(DraftStore *ds, LJEntry *entry, GError **err) {
	char *path;
	gboolean ret;
	if (!verify_path(ds->path, TRUE, err))
		return FALSE;
	path = draft_store_make_path(ds, entry->itemid);
	ret = lj_entry_to_xml_file(entry, path, NULL);
	g_free(path);
	return ret;
}

gboolean
draft_store_remove_entry(DraftStore *ds, int itemid, GError **err) {
	char *path;
	path = draft_store_make_path(ds, itemid);
	if (unlink(path) < 0)
		return FALSE;
	return TRUE;
}

int
draft_store_find_itemid(DraftStore *ds) {
	size_t pathlen;
	int itemid;
	char *pathbuf;
	struct stat statbuf;

	pathlen = strlen(ds->path);
	pathbuf = g_new0(char, pathlen+5);
	strcpy(pathbuf, ds->path);
	for (itemid = 1; itemid < 100; itemid++) {
		g_snprintf(pathbuf+pathlen, 5, "/%d", itemid);
		if (stat(pathbuf, &statbuf) < 0 && errno == ENOENT) {
			break;
		}
	}
	g_free(pathbuf);
	if (itemid == 100)
		return 0;
	return -itemid;
}

DraftStore*
draft_store_new(JamAccount *acc) {
	DraftStore *ds;

	ds = g_new0(DraftStore, 1);
	ds->path = conf_make_account_path(acc, "drafts");

	return ds;
}

void
draft_store_free(DraftStore *ds) {
	g_free(ds->path);
	g_free(ds);
}

