/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "glib-all.h"

#include <string.h>

#include "draftstore.h"
#include "jamdoc.h"
#include "account.h"
#include "conf.h"

#ifdef HAVE_GTK
#include "get_cmd_out.h"
#include "smartquotes.h"
#endif

#undef USE_STRUCTUREDTEXT

enum {
	ENTRY_CHANGED,
	UPDATE_DOC,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_DIRTY,
	PROP_USEJOURNAL,
	PROP_ACCOUNT
};

struct _JamDoc {
	GObject parent;

	/* for drafts, the current entry is tracked through entry->itemid.
	 * for files, we need to keep the filename around. */
	char *filename;
	/* we only save as xml, so we need to remember whether they opened
	 * a non-xml file. */
	gboolean filename_not_xml;
	/* the convention is:
	 *   if there's a filename, we're editing a file.
	 *   if itemid is less than zero, we're editing a draft.
	 *   otherwise, it's an untitled/unattached doc.
	 */

	/* if autosave is on, each document gets its own autosave file.
	 * this is not related to the drafts mentioned above. */
	char *autosave_filename;

	gboolean dirty;

	LJEntry *entry;
#ifdef HAVE_GTK
	GtkTextBuffer *buffer;
#endif /* HAVE_GTK */
	guint buffer_signal;
	gchar *usejournal;

#ifdef USE_STRUCTUREDTEXT
	TextStyle textstyle;
#endif
	JamAccount *account;
};

struct _JamDocClass {
	GObjectClass parent_class;
	void (*entry_changed)(JamDoc *doc);
	void (*update_doc)(JamDoc *doc);
};

static void jam_doc_update_doc(JamDoc *doc);
static void jam_doc_set_property(GObject *object, guint prop_id,
                        const GValue *value, GParamSpec *pspec);
static void jam_doc_get_property(GObject *object, guint prop_id,
                        GValue *value, GParamSpec *pspec);

static guint signals[LAST_SIGNAL] = { 0 };

static void
jam_doc_class_init(gpointer klass, gpointer class_data) {
	GObjectClass *gclass = G_OBJECT_CLASS(klass);

	gclass->set_property = jam_doc_set_property;
	gclass->get_property = jam_doc_get_property;

	signals[ENTRY_CHANGED] = g_signal_new("entry_changed",
			LOGJAM_TYPE_DOC,
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(JamDocClass, entry_changed),
			NULL, NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);

	signals[UPDATE_DOC] = g_signal_new("update_doc",
			LOGJAM_TYPE_DOC,
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(JamDocClass, update_doc),
			NULL, NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);

	g_object_class_install_property(gclass, PROP_DIRTY,
			g_param_spec_boolean("dirty",
				"Is dirty",
				"If TRUE, the document has changed since it was last saved.",
				FALSE, G_PARAM_READWRITE));

	g_object_class_install_property(gclass, PROP_USEJOURNAL,
			g_param_spec_string("usejournal",
				"use journal",
				"The current journal that the account is working with.",
				NULL, G_PARAM_READWRITE));

	g_object_class_install_property(gclass, PROP_ACCOUNT,
			g_param_spec_pointer("account",
				"account",
				"The current account that the user is working with.",
				G_PARAM_READWRITE));
}

#ifdef HAVE_GTK
static void
buffer_changed_cb(GtkTextBuffer *buf, JamDoc *doc) {
	if (gtk_text_buffer_get_modified(buf))
		jam_doc_set_dirty(doc, TRUE);
}

GtkTextBuffer*
jam_doc_get_text_buffer(JamDoc *doc) {
	return doc->buffer;
}
#endif /* HAVE_GTK */

static void
jam_doc_init(GTypeInstance *inst, gpointer g_class) {
	JamDoc *doc = LOGJAM_DOC(inst);
	doc->entry = lj_entry_new();
	doc->entry->security = conf.defaultsecurity;
	doc->dirty = FALSE;
#ifdef HAVE_GTK
	doc->buffer = gtk_text_buffer_new(NULL);
	doc->buffer_signal = g_signal_connect(G_OBJECT(doc->buffer),
			"modified-changed",
			G_CALLBACK(buffer_changed_cb), doc);
	gtk_text_buffer_set_modified(doc->buffer, FALSE);
	if (conf.options.smartquotes)
		smartquotes_attach(doc->buffer);
#endif /* HAVE_GTK */
}

GType
jam_doc_get_type() {
	static GType doc_type = 0;
	if (!doc_type) {
		static const GTypeInfo doc_info = {
			sizeof(JamDocClass),
			NULL,
			NULL,
			jam_doc_class_init,
			NULL,
			NULL,
			sizeof(JamDoc),
			0,
			jam_doc_init
		};
		doc_type = g_type_register_static(G_TYPE_OBJECT, "JamDoc",
				&doc_info, 0);
	}
	return doc_type;
}

static void
jam_doc_set_property(GObject *object, guint prop_id,
                        const GValue *value, GParamSpec *pspec)
{
	/* unimplemented. */
}
static void
jam_doc_get_property(GObject *object, guint prop_id,
                        GValue *value, GParamSpec *pspec)
{
	/* unimplemented. */
}

JamDoc*
jam_doc_new(void) {
	return LOGJAM_DOC(g_object_new(jam_doc_get_type(), NULL));
}

void
jam_doc_set_dirty(JamDoc *doc, gboolean dirty) {
	if (doc->dirty == dirty)
		return;
	doc->dirty = dirty;
#ifdef HAVE_GTK
	if (doc->dirty == FALSE)
		gtk_text_buffer_set_modified(doc->buffer, FALSE);
#endif /* HAVE_GTK */
	g_object_notify(G_OBJECT(doc), "dirty");
}

gboolean
jam_doc_get_dirty(JamDoc *doc) {
	return doc->dirty;
}

const char *
jam_doc_get_subject(JamDoc *doc) {
	return doc->entry->subject;
}
void
jam_doc_set_subject(JamDoc *doc, const char *subject) {
	string_replace(&doc->entry->subject, subject ? g_strdup(subject) : NULL);
}
const char *
jam_doc_get_mood(JamDoc *doc) {
	return doc->entry->mood;
}
void
jam_doc_set_mood(JamDoc *doc, const char *mood) {
	string_replace(&doc->entry->mood, mood ? g_strdup(mood) : NULL);
}
void
jam_doc_set_moodid(JamDoc *doc, int moodid) {
	doc->entry->moodid = moodid;
}
const char *
jam_doc_get_music(JamDoc *doc) {
	return doc->entry->music;
}
void
jam_doc_set_music(JamDoc *doc, const char *music) {
	string_replace(&doc->entry->music, music ? g_strdup(music) : NULL);
}
const char *
jam_doc_get_taglist(JamDoc *doc) {
	return doc->entry->taglist;
}
void
jam_doc_set_taglist(JamDoc *doc, const char *taglist) {
	string_replace(&doc->entry->taglist, taglist ? g_strdup(taglist) : NULL);
}
LJSecurity
jam_doc_get_security(JamDoc *doc) {
	return doc->entry->security;
}
void
jam_doc_set_security(JamDoc *doc, LJSecurity *sec) {
	doc->entry->security = *sec;
}
LJCommentsType
jam_doc_get_comments(JamDoc *doc) {
	return doc->entry->comments;
}
void
jam_doc_set_comments(JamDoc *doc, LJCommentsType type) {
	doc->entry->comments = type;
}

void
jam_doc_get_time(JamDoc *doc, struct tm *ptm) {
	*ptm = doc->entry->time;
}
void
jam_doc_set_time(JamDoc *doc, const struct tm *ptm) {
	if (ptm)
		doc->entry->time = *ptm;
	else
		memset((void*)&doc->entry->time, 0, sizeof(struct tm));
}

gboolean
jam_doc_get_backdated(JamDoc *doc) {
	return doc->entry->backdated;
}
void
jam_doc_set_backdated(JamDoc *doc, gboolean backdated) {
	doc->entry->backdated = backdated;
}

gboolean
jam_doc_get_preformatted(JamDoc *doc) {
	return doc->entry->preformatted;
}
void
jam_doc_set_preformatted(JamDoc *doc, gboolean preformatted) {
	doc->entry->preformatted = preformatted;
}

void
jam_doc_set_event(JamDoc *doc, const char *event) {
	g_free(doc->entry->event);
	doc->entry->event = (event ? g_strdup(event) : NULL);
}

const char *
jam_doc_get_picture(JamDoc *doc) {
	return doc->entry->pickeyword;
}
void
jam_doc_set_picture(JamDoc *doc, const char *picture) {
	string_replace(&doc->entry->pickeyword, picture ? g_strdup(picture) : NULL);
}
void
jam_doc_set_pickeyword(JamDoc *doc, const char *keyword) {
	jam_doc_set_picture(doc, keyword);
}

static void
set_entry(JamDoc *doc, LJEntry *entry) {
#ifdef HAVE_GTK
	GtkTextIter start, end;
#endif /* HAVE_GTK */

	lj_entry_free(doc->entry);
	if (entry)
		doc->entry = entry;
	else
		entry = doc->entry = lj_entry_new();

#ifdef HAVE_GTK
	/* block the buffer signal so we don't rapidly flip to dirty and back. */
	g_signal_handler_block(doc->buffer, doc->buffer_signal);
	gtk_text_buffer_get_bounds(doc->buffer, &start, &end);
	gtk_text_buffer_delete(doc->buffer, &start, &end);
	if (entry->event)
		gtk_text_buffer_insert(doc->buffer, &start, entry->event, -1);
	g_signal_handler_unblock(doc->buffer, doc->buffer_signal);
#endif /* HAVE_GTK */

	jam_doc_set_dirty(doc, FALSE);
#ifdef HAVE_GTK
	/* since the buffer signal was blocked, we need to do this manually. */
	gtk_text_buffer_set_modified(doc->buffer, FALSE);
#endif /* HAVE_GTK */
}

gboolean
jam_doc_load_file(JamDoc *doc, const char *filename,
                  LJEntryFileType type, GError **err) {
	LJEntry *entry;
	entry = lj_entry_new_from_filename(filename, type, &type, err);
	if (!entry)
		return FALSE;
	entry->itemid = 0;
	set_entry(doc, entry);
	string_replace(&doc->filename, g_strdup(filename));
	doc->filename_not_xml = (type != LJ_ENTRY_FILE_XML);
	g_signal_emit_by_name(doc, "entry_changed");
	return TRUE;
}

void
jam_doc_load_draft(JamDoc *doc, LJEntry *entry) {
	set_entry(doc, lj_entry_copy(entry));
	g_signal_emit_by_name(doc, "entry_changed");
}

void
jam_doc_load_entry(JamDoc *doc, LJEntry *entry) {
	set_entry(doc, lj_entry_copy(entry));
	g_signal_emit_by_name(doc, "entry_changed");
}

static gboolean
save_draft(LJEntry *entry, JamAccount *acc, GError **err) {
	DraftStore *ds;

	ds = draft_store_new(acc);

	if (entry->itemid == 0)
		entry->itemid = draft_store_find_itemid(ds);

	if (!draft_store_put_entry(ds, entry, err)) {
		draft_store_free(ds);
		return FALSE;
	}

	draft_store_free(ds);

	return TRUE;
}

gboolean
jam_doc_would_save_over_nonxml(JamDoc *doc) {
	return (doc->filename && doc->filename_not_xml);
}
gboolean
jam_doc_has_save_target(JamDoc *doc) {
	/* filename or a draft. */
	return (doc->filename || doc->entry->itemid < 0);
}

gboolean
jam_doc_save(JamDoc *doc, JamAccount *acc, GError **err) {
	jam_doc_update_doc(doc);

	if (doc->filename) {
		if (!lj_entry_to_xml_file(doc->entry, doc->filename, err))
			return FALSE;
	} else if (doc->entry->itemid < 0) {
		if (!save_draft(doc->entry, acc, err))
			return FALSE;
	} else {
		g_error("jam_doc_save: shouldn't be called without savetarget.\n");
		return FALSE;
	}

	jam_doc_set_dirty(doc, FALSE);

	return TRUE;
}

gboolean
jam_doc_save_as_file(JamDoc *doc, const char *filename, GError **err) {
	LJEntry *entry;

	entry = jam_doc_get_entry(doc);

	/* always save to files without an itemid. */
	entry->itemid = 0;
	if (!lj_entry_to_xml_file(entry, filename, err)) {
		lj_entry_free(entry);
		return FALSE;
	}
	lj_entry_free(entry);

	doc->entry->itemid = 0;
	string_replace(&doc->filename, g_strdup(filename));

	jam_doc_set_dirty(doc, FALSE);

	return TRUE;
}

gboolean
jam_doc_save_as_draft(JamDoc *doc, const char *title, JamAccount *acc, GError **err) {
	LJEntry *entry;
	
	entry = jam_doc_get_entry(doc);
	string_replace(&entry->subject, g_strdup(title));
	entry->itemid = 0;
	if (!save_draft(entry, acc, err)) {
		lj_entry_free(entry);
		return FALSE;
	}
	/* retrieve the new draft id. */
	doc->entry->itemid = entry->itemid;
	lj_entry_free(entry);

	string_replace(&doc->filename, NULL);

	jam_doc_set_dirty(doc, FALSE);

	return TRUE;
}

char*
jam_doc_get_title(JamDoc *doc) {
	if (doc->filename) {
		const gchar *homedir = g_get_home_dir();
		int homelen = (int)strlen(homedir);
		if (strncmp(homedir, doc->filename, homelen) == 0)
			return g_build_filename("~", doc->filename+homelen, NULL);
		else
			return g_strdup(doc->filename);
	} else if (doc->entry->itemid < 0) {
		return g_strdup_printf(_("Draft %d"), -doc->entry->itemid);
	} else if (doc->entry->itemid > 0) {
		return g_strdup_printf(_("Entry %d"), doc->entry->itemid);
	} else {
		return g_strdup(_("New Entry"));
	}
}

char*
jam_doc_get_draftname(JamDoc *doc) {
	jam_doc_update_doc(doc);
	if (doc->entry->subject)
		return g_strdup(doc->entry->subject);
	return NULL;
}

static void
jam_doc_update_doc(JamDoc *doc) {
	/* XXX this is sorta weird; we emit a signal to get our internal version
	 * of the entry in sync with the state of the widgets.  this is important
	 * in decoupling the widgets from the document, but it feels a little
	 * backwards. */
	g_signal_emit_by_name(doc, "update_doc");
}

LJEntry*
jam_doc_get_entry(JamDoc *doc) {
	jam_doc_update_doc(doc);
	return lj_entry_copy(doc->entry);
}

#ifdef HAVE_GTK
gboolean
jam_doc_insert_file(JamDoc *doc, const char *filename, const char *encoding, GError **err) {
	char *text;
	gsize len;

	if (!g_file_get_contents(filename, &text, &len, err))
		return FALSE;

	if (strcmp(encoding, "UTF-8") == 0) {
		const gchar *end;
		if (!g_utf8_validate(text, len, &end)) {
			g_set_error(err, 0, 0,
					_("Invalid UTF-8 starting at byte %d."), end-text);
			g_free(text);
			return FALSE;
		}
		gtk_text_buffer_insert_at_cursor(doc->buffer, text, -1);
	} else {
		char *newtext;
		newtext = g_convert(text, -1, "UTF-8", encoding, NULL, NULL, err);
		if (!newtext) {
			g_free(text);
			return FALSE;
		}
		gtk_text_buffer_insert_at_cursor(doc->buffer, newtext, -1);
		g_free(newtext);
	}
	g_free(text);
	return TRUE;
}

gboolean
jam_doc_insert_command_output(JamDoc *doc, const char *command,
		const char *encoding, GError **err, GtkWindow *parent) {
	/* FIXME: encoding is not currently used, instead we recode from
	 * the current locale. Does anybody knows a program which output
	 * is NOT in the current locale's encoding? If yes, let me know.
	 */
	GString * output;
	const gchar *end;

	output = get_command_output(command, err, parent);
	if (output == NULL)
		return FALSE;

	if (g_utf8_validate(output->str, output->len, &end)) {
		gtk_text_buffer_insert_at_cursor(doc->buffer,
				output->str, output->len);
	} else {
		gchar* newtext;

		newtext = g_locale_to_utf8(output->str, output->len,
				NULL, NULL, err);
		if (!newtext) {
			g_string_free(output, TRUE);
			return FALSE;
		}
		gtk_text_buffer_insert_at_cursor(doc->buffer, newtext, -1);
		g_free(newtext);
	}
	g_string_free(output, TRUE);
	return TRUE;
}
#endif /* HAVE_GTK */

int
jam_doc_get_flags(JamDoc *doc) {
	LJEntryType type = jam_doc_get_entry_type(doc);
	int flags = 0;

	switch (type) {
		case ENTRY_DRAFT:
			flags = LOGJAM_DOC_CAN_DELETE | LOGJAM_DOC_CAN_SUBMIT;
			break;
		case ENTRY_NEW:
			flags = LOGJAM_DOC_CAN_SUBMIT;
			break;
		case ENTRY_SERVER:
			flags = LOGJAM_DOC_CAN_DELETE | LOGJAM_DOC_CAN_SAVE;
			break;
	}
	return flags;
}

LJEntryType
jam_doc_get_entry_type(JamDoc *doc) {
	gint itemid = jam_doc_get_entry_itemid(doc);
	return itemid < 0 ? ENTRY_DRAFT :
		itemid == 0 ? ENTRY_NEW :
		ENTRY_SERVER;
}

gint
jam_doc_get_entry_itemid(JamDoc *doc) {
	return doc->entry->itemid;
}

static void
jam_doc_user_changed(JamDoc *doc) {
	/* if they were editing an old entry or a draft,
	 * that itemid is no longer relevant for the current user. */
	jam_doc_update_doc(doc);
	if (doc->entry->itemid != 0) {
		doc->entry->itemid = 0;
		g_signal_emit_by_name(doc, "entry_changed");
	}
}

gchar*
jam_doc_get_usejournal(JamDoc *doc) {
	return doc->usejournal;
}

void
jam_doc_set_usejournal(JamDoc *doc, const gchar *usejournal) {
	gchar *uj = usejournal ? g_strdup(usejournal) : NULL;
	string_replace(&doc->usejournal, uj);
	jam_doc_user_changed(doc);
	g_object_notify(G_OBJECT(doc), "usejournal");
}

JamAccount*
jam_doc_get_account(JamDoc *doc) {
	return doc->account;
}

void
jam_doc_set_account(JamDoc *doc, JamAccount *acc) {
	g_object_ref(acc);
	if (doc->account)
		g_object_unref(doc->account);
	doc->account = acc;

	g_object_notify(G_OBJECT(doc), "account");
	jam_doc_set_usejournal(doc, NULL);
	/* set_usejournal calls this for us:  jam_doc_user_changed(doc); */
}

#ifdef USE_STRUCTUREDTEXT
char* structuredtext_to_html(const char *src, GError **err);
char* html_to_structuredtext(const char *src, GError **err);
typedef char* (*TextStyleFunc)(const char *src, GError **err);
static TextStyleFunc to_html[] = {
	NULL,
	structuredtext_to_html
};
static TextStyleFunc from_html[] = {
	NULL,
	html_to_structuredtext
};

gboolean
jam_doc_change_textstyle(JamDoc *doc, TextStyle newstyle, GError **err) {
	GtkTextBuffer *buffer = doc->buffer;
	GtkTextIter start, end;
	char *src;
	char *html, *result;

	gtk_text_buffer_get_bounds(buffer, &start, &end);
	src = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);

	if (to_html[doc->jam_doc_get_textstyle(doc)])
		html = to_html[doc->jam_doc_get_textstyle(doc)](src, err);
	else
		html = src;

	if (html == NULL) {
		g_free(src);
		return FALSE;
	}

	if (from_html[newstyle])
		result = from_html[newstyle](src, err);
	else
		result = html;

	if (result == NULL) {
		g_free(src);
		if (html != src)
			g_free(html);
		return FALSE;
	}
	
	doc->textstyle = newstyle;
	gtk_text_buffer_set_text(buffer, result, -1);

	g_free(src);
	if (src != html)
		g_free(html);
	if (html != result)
		g_free(result);
	return TRUE;
}
#endif /* USE_STRUCTUREDTEXT */
