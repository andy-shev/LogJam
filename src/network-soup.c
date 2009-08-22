/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2005 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "glib-all.h"

#include <libsoup/soup.h>
#include <string.h>

#include "conf.h"
#include "network.h"
#include "network-internal.h"

typedef struct {
	NetStatusCallback user_cb;
	gpointer user_data;
	int current, total;
} CallbackInfo;

static void
got_chunk_cb(SoupMessage *msg, SoupBuffer *chunk, CallbackInfo *info) {
	NetStatusProgress progress = {0};
	const char* clen;

	if (info->total == 0) {
		info->total = soup_message_headers_get_content_length(msg->response_headers);
		if (info->total == 0)
			return;
	}
	info->current += chunk->length;

	progress.current = info->current;
	progress.total = info->total;
	info->user_cb(NET_STATUS_PROGRESS, &progress, info->user_data);
}

GString*
net_post_blocking(const char *url, GSList *headers, GString *post,
                  NetStatusCallback cb, gpointer data,
                  GError **err) {
	SoupURI* suri = NULL;
	SoupMessage *req = NULL;
	SoupSession *session = NULL;
	guint status = 0;
	GString *response = NULL;
	CallbackInfo info = { cb, data, 0, 0 };

	if (conf.options.useproxy) {
		suri = soup_uri_new(conf.proxy);
		if (conf.options.useproxyauth) {
			soup_uri_set_user(suri, conf.proxyuser);
			soup_uri_set_password(suri, conf.proxypass);
		}
		session = soup_session_sync_new_with_options (
			SOUP_SESSION_PROXY_URI, suri,
			SOUP_SESSION_USER_AGENT, LOGJAM_USER_AGENT,
			NULL);
		soup_uri_free(suri);
	} else
		session = soup_session_sync_new_with_options (
			SOUP_SESSION_USER_AGENT, LOGJAM_USER_AGENT,
			NULL);

	req = soup_message_new(post ? "POST" : "GET", url);
	g_signal_connect(G_OBJECT(req), "got-chunk",
			G_CALLBACK(got_chunk_cb), &info);
	for (; headers; headers = headers->next) {
		char *header = headers->data;
		/* soup wants the key and value separate, so we have to munge this
		 * a bit. */
		char *colonpos = strchr(header, ':');
		*colonpos = 0;
		soup_message_headers_append(req->request_headers, header, colonpos+1);
		*colonpos = ':';
	}
	soup_message_set_request(req, "application/x-www-form-urlencoded",
			SOUP_MEMORY_TEMPORARY, post? post->str : NULL, post? post->len : 0);

	status = soup_session_send_message(session, req);
	if (status != SOUP_STATUS_OK) {
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC,
			    req->reason_phrase);
		goto out;
	}

	response = g_string_new_len(req->response_body->data, req->response_body->length);

	if (conf.options.netdump) 
		fprintf(stderr, _("Response: [%s]\n"), response->str);

out:
	if (req) g_object_unref(G_OBJECT(req));
	if (session) g_object_unref(G_OBJECT(session));

	return response;
}

