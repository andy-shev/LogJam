/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include "util-gtk.h"
#include "poll.h"

typedef struct {
	GtkWidget *dlg;

	GtkWidget *typemenu;
	GtkWidget *question;
	JamReorderable answers;
} PollMultiDlg;

typedef struct {
	GtkWidget *dlg;

	GtkWidget *question;
	GtkWidget *size, *width;
} PollTextDlg;

typedef struct {
	GtkWidget *dlg;

	GtkWidget *question;
	GtkWidget *from, *to, *by;
} PollScaleDlg;

static char*
pollmultidlg_option(GtkWindow *parent, const char *current) {
	GtkWidget *dlg, *vbox, *label, *view;
	GtkTextBuffer *buffer;
	char *newtext = NULL;

	dlg = gtk_dialog_new_with_buttons(_("Multi Poll Option"),
			parent, GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
	jam_win_set_size(GTK_WINDOW(dlg), 300, -1);

	vbox = gtk_vbox_new(FALSE, 6);

	label = gtk_label_new_with_mnemonic(_("O_ption:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	view = gtk_text_view_new();
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD);
	if (current)
		gtk_text_buffer_insert_at_cursor(buffer, current, -1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), view);
	gtk_box_pack_start(GTK_BOX(vbox), scroll_wrap(view), TRUE, TRUE, 0);

	jam_dialog_set_contents(GTK_DIALOG(dlg), vbox);

	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
		GtkTextIter start, end;
		gtk_text_buffer_get_bounds(buffer, &start, &end);
		newtext = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	}
	gtk_widget_destroy(dlg);
	return newtext;
}

static void
add_option_cb(PollMultiDlg *pmdlg) {
	char *newtext;
	GtkTreeIter iter;

	newtext = pollmultidlg_option(GTK_WINDOW(pmdlg->dlg), NULL);
	if (!newtext) return;
	gtk_list_store_append(pmdlg->answers.store, &iter);
	gtk_list_store_set(pmdlg->answers.store, &iter, 0, newtext, -1);
	g_free(newtext);
}
static void
edit_option_cb(PollMultiDlg *pmdlg) {
	GtkTreeSelection *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *text, *newtext;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(pmdlg->answers.view));

	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;
	gtk_tree_model_get(model, &iter, 0, &text, -1);
	newtext = pollmultidlg_option(GTK_WINDOW(pmdlg->dlg), text);
	if (newtext)
		gtk_list_store_set(pmdlg->answers.store, &iter, 0, newtext, -1);
	g_free(text);
	g_free(newtext);
}
static void
remove_option_cb(PollMultiDlg *pmdlg) {
	GtkTreeSelection *sel;
	GtkTreeIter iter;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(pmdlg->answers.view));
	if (!gtk_tree_selection_get_selected(sel, NULL, &iter))
		return;
	gtk_list_store_remove(pmdlg->answers.store, &iter);
}

static void
pollmulti_make_list(PollMultiDlg *pmdlg) {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	pmdlg->answers.store = gtk_list_store_new(1, G_TYPE_STRING);

	jam_reorderable_make(&pmdlg->answers);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(pmdlg->answers.view), FALSE);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(pmdlg->answers.view), TRUE);
	g_object_unref(pmdlg->answers.store);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", 0);
	gtk_tree_view_append_column(GTK_TREE_VIEW(pmdlg->answers.view), column);

	g_signal_connect_swapped(G_OBJECT(pmdlg->answers.add), "clicked",
			G_CALLBACK(add_option_cb), pmdlg);
	g_signal_connect_swapped(G_OBJECT(pmdlg->answers.edit), "clicked",
			G_CALLBACK(edit_option_cb), pmdlg);
	g_signal_connect_swapped(G_OBJECT(pmdlg->answers.remove), "clicked",
			G_CALLBACK(remove_option_cb), pmdlg);
}

static void
pollmultidlg_init(PollMultiDlg *pmdlg, GtkWindow *parent) {
	GtkWidget *mainbox, *vbox, *paned;
	GtkWidget *menu, *label;

	pmdlg->dlg = gtk_dialog_new_with_buttons(_("Multi Poll Question"),
			parent, GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
	jam_win_set_size(GTK_WINDOW(pmdlg->dlg), 500, -1);

	mainbox = gtk_vbox_new(FALSE, 12);

	menu = gtk_menu_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
		gtk_menu_item_new_with_mnemonic(_("Choose best answer (_radio buttons)")));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
		gtk_menu_item_new_with_mnemonic(_("Choose best answer (_drop-down menu)")));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
		gtk_menu_item_new_with_mnemonic(_("Select _all that apply (checkboxes)")));

	pmdlg->typemenu = gtk_option_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(pmdlg->typemenu), menu);
	gtk_box_pack_start(GTK_BOX(mainbox),
			labelled_box_new_expand(_("Choice _Type:"), pmdlg->typemenu, TRUE),
			FALSE, FALSE, 0);

	paned = gtk_hpaned_new();
	gtk_box_pack_start(GTK_BOX(mainbox), paned, TRUE, TRUE, 0);

	vbox = gtk_vbox_new(FALSE, 6);
	label = gtk_label_new_with_mnemonic(_("_Question:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	pmdlg->question = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(pmdlg->question), GTK_WRAP_WORD);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), pmdlg->question);
	gtk_box_pack_start(GTK_BOX(vbox),
			scroll_wrap(pmdlg->question),
			TRUE, TRUE, 0);

	gtk_paned_pack1(GTK_PANED(paned), vbox, TRUE, FALSE);

	vbox = gtk_vbox_new(FALSE, 6);
	pollmulti_make_list(pmdlg);
	label = gtk_label_new_with_mnemonic(_("O_ptions:"));
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), pmdlg->answers.view);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), pmdlg->answers.box, TRUE, TRUE, 0);

	gtk_paned_pack2(GTK_PANED(paned), vbox, TRUE, FALSE);

	jam_dialog_set_contents(GTK_DIALOG(pmdlg->dlg), mainbox);
}

PollQuestion*
pollmultidlg_run(GtkWindow *parent, PollQuestionMulti *qm) {
	PollQuestion *q = NULL;
	GtkTextBuffer *buffer;
	STACK(PollMultiDlg, pmdlg);

	pollmultidlg_init(pmdlg, parent);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pmdlg->question));
	if (qm) {
		GSList *l;
		GtkTreeIter iter;
		q = (PollQuestion*)qm;
		gtk_option_menu_set_history(GTK_OPTION_MENU(pmdlg->typemenu), q->type);
		gtk_text_buffer_insert_at_cursor(buffer, q->question, -1);
		for (l = qm->answers; l; l = l->next) {
			gtk_list_store_append(pmdlg->answers.store, &iter);
			gtk_list_store_set(pmdlg->answers.store, &iter, 0, l->data, -1);
		}
	}

	if (gtk_dialog_run(GTK_DIALOG(pmdlg->dlg)) == GTK_RESPONSE_OK) {
		GtkTextIter start, end;
		GtkTreeModel *model = GTK_TREE_MODEL(pmdlg->answers.store);
		GtkTreeIter iter;

		if (qm == NULL)
			qm = g_new0(PollQuestionMulti, 1);
		q = (PollQuestion*)qm;
		q->type = gtk_option_menu_get_history(GTK_OPTION_MENU(pmdlg->typemenu));

		gtk_text_buffer_get_bounds(buffer, &start, &end);
		g_free(q->question);
		q->question = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

		g_slist_foreach(qm->answers, (GFunc)g_free, NULL);
		g_slist_free(qm->answers);
		qm->answers = NULL;
		if (gtk_tree_model_get_iter_first(model, &iter)) {
			/* this is probably O(n^2) or something like that.
			 * but there hopefully won't be that many answers. */
			do {
				char *text;
				gtk_tree_model_get(model, &iter, 0, &text, -1);
				qm->answers = g_slist_append(qm->answers, text);
			} while (gtk_tree_model_iter_next(model, &iter));
		}
	}
	gtk_widget_destroy(GTK_WIDGET(pmdlg->dlg));
	return q;
}

static void
polltextdlg_init(PollTextDlg *ptdlg, GtkWindow *parent) {
	GtkWidget *mainbox, *vbox;
	GtkWidget *label, *view;

	ptdlg->dlg = gtk_dialog_new_with_buttons(_("Text Poll Question"),
			parent, GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);

	mainbox = gtk_vbox_new(FALSE, 12);

	vbox = gtk_vbox_new(FALSE, 6);

	label = gtk_label_new_with_mnemonic(_("_Question:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	ptdlg->question = view = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), view);
	gtk_box_pack_start(GTK_BOX(vbox), scroll_wrap(view), TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(mainbox), vbox, TRUE, TRUE, 0);

	vbox = gtk_vbox_new(FALSE, 6);

	ptdlg->size = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(ptdlg->size), 4);
	ptdlg->width = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(ptdlg->width), 4);

	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new(_("_Text field size (optional): "),
				ptdlg->size),
			FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new(_("_Maximum text length (optional): "),
				ptdlg->width),
			FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(mainbox), vbox, FALSE, FALSE, 0);

	jam_dialog_set_contents(GTK_DIALOG(ptdlg->dlg), mainbox);
}

PollQuestion*
polltextdlg_run(GtkWindow *parent, PollQuestionText *pqt) {
	STACK(PollTextDlg, ptdlg);
	PollQuestion *q = NULL;
	GtkTextBuffer *buffer;

	polltextdlg_init(ptdlg, parent);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ptdlg->question));
	if (pqt) {
		char *text;
		q = (PollQuestion*)pqt;

		gtk_text_buffer_insert_at_cursor(buffer, q->question, -1);

		if (pqt->size) {
			text = g_strdup_printf("%d", pqt->size);
			gtk_entry_set_text(GTK_ENTRY(ptdlg->size), text);
			g_free(text);
		}
		if (pqt->width) {
			text = g_strdup_printf("%d", pqt->width);
			gtk_entry_set_text(GTK_ENTRY(ptdlg->width), text);
			g_free(text);
		}
	}

	if (gtk_dialog_run(GTK_DIALOG(ptdlg->dlg)) == GTK_RESPONSE_OK) {
		GtkTextIter start, end;
		const char *text;

		if (!pqt)
			pqt = g_new0(PollQuestionText, 1);
		q = (PollQuestion*)pqt;
		q->type = PQ_TEXT;

		gtk_text_buffer_get_bounds(buffer, &start, &end);
		g_free(q->question);
		q->question = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

		text = gtk_entry_get_text(GTK_ENTRY(ptdlg->size));
		if (text && text[0])
			pqt->size = atoi(text);

		text = gtk_entry_get_text(GTK_ENTRY(ptdlg->width));
		if (text && text[0])
			pqt->width = atoi(text);
	}
	gtk_widget_destroy(GTK_WIDGET(ptdlg->dlg));
	return q;
}

static void
pollscaledlg_init(PollScaleDlg *psdlg, GtkWindow *parent) {
	GtkWidget *mainbox, *vbox;
	GtkWidget *label;
	GtkSizeGroup *sizegroup;

	GtkAdjustment *adj1 = (GtkAdjustment*)gtk_adjustment_new(
			1.0,  -32000.0, 32000.0, 1.0, 5.0, 0);
	GtkAdjustment *adj2 = (GtkAdjustment*)gtk_adjustment_new(
			10.0, -32000.0, 32000.0, 1.0, 5.0, 0);
	GtkAdjustment *adj3 = (GtkAdjustment*)gtk_adjustment_new(
			1.0,  -32000.0, 32000.0, 1.0, 5.0, 0);

	psdlg->dlg = gtk_dialog_new_with_buttons(_("Scale Poll Question"),
			parent, GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
	jam_win_set_size(GTK_WINDOW(psdlg->dlg), 400, -1);

	mainbox = gtk_hbox_new(FALSE, 12);

	vbox = gtk_vbox_new(FALSE, 6);
	label = gtk_label_new_with_mnemonic(_("_Question:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	psdlg->question = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(psdlg->question), GTK_WRAP_WORD);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), psdlg->question);
	gtk_box_pack_start(GTK_BOX(vbox),
			scroll_wrap(psdlg->question),
			TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(mainbox), vbox, TRUE, TRUE, 0);

	vbox = gtk_vbox_new(FALSE, 6);
	sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	psdlg->from = gtk_spin_button_new(adj1, 1.0, 0);
	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new_sg(_("_From:"), psdlg->from, sizegroup),
			FALSE, FALSE, 0);
	psdlg->to = gtk_spin_button_new(adj2, 1.0, 0);
	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new_sg(_("_To:"), psdlg->to, sizegroup),
			FALSE, FALSE, 0);
	psdlg->by = gtk_spin_button_new(adj3, 1.0, 0);
	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new_sg(_("_By:"), psdlg->by, sizegroup),
			FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(mainbox), vbox, FALSE, FALSE, 0);

	jam_dialog_set_contents(GTK_DIALOG(psdlg->dlg), mainbox);
}

PollQuestion*
pollscaledlg_run(GtkWindow *parent, PollQuestionScale *pqs) {
	STACK(PollScaleDlg, psdlg);
	PollQuestion *q = NULL;
	GtkTextBuffer *buffer;

	pollscaledlg_init(psdlg, parent);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(psdlg->question));
	if (pqs) {
		q = (PollQuestion*)pqs;

		gtk_text_buffer_insert_at_cursor(buffer, q->question, -1);

		gtk_spin_button_set_value(GTK_SPIN_BUTTON(psdlg->from),
				(gdouble)pqs->from);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(psdlg->to),
				(gdouble)pqs->to);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(psdlg->by),
				(gdouble)pqs->by);
	}

	if (gtk_dialog_run(GTK_DIALOG(psdlg->dlg)) == GTK_RESPONSE_OK) {
		GtkTextIter start, end;

		if (!pqs)
			pqs = g_new0(PollQuestionScale, 1);
		q = (PollQuestion*)pqs;
		q->type = PQ_SCALE;

		gtk_text_buffer_get_bounds(buffer, &start, &end);
		g_free(q->question);
		q->question = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

		pqs->from = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(psdlg->from));
		pqs->to = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(psdlg->to));
		pqs->by = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(psdlg->by));
	}
	gtk_widget_destroy(GTK_WIDGET(psdlg->dlg));
	return q;
}


void
poll_question_free(PollQuestion *pq) {
	switch (pq->type) {
		case PQ_RADIO:
		case PQ_COMBO:
		case PQ_CHECK:
			g_slist_foreach(((PollQuestionMulti*)pq)->answers,
					(GFunc)g_free, NULL);
			g_slist_free(((PollQuestionMulti*)pq)->answers);
			break;
		case PQ_TEXT:
		case PQ_SCALE:
			break;
	}
	g_free(pq);
}
