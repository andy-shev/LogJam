/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Gaal Yahas <gaal@forum2.org>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#ifdef HAVE_GTKSPELL
#include <gtkspell/gtkspell.h>
#endif /* HAVE_GTKSPELL */
#include "util-gtk.h"
#include "jamdoc.h"
#include "music.h"
#include "undo.h"
#include "jamview.h"
#include "security.h"
#include "marshalers.h"
#include "datesel.h"
#include "tags.h"
#include "lj_dbus.h"

#define KEY_PICTUREKEYWORD "logjam-picturekeyword"

enum {
	META_TOGGLE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _JamView {
	GtkVBox box;

	GtkWidget *subjectbar;
	GtkWidget *subject, *security;

	GtkWidget *moodpicbar;
	GtkWidget *moodbox, *moodbutton, *moodcombo;
	GtkWidget *picbox, *pic;

	GtkWidget *musicbar;
	GtkWidget *musicbutton, *music;

	GtkWidget *locationbar;
	GtkWidget *location;

	GtkWidget *tagsbar;
	GtkWidget *tags;

	GtkWidget *entry;

	GtkWidget *optionbar;
	GtkWidget *preformatted;
	GtkWidget *datesel;
	GtkWidget *commentsbox, *comments;
	GtkWidget *screeningbox, *screening;

	GtkSizeGroup *sizegroup;
	UndoMgr *undomgr;
	JamAccount *account;
	JamDoc *doc;
};

struct _JamViewClass {
	GtkVBoxClass parent_class;
	void (*meta_toggle)(JamView *view, JamViewMeta meta);
};

static void
show_meta(JamView *view, JamViewMeta meta) {
	if (jam_view_get_meta_visible(view, meta))
		return;
	jam_view_toggle_meta(view, meta, TRUE);
	g_signal_emit_by_name(view, "meta_toggle", meta, TRUE);
}

static void
subject_add(JamView *view) {
	/* subject is always visible. */
	view->subjectbar = gtk_hbox_new(FALSE, 12);

	view->subject = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(view->subjectbar),
			labelled_box_new_sg(_("_Subject:"), view->subject, view->sizegroup),
			TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(view), view->subjectbar, FALSE, FALSE, 0);
}
static void
subject_load(JamView *view) {
	const char *subject = jam_doc_get_subject(view->doc);
	gtk_entry_set_text(GTK_ENTRY(view->subject), subject ? subject : "");
}
static void
subject_store(JamView *view) {
	const char *subject = gtk_entry_get_text(GTK_ENTRY(view->subject));
	if (subject[0] == 0) subject = NULL;
	jam_doc_set_subject(view->doc, subject);
}

static void
security_add(JamView *view) {
	view->security = secmgr_new(TRUE);
	secmgr_set_account(SECMGR(view->security), JAM_ACCOUNT_LJ(view->account));
	gtk_widget_show(view->security);
	gtk_box_pack_start(GTK_BOX(view->subjectbar), view->security, FALSE, FALSE, 0);
	gtk_box_reorder_child(GTK_BOX(view->subjectbar), view->security, 1);
}
static void
security_remove(JamView *view) {
	LJSecurity sec = {0};
	jam_doc_set_security(view->doc, &sec);
	gtk_widget_destroy(view->security);
	view->security = NULL;
}
static gboolean
security_visible(JamView *view) {
	return view->security != NULL;
}
static void
security_account_changed(JamView *view) {
	/* security shouldn't even be around for non-lj accounts, so the downcast to
	 * account_lj is ok. */
	secmgr_set_account(SECMGR(view->security), JAM_ACCOUNT_LJ(view->account));
}
static void
security_load(JamView *view) {
	LJSecurity sec = jam_doc_get_security(view->doc);
	if (sec.type != LJ_SECURITY_PUBLIC)
		show_meta(view, JAM_VIEW_SECURITY);
	if (security_visible(view))
		secmgr_security_set_force(SECMGR(view->security), &sec);
}
static void
security_store(JamView *view) {
	LJSecurity sec;
	secmgr_security_get(SECMGR(view->security), &sec);
	jam_doc_set_security(view->doc, &sec);
}

static void
datesel_add(JamView *view) {
	view->datesel = datesel_new();
	gtk_box_pack_start(GTK_BOX(view->subjectbar), view->datesel, FALSE, FALSE, 0);
	gtk_widget_show_all(view->datesel);
}
static void
datesel_remove(JamView *view) {
	jam_doc_set_time(view->doc, NULL);
	gtk_widget_destroy(view->datesel);
	view->datesel = NULL;
}
static gboolean
datesel_visible(JamView *view) {
	return view->datesel != NULL;
}
static void
datesel_load(JamView *view) {
	struct tm vtm = {0};
	gboolean backdated;
	jam_doc_get_time(view->doc, &vtm);
	backdated = jam_doc_get_backdated(view->doc);
	if (vtm.tm_year > 0 || backdated)
		show_meta(view, JAM_VIEW_DATESEL);
	if (datesel_visible(view)) {
		datesel_set_tm(DATESEL(view->datesel), &vtm);
		datesel_set_backdated(DATESEL(view->datesel), backdated);
	}
}
static void
datesel_store(JamView *view) {
	struct tm vtm = {0};
	datesel_get_tm(DATESEL(view->datesel), &vtm);
	jam_doc_set_time(view->doc, &vtm);
	jam_doc_set_backdated(view->doc,
			datesel_get_backdated(DATESEL(view->datesel)));
}


static void
moodpic_add(JamView *view) {
	if (view->moodpicbar)
		return;
	view->moodpicbar = gtk_hbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(view), view->moodpicbar, FALSE, FALSE, 0);
	gtk_box_reorder_child(GTK_BOX(view), view->moodpicbar, 1);
	gtk_widget_show(view->moodpicbar);
}
static void
moodpic_remove(JamView *view) {
	if (!view->moodpicbar)
		return;
	if (view->moodbox || view->pic)
		return;
	gtk_widget_destroy(view->moodpicbar);
	view->moodpicbar = NULL;
}

static void
mood_populate(JamView *view) {
	if (JAM_ACCOUNT_IS_LJ(view->account)) {
		/* load moods. */
		LJServer *server = jam_host_lj_get_server(JAM_HOST_LJ(jam_account_get_host(view->account)));
		LJMood *m;
		char *text;
		GList *strings = NULL;
		GSList *l;

		text = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(view->moodcombo)->entry), 0, -1);

		for (l = server->moods; l; l = l->next) {
			m = l->data;
			strings = g_list_insert_sorted(strings, m->name,
					(GCompareFunc)g_ascii_strcasecmp);
		}
		if (strings)
			gtk_combo_set_popdown_strings(GTK_COMBO(view->moodcombo), strings);

		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(view->moodcombo)->entry), text);
		g_free(text);
	}
}
static void
mood_add(JamView *view) {
	moodpic_add(view);

	/*view->moodbutton = gtk_button_new_with_label(_("Mood"));
	gtk_size_group_add_widget(view->sizegroup, view->moodbutton);*/

	view->moodcombo = gtk_combo_new();
	gtk_widget_set_usize(GTK_COMBO(view->moodcombo)->entry, 100, -1);
	mood_populate(view);

	/*view->moodbox = gtk_hbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(view->moodbox), view->moodbutton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(view->moodbox), view->moodcombo, TRUE, TRUE, 0);*/
	view->moodbox = labelled_box_new_sg(_("_Mood:"), view->moodcombo, view->sizegroup),
	gtk_widget_show_all(view->moodbox);

	gtk_box_pack_start(GTK_BOX(view->moodpicbar), view->moodbox, TRUE, TRUE, 0);
	if (view->pic)
		gtk_box_reorder_child(GTK_BOX(view->moodpicbar), view->moodbox, 0);
}
static void
mood_remove(JamView *view) {
	jam_doc_set_mood(view->doc, NULL);
	gtk_widget_destroy(view->moodbox);
	view->moodbox = view->moodbutton = view->moodcombo = NULL;
	moodpic_remove(view);
}
static gboolean
mood_visible(JamView *view) {
	return view->moodbox != NULL;
}
static void
mood_account_changed(JamView *view) {
	mood_populate(view);
}
static void
mood_load(JamView *view) {
	const char *mood = jam_doc_get_mood(view->doc);
	if (mood)
		show_meta(view, JAM_VIEW_MOOD);
	if (mood_visible(view))
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(view->moodcombo)->entry),
				mood ? mood : "");
}
static void
mood_store(JamView *view) {
	const char *mood = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(view->moodcombo)->entry));
	/* XXX temporary: set moodid based on mood string. */
	int id = lj_mood_id_from_name(
			jam_host_lj_get_server(JAM_HOST_LJ(jam_account_get_host(view->account))),
			mood);

	jam_doc_set_mood(view->doc, mood);
	if (id < 0) id = 0;
	jam_doc_set_moodid(view->doc, id);
}

static void
picture_populate(JamView *view) {
	GtkWidget *menu;
	GtkWidget *item;
	GSList *l;
	LJUser *user = jam_account_lj_get_user(JAM_ACCOUNT_LJ(view->account));

	menu = gtk_menu_new();
	item = gtk_menu_item_new_with_label(_("[default]"));
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	for (l = user->pickws; l; l = l->next) {
		item = gtk_menu_item_new_with_label(l->data);
		g_object_set_data(G_OBJECT(item), KEY_PICTUREKEYWORD, l->data);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(view->pic), menu);
}
static void
picture_add(JamView *view) {
	moodpic_add(view);
	view->pic = gtk_option_menu_new();
	picture_populate(view);
	view->picbox = labelled_box_new_sg(_("_Picture:"), view->pic, view->sizegroup);
	gtk_box_pack_start(GTK_BOX(view->moodpicbar), view->picbox, FALSE, FALSE, 0);
	gtk_widget_show_all(view->picbox);
}
static void
picture_remove(JamView *view) {
	jam_doc_set_picture(view->doc, NULL);
	gtk_widget_destroy(view->picbox);
	view->picbox = view->pic = NULL;
	moodpic_remove(view);
}
static gboolean
picture_visible(JamView *view) {
	return view->picbox != NULL;
}
static void
picture_account_changed(JamView *view) {
	picture_populate(view);
}
static void
picture_load(JamView *view) {
	const char *pic = jam_doc_get_picture(view->doc);
	GSList *l;
	int sel = 0;
	LJUser *user = jam_account_lj_get_user(JAM_ACCOUNT_LJ(view->account));

	if (pic) {
		for (l = user->pickws, sel = 1; l; l = l->next, sel++) {
			if (strcmp(pic, l->data) == 0)
				break;
		}
		if (l == NULL) sel = 0;
	}
	if (sel)
		show_meta(view, JAM_VIEW_PIC);
	if (picture_visible(view))
		gtk_option_menu_set_history(GTK_OPTION_MENU(view->pic), sel);
}

static void
picture_store(JamView *view) {
	int sel = gtk_option_menu_get_history(GTK_OPTION_MENU(view->pic));
	LJUser *user = jam_account_lj_get_user(JAM_ACCOUNT_LJ(view->account));
	const char *pickw = NULL;

	if (sel > 0)
		pickw = g_slist_nth(user->pickws, sel-1)->data;

	jam_doc_set_picture(view->doc, pickw);
}

static void
music_refresh_cb(JamView *view) {
	GError *err = NULL;
	gchar *music;

	if (conf.music_mpris)
		music = lj_dbus_mpris_current_music(jdbus, &err);
	else
		music = music_detect(&err);

	if (music) {
		gtk_entry_set_text(GTK_ENTRY(view->music), music);
		g_free(music);
	} else if (err) {
		GtkWindow *toplevel;
		toplevel = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view)));
		jam_warning(toplevel, _("Error detecting music: %s"), err->message);
		g_error_free(err);
	}
}

static void
music_add(JamView *view) {
	view->music = gtk_entry_new();
	view->musicbar = labelled_box_new_sg(_("_Music:"), view->music, view->sizegroup);
	if (conf.music_mpris || music_can_detect(NULL)) {
		GtkWidget *refresh = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
		g_signal_connect_swapped(G_OBJECT(refresh), "clicked",
				G_CALLBACK(music_refresh_cb), view);
		gtk_box_pack_start(GTK_BOX(view->musicbar),
				refresh, FALSE, FALSE, 0);
	}
	gtk_box_pack_start(GTK_BOX(view), view->musicbar, FALSE, FALSE, 0);
	gtk_box_reorder_child(GTK_BOX(view), view->musicbar, view->moodpicbar ? 2 : 1);
	gtk_widget_show_all(view->musicbar);
}
static void
music_remove(JamView *view) {
	jam_doc_set_music(view->doc, NULL);
	gtk_widget_destroy(view->musicbar);
	view->musicbar = view->music = NULL;
}
static gboolean
music_visible(JamView *view) {
	return view->musicbar != NULL;
}
static void
music_load(JamView *view) {
	const char *music = jam_doc_get_music(view->doc);
	if (music)
		show_meta(view, JAM_VIEW_MUSIC);
	if (music_visible(view))
		gtk_entry_set_text(GTK_ENTRY(view->music), music ? music : "");
}
static void
music_store(JamView *view) {
	const char *music = gtk_entry_get_text(GTK_ENTRY(view->music));
	if (music[0] == 0) music = NULL;
	jam_doc_set_music(view->doc, music);
}

static void tags_store(JamView *view);

static void
tags_select_cb(JamView *view) {
  gchar *tags;
  GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(view));

  tags = tags_dialog(toplevel,
		     JAM_ACCOUNT_LJ(view->account),
		     jam_doc_get_usejournal(view->doc),
		     (gchar *) gtk_entry_get_text(GTK_ENTRY(view->tags)));

  if (tags) {
      gtk_entry_set_text(GTK_ENTRY(view->tags), tags);
      tags_store(view);
  }
}

static void
location_add(JamView *view) {
	view->location = gtk_entry_new();
	view->locationbar = labelled_box_new_sg(_("_Location:"), view->location, view->sizegroup);
	gtk_box_pack_start(GTK_BOX(view), view->locationbar, FALSE, FALSE, 0);
	gtk_box_reorder_child(GTK_BOX(view), view->locationbar, view->moodpicbar ? 2 : 1);
	gtk_widget_show_all(view->locationbar);
}
static void
location_remove(JamView *view) {
	jam_doc_set_location(view->doc, NULL);
	gtk_widget_destroy(view->locationbar);
	view->locationbar = view->location = NULL;
}
static gboolean
location_visible(JamView *view) {
	return view->locationbar != NULL;
}
static void
location_load(JamView *view) {
	const char *location = jam_doc_get_location(view->doc);
	if (location)
		show_meta(view, JAM_VIEW_LOCATION);
	if (location_visible(view))
		gtk_entry_set_text(GTK_ENTRY(view->location), location ? location : "");
}
static void
location_store(JamView *view) {
	const char *location = gtk_entry_get_text(GTK_ENTRY(view->location));
	if (location[0] == 0) location = NULL;
	jam_doc_set_location(view->doc, location);
}

static void
tags_add(JamView *view) {
	GtkWidget *tagbutton;
	view->tags = gtk_entry_new();
	view->tagsbar = labelled_box_new_sg(_("_Tags:"), view->tags, view->sizegroup);
	gtk_box_pack_start(GTK_BOX(view), view->tagsbar, FALSE, FALSE, 0);
	tagbutton = gtk_button_new_with_label("...");
	g_signal_connect_swapped(G_OBJECT(tagbutton), "clicked",
				 G_CALLBACK(tags_select_cb), view);
	gtk_box_pack_start(GTK_BOX(view->tagsbar),
			   tagbutton, FALSE, FALSE, 0);
	gtk_box_reorder_child(GTK_BOX(view), view->tagsbar, view->musicbar ? 2 : 1);
	gtk_widget_show_all(view->tagsbar);
}
static void
tags_remove(JamView *view) {
	jam_doc_set_taglist(view->doc, NULL);
	gtk_widget_destroy(view->tagsbar);
	view->tagsbar = view->tags = NULL;
}
static gboolean
tags_visible(JamView *view) {
	return view->tagsbar != NULL;
}
static void
tags_load(JamView *view) {
	const char *tags = jam_doc_get_taglist(view->doc);
	if (tags)
		show_meta(view, JAM_VIEW_TAGS);
	if (tags_visible(view))
		gtk_entry_set_text(GTK_ENTRY(view->tags), tags ? tags : "");
}
static void
tags_store(JamView *view) {
	const char *tags = gtk_entry_get_text(GTK_ENTRY(view->tags));
	if (tags[0] == 0) tags = NULL;
	jam_doc_set_taglist(view->doc, tags);
}


static void
option_add(JamView *view) {
	if (view->optionbar)
		return;
	view->optionbar = gtk_hbox_new(FALSE, 12);
	gtk_box_pack_end(GTK_BOX(view), view->optionbar, FALSE, FALSE, 0);
	gtk_widget_show(view->optionbar);
}
static void
option_remove(JamView *view) {
	if (!view->optionbar)
		return;
	if (view->preformatted || view->datesel || view->commentsbox ||
			view->screeningbox)
		return;
	gtk_widget_destroy(view->optionbar);
	view->optionbar = NULL;
}

static void
preformatted_add(JamView *view) {
	option_add(view);
	view->preformatted = gtk_check_button_new_with_mnemonic(_("_Preformatted"));
	gtk_box_pack_start(GTK_BOX(view->optionbar), view->preformatted, FALSE, FALSE, 0);
	gtk_widget_show_all(view->preformatted);
}
static void
preformatted_remove(JamView *view) {
	jam_doc_set_preformatted(view->doc, FALSE);
	gtk_widget_destroy(view->preformatted);
	view->preformatted = NULL;
	option_remove(view);
}
static gboolean
preformatted_visible(JamView *view) {
	return view->preformatted != NULL;
}
static void
preformatted_load(JamView *view) {
	gboolean pre = jam_doc_get_preformatted(view->doc);
	if (pre)
		show_meta(view, JAM_VIEW_PREFORMATTED);
	if (preformatted_visible(view))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(view->preformatted),
				pre);
}
static void
preformatted_store(JamView *view) {
	jam_doc_set_preformatted(view->doc,
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(view->preformatted)));
}

static void
comments_add(JamView *view) {
	GtkWidget *menu, *item;
	static const char * items[] = {
		N_("Default"),
		N_("Don't Email"),
		N_("Disable")
	};
	int i;

	option_add(view);
	view->comments = gtk_option_menu_new();

	menu = gtk_menu_new();
	for (i = 0; i < sizeof(items)/sizeof(char*); i++) {
		item = gtk_menu_item_new_with_label(_(items[i]));
		gtk_widget_show(item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}
	gtk_option_menu_set_menu(GTK_OPTION_MENU(view->comments), menu);

	view->commentsbox = labelled_box_new(_("_Comments:"), view->comments);
	gtk_box_pack_start(GTK_BOX(view->optionbar), view->commentsbox, FALSE, FALSE, 0);
	gtk_widget_show_all(view->commentsbox);
}
static void
comments_remove(JamView *view) {
	jam_doc_set_comments(view->doc, LJ_COMMENTS_DEFAULT);
	gtk_widget_destroy(view->commentsbox);
	view->commentsbox = view->comments = NULL;
	option_remove(view);
}
static gboolean
comments_visible(JamView *view) {
	return view->commentsbox != NULL;
}
static void
comments_load(JamView *view) {
	LJCommentsType type = jam_doc_get_comments(view->doc);
	if (type != LJ_COMMENTS_DEFAULT)
		show_meta(view, JAM_VIEW_COMMENTS);
	if (comments_visible(view))
		gtk_option_menu_set_history(GTK_OPTION_MENU(view->comments), type);
}
static void
comments_store(JamView *view) {
	LJCommentsType type = gtk_option_menu_get_history(GTK_OPTION_MENU(view->comments));
	jam_doc_set_comments(view->doc, type);
}

static void
screening_add(JamView *view) {
	GtkWidget *menu, *item;
	static const char * items[] = {
		N_("Default"),
		N_("None"),
		N_("Anonymous Only"),
		N_("Non-Friends"),
		N_("All")
	};
	int i;

	option_add(view);
	view->screening = gtk_option_menu_new();

	menu = gtk_menu_new();
	for (i = 0; i < sizeof(items)/sizeof(char*); i++) {
		item = gtk_menu_item_new_with_label(_(items[i]));
		gtk_widget_show(item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}
	gtk_option_menu_set_menu(GTK_OPTION_MENU(view->screening), menu);

	view->screeningbox = labelled_box_new(_("Scr_eening:"), view->screening);
	gtk_box_pack_end(GTK_BOX(view->optionbar), view->screeningbox, FALSE, FALSE, 0);
	gtk_widget_show_all(view->screeningbox);
}
static void
screening_remove(JamView *view) {
	jam_doc_set_screening(view->doc, LJ_SCREENING_DEFAULT);
	gtk_widget_destroy(view->screeningbox);
	view->screeningbox = view->screening = NULL;
	option_remove(view);
}
static gboolean
screening_visible(JamView *view) {
	return view->screeningbox != NULL;
}
static void
screening_load(JamView *view) {
	LJCommentsType type = jam_doc_get_screening(view->doc);
	if (type != LJ_SCREENING_DEFAULT)
		show_meta(view, JAM_VIEW_SCREENING);
	if (screening_visible(view))
		gtk_option_menu_set_history(GTK_OPTION_MENU(view->screening), type);
}
static void
screening_store(JamView *view) {
	LJCommentsType type = gtk_option_menu_get_history(GTK_OPTION_MENU(view->screening));
	jam_doc_set_screening(view->doc, type);
}

static struct {
	char *name;
	gboolean lj_only;

	/* standard for all. */
	void     (*add)(JamView *view);
	void     (*remove)(JamView *view);
	gboolean (*visible)(JamView *view);

	void     (*load)(JamView *view);
	void     (*store)(JamView *view);

	/* only defined if it matters when the account changes. */
	void     (*account_changed)(JamView *view);
} metas[] = {
/* these must match JamViewMeta in jamview.h. */
#define STD(x) x##_add, x##_remove, x##_visible, x##_load, x##_store
	{ "subject",      TRUE, NULL, NULL, NULL, subject_load, subject_store, NULL },
	{ "security",     TRUE, STD(security),     security_account_changed },
	{ "mood",         TRUE, STD(mood),         mood_account_changed     },
	{ "picture",      TRUE, STD(picture),      picture_account_changed  },
	{ "music",        TRUE, STD(music),        NULL },
	{ "location",     TRUE, STD(location),     NULL },
	{ "tags",         TRUE, STD(tags),         NULL },
	{ "preformatted", TRUE, STD(preformatted), NULL },
	{ "datesel",      TRUE, STD(datesel),      NULL },
	{ "comments",     TRUE, STD(comments),     NULL },
	{ "screening",    TRUE, STD(screening),    NULL },
	{ 0 }
};

gboolean
jam_view_get_meta_visible(JamView *view, JamViewMeta meta) {
	if (!metas[meta].visible) return TRUE;
	return metas[meta].visible(view);
}

void
jam_view_toggle_meta(JamView *view, JamViewMeta meta, gboolean show) {
	if (show == metas[meta].visible(view)) return;
	if (show) metas[meta].add(view);
	else metas[meta].remove(view);
}

const char *
jam_view_meta_to_name(JamViewMeta meta) {
	return metas[meta].name;
}

gboolean
jam_view_meta_from_name(const char *name, JamViewMeta *meta) {
	int i;
	for (i = 0; i <= JAM_VIEW_META_LAST; i++) {
		if (strcmp(metas[i].name, name) == 0) {
			*meta = i;
			return TRUE;
		}
	}
	return FALSE;
}

void
jam_view_emit_conf(JamView *view) {
	int i;
	for (i = JAM_VIEW_META_FIRST; i <= JAM_VIEW_META_LAST; i++)
		g_signal_emit_by_name(view, "meta_toggle", i, metas[i].visible(view));
}

static void
jam_view_load_conf(JamView *view) {
	int meta;
	for (meta = JAM_VIEW_META_FIRST; meta <= JAM_VIEW_META_LAST; meta++) {
		if (conf.options.showmeta[meta] && !metas[meta].visible(view)) {
			if (JAM_ACCOUNT_IS_LJ(view->account) || !metas[meta].lj_only) {
				jam_view_toggle_meta(view, meta, TRUE);
				g_signal_emit_by_name(view, "meta_toggle", meta, TRUE);
			}
		} else if (!conf.options.showmeta[meta] && metas[meta].visible(view)) {
			jam_view_toggle_meta(view, meta, FALSE);
			g_signal_emit_by_name(view, "meta_toggle", meta, FALSE);
		}
	}
}

static void
jam_view_class_init(JamViewClass *viewclass) {
	signals[META_TOGGLE] = g_signal_new("meta_toggle",
			G_OBJECT_CLASS_TYPE(viewclass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(JamViewClass, meta_toggle),
			NULL, NULL,
			logjam_marshal_VOID__INT_BOOLEAN,
			G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_BOOLEAN);
}

static void
undo_cb(JamView *view) {
	if (undomgr_can_undo(view->undomgr))
		undomgr_undo(view->undomgr);
}
static void
redo_cb(JamView *view) {
	if (undomgr_can_redo(view->undomgr))
		undomgr_redo(view->undomgr);
}

static void
populate_entry_popup(GtkTextView *text, GtkMenu *menu, JamView *view) {
	GtkWidget *menu_item;

	g_return_if_fail(menu != NULL);
	g_return_if_fail(GTK_IS_MENU_SHELL(menu));

	menu_item = gtk_separator_menu_item_new();
	gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), menu_item);
	gtk_widget_show(menu_item);

	menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_REDO, NULL);
	gtk_widget_set_sensitive(menu_item,
			undomgr_can_redo(view->undomgr));
	g_signal_connect_swapped(G_OBJECT(menu_item), "activate",
			G_CALLBACK(redo_cb), view);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), menu_item);
	gtk_widget_show(menu_item);

	menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_UNDO, NULL);
	gtk_widget_set_sensitive(menu_item,
			undomgr_can_undo(view->undomgr));
	g_signal_connect_swapped(G_OBJECT(menu_item), "activate",
			G_CALLBACK(undo_cb), view);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), menu_item);
	gtk_widget_show(menu_item);
}

static void
jam_view_init(JamView *view) {
	gtk_box_set_spacing(GTK_BOX(view), 6);

	view->entry = gtk_text_view_new();
	view->undomgr = UNDOMGR(undomgr_new());

	gtk_text_view_set_editable(GTK_TEXT_VIEW(view->entry), TRUE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view->entry), GTK_WRAP_WORD);
	g_signal_connect(G_OBJECT(view->entry), "populate-popup",
		G_CALLBACK(populate_entry_popup), view);
	if (conf.uifont)
		jam_widget_set_font(view->entry, conf.uifont);

	undomgr_attach(UNDOMGR(view->undomgr), view->entry);

	view->sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	subject_add(view);
	jam_view_load_conf(view);
	gtk_box_pack_start(GTK_BOX(view), scroll_wrap(view->entry),
			TRUE, TRUE, 0);
}

GtkWidget*
jam_view_new(JamDoc *doc) {
	JamView *view = JAM_VIEW(g_object_new(jam_view_get_type(), NULL));

	jam_view_set_doc(view, doc);

	return GTK_WIDGET(view);
}

GObject*
jam_view_get_undomgr(JamView *view) {
	return G_OBJECT(view->undomgr);
}

static void
jam_view_account_changed(JamView *view) {
	int i;

	view->account = jam_doc_get_account(view->doc);

	/* notify all visible meta views that the account
	 * has changed, and hide those that shouldn't be shown. */
	for (i = JAM_VIEW_META_FIRST; i <= JAM_VIEW_META_LAST; i++) {
		if (!metas[i].visible(view))
			continue;
		if (!JAM_ACCOUNT_IS_LJ(view->account) && metas[i].lj_only) {
			jam_view_toggle_meta(view, i, FALSE);
		} else {
			if (metas[i].account_changed && metas[i].visible(view))
				metas[i].account_changed(view);
		}
	}
}

static void
jam_view_load_doc(JamView *view) {
	int i;
	/* if the current account type doesn't support some meta views,
	 * they should've already been hidden by _account_changed. */
	for (i = 0; i < JAM_VIEW_META_COUNT; i++) {
		if (!metas[i].lj_only || JAM_ACCOUNT_IS_LJ(view->account))
			metas[i].load(view);
	}
}

static void
jam_view_store_doc(JamView *view) {
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	char *event;
	int i;

	for (i = 0; i < JAM_VIEW_META_COUNT; i++) {
		if (jam_view_get_meta_visible(view, i)) {
			if (metas[i].store)
				metas[i].store(view);
		}
	}

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view->entry));
	gtk_text_buffer_get_bounds(buffer, &start, &end);
	event = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);

	jam_doc_set_event(view->doc, event);
	g_free(event);
}

void
jam_view_set_doc(JamView *view, JamDoc *doc) {
	view->doc = doc;

	undomgr_detach(view->undomgr, view->entry);

	if (view->account != jam_doc_get_account(doc))
		jam_view_account_changed(view);
	g_signal_connect_swapped(G_OBJECT(doc), "notify::account",
			G_CALLBACK(jam_view_account_changed), view);
	g_signal_connect_swapped(G_OBJECT(doc), "update_doc",
			G_CALLBACK(jam_view_store_doc), view);
	g_signal_connect_swapped(G_OBJECT(doc), "entry_changed",
			G_CALLBACK(jam_view_load_doc), view);

	jam_view_load_conf(view);
	jam_view_load_doc(view);

	/* XXX: gtkspell gets confused if you detach its document,
	 * so we detach the spell checker completely, then reattach
	 * it at the end. */
#ifdef HAVE_GTKSPELL
	if (conf.options.usespellcheck && view->entry) {
		GtkSpell *spell;
		spell = gtkspell_get_from_text_view(GTK_TEXT_VIEW(view->entry));
		if (spell)
			gtkspell_detach(spell);
	}
#endif /* HAVE_GTKSPELL */

	gtk_text_view_set_buffer(GTK_TEXT_VIEW(view->entry),
			jam_doc_get_text_buffer(doc));

#ifdef HAVE_GTKSPELL
	if (conf.options.usespellcheck) {
		GError *err = NULL;
		if (gtkspell_new_attach(GTK_TEXT_VIEW(view->entry),
					conf.spell_language, &err) == NULL) {
			GtkWindow *toplevel;
			toplevel = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view)));
			jam_warning(toplevel,
					_("GtkSpell error: %s"), err->message);
			g_error_free(err);
		}
	}
#endif /* HAVE_GTKSPELL */

	undomgr_attach(view->undomgr, view->entry);
}

void
jam_view_settings_changed(JamView *view) {
#ifdef HAVE_GTKSPELL
	GtkSpell *spell;
	GError *err = NULL;
	spell = gtkspell_get_from_text_view(GTK_TEXT_VIEW(view->entry));
	if (conf.options.usespellcheck) {
		GtkWindow *toplevel;
		toplevel = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view)));
		if (spell) {
			if (!gtkspell_set_language(spell, conf.spell_language, &err)) {
				jam_warning(toplevel,
						_("GtkSpell error: %s"), err->message);
				g_error_free(err);
			}
		} else {
			GError *err = NULL;
			if (gtkspell_new_attach(GTK_TEXT_VIEW(view->entry), conf.spell_language , &err) == NULL) {
				jam_warning(toplevel,
						_("GtkSpell error: %s"), err->message);
				conf.options.usespellcheck = FALSE;
				g_error_free(err);
			}
		}
	} else {
		if (spell)
			gtkspell_detach(spell);
	}
#endif
	if (conf.uifont)
		jam_widget_set_font(view->entry, conf.uifont);

	if (view->musicbar) {
	    /* There are many cleaner ways to add/remove the Refresh button when
	     * the music preference has changed, but this works fine. */
		music_remove(view);
		music_add(view);
	}
}

GType
jam_view_get_type(void) {
	static GType new_type = 0;
	if (!new_type) {
		const GTypeInfo new_info = {
			sizeof(JamViewClass),
			NULL,
			NULL,
			(GClassInitFunc) jam_view_class_init,
			NULL,
			NULL,
			sizeof(JamView),
			0,
			(GInstanceInitFunc) jam_view_init,
		};
		new_type = g_type_register_static(GTK_TYPE_VBOX,
				"JamView", &new_info, 0);
	}
	return new_type;
}

