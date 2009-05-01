/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef DRAFTSTORE_H
#define DRAFTSTORE_H

#include <livejournal/livejournal.h>
#include "account.h"

typedef struct _DraftStore DraftStore;

typedef void (*DraftStoreHeaderFunc)(DraftStore *, LJEntry *, gpointer);

DraftStore* draft_store_new(JamAccount *acc);
void draft_store_free(DraftStore *ds);

gboolean draft_store_each_header(DraftStore *ds,
                                 LJEntry *entry,
                                 DraftStoreHeaderFunc func,
                                 gpointer data);

LJEntry*   draft_store_get_entry   (DraftStore *ds, int itemid, GError **err);
gboolean draft_store_put_entry   (DraftStore *ds, LJEntry *entry, GError **err);
gboolean draft_store_remove_entry(DraftStore *ds, int itemid, GError **err);
int      draft_store_find_itemid (DraftStore *ds);
gboolean draft_store_flush       (DraftStore *ds, GError **err);

#ifdef HAVE_GTK
LJEntry*   draft_store_ui_select   (DraftStore *ds, GtkWindow *parent);
#endif

#endif /* DRAFTSTORE_H */
