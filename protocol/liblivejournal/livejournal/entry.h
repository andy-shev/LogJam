/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_ENTRY_H__
#define __LIVEJOURNAL_ENTRY_H__

#include <time.h>  /* post timestamp */
#include <stdio.h> /* FILE* for loading entries */

#include "protocol.h"

/* --- security --- */

typedef enum {
	LJ_SECURITY_PUBLIC = 0,
	LJ_SECURITY_FRIENDS,
	LJ_SECURITY_PRIVATE,
	LJ_SECURITY_CUSTOM
} LJSecurityType;

typedef struct _LJSecurity {
	LJSecurityType type;
	unsigned int allowmask;
} LJSecurity;
/* an allowmask of 1 means friends-only. */
#define LJ_SECURITY_ALLOWMASK_FRIENDS 1

void lj_security_from_strings     (LJSecurity *security, const char *sectext, const char *allowmask);
void lj_security_to_strings       (LJSecurity *security, char **sectext,      char **allowmask);
void lj_security_append_to_request(LJSecurity *security, LJRequest *request);

/* --- comments --- */

typedef enum {
	LJ_COMMENTS_DEFAULT,
	LJ_COMMENTS_NOEMAIL,
	LJ_COMMENTS_DISABLE
} LJCommentsType;

/* -- entry --- */

typedef struct _LJEntry {
	/*        < 0: draft
	 * itemid = 0: unknown itemid
	 *        > 0: known itemid
	 */
	int itemid, anum;
	char *subject;
	char *event;

	int moodid;  /* a moodid is only meaningful in the context of a particular LJServer. */
	char *mood, *music, *taglist, *pickeyword;
	gboolean preformatted;
	gboolean backdated;
	LJCommentsType comments;

	struct tm time;
	LJSecurity security;
} LJEntry;

typedef enum {
	LJ_ENTRY_FILE_AUTODETECT,
	LJ_ENTRY_FILE_XML,
	LJ_ENTRY_FILE_RFC822,
	LJ_ENTRY_FILE_PLAIN
} LJEntryFileType;

/* avoid including libxml */
#define xmlDocPtr void*
#define xmlNodePtr void*

LJEntry*   lj_entry_new                  (void);
LJEntry*   lj_entry_new_from_file        (FILE *f, LJEntryFileType type,
                                          LJEntryFileType *typeret, GError **err);
LJEntry*   lj_entry_new_from_filename    (const char *filename, LJEntryFileType type,
                                          LJEntryFileType *typeret, GError **err);
LJEntry*   lj_entry_new_from_xml_node    (xmlDocPtr doc, xmlNodePtr node);

LJEntry*   lj_entry_new_single_from_result(LJResult *result, GError **err);

LJEntry*   lj_entry_copy                 (LJEntry *e);
void       lj_entry_free                 (LJEntry *e);

const char* lj_entry_make_summary         (LJEntry *entry);
const char* lj_get_summary               (const char *subject, const char *event);
void       lj_entry_set_request_fields   (LJEntry *entry, LJRequest *request);

xmlNodePtr lj_entry_to_xml_node          (LJEntry *entry, xmlDocPtr doc);
gboolean   lj_entry_to_xml_file          (LJEntry *entry, const char *filename,
                                          GError **err);

GString*   lj_entry_to_rfc822            (LJEntry *entry, gboolean includeempty);
gboolean   lj_entry_to_rfc822_file       (LJEntry *entry, const char *filename,
                                          gboolean includeempty, GError **err);

gboolean   lj_entry_edit_with_usereditor (LJEntry *entry, const char *basepath,
                                          GError **err);

LJEntry**  lj_entries_new_from_result(LJResult *result, int count,
                                      GSList **warnings);

gint       lj_entry_get_itemid           (LJEntry *e);
gint       lj_entry_get_anum             (LJEntry *e);
void       lj_entry_get_time             (LJEntry *e, struct tm *time);

#undef xmlDocPtr
#undef xmlNodePtr

#endif /* __LIVEJOURNAL_ENTRY_H__ */
