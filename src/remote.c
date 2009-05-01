/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

/* remote code written after reading xmms remote code.
 * Copyright (C) 1998-2002  Peter Alm, Mikael Alm, Olle Hallnas,
 *                          Thomas Nilsson and 4Front Technologies
 * Copyright (C) 1999-2002  Haavard Kvaalen
 */

#include "config.h"

#include "gtk-all.h"
#ifndef G_OS_WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#endif

#include "marshalers.h"

#include "conf.h"
#include "remote.h"

/*** remote protocol structure ***/
#define REMOTE_PROTOCOL_ACK         'A'
#define REMOTE_PROTOCOL_PRESENT     'P'
#define REMOTE_PROTOCOL_CHANGE_USER 'U'

typedef struct {
	char username[50];
	gboolean login;
} RemoteChangeLJUser;


/*** LogJamRemote object ***/
struct _LogJamRemote {
	GObject parent;

	int fd;
};

struct _LogJamRemoteClass {
	GObjectClass parent_class;
	void (*present)(LogJamRemote *remote);
	gboolean (*change_user)(LogJamRemote *remote,
			char* username, gboolean skip, GError **err);
};

enum {
	PRESENT,
	CHANGE_USER,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
logjam_remote_finalize(GObject *object) {
	GObjectClass *parent_class;

	logjam_remote_stop_listening((LogJamRemote*)object, NULL);

	parent_class = g_type_class_peek_parent(G_OBJECT_GET_CLASS(object));
	parent_class->finalize(object);
}

static void
logjam_remote_class_init(gpointer klass, gpointer class_data) {
	GObjectClass *gclass = G_OBJECT_CLASS(klass);

	gclass->finalize = logjam_remote_finalize;

	signals[PRESENT] = g_signal_new("present",
			G_OBJECT_CLASS_TYPE(gclass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(LogJamRemoteClass, present),
			NULL, NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);

	signals[CHANGE_USER] = g_signal_new("change_user",
			G_OBJECT_CLASS_TYPE(gclass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(LogJamRemoteClass, change_user),
			NULL, NULL,
			logjam_marshal_BOOLEAN__STRING_POINTER,
			G_TYPE_BOOLEAN, 2, G_TYPE_STRING, G_TYPE_POINTER);
}

static void
logjam_remote_init(GTypeInstance *inst, gpointer g_class) {
	LogJamRemote *remote = (LogJamRemote*)inst;
	remote->fd = -1;
}

GType
logjam_remote_get_type() {
	static GType remote_type = 0;
	if (!remote_type) {
		static const GTypeInfo remote_info = {
			sizeof(LogJamRemoteClass),
			NULL,
			NULL,
			logjam_remote_class_init,
			NULL,
			NULL,
			sizeof(LogJamRemote),
			0,
			logjam_remote_init
		};
		remote_type = g_type_register_static(G_TYPE_OBJECT, "LogJamRemote",
				&remote_info, 0);
	}
	return remote_type;
}

LogJamRemote*
logjam_remote_new(void) {
	return g_object_new(logjam_remote_get_type(), NULL);
}

GQuark
remote_error_quark(void) {
	static GQuark quark = 0;
	if (quark == 0)
		quark = g_quark_from_static_string("remote-error-quark");
	return quark;
}

#ifndef G_OS_WIN32
static void
make_remote_path(char *buf, int len) {
	g_snprintf(buf, len, "%s/remote", app.conf_dir);
}

static int
remote_connect(GError **err) {
	int fd;
	struct sockaddr_un saddr;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		g_set_error(err, REMOTE_ERROR, REMOTE_ERROR_SYSTEM, 
				"socket: %s", g_strerror(errno));
		return -1;
	}

	saddr.sun_family = AF_UNIX;
	make_remote_path(saddr.sun_path, 100);
	if (connect(fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
		close(fd);
		if (errno != ENOENT && errno != ECONNREFUSED) {
			g_set_error(err, REMOTE_ERROR, REMOTE_ERROR_SYSTEM, 
					"connect: %s", g_strerror(errno));
		}
		/* ENOENT is not a real error; there was just nobody there. */
		return -1;
	}
	
	return fd;
}

static gboolean
remote_send(char command, gpointer data, int datalen, GError **err) {
	int fd, len;
	char response;

	if ((fd = remote_connect(err)) < 0)
		return FALSE;

	len = write(fd, &command, 1);
	if (len < 0) {
		g_set_error(err, REMOTE_ERROR, REMOTE_ERROR_SYSTEM, 
				"write: %s", g_strerror(errno));
		return FALSE;
	}
	if (datalen > 0) {
		len = write(fd, data, datalen);
		if (len < 0) {
			g_set_error(err, REMOTE_ERROR, REMOTE_ERROR_SYSTEM, 
					"write: %s", g_strerror(errno));
			return FALSE;
		}
	}

	len = read(fd, &response, 1);
	if (len < 0) {
		g_set_error(err, REMOTE_ERROR, REMOTE_ERROR_SYSTEM, 
				"read: %s", g_strerror(errno));
		return FALSE;
	}
	if (len < 1)
		return FALSE;

	if (response == REMOTE_PROTOCOL_ACK)
		return TRUE;
	return FALSE;
}
#endif /* G_OS_WIN32 */

gboolean
remote_send_user(const char *username, GError **err) {
#ifdef G_OS_WIN32
	return FALSE;
#else
	if (username) {
		RemoteChangeLJUser user;
		strcpy(user.username, username);
		user.login = TRUE;
		return remote_send(REMOTE_PROTOCOL_CHANGE_USER,
				&user, sizeof(RemoteChangeLJUser), err);
	} else {
		return remote_send(REMOTE_PROTOCOL_PRESENT, NULL, 0, err);
	}
#endif /* G_OS_WIN32 */
}

#ifndef G_OS_WIN32
static gboolean
remote_recieve_change_user(LogJamRemote *remote, int fd) {
	RemoteChangeLJUser user;
	int readret;
	gboolean signalret;
	GError *err = NULL;

	readret = read(fd, &user, sizeof(RemoteChangeLJUser));
	if (readret < 0) {
		/*jam_warning(win, _("Accepting remote command: %s: %s."),
				"read", g_strerror(errno));*/
		close(fd);
		return FALSE;
	}
	
	g_signal_emit_by_name(remote, "change_user",
			user.username, &err, &signalret);
	return TRUE;
}

static gboolean
io_cb(GIOChannel *channel, GIOCondition cond, gpointer data) {
	LogJamRemote *remote = (LogJamRemote*)data;
	char command;
	int fd, ret;
	struct sockaddr_un saddr;
	socklen_t addrlen = sizeof(struct sockaddr_un);

	if (cond == G_IO_IN) {
		fd = accept(remote->fd, (struct sockaddr*)&saddr, &addrlen);
		if (fd < 0) {
			/*jam_warning(win, _("Accepting remote command: %s: %s."),
					"accept", g_strerror(errno));*/
			return TRUE;
		}

		ret = read(fd, &command, 1);
		if (ret < 0) {
			/*jam_warning(win, _("Accepting remote command: %s: %s."),
					"read", g_strerror(errno));*/
			close(fd);
			return TRUE;
		}

		switch (command) {
		case REMOTE_PROTOCOL_PRESENT:
			g_signal_emit_by_name(remote, "present");
			break;
		case REMOTE_PROTOCOL_CHANGE_USER:
			if (!remote_recieve_change_user(remote, fd))
				return TRUE;
			break;
		default:
			//jam_warning(win, _("Unknown remote command (%c)."), command);
			close(fd);
			return TRUE;
		}

		command = REMOTE_PROTOCOL_ACK;
		write(fd, &command, 1);
		close(fd);
	}

	return TRUE;
}
#endif /* G_OS_WIN32 */

gboolean
logjam_remote_listen(LogJamRemote *remote, GError **err) {
#ifdef G_OS_WIN32
	return FALSE;
#else
	struct sockaddr_un saddr;
	GIOChannel *channel;

	if (remote->fd > 0)
		return TRUE;

	if ((remote->fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		g_set_error(err, REMOTE_ERROR, REMOTE_ERROR_SYSTEM, 
				"socket: %s", g_strerror(errno));
		return FALSE;
	}

	saddr.sun_family = AF_UNIX;
	make_remote_path(saddr.sun_path, 100);
	unlink(saddr.sun_path);
	conf_verify_dir();
	if (bind(remote->fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
		close(remote->fd);
		remote->fd = -1;
		g_set_error(err, REMOTE_ERROR, REMOTE_ERROR_SYSTEM, 
				"bind: %s", g_strerror(errno));
		return FALSE;
	}

	if (listen(remote->fd, 1) < 0) {
		close(remote->fd);
		remote->fd = -1;
		g_set_error(err, REMOTE_ERROR, REMOTE_ERROR_SYSTEM, 
				"listen: %s", g_strerror(errno));
		return FALSE;
	}
	
	channel = g_io_channel_unix_new(remote->fd);
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);
	g_io_add_watch(channel, G_IO_IN|G_IO_ERR,
			io_cb, remote);
	g_io_channel_unref(channel);

	return TRUE;
#endif /* G_OS_WIN32 */
}

gboolean
logjam_remote_stop_listening(LogJamRemote *remote, GError **err) {
#ifdef G_OS_WIN32
	return FALSE;
#else
	char buf[128];

	if (remote->fd <= 0)
		return TRUE;

	close(remote->fd);
	remote->fd = -1;

	make_remote_path(buf, 128);
	unlink(buf);

	return TRUE;
#endif /* G_OS_WIN32 */
}

