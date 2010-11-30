/* logjam - a GTK client for LiveJournal.
 *
 * Functions to run some external program and return its output.
 * Copyright (C) 2004, Kir Kolyshkin <kir@sacred.ru>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/signal.h>

struct mypopen_t {
	int fd;
	pid_t pid;
	gboolean cancelled;
};

/* An analogue of popen(command, "r").
 * A bit more useful since we know child PID.
 */
gboolean mypopen_r(const char *command, struct mypopen_t *p)
{
	int fds[2];
	/* parent fd is fds[0], child fd is fds[1] */

	if (pipe(fds) == -1)
		return FALSE;

	if ((p->pid = fork()) == -1)
	{
		close(fds[0]);
		close(fds[1]);
		return FALSE;
	}

	if (p->pid == 0) {
		/* Child */
		close(fds[0]);
		dup2(fds[1], 1);
		close(fds[1]);
		execl("/bin/sh", "sh", "-c", command, NULL);
		/* if execl failed, exit with code 127 */
		exit(127);
	}
	/* Parent */
	close(fds[1]);
	p->fd = fds[0];
	return TRUE;
}

int mypclose(const struct mypopen_t *p)
{
	int ret, status;

/*	fprintf(stderr, "mypclose: pid=%lu, fd=%d\n", p->pid, p->fd); */

	close(p->fd);
	do {
		ret = waitpid(p->pid, &status, 0);
	} while ((ret == -1) && (errno == EINTR));

/*	fprintf(stderr, "waitpid returned %d; errno = %d; "
			"status = %d; exit = %d; signaled = %d\n",
			ret, errno, status, WEXITSTATUS(status),
			WIFSIGNALED(status)); */
	if (ret == -1)
		return -1;
	return WEXITSTATUS(status);
}

/* Callback function for "Cancel" button of 'command is running' window
 */
static void
cancel_cb(struct mypopen_t *p) {
	p->cancelled = TRUE;
	if (p->pid)
		kill(p->pid, SIGTERM);
	/* FIXME: should we send SIGKILL some time later
	 * if process still exists?
	 */
}

/* Dialog showing that command is being executed.
 * Click on "Cancel" should kill the process which pid is p->pid
 */
GtkWidget *command_is_running(const char * command, struct mypopen_t *p,
		GtkWindow *parent) {
	GtkWidget *win, *vbox, *frame, *text, *cancel;

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(win), _("Command is running..."));
	gtk_window_set_transient_for(GTK_WINDOW(win), parent);
	gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
	gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_modal(GTK_WINDOW(win), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(win), 250, -1);
	gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER_ON_PARENT);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(win), frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	text = gtk_label_new(_("Command is running..."));
	gtk_box_pack_start(GTK_BOX(vbox), text, TRUE, TRUE, 0);

	cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_box_pack_start(GTK_BOX(vbox), cancel, FALSE, FALSE, 0);
	g_signal_connect_swapped(G_OBJECT(cancel), "clicked",
			G_CALLBACK(cancel_cb), p);

	gtk_widget_show_all(win);
	return win;
}

/* Callback function for gtk_input_add_full(). Reads data from the
 * file descriptor. Calls gtk_quit if there's nothing to read.
 */
void pipe_read_cb(gpointer data, gint source, GdkInputCondition condition) {
	gchar buf[BUFSIZ];
	ssize_t c;

	c = read(source, buf, sizeof(buf));
	if (c == 0) /* EOF */
		gtk_main_quit();
	else
		g_string_append_len((GString *)data, buf, c);
}

GString * get_command_output(const char *command, GError **err,
		GtkWindow *parent){
	GtkWidget *win;
	GString *output;
	int ret;
	guint evid;
	struct mypopen_t pd;

	pd.cancelled = FALSE;
	if (mypopen_r(command, &pd) == FALSE) {
		g_set_error(err, 0, 0, _("command failed: %s"),
				g_strerror(errno));
		return NULL;
	}

/*	fprintf(stderr, "before read: pid=%lu, fd=%d\n", pd.pid, pd.fd); */
	output = g_string_sized_new(BUFSIZ);
	evid = gtk_input_add_full(pd.fd, GDK_INPUT_READ,
			pipe_read_cb, NULL, output, NULL);

	win = command_is_running(command, &pd, parent);
	gtk_main();

	gtk_input_remove(evid);
	gtk_widget_destroy(win);
	ret = mypclose(&pd);
	if (pd.cancelled)
	{
		g_string_free(output, TRUE);
		return NULL;
	}
	/* Positive 'ret' values are command exit code, while negative one
	 * means execution error or termination.
	 * Let's ignore all non-zero exit codes but 127, since 127
	 * means "command not found" from sh. Ugly hack?
	 */
	if (ret < 0) {
		g_string_free(output, TRUE);
		g_set_error(err, 0, 0, _("command failed: %s"),
				g_strerror(errno));
		return NULL;
	} else if (ret == 127)
	{
		g_string_free(output, TRUE);
		/* Set err->code to 127 so caller will know that command
		 * was not found. It is probably a spelling error
		 * in command string and in this case caller should not
		 * close a dialog asking for command name, but rather let
		 * the user correct it (or click "Cancel").
		 */
		g_set_error(err, 0, 127, _("command not found"));
		return NULL;
	}
	return output;
}
