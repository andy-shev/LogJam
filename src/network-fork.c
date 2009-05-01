/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

/* network-fork.c:  provide a net_post_mainloop that uses an
 * external net_post_blocking via a fork.  used by network-curl and
 * network-libxml.
 */

#include "config.h"

#include "gtk-all.h"

#ifndef G_OS_WIN32
#include <unistd.h>
#endif
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>  /* waitpid */
#include <signal.h>    /* kill */

#include "conf.h"
#include "network.h"
#include "network-internal.h"

typedef struct {
	GString *response;

	GError **err;

	guint pipe_tag;
	int pipefds[2];

	pid_t pid;

	NetStatusCallback user_cb;
	gpointer user_data;
} ForkData;

static int
pipe_write(int pipe, NetStatusType type, int len, void *data) {
	char t = type;
	if (write(pipe, &t, 1) < 1)
		return -1;
	if (write(pipe, &len, sizeof(int)) < sizeof(int))
		return -1;
	if (data) {
		if (write(pipe, data, len) < len)
			return -1;
	}
	return 0;
}

static void
fork_cb(NetStatusType status, gpointer statusdata, gpointer data) {
	ForkData *forkdata = data;
	if (status == NET_STATUS_PROGRESS)
		pipe_write(forkdata->pipefds[1], NET_STATUS_PROGRESS,
				sizeof(NetStatusProgress), statusdata);
}

static gboolean
readall(int fd, void *buf, int len) {
	int ret;
	int rlen = 0;
	do {
		ret = read(fd, ((char*)buf)+rlen, len-rlen);
		if (ret < 0)
			return FALSE;
		rlen += ret;
		if (ret == 0 && rlen < len)
			return FALSE;
	} while (rlen < len);
	return TRUE;
}

static void
pipe_cb(ForkData *forkdata, gint pipe, GdkInputCondition cond) {
	char t;
	int len;
	char *buf;
	NetStatusProgress progress;

	len = read(pipe, &t, 1);
	if (len == 0)
		g_print("Pipe unexpectedly closed.\n");
	else if (len < 0)
		perror("read");

	if (!readall(pipe, &len, sizeof(int)))
		return;

	switch (t) {
		case NET_STATUS_SUCCESS:
			buf = g_new0(char, len+1);
			readall(pipe, buf, len);
			buf[len] = 0;
			waitpid(forkdata->pid, NULL, 0);
			forkdata->pid = 0;
			close(pipe);
			forkdata->response = g_string_new_len(buf, len);
			g_free(buf);
			gtk_main_quit();
			break;
		case NET_STATUS_ERROR:
			buf = g_new0(char, len+1);
			readall(pipe, buf, len);
			buf[len] = 0;
			waitpid(forkdata->pid, NULL, 0);
			forkdata->pid = 0;
			close(pipe);
			g_set_error(forkdata->err, NET_ERROR, NET_ERROR_GENERIC, buf);
			g_free(buf);
			gtk_main_quit();
			break;
		case NET_STATUS_PROGRESS:
			readall(pipe, &progress, sizeof(NetStatusProgress));
			if (forkdata->user_cb)
				forkdata->user_cb(NET_STATUS_PROGRESS, &progress, forkdata->user_data);
			break;
		default:
			g_warning("pipe_cb: unhandled status %d.\n", t);
	}
}

void
net_mainloop_cancel(NetMainloopHandle handle) {
	ForkData *forkdata = (ForkData*)handle;

	if (forkdata->pid > 0) {
		kill(forkdata->pid, SIGKILL);
		gtk_input_remove(forkdata->pipe_tag);
		forkdata->pipe_tag = 0;
		waitpid(forkdata->pid, NULL, 0);
		forkdata->pid = 0;

		close(forkdata->pipefds[0]);
		close(forkdata->pipefds[1]);
		g_set_error(forkdata->err, NET_ERROR, NET_ERROR_CANCELLED,
				_("Cancelled."));
	}
	gtk_main_quit();
}

GString*
net_post_mainloop(const char *url, GSList *headers, GString *post,
                  NetStatusCallback cb, gpointer data,
                  GError **err) {
	ForkData forkdata = {0};

	forkdata.err = err;
	forkdata.user_cb = cb;
	forkdata.user_data = data;

	/* fork, run the request, then pipe the data back out of the fork. */
	if (pipe(forkdata.pipefds) < 0) {
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC,
				_("Error creating pipe (pipe(): %s)."), g_strerror(errno));
		return NULL;
	}
	forkdata.pid = fork();
	if (forkdata.pid < 0) {
		g_set_error(err, 0, NET_ERROR_GENERIC,
				_("Error forking (fork(): %s)."), g_strerror(errno));
		return NULL;
	} else if (forkdata.pid == 0) { /* child. */
		GString *response;
		GError *err = NULL;

		response = net_post_blocking(url, headers, post, fork_cb, &forkdata, &err);
		if (response == NULL) {
			int len = strlen(err->message);
			pipe_write(forkdata.pipefds[1], NET_STATUS_ERROR, len, err->message);
			g_error_free(err);
		} else {
			pipe_write(forkdata.pipefds[1], NET_STATUS_SUCCESS, response->len, response->str);
			g_string_free(response, TRUE);
		}

		close(forkdata.pipefds[0]);
		close(forkdata.pipefds[1]);

		_exit(0);
	} 
	/* otherwise, we're the parent. */
	forkdata.pipe_tag = gtk_input_add_full(forkdata.pipefds[0], GDK_INPUT_READ, 
			(GdkInputFunction)pipe_cb, NULL, &forkdata, NULL);
	if (cb)
		cb(NET_STATUS_BEGIN, &forkdata, data);

	gtk_main(); /* wait for the response. */
	if (forkdata.pipe_tag) {
		gtk_input_remove(forkdata.pipe_tag);
		forkdata.pipe_tag = 0;
	}
	forkdata.pid = 0;

	close(forkdata.pipefds[0]);
	close(forkdata.pipefds[1]);

	if (cb)
		cb(NET_STATUS_DONE, &forkdata, data);

	return forkdata.response;
}

