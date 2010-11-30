/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __jam_doc_h__
#define __jam_doc_h__

#include <livejournal/livejournal.h>
#include "account.h"

typedef enum {
	ENTRY_DRAFT,
	ENTRY_NEW,
	ENTRY_SERVER
} LJEntryType;

typedef struct _JamDoc JamDoc;
typedef struct _JamDocClass JamDocClass;

GType    jam_doc_get_type(void);
JamDoc*  jam_doc_new(void);

#ifdef HAVE_GTK
#include <gtk/gtktextbuffer.h>
GtkTextBuffer* jam_doc_get_text_buffer(JamDoc *doc);
gboolean jam_doc_insert_file(JamDoc *doc, const char *filename, const char *encoding, GError **err);

gboolean jam_doc_insert_command_output(JamDoc *doc, const char *command,
		const char *encoding, GError **err, GtkWindow *parent);
#endif

char *         jam_doc_get_title      (JamDoc *doc);
LJEntry*       jam_doc_get_entry      (JamDoc *doc);

char *         jam_doc_get_draftname  (JamDoc *doc);

/* these flags correspond to server-side capabilities;
 * any document can be saved with a file->save as sort of command. */
#define LOGJAM_DOC_CAN_DELETE (1<<0)
#define LOGJAM_DOC_CAN_SAVE   (1<<1)
#define LOGJAM_DOC_CAN_SUBMIT (1<<2)
int            jam_doc_get_flags      (JamDoc *doc);

void           jam_doc_set_dirty      (JamDoc *doc, gboolean dirty);
gboolean       jam_doc_get_dirty      (JamDoc *doc);

JamAccount*    jam_doc_get_account    (JamDoc *doc);
void           jam_doc_set_account    (JamDoc *doc, JamAccount* acc);

const char *   jam_doc_get_subject(JamDoc *doc);
void           jam_doc_set_subject(JamDoc *doc, const char *subject);

LJSecurity     jam_doc_get_security(JamDoc *doc);
void           jam_doc_set_security(JamDoc *doc, LJSecurity *sec);

const char *   jam_doc_get_mood(JamDoc *doc);
void           jam_doc_set_mood(JamDoc *doc, const char *mood);

void           jam_doc_set_moodid(JamDoc *doc, int moodid);

const char *   jam_doc_get_music(JamDoc *doc);
void           jam_doc_set_music(JamDoc *doc, const char *music);

const char *   jam_doc_get_location(JamDoc *doc);
void           jam_doc_set_location(JamDoc *doc, const char *location);

const char *   jam_doc_get_taglist(JamDoc *doc);
void           jam_doc_set_taglist(JamDoc *doc, const char *taglist);

const char *   jam_doc_get_picture(JamDoc *doc);
void           jam_doc_set_picture(JamDoc *doc, const char *keyword);

LJCommentsType jam_doc_get_comments(JamDoc *doc);
void           jam_doc_set_comments(JamDoc *doc, LJCommentsType type);

LJScreeningType
               jam_doc_get_screening(JamDoc *doc);
void           jam_doc_set_screening(JamDoc *doc, LJScreeningType type);

void           jam_doc_get_time(JamDoc *doc, struct tm *ptm);
void           jam_doc_set_time(JamDoc *doc, const struct tm *ptm);

gboolean       jam_doc_get_backdated(JamDoc *doc);
void           jam_doc_set_backdated(JamDoc *doc, gboolean backdated);

gboolean       jam_doc_get_preformatted(JamDoc *doc);
void           jam_doc_set_preformatted(JamDoc *doc, gboolean preformatted);

void jam_doc_set_event(JamDoc *doc, const char *event);

gchar* jam_doc_get_usejournal(JamDoc *doc);
void   jam_doc_set_usejournal(JamDoc *doc, const gchar *usejournal);

gboolean jam_doc_load_file (JamDoc *doc, const char *filename,
                               LJEntryFileType type, GError **err);
void     jam_doc_load_draft(JamDoc *doc, LJEntry *entry);
void     jam_doc_load_entry(JamDoc *doc, LJEntry *entry);

/* these are terrible names. */
gboolean jam_doc_would_save_over_nonxml(JamDoc *doc);
gboolean jam_doc_has_save_target(JamDoc *doc);

gboolean jam_doc_save      (JamDoc *doc, JamAccount *acc, GError **err);

/* these emit the "title-changed" signal */
gboolean jam_doc_save_as_file (JamDoc *doc,
                                  const char *filename, GError **err);
gboolean jam_doc_save_as_draft(JamDoc *doc,
                                  const char *title, JamAccount *acc, GError **err);


LJEntryType jam_doc_get_entry_type(JamDoc *doc);
gint jam_doc_get_entry_itemid(JamDoc *doc);

#define LOGJAM_TYPE_DOC (jam_doc_get_type())
#define LOGJAM_DOC(object) (G_TYPE_CHECK_INSTANCE_CAST((object), LOGJAM_TYPE_DOC, JamDoc))
#define LOGJAM_DOC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), LOGJAM_TYPE_DOC, JamDocClass))


#endif /* __jam_doc_h__ */
