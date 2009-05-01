/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2004 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include "util-gtk.h"
#include "conf.h"
#include "jamdoc.h"

typedef struct {
	GtkWidget *dlg;
	GtkWidget *url, *get, *width, *height;
} ImageDlg;

static void
url_changed_cb(ImageDlg *idlg) {
	const char *text = gtk_entry_get_text(GTK_ENTRY(idlg->url));
	gtk_widget_set_sensitive(idlg->get, text && text[0]);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(idlg->dlg),
			GTK_RESPONSE_OK, text && text[0]);
}

static void
size_cb(ImageDlg *idlg, gint width, gint height) {
	char *text;
	text = g_strdup_printf("%d", width);
	gtk_entry_set_text(GTK_ENTRY(idlg->width), text);
	g_free(text);
	text = g_strdup_printf("%d", height);
	gtk_entry_set_text(GTK_ENTRY(idlg->height), text);
	g_free(text);
}

static void
get_url_cb(ImageDlg *idlg) {
	GdkPixbufLoader *loader;
	const char *url;
	GString *img;
	GError *err = NULL;

	/* XXX suboptimal: we retrieve the entire image
	 * when we only need a few bytes. */
	url = gtk_entry_get_text(GTK_ENTRY(idlg->url));
	img = net_get_run(_("Retrieving Image..."), GTK_WINDOW(idlg->dlg), url);

	if (!img)
		return;

	loader = gdk_pixbuf_loader_new();
	g_signal_connect_swapped(G_OBJECT(loader), "size-prepared",
			G_CALLBACK(size_cb), idlg);
	if (!gdk_pixbuf_loader_write(loader, (unsigned char*)img->str, img->len, &err)) {
		jam_warning(GTK_WINDOW(idlg->dlg),
				_("Error loading image: %s."), err->message);
		g_error_free(err);
	}
	gdk_pixbuf_loader_close(loader, NULL);
	g_object_unref(G_OBJECT(loader));
}

static void
make_dialog(ImageDlg *idlg, GtkWindow *win) {
	GtkWidget *vbox, *hbox, *dimbox;
	GtkSizeGroup *sg;

	idlg->dlg = gtk_dialog_new_with_buttons(_("Insert Image"),
			win, GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(idlg->dlg),
			GTK_RESPONSE_OK);
	g_signal_connect_swapped(G_OBJECT(idlg->dlg), "show",
			G_CALLBACK(url_changed_cb), idlg);
	
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	vbox = gtk_vbox_new(FALSE, 12);

	idlg->url = gtk_entry_new();
	g_signal_connect_swapped(G_OBJECT(idlg->url), "changed",
			G_CALLBACK(url_changed_cb), idlg);
	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new_sg(_("_URL:"), idlg->url, sg),
			FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 6);

	dimbox = gtk_vbox_new(FALSE, 6);
	idlg->width = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(idlg->width), 6);
	gtk_box_pack_start(GTK_BOX(dimbox),
			labelled_box_new_sg(_("_Width:"), idlg->width, sg),
			FALSE, FALSE, 0);

	idlg->height = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(idlg->height), 6);
	gtk_box_pack_start(GTK_BOX(dimbox),
			labelled_box_new_sg(_("_Height:"), idlg->height, sg),
			FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), dimbox, FALSE, FALSE, 0);

	dimbox = gtk_vbox_new(FALSE, 6);
	idlg->get = gtk_button_new_with_mnemonic(_("_Retrieve Dimensions from Network"));
	g_signal_connect_swapped(G_OBJECT(idlg->get), "clicked",
			G_CALLBACK(get_url_cb), idlg);
	gtk_box_pack_start(GTK_BOX(dimbox), idlg->get, TRUE, FALSE, 0);

	gtk_box_pack_end(GTK_BOX(hbox), dimbox, FALSE, FALSE, 0);
	
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	jam_dialog_set_contents(GTK_DIALOG(idlg->dlg), vbox);
}

static gboolean
is_image_link(const char *str) {
	if (!str)
		return FALSE;
	return (g_str_has_prefix(str, "http://") ||
	        g_str_has_prefix(str, "ftp://"));
}

void
image_dialog_run(GtkWindow *win, JamDoc *doc) {
	STACK(ImageDlg, idlg);
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	char *sel = NULL, *url = NULL;
	const char *data;
	int width = -1, height = -1;

	make_dialog(idlg, win);

	buffer = jam_doc_get_text_buffer(doc);
	if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
		sel = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
		if (is_image_link(sel))
			gtk_entry_set_text(GTK_ENTRY(idlg->url), sel);
	} else {
		char *clip;
		clip = jam_clipboard_wait_for_text_timeout(
				gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
				50 /* ms */);
		if (is_image_link(clip))
			gtk_entry_set_text(GTK_ENTRY(idlg->url), clip);
		g_free(clip);
	}

	if (gtk_dialog_run(GTK_DIALOG(idlg->dlg)) == GTK_RESPONSE_OK) {
		if (sel)
			gtk_text_buffer_delete(buffer, &start, &end);
		else
			gtk_text_buffer_get_iter_at_mark(buffer, &start,
					gtk_text_buffer_get_insert(buffer));

		url = g_strdup(gtk_entry_get_text(GTK_ENTRY(idlg->url)));
		xml_escape(&url);
		gtk_text_buffer_insert(buffer, &start, "<img src='", -1);
		gtk_text_buffer_insert(buffer, &start, url, -1);
		gtk_text_buffer_insert(buffer, &start, "'", -1);
		g_free(url);

		data = gtk_entry_get_text(GTK_ENTRY(idlg->width));
		width = atoi(data);

		data = gtk_entry_get_text(GTK_ENTRY(idlg->height));
		height = atoi(data);

		if (width > 0) {
			char *data = g_strdup_printf(" width='%d'", width);
			gtk_text_buffer_insert(buffer, &start, data, -1);
			g_free(data);
		}
		if (height > 0) {
			char *data = g_strdup_printf(" height='%d'", height);
			gtk_text_buffer_insert(buffer, &start, data, -1);
			g_free(data);
		}
		gtk_text_buffer_insert(buffer, &start, " />", -1);
	}

	g_free(sel);

	gtk_widget_destroy(idlg->dlg);
}
