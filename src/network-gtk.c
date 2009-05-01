/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include "conf.h"
#include "network.h"
#include "network-internal.h"
#include "throbber.h"
#include "progress.h"

static void
statuscb(NetStatusType status, gpointer statusdata, gpointer data) {
	ProgressWindow *pw = PROGRESS_WINDOW(data);
	NetStatusProgress *progress;
	float fraction = 0;

	switch (status) {
	case NET_STATUS_BEGIN:
		progress_window_set_cancel_cb(pw, net_mainloop_cancel, statusdata);
		progress_window_set_progress(pw, 0);
		break;
	case NET_STATUS_PROGRESS:
		progress = (NetStatusProgress*)statusdata;
		if (progress->current > 0 && progress->total > 0) {
			fraction = (float)progress->current / progress->total;
			progress_window_set_progress(pw, fraction);
		}
		break;
	case NET_STATUS_DONE:
		progress_window_set_cancel_cb(pw, NULL, NULL);
		progress_window_set_progress(pw, -1);
		break;
	default:
		g_warning("unhandled network status %d\n", status);
	}
}

typedef struct {
	NetContext ctx;
	GtkWindow *parent;
	GtkWidget *pw;
} NetContextGtk;

static void
statuscb_ctx(NetStatusType status, gpointer statusdata, gpointer data) {
	NetContextGtk *ctx = data;
	statuscb(status, statusdata, ctx->pw);
}

static void
ctx_gtk_title(NetContext *ctx, const char *title) {
	ProgressWindow *pw = PROGRESS_WINDOW(((NetContextGtk*)ctx)->pw);
	progress_window_set_title(pw, title);
}
static void
ctx_gtk_progress_str(NetContext *ctx, const char *str) {
	ProgressWindow *pw = PROGRESS_WINDOW(((NetContextGtk*)ctx)->pw);
	progress_window_set_text(pw, str);
}

static void 
ctx_gtk_error(NetContext *ctx, GError *err) {
	ProgressWindow *pw = PROGRESS_WINDOW(((NetContextGtk*)ctx)->pw);
	if (!g_error_matches(err, NET_ERROR, NET_ERROR_CANCELLED))
		progress_window_show_error(pw, err->message);
}

static GString*
ctx_gtk_http(NetContext *ctx, const char *url, GSList *headers,
		GString *post, GError **err) {
	if (conf.options.nofork)
		return net_post_blocking(url, NULL, post, statuscb_ctx, ctx, err);
	else
		return net_post_mainloop(url, NULL, post, statuscb_ctx, ctx, err);
}

NetContext*
net_ctx_gtk_new(GtkWindow *parent, const char *title) {
	NetContext *ctx = (NetContext*)g_new0(NetContextGtk, 1);
	NetContextGtk *gtkctx = (NetContextGtk*)ctx;
	ctx->title = ctx_gtk_title;
	ctx->progress_str = ctx_gtk_progress_str;
	//XXX ctx->progress_int = ctx_cmdline_print_string;
	ctx->http = ctx_gtk_http;
	ctx->error = ctx_gtk_error;
	gtkctx->pw = progress_window_new(parent, title);
	gtk_widget_show(gtkctx->pw);
	return ctx;
}

void
net_ctx_gtk_free(NetContext *ctx) {
	NetContextGtk *gtkctx = (NetContextGtk*)ctx;
	gtk_widget_destroy(gtkctx->pw);
	g_free(ctx);
}

gboolean
net_window_run_verb(ProgressWindow *pw, LJVerb *verb) {
	GError *err = NULL;
	gboolean ret;

	gtk_widget_show(GTK_WIDGET(pw));
	ret = net_verb_run_internal(verb, statuscb, pw, &err);

	if (!ret) {
		if (!g_error_matches(err, NET_ERROR, NET_ERROR_CANCELLED))
			progress_window_show_error(pw, err->message);
		g_error_free(err);
		return FALSE;
	}

	return TRUE;
}

gboolean
net_window_run_post(ProgressWindow *pw, const char *url, GSList *headers,
                    GString *request, GString *response,
					GError **err) {
	GString *res;
	gboolean ret;

	gtk_widget_show(GTK_WIDGET(pw));
	res = net_post_mainloop(url, headers, request, statuscb, pw, err);
	if (res) {
		g_string_append_len(response, res->str, res->len);
		ret = TRUE;
	} else {
		ret = FALSE;
	}
	g_string_free(res, TRUE);

	return ret;
}

gboolean
net_verb_run(LJVerb *verb, const char *title, GtkWindow *parent) {
	GtkWidget *pw;
	gboolean ret;

	pw = progress_window_new(parent, title);
	ret = net_window_run_verb(PROGRESS_WINDOW(pw), verb);
	gtk_widget_destroy(pw);

	return ret;
}

GString*
net_get_run(const char *title, GtkWindow *parent,
            const char *url) {
	GtkWidget *pw;
	GString *ret;
	GError *err = NULL;

	pw = progress_window_new(parent, title);
	gtk_widget_show(GTK_WIDGET(pw));

	ret = net_post_mainloop(url, NULL, NULL, statuscb, pw, &err);
	if (!ret) {
		if (!g_error_matches(err, NET_ERROR, NET_ERROR_CANCELLED))
			progress_window_show_error(PROGRESS_WINDOW(pw), err->message);
		g_error_free(err);
	}
	gtk_widget_destroy(pw);

	return ret;
}

