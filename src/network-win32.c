/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include <windows.h>
#include <wininet.h>
#include <io.h>
#include <fcntl.h>

#include "protocol.h"
#include "conf.h"
#include "util.h"
#include "network-internal.h"

/* InternetSetStatusCallback is pretty much useless for sync sockets.
 * XXX should we change to async sockets?  IIRC they're nothing but trouble.
 * for now, not worth the effort. */
#undef USE_INTERNET_STATUS_CALLBACK

static void
net_getlasterror(GError **err) {
	LPVOID lpMsgBuf;
	HMODULE hModule = NULL;
	DWORD errcode = GetLastError();

	if (errcode > INTERNET_ERROR_BASE && errcode <= INTERNET_ERROR_LAST)
		hModule = LoadLibraryEx("wininet.dll", NULL,
				LOAD_LIBRARY_AS_DATAFILE);

	if (!FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_IGNORE_INSERTS |
			(hModule ?
				FORMAT_MESSAGE_FROM_HMODULE :
				FORMAT_MESSAGE_FROM_SYSTEM),
			hModule,
			errcode,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR) &lpMsgBuf,
			0,
			NULL )) {
		g_set_error(err, 0, 0, "Unable to lookup error code.");
	} else {
		g_set_error(err, 0, 0, lpMsgBuf);
	}
	LocalFree(lpMsgBuf);
}

#ifdef USE_INTERNET_STATUS_CALLBACK
static void CALLBACK
net_internetstatus_cb(HINTERNET hInternet, DWORD_PTR dwContext,
		DWORD dwInternetStatus, LPVOID lpStatusInformation,
		DWORD dwStatusLength) {

	g_print("status %ld\n", dwInternetStatus);
}
#endif /* USE_INTERNET_STATUS_CALLBACK */

static GString *
readresponse(HINTERNET hRequest,
             NetStatusCallback cb, gpointer data,
             GError **err) {
	char buf[READ_BLOCK_SIZE];
	DWORD idx, len;
	NetStatusProgress progress = {0};
	GString *response;

	idx = 0;
	len = sizeof(DWORD);
	HttpQueryInfo(hRequest,
			HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
			&progress.total, &len, NULL);

	if (cb) cb(NET_STATUS_PROGRESS, &progress, data);

	response = g_string_sized_new(READ_BLOCK_SIZE);
	len = READ_BLOCK_SIZE;
	while (InternetReadFile(hRequest, buf, READ_BLOCK_SIZE, &len) &&
			len > 0) {
		g_string_append_len(response, buf, len);
		progress.current += len;
		if (cb) cb(NET_STATUS_PROGRESS, &progress, data);
		len = READ_BLOCK_SIZE;
	}
	if (len != 0) {
		net_getlasterror(err);
		g_string_free(response, TRUE);
		return NULL;
	}
	if (progress.total > 0 && progress.current < progress.total) {
		g_set_error(err, 0, 0, _("The connection was closed."));
		g_string_free(response, TRUE);
		return NULL;
	}

	return response;
}

GString*
net_post_blocking(const char *surl, GSList *headers, GString *post,
                  NetStatusCallback cb, gpointer data,
                  GError **err) {
	HINTERNET hInternet, hConnection, hRequest;
	GString *response = NULL;
	char *url, *host;
	URL_COMPONENTS urlc = {0};

	url = g_strdup(surl);
	urlc.dwStructSize = sizeof(URL_COMPONENTS);
	urlc.lpszHostName = NULL;
	urlc.dwHostNameLength = (DWORD)strlen(url);
	if (!InternetCrackUrl(url, urlc.dwHostNameLength, 0, &urlc)) {
		net_getlasterror(err);
		g_free(url);
		return NULL;
	}
	/* host is a pointer to a place within url;
	   we modify this string directly, and just free
	   url when we're done. */
	host = urlc.lpszHostName;
	host[urlc.dwHostNameLength] = 0;

	if ((hInternet = InternetOpen(LOGJAM_USER_AGENT,
			INTERNET_OPEN_TYPE_PRECONFIG,
			NULL, NULL, 0)) != NULL) {

#ifdef USE_INTERNET_STATUS_CALLBACK
		if (!InternetSetStatusCallback(hInternet, net_internetstatus_cb)) {
			net_getlasterror(err);
		}

		if ((hConnection = InternetConnect(hInternet,
				host, urlc.nPort,
				NULL, NULL,
				INTERNET_SERVICE_HTTP, 0, 0xDEADBEEF)) != NULL) {
#else /* !USE_INTERNET_STATUS_CALLBACK */
		if ((hConnection = InternetConnect(hInternet,
				host, urlc.nPort,
				NULL, NULL,
				INTERNET_SERVICE_HTTP, 0, 0)) != NULL) {
#endif /* USE_INTERNET_STATUS_CALLBACK */

			if ((hRequest = HttpOpenRequest(hConnection,
					"POST", "/interface/flat",
					NULL, NULL, NULL, 0, 0)) != NULL) {

				if (HttpSendRequest(hRequest, NULL, 0,
						post->str, post->len)) {
					response = readresponse(hRequest, cb, data, err);
				} else {
					net_getlasterror(err);
				}
				InternetCloseHandle(hRequest);
			} else {
				net_getlasterror(err);
			}
			InternetCloseHandle(hConnection);
		} else {
			net_getlasterror(err);
		}
		InternetCloseHandle(hInternet);
	} else {
		net_getlasterror(err);
	}
	g_free(url);

	return response;
}

typedef struct {
	GMainLoop *mainloop;
	GSource *source;
	int pipe;
	const char *url;
	GSList *headers;
	GString *post;
	GError **err;
	GString *response;
	NetStatusCallback user_cb;
	gpointer user_data;
} ThreadData;

void status_to_pipe_cb(NetStatusType status,
		gpointer statusdata, gpointer data) {
	ThreadData *threaddata = (ThreadData*)data;
	char t = status;
	switch (status) {
	case NET_STATUS_PROGRESS:
		write(threaddata->pipe, &t, 1);
		write(threaddata->pipe, statusdata, sizeof(NetStatusProgress));
		break;
	default:
		g_warning("status_to_pipe_cb: unknown status %d.\n", status);
	}
}

static DWORD WINAPI
net_post_threadproc(LPVOID param) {
	ThreadData *threaddata = (ThreadData*)param;
	char t = NET_STATUS_DONE;
	threaddata->response =
			net_post_blocking(threaddata->url,
			threaddata->headers, threaddata->post,
			status_to_pipe_cb, threaddata,
			threaddata->err);
	write(threaddata->pipe, &t, 1);
	return 0;
}

static gboolean
net_post_input_cb(GIOChannel *source, GIOCondition condition, gpointer data) {
	ThreadData *threaddata = (ThreadData*)data;
	char t;
	NetStatusProgress progress;

	GDK_THREADS_ENTER();
	g_io_channel_read_chars(source, &t, 1, NULL, NULL);
	switch (t) {
	case NET_STATUS_PROGRESS:
		g_io_channel_read_chars(source, (gchar*)&progress,
				sizeof(NetStatusProgress), NULL, NULL);
		threaddata->user_cb(NET_STATUS_PROGRESS, &progress,
				threaddata->user_data);
		break;
	case NET_STATUS_DONE:
		g_source_destroy(threaddata->source);
		g_main_loop_quit(threaddata->mainloop);
		break;
	default:
		g_warning("net_post_input_cb: unknown status %d.\n", t);
	}
	GDK_THREADS_LEAVE();
	if (t == NET_STATUS_DONE)
		return FALSE;
	return TRUE;
}

GString*
net_post_mainloop(const char *url, GSList *headers, GString *post,
                  NetStatusCallback cb, gpointer data,
                  GError **err) {
	ThreadData threaddata = {0};
	GMainLoop *mainloop;
	GIOChannel *channel;
	GSource *source;
	DWORD threadid;
	int fds[2];

	if (_pipe(fds, 4096, _O_BINARY) < 0)
		perror("pipe");

	mainloop = g_main_loop_new(NULL, TRUE);

	// XXX http://mail.gnome.org/archives/gtk-devel-list/2003-June/msg00102.html
	channel = g_io_channel_unix_new(fds[0]);
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);
	source = g_io_create_watch(channel, G_IO_IN | G_IO_HUP | G_IO_ERR);
	g_source_set_callback(source,
			(GSourceFunc)net_post_input_cb,
			&threaddata, NULL);
	g_source_attach(source, NULL);

	threaddata.mainloop = mainloop;
	threaddata.source = source;
	threaddata.pipe = fds[1];
	threaddata.headers = headers;
	threaddata.post = post;
	threaddata.url = url;
	threaddata.err = err;
	threaddata.user_cb = cb;
	threaddata.user_data = data;

	g_print("starting thread\n");

	CreateThread(NULL, 0, net_post_threadproc, &threaddata, 0, &threadid);

	g_main_loop_run(mainloop);

	/* XXX: closing these in the opposite order causes deadlocks? */
	close(fds[1]);
	close(fds[0]);

	g_io_channel_unref(channel);
	/* source is destroyed in the callback. */
	g_main_loop_unref(mainloop);

	return threaddata.response;
}

void
net_mainloop_cancel(NetMainloopHandle handle) {
	/* XXX implement me. */
}

