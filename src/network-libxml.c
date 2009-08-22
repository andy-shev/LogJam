/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "glib-all.h"

#include <libxml/nanohttp.h>

#include "conf.h"
#include "network.h"
#include "network-internal.h"

GString*
net_post_blocking(const char *url, GSList *headers, GString *post,
                  NetStatusCallback cb, gpointer data,
                  GError **err) {
	void *ctx = NULL;
	char buf[2048];
	char *headerstr = NULL;
	GString *response = NULL;
	gboolean success = FALSE;
	int len;
	NetStatusProgress progress;
	gchar *user_agent;

	if (conf.options.netdump && post) 
		fprintf(stderr, _("Request: [%s]\n"), post->str);

	xmlNanoHTTPInit();
	
	if (conf.options.useproxy) {
		if (conf.proxyuser) {
			g_set_error(err, NET_ERROR, NET_ERROR_GENERIC,
					_("LogJam was built with libxml's NanoHTTP support, "
						"which does not support proxy authentication."));
			goto out;
		}
		xmlNanoHTTPScanProxy(conf.proxy);
	} else {
		/* reset any potentially lingering proxy information. */
		xmlNanoHTTPScanProxy(NULL);
	}

	user_agent = g_strdup_printf("User-Agent: %s\r\n", LOGJAM_USER_AGENT);
	if (headers) {
		GString *hs = g_string_new(NULL);
		GSList *l;
		g_string_append(hs, user_agent);
		for (l = headers; l; l = l->next) {
			g_string_append(hs, l->data);
			g_string_append(hs, "\r\n");
		}
		headerstr = hs->str;
		g_string_free(hs, FALSE);
		g_free(user_agent);
	} else {
		headerstr = user_agent;
	}

	if (post)
		ctx = xmlNanoHTTPMethod(url, "POST",
				post->str, NULL, headerstr, post->len);
	else
		ctx = xmlNanoHTTPMethod(url, "GET", NULL, NULL, headerstr, 0);

	if (!ctx) {
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC,
				_("xmlNanoHTTPMethod error."));
		goto out;
	}

	switch (xmlNanoHTTPReturnCode(ctx)) {
	case 404:
		/* HTTP 404 message: file not found. */
		g_set_error(err, NET_ERROR, NET_ERROR_PROTOCOL,
				_("File not found."));
		goto out;
	default:
		; /* supress warning */
	}

	response = g_string_new(NULL);
	for (;;) {
		len = xmlNanoHTTPRead(ctx, buf, sizeof(buf));
		if (len < 0) {
			g_set_error(err, NET_ERROR, NET_ERROR_GENERIC,
					_("xmlNanoHTTPRead error."));
			goto out;
		} else if (len == 0) {
			break;
		} else {
			g_string_append_len(response, buf, len);
		}

		if (cb) {
			progress.current = response->len;
			progress.total = 0;/* XXX (int)contentlength; */

			cb(NET_STATUS_PROGRESS, &progress, data);
		}
	}

	success = TRUE;

	if (conf.options.netdump) 
		fprintf(stderr, _("Response: [%s]\n"), response->str);

out:
	g_free(headerstr);
	if (ctx)
		xmlNanoHTTPClose(ctx);
	if (!success && response) {
		g_string_free(response, TRUE);
		response = NULL;
	}
	return response;
}

