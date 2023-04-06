/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2005 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "glib-all.h"

#include <sqlite3.h>

#include "conf.h"
#include "journalstore.h"
#include "util.h"

struct _JournalStore {
	JamAccount *account;
	sqlite3 *db;
};

#define SCHEMA "\
CREATE TABLE meta (\n\
	schemaver INTEGER\n\
);\n\
CREATE TABLE entry (\n\
	itemid INTEGER PRIMARY KEY,\n\
	anum INTEGER,\n\
	subject STRING,\n\
	event STRING,\n\
	moodid INTEGER, mood STRING, music STRING, taglist STRING, \n\
	pickeyword STRING, preformatted INTEGER, backdated INTEGER, \n\
	comments INTEGER, year INTEGER, month INTEGER, day INTEGER, \n\
	timestamp INTEGER, security INTEGER\n\
);\n\
CREATE INDEX dateindex ON entry (year, month, day);\n\
CREATE INDEX timeindex ON entry (timestamp);"

/* a number we can tweak if we change the schema. */
#define SCHEMA_VERSION 1
/* this can be done with preprocessor tricks but this works just as well. */
#define SCHEMA_VERSION_STRING "1"

#define SQLCHECK(s) if (SQLITE_OK != s) { g_warning("FIXME: sqlite error %d in " #s ".\n", s); }

static void
report_sqlite_error(sqlite3 *db, int ret, GError **err) {
	g_set_error(err, 0, 0, "FIXME sqlite error %d: %s", ret, sqlite3_errmsg(db));
}

static void
sql_trace(void *data, const char *sql) {
	/* g_print("SQL> %s\n", sql); */
}

static gboolean
begin_t(sqlite3 *db, GError **err) {
	int ret;
	ret = sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
	if (ret != SQLITE_OK) {
		report_sqlite_error(db, ret, err);
		return FALSE;
	}
	return TRUE;
}
static gboolean
end_t(sqlite3 *db, GError **err) {
	int ret;
	ret = sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, NULL);
	if (ret != SQLITE_OK) {
		report_sqlite_error(db, ret, err);
		return FALSE;
	}
	return TRUE;
}

static gboolean
init_db(sqlite3 *db, GError **err) {
	int ret;

	if (!begin_t(db, err))
		return FALSE;

	ret = sqlite3_exec(db, SCHEMA, NULL, NULL, NULL);
	if (ret != SQLITE_OK) {
		report_sqlite_error(db, ret, err);
		return FALSE;
	}
	ret = sqlite3_exec(db, "INSERT INTO meta (schemaver) "
			           "VALUES (" SCHEMA_VERSION_STRING ")",
	                   NULL, NULL, NULL);
	if (ret != SQLITE_OK) {
		report_sqlite_error(db, ret, err);
		return FALSE;
	}

	if (!end_t(db, err))
		return FALSE;

	return TRUE;
}

static gboolean
check_version(sqlite3 *db) {
	int ret, ver;
	sqlite3_stmt *stmt = NULL;

	SQLCHECK(sqlite3_prepare(db,
		"SELECT schemaver FROM meta",
		-1, &stmt, NULL));

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_ROW) {
		SQLCHECK(ret);
		SQLCHECK(sqlite3_finalize(stmt));
		return FALSE;
	}

	ver = sqlite3_column_int(stmt, 0);
	SQLCHECK(sqlite3_finalize(stmt));

	return ver == SCHEMA_VERSION;
}

JournalStore* journal_store_open(JamAccount *acc, gboolean create, GError **err) {
	JournalStore *js = NULL;
	char *path = NULL;
	sqlite3 *db = NULL;
	int ret;
	gboolean exists;

	path = conf_make_account_path(acc, "journal.db");
	exists = g_file_test(path, G_FILE_TEST_EXISTS);

	if (!exists && !create) {
		g_set_error(err, 0, 0, "No offline copy of this journal.");
		goto out;
	}

	if (!verify_path(path, FALSE, &err))
		goto out;

	ret = sqlite3_open(path, &db);
	if (ret != SQLITE_OK) {
		g_set_error(err, 0, 0, "sqlite error %d: %s", ret, sqlite3_errmsg(db));
		goto out;
	}

	sqlite3_trace(db, sql_trace, NULL);

	if (exists) {
		if (!check_version(db)) {
			g_set_error(err, 0, 0,
					"The on-disk journal version differs from "
					"the version understood by this version of LogJam.  "
					"You need to resynchronize your journal.");
			goto out;
		}
	} else {
		if (!init_db(db, err))
			goto out;
	}

	js = g_new0(JournalStore, 1);
	js->account = acc;
	js->db = db;

out:
	g_free(path);
	if (!js && db) sqlite3_close(db);
	return js;
}
void
journal_store_free(JournalStore *js) {
	sqlite3_close(js->db);
	g_free(js);
}

/* stubs */
gboolean journal_store_reindex(JamAccount *acc, GError **err) { return TRUE; }
gboolean journal_store_get_invalid(JournalStore *js) { return FALSE; }

int
journal_store_get_count(JournalStore *js) {
	int ret;
	sqlite3_stmt *stmt = NULL;

	SQLCHECK(sqlite3_prepare(js->db, "SELECT COUNT(*) FROM entry", -1,
	                         &stmt, NULL));
	ret = sqlite3_step(stmt);
	if (ret != SQLITE_ROW) {
		SQLCHECK(ret);
		SQLCHECK(sqlite3_finalize(stmt));
		return -1;
	}

	ret = sqlite3_column_int(stmt, 0);
	SQLCHECK(sqlite3_finalize(stmt));
	return ret;
}

static char*
dup_col_or_null(sqlite3_stmt *stmt, int col) {
	if (sqlite3_column_type(stmt, col) != SQLITE_NULL)
		return g_strdup((char*)sqlite3_column_text(stmt, col));
	return NULL;
}

static void
security_from_int(LJSecurity *security, guint32 value) {
	if (value == 0) {
		security->type = LJ_SECURITY_PRIVATE;
	} else if (value == 1) {
		security->type = LJ_SECURITY_FRIENDS;
	} else if (value > 1) {
		security->type = LJ_SECURITY_CUSTOM;
		security->allowmask = value;
	}
}

LJEntry*
journal_store_get_entry(JournalStore *js, int get_itemid) {
	LJEntry *entry;
	sqlite3_stmt *stmt = NULL;
	int ret;
	time_t timestamp;

	SQLCHECK(sqlite3_prepare(js->db,
		"SELECT anum, subject, event, moodid, mood, " /* 0-4 */
		"music, pickeyword, preformatted, backdated, " /* 5-9 */
		"comments, timestamp, security, taglist " /* 10-13 */
		"FROM entry WHERE itemid=?1",
		-1, &stmt, NULL));
	SQLCHECK(sqlite3_bind_int(stmt, 1, get_itemid));

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_ROW) {
		SQLCHECK(sqlite3_finalize(stmt));
		return NULL;
	}

	entry = lj_entry_new();
	entry->itemid = get_itemid;
	entry->anum = sqlite3_column_int(stmt, 0);
	entry->subject = dup_col_or_null(stmt, 1);
	entry->event = dup_col_or_null(stmt, 2);
	entry->moodid = sqlite3_column_int(stmt, 3);
	entry->mood = dup_col_or_null(stmt, 4);

	entry->music = dup_col_or_null(stmt, 5);
	entry->pickeyword = dup_col_or_null(stmt, 6);
	entry->preformatted = sqlite3_column_int(stmt, 7);
	entry->backdated = sqlite3_column_int(stmt, 8);
	entry->comments = sqlite3_column_int(stmt, 9);
	entry->taglist = dup_col_or_null(stmt, 12);

	timestamp = sqlite3_column_int(stmt, 10);
	gmtime_r(&timestamp, &entry->time);

	if (sqlite3_column_type(stmt, 11) != SQLITE_NULL)
		security_from_int(&entry->security, sqlite3_column_int(stmt, 11));

	SQLCHECK(sqlite3_finalize(stmt));

	return entry;
}

int
journal_store_get_latest_id(JournalStore *js) {
	#if 0
	LJEntry *entry;
	sqlite3_stmt *stmt = NULL;
	int ret;
	SQLCHECK(sqlite3_prepare(js->db,
		"SELECT foo, bar FROM baz"
		-1, &stmt, NULL));
	SQLCHECK(sqlite3_bind_int(stmt, 0, fizz));

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_ROW) {
		SQLCHECK(sqlite3_finalize(stmt));
		return NULL;
	}
	#endif
	g_error("unimplemented");
	return -1;
}

gboolean
journal_store_find_relative_by_time(JournalStore *js, time_t when,
                                    int *ritemid, int dir, GError *err) {
	int itemid = -1;
	int ret;
	sqlite3_stmt *stmt = NULL;

	if (dir < 0) {
		SQLCHECK(sqlite3_prepare(js->db,
			"SELECT itemid FROM entry "
			"WHERE timestamp < ?1 "  /* XXX what about entries at the same time? */
			"ORDER BY timestamp DESC "
			"LIMIT 1",
			-1, &stmt, NULL));
	} else {
		SQLCHECK(sqlite3_prepare(js->db,
			"SELECT itemid FROM entry "
			"WHERE timestamp > ?1 "  /* XXX what about entries at the same time? */
			"ORDER BY timestamp ASC "
			"LIMIT 1",
			-1, &stmt, NULL));
	}

	SQLCHECK(sqlite3_bind_int(stmt, 1, when));

	ret = sqlite3_step(stmt);
	if (ret == SQLITE_ROW) {
		itemid = sqlite3_column_int(stmt, 0);
	} else if (ret =! SQLITE_DONE) {
		SQLCHECK(ret);
	}

	SQLCHECK(sqlite3_finalize(stmt));
	if (itemid >= 0) {
		*ritemid = itemid;
		return TRUE;
	}

	return FALSE;
}

time_t
journal_store_lookup_entry_time(JournalStore *js, int itemid) {
	int ret;
	sqlite3_stmt *stmt = NULL;

	SQLCHECK(sqlite3_prepare(js->db,
		"SELECT timestamp FROM entry WHERE itemid = ?1",
		-1, &stmt, NULL));
	SQLCHECK(sqlite3_bind_int(stmt, 1, itemid));

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_ROW) {
		SQLCHECK(ret);
		SQLCHECK(sqlite3_finalize(stmt));
		return 0;
	}

	ret = sqlite3_column_int(stmt, 0);
	SQLCHECK(sqlite3_finalize(stmt));
	return ret;
}
JamAccount*
journal_store_get_account(JournalStore *js) {
	return js->account;
}

gboolean journal_store_begin_group(JournalStore *js, GError **err) {
	return begin_t(js->db, err);
}
gboolean journal_store_end_group(JournalStore *js, GError **err) {
	return end_t(js->db, err);
}

gboolean
journal_store_put(JournalStore *js, LJEntry *entry, GError **err) {
	sqlite3_stmt *stmt = NULL;
	int ret;

	SQLCHECK(sqlite3_prepare(js->db,
		"INSERT OR REPLACE INTO entry (itemid, anum, subject, event, "
		"moodid, mood, music, pickeyword, preformatted, backdated, comments, "
		"taglist, year, month, day, timestamp, security) "
		"VALUES (?1, ?2, ?3, ?4, "
		"?5, ?6, ?7, ?8, ?9, ?10, ?11, "
		"?12, ?13, ?14, ?15, ?16, ?17)",
		-1, &stmt, NULL));
	SQLCHECK(sqlite3_bind_int(stmt, 1, entry->itemid));
	SQLCHECK(sqlite3_bind_int(stmt, 2, entry->anum));
	if (entry->subject)
		SQLCHECK(sqlite3_bind_text(stmt, 3, entry->subject, -1,
		                           SQLITE_TRANSIENT));
	SQLCHECK(sqlite3_bind_text(stmt, 4, entry->event, -1, SQLITE_TRANSIENT));
	if (entry->moodid)
		SQLCHECK(sqlite3_bind_int(stmt, 5, entry->moodid));
	if (entry->mood)
		SQLCHECK(sqlite3_bind_text(stmt, 6, entry->mood, -1, SQLITE_TRANSIENT));
	if (entry->music)
		SQLCHECK(sqlite3_bind_text(stmt, 7, entry->music, -1,
		                           SQLITE_TRANSIENT));
	if (entry->pickeyword)
		SQLCHECK(sqlite3_bind_text(stmt, 8, entry->pickeyword, -1,
		                           SQLITE_TRANSIENT));
	if (entry->taglist)
		SQLCHECK(sqlite3_bind_text(stmt, 12, entry->taglist, -1,
		                           SQLITE_TRANSIENT));

	if (entry->preformatted)
		SQLCHECK(sqlite3_bind_int(stmt, 9, 1));
	if (entry->backdated)
		SQLCHECK(sqlite3_bind_int(stmt, 10, 1));
	if (entry->comments)
		SQLCHECK(sqlite3_bind_int(stmt, 11, 1));

	if (entry->time.tm_year) {
		SQLCHECK(sqlite3_bind_int(stmt, 13, entry->time.tm_year+1900));
		SQLCHECK(sqlite3_bind_int(stmt, 14, entry->time.tm_mon+1));
		SQLCHECK(sqlite3_bind_int(stmt, 15, entry->time.tm_mday));
		SQLCHECK(sqlite3_bind_int(stmt, 16, timegm(&entry->time)));
	}
	if (entry->security.type != LJ_SECURITY_PUBLIC) {
		int sec = -1;
		switch (entry->security.type) {
		case LJ_SECURITY_FRIENDS:
			sec = 1; break;
		case LJ_SECURITY_CUSTOM:
			sec = entry->security.allowmask; break;
		case LJ_SECURITY_PRIVATE:
		default:
			sec = 0; break;
		}
		SQLCHECK(sqlite3_bind_int(stmt, 17, sec));
	}

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE) {
		SQLCHECK(ret /* step */);
		SQLCHECK(sqlite3_finalize(stmt));
		return FALSE;
	}
	SQLCHECK(sqlite3_finalize(stmt));

	return TRUE;
}

gboolean
journal_store_put_group(JournalStore *js, LJEntry **entries, int c, GError **err) {
	int i;

	if (!begin_t(js->db, err))
		goto err;

	for (i = 0; i < c; i++) {
		if (!journal_store_put(js, entries[i], err))
			goto err;
	}

	if (!end_t(js->db, err))
		goto err;

	return TRUE;

err:
	end_t(js->db, NULL);
	return FALSE;
}

guint32
journal_store_get_month_entries(JournalStore *js, int year, int mon) {
	guint32 month = 0;
	int ret;
	sqlite3_stmt *stmt = NULL;

	SQLCHECK(sqlite3_prepare(js->db,
		"SELECT day FROM entry WHERE year=?1 AND month=?2",
		-1, &stmt, NULL));
	SQLCHECK(sqlite3_bind_int(stmt, 1, year));
	SQLCHECK(sqlite3_bind_int(stmt, 2, mon));

	ret = sqlite3_step(stmt);
	while (ret == SQLITE_ROW) {
		int day = sqlite3_column_int(stmt, 0);
		month |= (1 << day);
		ret = sqlite3_step(stmt);
	}

	if (ret != SQLITE_DONE) {
		SQLCHECK(ret /* step */);
		SQLCHECK(sqlite3_finalize(stmt));
		return 0;
	}
	SQLCHECK(sqlite3_finalize(stmt));

	return month;
}

gboolean
journal_store_get_day_entries(JournalStore *js,
                              int year, int mon, int day,
                              JournalStoreSummaryCallback cb_func,
                              gpointer cb_data) {
	int ret;
	sqlite3_stmt *stmt = NULL;
	const char *subject;
	const char *event;
	LJSecurity sec = {0};

	SQLCHECK(sqlite3_prepare(js->db,
		"SELECT itemid, timestamp, subject, event "
		"FROM entry WHERE year=?1 AND month=?2 AND day=?3",
		-1, &stmt, NULL));
	SQLCHECK(sqlite3_bind_int(stmt, 1, year));
	SQLCHECK(sqlite3_bind_int(stmt, 2, mon));
	SQLCHECK(sqlite3_bind_int(stmt, 3, day));

	ret = sqlite3_step(stmt);
	while (ret == SQLITE_ROW) {
		int itemid = sqlite3_column_int(stmt, 0);
		time_t timestamp = sqlite3_column_int(stmt, 1);
		subject = (char*)sqlite3_column_text(stmt, 2);
		event = (char*)sqlite3_column_text(stmt, 3);
		cb_func(itemid, timestamp,
				lj_get_summary(subject, event), &sec /* XXX */,
				cb_data);
		ret = sqlite3_step(stmt);
	}
	if (ret != SQLITE_DONE) {
		SQLCHECK(ret);
		SQLCHECK(sqlite3_finalize(stmt));
		return FALSE;
	}

	SQLCHECK(sqlite3_finalize(stmt));

	return TRUE;
}

gboolean
journal_store_scan(JournalStore *js,
                   JournalStoreScanCallback scan_cb,
                   const gpointer scan_data,
                   JournalStoreSummaryCallback cb_func,
                   const gpointer cb_data) {
	int ret;
	sqlite3_stmt *stmt = NULL;
	int matchcount = 0;

	SQLCHECK(sqlite3_prepare(js->db,
		"SELECT itemid, timestamp, subject, event, security "
		"FROM entry ORDER BY itemid ASC",
		-1, &stmt, NULL));

	ret = sqlite3_step(stmt);
	while (ret == SQLITE_ROW) {
		int itemid = sqlite3_column_int(stmt, 0);
		time_t timestamp = sqlite3_column_int(stmt, 1);
		const char *subject = (char*)sqlite3_column_text(stmt, 2);
		const char *event = (char*)sqlite3_column_text(stmt, 3);
		LJSecurity sec = {0};
		if (sqlite3_column_type(stmt, 4) != SQLITE_NULL)
			security_from_int(&sec, sqlite3_column_int(stmt, 4));

		if (scan_cb(subject, scan_data) || scan_cb(event, scan_data)) {
			const char *summary = lj_get_summary(subject, event);
			cb_func(itemid, timestamp, summary, &sec, cb_data);
			if (++matchcount == MAX_MATCHES)
				break;
		}
		ret = sqlite3_step(stmt);
	}

	if (ret != SQLITE_ROW && ret != SQLITE_DONE) {
		SQLCHECK(ret);
		SQLCHECK(sqlite3_finalize(stmt));
		return FALSE;
	}

	SQLCHECK(sqlite3_finalize(stmt));

	return TRUE;
}

