/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <glib.h>
#include <livejournal.h>

#include <curl/curl.h>
#include "curl.h"

typedef struct {
	CURL *curl;
	GString *response;
} RequestData;

static size_t 
curlwrite_cb(void *ptr, size_t size, size_t nmemb, void *data) {
	RequestData *requestdata = data;
	g_string_append_len(requestdata->response, ptr, size*nmemb);
	return size*nmemb;
}

GString*
run_request(LJRequest *request, GError **err) {
	LJServer *server = lj_request_get_user(request)->server;
	CURL *curl;
	CURLcode curlres;
	char urlreq[1024]; //, proxyuserpass[1024];
	char errorbuf[CURL_ERROR_SIZE];
	RequestData requestdata = {0};

	GString *post;
	post = lj_request_to_string(request);

	curl = curl_easy_init();
	if (curl == NULL) {
		g_set_error(err, 0, 0, "Unable to init curl");
		return NULL;
	}

	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuf);

	g_snprintf(urlreq, sizeof(urlreq), 
			"%s/interface/flat", server->url);
	curl_easy_setopt(curl, CURLOPT_URL, urlreq);

	/*if (conf.options.useproxy) {
		curl_easy_setopt(curl, CURLOPT_PROXY, conf.proxy);
		if (conf.options.useproxyauth) {
			g_snprintf(proxyuserpass, sizeof(proxyuserpass), "%s:%s", 
					conf.proxyuser, conf.proxypass);
			curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, proxyuserpass);
		}
	}*/

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post->str);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post->len);
	curl_easy_setopt(curl, CURLOPT_POST, 1);

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlwrite_cb);
	requestdata.curl = curl;
	requestdata.response = g_string_sized_new(4096);
	curl_easy_setopt(curl, CURLOPT_FILE, &requestdata);
	
	curlres = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if (curlres != CURLE_OK) {
		g_set_error(err, 0, 0, "cURL error: %s", errorbuf);
		return NULL;
	}

	return requestdata.response;
}

gboolean
run_verb(LJVerb *verb, GError **err) {
	gboolean ret;
	GString *s;
	s = run_request(verb->request, err);
	if (!s)
		return FALSE;
	ret = lj_verb_handle_response(verb, s->str, err);
	g_string_free(s, TRUE);
	return ret;
}


