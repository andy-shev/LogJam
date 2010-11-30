/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "glib-all.h"
#include <stdlib.h>

#include <livejournal/livejournal.h>
#include <livejournal/getchallenge.h>

#include "conf.h"
#include "network.h"
#include "network-internal.h"

GQuark
net_error_quark(void) {
	static GQuark quark = 0;
	if (quark == 0)
		quark = g_quark_from_static_string("logjam-net-error-quark");
	return quark;
}

static GString*
real_run_request(const char *url, GString *post,
                 NetStatusCallback cb, gpointer data,
                 GError **err) {
	/* XXX forking disabled on win32 until they fix a GTK bug--
	 * see network-win32.c for details. */
#if defined(HAVE_GTK) && !defined(G_OS_WIN32)
	if (conf.options.nofork || app.cli) {
		return net_post_blocking(url, NULL, post, cb, data, err);
	} else {
		return net_post_mainloop(url, NULL, post, cb, data, err);
	}
#else
	return net_post_blocking(url, NULL, post, cb, data, err);
#endif
}

gboolean
net_verb_run_directly(LJVerb *verb,
                      NetStatusCallback cb, gpointer data,
                      GError **err) {
	GString *post, *res;
	const char *urlbase;
	char *url;
	gboolean ret;

	urlbase = lj_request_get_user(verb->request)->server->url;
	url = g_strdup_printf("%s/interface/flat", urlbase);

	post = lj_request_to_string(verb->request);
	res = real_run_request(url, post, cb, data, err);
	g_string_free(post, TRUE);
	g_free(url);

	if (res == NULL)
		return FALSE;

	ret = lj_verb_handle_response(verb, res->str, err);
	g_string_free(res, TRUE);

	return ret;
}

gboolean
net_verb_run_internal(LJVerb *verb,
                      NetStatusCallback cb, gpointer data,
                      GError **err) {
	LJUser *user = lj_request_get_user(verb->request);
	LJServer *server = user->server;

	if (server->authscheme == LJ_AUTH_SCHEME_UNKNOWN ||
			server->authscheme == LJ_AUTH_SCHEME_C0) {
		LJGetChallenge *getchal = lj_getchallenge_new(user);
		GError *chalerr = NULL;
		if (!net_verb_run_directly((LJVerb*)getchal, cb, data, &chalerr)) {
			/* failed.  no auth scheme available? */
			server->authscheme = LJ_AUTH_SCHEME_NONE;
		} else {
			server->authscheme = getchal->authscheme;
			if (server->authscheme == LJ_AUTH_SCHEME_C0)
				lj_verb_use_challenge(verb, getchal->challenge);
		}
		lj_getchallenge_free(getchal);

		if (chalerr) {
			/* if there was a total failure when we tried to send the
			 * challenge, stop here.  we don't even need to attempt
			 * the subsequent request. */
			if (g_error_matches(chalerr, NET_ERROR, NET_ERROR_CANCELLED) ||
					g_error_matches(chalerr, NET_ERROR, NET_ERROR_GENERIC)) {
				g_propagate_error(err, chalerr);
				return FALSE;
			}
			g_error_free(chalerr);
		}
	}

	return net_verb_run_directly(verb, cb, data, err);
}





void
ctx_cmdline_print_string(NetContext *ctx, const char *text) {
	if (!app.quiet)
		g_print("%s\n", text);
}

static void
cmdline_statuscb(NetStatusType status, gpointer statusdata, gpointer data) {
	/* what do we do? */
}

static GString*
ctx_cmdline_http(NetContext *ctx, const char *url, GSList *headers,
		GString *post, GError **err) {
	return net_post_blocking(url, NULL, post, cmdline_statuscb, ctx, err);
}

void
ctx_cmdline_error(NetContext *ctx, GError *err) {
	g_printerr(_("Error: %s\n"), err->message);
}

/* f() means *any* argument list, not just void */
void
ctx_silent_noop() {
}

static NetContext network_ctx_silent_real = {
	.title        = NULL,
	.progress_str = ctx_silent_noop,
	.progress_int = NULL,
	.error        = ctx_silent_noop,
	.http         = ctx_cmdline_http,
};
NetContext *network_ctx_silent = &network_ctx_silent_real;

static NetContext network_ctx_cmdline_real = {
	.title = NULL,
	.progress_str = ctx_cmdline_print_string,
	.progress_int = NULL,
	.error = ctx_cmdline_error,
	.http = ctx_cmdline_http,
};
NetContext *network_ctx_cmdline = &network_ctx_cmdline_real;

gboolean
run_verb(LJVerb *verb, NetContext *ctx, GError **err) {
	GString *post, *res;
	const char *urlbase;
	char *url;
	gboolean ret;

	urlbase = lj_request_get_user(verb->request)->server->url;
	url = g_strdup_printf("%s/interface/flat", urlbase);

	post = lj_request_to_string(verb->request);
	res = ctx->http(ctx, url, NULL, post, err);
	g_string_free(post, TRUE);
	g_free(url);

	if (res == NULL)
		return FALSE;

	ret = lj_verb_handle_response(verb, res->str, err);
	g_string_free(res, TRUE);

	return ret;
}


gboolean
net_run_verb_ctx(LJVerb *verb, NetContext *ctx, GError **reterr) {
	LJUser *user = lj_request_get_user(verb->request);
	LJServer *server = user->server;
	GError *err = FALSE;
	gboolean success = FALSE;

	if (server->authscheme == LJ_AUTH_SCHEME_UNKNOWN ||
			server->authscheme == LJ_AUTH_SCHEME_C0) {
		LJGetChallenge *getchal = lj_getchallenge_new(user);

		ctx->progress_str(ctx, _("Requesting challenge..."));
		if (!run_verb((LJVerb*)getchal, ctx, &err)) {
			/* failed.  no auth scheme available? */
			server->authscheme = LJ_AUTH_SCHEME_NONE;
		} else {
			server->authscheme = getchal->authscheme;
			if (server->authscheme == LJ_AUTH_SCHEME_C0)
				lj_verb_use_challenge(verb, getchal->challenge);
		}
		lj_getchallenge_free(getchal);

		if (err) {
			/* if there was a total failure when we tried to send the
			 * challenge, stop here.  we don't even need to attempt
			 * the subsequent request. */
			if (g_error_matches(err, NET_ERROR, NET_ERROR_CANCELLED) ||
					g_error_matches(err, NET_ERROR, NET_ERROR_GENERIC)) {
				goto out;
			}
			g_error_free(err);
			err = NULL;
		}
	}

	ctx->progress_str(ctx, _("Running request..."));
	success = run_verb(verb, ctx, &err);
out:
	if (!success) {
		if (ctx->error)
			ctx->error(ctx, err);
		g_propagate_error(reterr, err);
	}
	return success;
}

