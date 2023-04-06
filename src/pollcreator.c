/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "gtk-all.h"
#include <stdlib.h> /* atoi */

#include "conf.h"
#include "jam.h"
#include "poll.h"
#include "pollcreator.h"
#include "util-gtk.h"
#include "util.h"

typedef struct _PollDlg PollDlg;

struct _PollDlg {
	GtkDialog dlg;                 /* Parent widget */

	GtkWidget *pollname, *viewers, *voters;
	JamReorderable questions;
};

#define POLLDLG(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), polldlg_get_type(), PollDlg))

/* macro to generate prototypes for type and instance init functions, as
 * well as the actual type function, in one swell foop. */
#define MAKETYPE(fname, newtypeclass, classinitf, instanceinitf, parenttype, parentclass) \
static void  instanceinitf(GtkWidget *w);                   \
static GType fname (void) G_GNUC_CONST;                     \
static GType                                                \
fname(void) {                                                   \
	static GType new_type = 0;                              \
	if (!new_type) {                                        \
		const GTypeInfo new_info = {                        \
			sizeof (parentclass),                           \
			NULL,                                           \
			NULL,                                           \
			(GClassInitFunc)classinitf,                     \
			NULL,                                           \
			NULL,                                           \
			sizeof (newtypeclass),                          \
			0,                                              \
			(GInstanceInitFunc) instanceinitf,              \
		};                                                  \
		new_type = g_type_register_static(parenttype,       \
				#newtypeclass, &new_info, 0);               \
	}                                                       \
	return new_type;                                        \
}

#define MENU_ADD(menu, mnemonic)                    \
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),     \
		gtk_menu_item_new_with_mnemonic(mnemonic));

/* Poll Creator container */
MAKETYPE(polldlg_get_type, PollDlg, NULL, polldlg_init,
         GTK_TYPE_DIALOG, GtkDialogClass)

static GtkWidget*
make_pollmeta(PollDlg *pdlg) {
	GtkWidget *hbox, *vbox, *menu;

	vbox = gtk_vbox_new(FALSE, 6);

	pdlg->pollname = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new(_("Poll _name: "),
				pdlg->pollname),
			FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 18);

	pdlg->voters = gtk_option_menu_new();
	menu = gtk_menu_new();
	MENU_ADD(menu, _("All users"));
	MENU_ADD(menu, _("Friends"));
	gtk_option_menu_set_menu(GTK_OPTION_MENU(pdlg->voters), menu);
	gtk_box_pack_start(GTK_BOX(hbox),
			labelled_box_new(_("V_oters:"), pdlg->voters),
			FALSE, FALSE, 0);

	pdlg->viewers = gtk_option_menu_new();
	menu = gtk_menu_new();
	MENU_ADD(menu, _("All users"));
	MENU_ADD(menu, _("Friends"));
	MENU_ADD(menu, _("Nobody"));
	gtk_option_menu_set_menu(GTK_OPTION_MENU(pdlg->viewers), menu);
	gtk_box_pack_end(GTK_BOX(hbox),
			labelled_box_new(_("_Results visible to:"), pdlg->viewers),
			FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	return vbox;
}

static void
add_question(PollDlg *pdlg, PollQuestion *pq) {
	GtkTreeIter iter;
	gtk_list_store_append(pdlg->questions.store, &iter);
	gtk_list_store_set(pdlg->questions.store, &iter,
			0, pq->question,
			1, pq,
			-1);
}

static void
add_multi_cb(PollDlg *pdlg) {
	PollQuestion *pq;
	pq = pollmultidlg_run(GTK_WINDOW(pdlg), NULL);
	if (pq) add_question(pdlg, pq);
}
static void
add_scale_cb(PollDlg *pdlg) {
	PollQuestion *pq;
	pq = pollscaledlg_run(GTK_WINDOW(pdlg), NULL);
	if (pq) add_question(pdlg, pq);
}
static void
add_text_cb(PollDlg *pdlg) {
	PollQuestion *pq;
	pq = polltextdlg_run(GTK_WINDOW(pdlg), NULL);
	if (pq) add_question(pdlg, pq);
}

static void
add_question_cb(PollDlg *pdlg) {
	GtkWidget *menu, *item;

	menu = gtk_menu_new();

	item = gtk_menu_item_new_with_label(_("Multiple Choice Question"));
	g_signal_connect_swapped(G_OBJECT(item), "activate",
			G_CALLBACK(add_multi_cb), pdlg);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	item = gtk_menu_item_new_with_label(_("Range Question"));
	g_signal_connect_swapped(G_OBJECT(item), "activate",
			G_CALLBACK(add_scale_cb), pdlg);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	item = gtk_menu_item_new_with_label(_("Text Question"));
	g_signal_connect_swapped(G_OBJECT(item), "activate",
			G_CALLBACK(add_text_cb), pdlg);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	gtk_widget_show_all(menu);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
			0 /* don't know button */, gtk_get_current_event_time());
}
static void
edit_question_cb(PollDlg *pdlg) {
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;
	PollQuestion *pq;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(pdlg->questions.view));

	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;
	gtk_tree_model_get(model, &iter, 1, &pq, -1);
	switch (pq->type) {
		case PQ_RADIO:
		case PQ_COMBO:
		case PQ_CHECK:
			pq = pollmultidlg_run(GTK_WINDOW(pdlg),
					(PollQuestionMulti*)pq);
			break;
		case PQ_TEXT:
			pq = polltextdlg_run(GTK_WINDOW(pdlg),
					(PollQuestionText*)pq);
			break;
		case PQ_SCALE:
			pq = pollscaledlg_run(GTK_WINDOW(pdlg),
					(PollQuestionScale*)pq);
			break;
		default:
			return;
	}
	if (pq)
		gtk_list_store_set(pdlg->questions.store, &iter,
				0, pq->question,
				1, pq,
				-1);
}
static void
remove_question_cb(PollDlg *pdlg) {
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;
	PollQuestion *pq;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(pdlg->questions.view));
	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;
	gtk_tree_model_get(model, &iter, 1, &pq, -1);
	poll_question_free(pq);
	gtk_list_store_remove(pdlg->questions.store, &iter);
}

static void
make_list(PollDlg *pdlg) {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	pdlg->questions.store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
	jam_reorderable_make(&pdlg->questions);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(pdlg->questions.view), FALSE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(pdlg->questions.view), TRUE);
	g_object_unref(pdlg->questions.store);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", 0);
	gtk_tree_view_append_column(GTK_TREE_VIEW(pdlg->questions.view), column);

	g_signal_connect_data(G_OBJECT(pdlg->questions.add), "clicked",
			G_CALLBACK(add_question_cb), pdlg,
			NULL, G_CONNECT_AFTER|G_CONNECT_SWAPPED);
	g_signal_connect_swapped(G_OBJECT(pdlg->questions.edit), "clicked",
			G_CALLBACK(edit_question_cb), pdlg);
	g_signal_connect_swapped(G_OBJECT(pdlg->questions.remove), "clicked",
			G_CALLBACK(remove_question_cb), pdlg);
}

static GtkWidget*
make_content(PollDlg *pdlg) {
	GtkWidget *vbox;
	GtkWidget *label;

	vbox = gtk_vbox_new(FALSE, 6);
	label = gtk_label_new_with_mnemonic(_("Poll _Questions:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	make_list(pdlg);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), pdlg->questions.view);
	gtk_box_pack_start(GTK_BOX(vbox), pdlg->questions.box, TRUE, TRUE, 0);

	return vbox;
}

static void
polldlg_init(GtkWidget *w) {
	PollDlg *pdlg  = POLLDLG(w);
	GtkWidget *vbox;

	gtk_window_set_title(GTK_WINDOW(pdlg), _("Poll Creator"));

	vbox = gtk_vbox_new(FALSE, 12);

	gtk_box_pack_start(GTK_BOX(vbox), make_pollmeta(pdlg),
			FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), make_content(pdlg),
			TRUE, TRUE, 0);

	gtk_dialog_add_buttons(GTK_DIALOG(pdlg),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			_("_Generate"),   GTK_RESPONSE_OK,
			NULL);

	jam_dialog_set_contents(GTK_DIALOG(pdlg), vbox);
}

static GtkWidget*
polldlg_new(GtkWidget *parent) {
	PollDlg *pdlg  = POLLDLG(g_object_new(polldlg_get_type(), NULL));
	gtk_window_set_transient_for(GTK_WINDOW(pdlg), GTK_WINDOW(parent));
	return GTK_WIDGET(pdlg);
}

#if 0
static PQType
_get_pqtype_from_name(G_CONST_RETURN gchar *text) {
	struct pqtypeinfo *p = (struct pqtypeinfo *)pqtype;
	while (strcmp(text, p->textname)) {
		if (p->id == PQ_UNDEF)
			g_error("unknown type [%s]", text);

		p++;
	}
	return p->id;
}

static void
_validate_text(PollQText *pqt, GtkWidget *anserrl, gboolean *rc) {
	gchar *data = NULL;
	gchar *tmp  = NULL;

	data = _gEText(pqt->SizeLJEntry);
	if (data && *data != '\0' &&
			g_ascii_strcasecmp(data, tmp = g_strdup_printf("%d", atoi(data)))) {
		gtk_label_set_markup(GTK_LABEL(anserrl),
				_("<b>[Text parameters must be integer.]</b>"));
		*rc = FALSE;
		gtk_widget_show(anserrl);
	}
	g_free(tmp);
	tmp = NULL;

	data = _gEText(pqt->MaxLJEntry);
	if (data && *data != '\0' &&
			g_ascii_strcasecmp(data, tmp = g_strdup_printf("%d", atoi(data)))) {
		gtk_label_set_markup(GTK_LABEL(anserrl),
				_("<b>[Text parameters must be integer.]</b>"));
		*rc = FALSE;
		gtk_widget_show(anserrl);
	}
	g_free(tmp);
}
#endif

#if 0
static void
_validate_scale(PollScaleDlg *psdlg, GtkWidget *anserrl, gboolean *rc) {
	gint from, to, by;
	from = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(psdlg->FromSpin));
	to   = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(psdlg->ToSpin));
	by   = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(psdlg->BySpin));

	/* make sure 20 elements cover scale range */
	if (( (by > 0) && ((from + 19*by) < to) ) ||
			( (by < 0) && ((from + 19*by) > to)) ) {
		gtk_label_set_markup(GTK_LABEL(anserrl),
				_("<b>[Too many elements in scale range. Either "
				"move \"From\" and \"To\" values closer or increase "
				"\"By\".]</b>"));
		*rc = FALSE;
		gtk_widget_show(anserrl);
	}

	if (( (by > 0) && (from > to) ) ||
			( (by < 0) && (from < to) )) {
		gtk_label_set_markup(GTK_LABEL(anserrl),
				_("<b>[\"By\" has wrong sign.]</b>"));
		*rc = FALSE;
		gtk_widget_show(anserrl);
	}

	if (by == 0) {
		gtk_label_set_markup(GTK_LABEL(anserrl),
				_("<b>[\"By\" cannot be zero.]</b>"));
		*rc = FALSE;
		gtk_widget_show(anserrl);
	}
}
#endif

static void
generate_multi(PollQuestion *pq, GString *ptext) {
	PollQuestionMulti *pqm = (PollQuestionMulti*)pq;
	GSList *l;

	static char* qtype_to_string[] = {
		"radio",
		"drop",
		"check"
	};

	g_string_append_printf(ptext, "<lj-pq type=\"%s\">\n",
			qtype_to_string[pq->type]);
	g_string_append_printf(ptext, "%s\n", pq->question);

	for (l = pqm->answers; l; l = l->next)
		g_string_append_printf(ptext, "<lj-pi>%s</lj-pi>\n", (char*)l->data);
	g_string_append(ptext, "</lj-pq>\n\n");
}

static void
generate_text(PollQuestion *pq, GString *ptext) {
	PollQuestionText *pqt = (PollQuestionText*)pq;

	g_string_append_printf(ptext, "<lj-pq type=\"text\"");

	if (pqt->size)
		g_string_append_printf(ptext, " size=\"%d\"", pqt->size);
	if (pqt->width)
		g_string_append_printf(ptext, " maxlength=\"%d\"", pqt->width);

	g_string_append_printf(ptext, ">\n%s\n</lj-pq>\n\n", pq->question);
}

static void
generate_scale(PollQuestion *pq, GString *ptext) {
	PollQuestionScale *pqs = (PollQuestionScale*)pq;
	g_string_append_printf(ptext,
			"<lj-pq type=\"scale\" from=\"%d\" to=\"%d\" by=\"%d\">"
			"\n%s\n</lj-pq>\n\n",
			pqs->from, pqs->to, pqs->by, pq->question);
}

static GString*
poll_to_text(Poll *poll) {
	GString *ptext;
	GSList *l;

	static char* psec_to_string[] = {
		"all",
		"friends",
		"none"
	};

	ptext = g_string_sized_new(2048);

	/* poll open tag */
	g_string_append(ptext, "<lj-poll ");
	if (poll->name)
		g_string_append_printf(ptext, "name=\"%s\" ", poll->name);

	/* ...with metadata */
	g_string_append_printf(ptext, "whoview=\"%s\" ",
			psec_to_string[poll->viewers]);
	g_string_append_printf(ptext, "whovote=\"%s\">\n\n",
			psec_to_string[poll->voters]);

	/* poll questions */
	for (l = poll->questions; l; l = l->next) {
		PollQuestion *pq = l->data;

		switch (pq->type) {
			case PQ_RADIO:
			case PQ_CHECK:
			case PQ_COMBO:
				generate_multi(pq, ptext);
				break;
			case PQ_TEXT:
				generate_text (pq, ptext);
				break;
			case PQ_SCALE:
				generate_scale(pq, ptext);
				break;
		}
	}

	g_string_append(ptext, "</lj-poll>\n");

	return ptext;
}

static Poll*
poll_read(PollDlg *pdlg) {
	Poll *poll;
	GtkTreeModel *model = GTK_TREE_MODEL(pdlg->questions.store);
	GtkTreeIter iter;

	poll = g_new0(Poll, 1);

	poll->name = gtk_editable_get_chars(GTK_EDITABLE(pdlg->pollname), 0, -1);
	if (poll->name && !poll->name[0])
		string_replace(&poll->name, NULL);

	poll->viewers = gtk_option_menu_get_history(GTK_OPTION_MENU(pdlg->viewers));
	poll->voters = gtk_option_menu_get_history(GTK_OPTION_MENU(pdlg->voters));

	if (gtk_tree_model_get_iter_first(model, &iter)) {
		/* this is probably O(n^2) or something like that.
		 * but there hopefully won't be that many answers. */
		do {
			PollQuestion *pq;
			gtk_tree_model_get(model, &iter, 1, &pq, -1);
			poll->questions = g_slist_append(poll->questions, pq);
		} while (gtk_tree_model_iter_next(model, &iter));
	}

	return poll;
}

static void
poll_free(Poll *poll) {
	g_free(poll->name);
	g_slist_foreach(poll->questions, (GFunc)poll_question_free, NULL);
	g_slist_free(poll->questions);
	g_free(poll);
}

static GString*
polltext_generate(PollDlg *pdlg) {
	Poll *poll;
	GString *str;

	poll = poll_read(pdlg);
	str = poll_to_text(poll);
	poll_free(poll);

	return str;
}

/* The only public function in this module.
 *
 * Run the Poll Creator dialog.  If a poll is made, paste it
 * into the main LogJam entry at the current location. */
void
run_poll_creator_dlg(JamWin *jw) {
	GString       *polltext = NULL;
	PollDlg       *pdlg     = POLLDLG(polldlg_new(GTK_WIDGET(jw)));
	GtkTextBuffer *buffer   = jam_doc_get_text_buffer(jw->doc);

	if (gtk_dialog_run(GTK_DIALOG(pdlg)) == GTK_RESPONSE_OK) {
		polltext = polltext_generate(pdlg);
		gtk_text_buffer_delete_selection(buffer, FALSE, FALSE);
		gtk_text_buffer_insert_at_cursor(buffer,
				polltext->str, polltext->len);
		g_string_free(polltext, TRUE);
	}

	gtk_widget_destroy(GTK_WIDGET(pdlg));
}

