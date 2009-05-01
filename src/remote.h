/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef remote_h
#define remote_h

GQuark remote_error_quark(void);
#define REMOTE_ERROR remote_error_quark()

typedef enum {
	REMOTE_ERROR_SYSTEM
} RemoteError;

typedef struct _LogJamRemote LogJamRemote;
typedef struct _LogJamRemoteClass LogJamRemoteClass;

LogJamRemote *logjam_remote_new(void);

gboolean logjam_remote_is_listening(LogJamRemote *remote);

gboolean logjam_remote_listen(LogJamRemote *remote, GError **err);
gboolean logjam_remote_stop_listening(LogJamRemote *remote, GError **err);
gboolean logjam_remote_send_present(LogJamRemote *remote, GError **err);


/* sending side doesn't need an object. */
gboolean remote_send_user(const char *username, GError **err);

#endif /* remote_h */
