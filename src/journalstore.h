/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef JOURNALSTORE_H
#define JOURNALSTORE_H

#include <livejournal/livejournal.h>

#include "account.h"

#define MAX_MATCHES 1000

typedef struct _JournalStore JournalStore;

JournalStore* journal_store_open(JamAccount *acc, gboolean create, GError **err);
gboolean journal_store_reindex(JamAccount *acc, GError **err);

JamAccount* journal_store_get_account(JournalStore *js);

void     journal_store_free             (JournalStore *js);

gboolean journal_store_put              (JournalStore *js, LJEntry *entry,
                                         GError **err);
gboolean journal_store_put_group        (JournalStore *js, LJEntry **entries, int c,
                                         GError **err);

time_t   journal_store_lookup_entry_time(JournalStore *js, int itemid);
guint32  journal_store_get_month_entries(JournalStore *js, int year, int mon);
LJEntry* journal_store_get_entry        (JournalStore *js, int get_itemid);
int      journal_store_get_latest_id    (JournalStore *js);
int      journal_store_get_count        (JournalStore *js);
gboolean journal_store_get_invalid      (JournalStore *js);

char *   journal_store_get_lastsync     (JournalStore *js);
gboolean journal_store_put_lastsync     (JournalStore *js, const char *lastsync,
                                         GError **err);

typedef void (*JournalStoreSummaryCallback)(int itemid, time_t etime,
                                            const char *summary, LJSecurity *sec,
                                            gpointer data);
gboolean journal_store_get_day_entries(JournalStore *js,
                                       int year, int mon, int day,
                                       JournalStoreSummaryCallback cb_func,
                                       gpointer cb_data);

gboolean journal_store_find_relative_by_time(JournalStore *js, time_t when,
                                     int *ritemid, int dir, GError *err);
gboolean journal_store_find_relative(JournalStore *js, gint itemid,
                                     int *ritemid, int dir, GError *err);

typedef gboolean (*JournalStoreScanCallback)(const char *str, gpointer data);
gboolean journal_store_scan(JournalStore *js,
                            JournalStoreScanCallback scan_cb,
                            const gpointer scan_data,
                            JournalStoreSummaryCallback cb_func,
                            const gpointer cb_data);

#endif /* JOURNALSTORE_H */
