/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "glib-all.h"

#ifndef G_OS_WIN32
#include <unistd.h>
#endif

#include <curl/curl.h>

#include "conf.h"
#include "network.h"
#include "network-internal.h"

typedef struct {
	CURL *curl;

	GString *response;

	NetStatusCallback user_cb;
	gpointer user_data;
} RequestData;

static size_t 
curlwrite_cb(void *ptr, size_t size, size_t nmemb, void *data) {
	RequestData *requestdata = data;
	double contentlength;
	
	g_string_append_len(requestdata->response, ptr, size*nmemb);

	curl_easy_getinfo(requestdata->curl, 
			CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentlength);

	if (requestdata->user_cb) {
		NetStatusProgress progress = {0};
		progress.current = requestdata->response->len;
		progress.total = (int)contentlength;

		requestdata->user_cb(NET_STATUS_PROGRESS, &progress, requestdata->user_data);
	}

	return size*nmemb;
}

/*
 * see comment in run_curl_request().
static int
curl_progress_cb(void *cp, size_t dlt, size_t dln, size_t ult, size_t uln) {
	g_print("progress: %d %d %d %d\n", dlt, dln, ult, uln);
	return 0;
}*/

GString*
net_post_blocking(const char *url, GSList *headers, GString *post,
                  NetStatusCallback cb, gpointer data,
                  GError **err) {
	CURL *curl;
	CURLcode curlres;
	char proxyuserpass[1024];
	char errorbuf[CURL_ERROR_SIZE];
	RequestData requestdata = {0};
	struct curl_slist *curlheaders = NULL;

	curl = curl_easy_init();
	if (curl == NULL) {
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC,
				_("Unable to intialize CURL"));
		return NULL;
	}

	curl_easy_setopt(curl, CURLOPT_VERBOSE, conf.options.netdump != 0);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuf);

	/*
	 * curl's progress function is mostly useless; we instead track writes.
	 *
	 * curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, curl_progress_cb);
	 * curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, nr);
	 */
	
	curl_easy_setopt(curl, CURLOPT_URL, url);

	if (conf.options.useproxy) {
		curl_easy_setopt(curl, CURLOPT_PROXY, conf.proxy);
		if (conf.options.useproxyauth) {
			g_snprintf(proxyuserpass, sizeof(proxyuserpass), "%s:%s", 
					conf.proxyuser, conf.proxypass);
			curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, proxyuserpass);
		}
	}

	if (headers) {
		GSList *l;
		for (l = headers; l; l = l->next)
			curlheaders = curl_slist_append(curlheaders, l->data);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlheaders);
	}

	if (post) {
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post->str);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post->len);
		curl_easy_setopt(curl, CURLOPT_POST, 1);
	}

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlwrite_cb);
	requestdata.curl = curl;
	requestdata.response = g_string_sized_new(READ_BLOCK_SIZE);
	requestdata.user_cb = cb;
	requestdata.user_data = data;
	curl_easy_setopt(curl, CURLOPT_FILE, &requestdata);
	
	if (conf.options.netdump && post) 
		fprintf(stderr, _("Request: [%s]\n"), post->str);

	curlres = curl_easy_perform(curl);
	if (curlheaders)
		curl_slist_free_all(curlheaders);
	curl_easy_cleanup(curl);

	if (curlres != CURLE_OK) {
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC,
				_("cURL error: %s."), errorbuf);
		g_string_free(requestdata.response, TRUE);
		return NULL;
	}

	if (conf.options.netdump) 
		fprintf(stderr, _("Response: [%s]\n"), requestdata.response->str);

	return requestdata.response;
}

