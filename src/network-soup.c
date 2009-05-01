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
got_chunk_cb(SoupMessage *msg, CallbackInfo *info) {
	NetStatusProgress progress = {0};
	const char* clen;

	if (info->total == 0) {
		clen = soup_message_get_header(msg->response_headers,
				"Content-length");
		if (!clen)
			return;
		info->total = atoi(clen);
	}
	info->current += msg->response.length;

	progress.current = info->current;
	progress.total = info->total;
	info->user_cb(NET_STATUS_PROGRESS, &progress, info->user_data);
}

GString*
net_post_blocking(const char *url, GSList *headers, GString *post,
                  NetStatusCallback cb, gpointer data,
                  GError **err) {
	SoupUri* suri = NULL;
	SoupMessage *req = NULL;
	SoupSocket *sock = NULL;
	guint status = 0;
	GString *response = NULL;
	CallbackInfo info = { cb, data, 0, 0 };

	suri = soup_uri_new(conf.options.useproxy ? conf.proxy : url);
	sock = soup_socket_client_new_sync(suri->host, suri->port, NULL, &status);
	if (status != SOUP_STATUS_OK) {
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC,
				soup_status_get_phrase(status));
		goto out;
	}
	g_free(suri);
	suri = NULL;

	suri = soup_uri_new(url);
	if (conf.options.useproxy && conf.options.useproxyauth) {
		g_free(suri->user);
		g_free(suri->passwd);
		suri->user = g_strdup(conf.proxyuser);
		suri->passwd = g_strdup(conf.proxypass);
	}
	req = soup_message_new_from_uri(post ? "POST" : "GET", suri);
	g_signal_connect(G_OBJECT(req), "got-chunk",
			G_CALLBACK(got_chunk_cb), &info);
	for (; headers; headers = headers->next) {
		char *header = headers->data;
		/* soup wants the key and value separate, so we have to munge this
		 * a bit. */
		char *colonpos = strchr(header, ':');
		*colonpos = 0;
		soup_message_add_header(req->request_headers, header, colonpos+1);
		*colonpos = ':';
	}
	soup_message_set_request(req, "application/x-www-form-urlencoded",
			SOUP_BUFFER_USER_OWNED, post->str, post->len);

	soup_message_send_request(req, sock, conf.options.useproxy);
	if (status != SOUP_STATUS_OK) {
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC,
				soup_status_get_phrase(status));
		goto out;
	}

	response = g_string_new_len(req->response.body, req->response.length);

	if (conf.options.netdump) 
		fprintf(stderr, _("Response: [%s]\n"), response->str);

out:
	if (suri) soup_uri_free(suri);
	if (sock) {
		soup_socket_disconnect(sock);
		g_object_unref(G_OBJECT(sock));
	}
	if (req) g_object_unref(G_OBJECT(req));

	return response;
}

