/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef network_internal_h
#define network_internal_h

#include "network.h"

/* network-internal -- interface to http, used by network.
 * provides a blocking and nonblocking interface.
 * implemented by
 *   network-curl -- curl (unix, fork);
 *   network-win32 -- windows api (windows, threads).
 */

#define READ_BLOCK_SIZE 2048

typedef enum {
	NET_STATUS_NULL,
	NET_STATUS_BEGIN,
	NET_STATUS_SUCCESS,
	NET_STATUS_ERROR,
	NET_STATUS_PROGRESS,
	NET_STATUS_DONE
} NetStatusType;

typedef struct {
	guint32 current;
	guint32 total;
} NetStatusProgress;

typedef void (*NetStatusCallback)(NetStatusType status,
                                  gpointer statusdata,
                                  gpointer data);

GString* net_post_blocking(const char *url, GSList *headers, GString *post,
                           NetStatusCallback cb, gpointer data,
                           GError **err);

typedef void* NetMainloopHandle;

GString* net_post_mainloop(const char *url, GSList *headers, GString *post,
                           NetStatusCallback cb, gpointer data,
                           GError **err);
void net_mainloop_cancel(NetMainloopHandle handle);

gboolean net_verb_run_internal(LJVerb *verb,
                               NetStatusCallback cb, gpointer data,
                               GError **err);

#endif /* network_internal_h */

