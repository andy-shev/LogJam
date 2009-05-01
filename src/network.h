/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LOGJAM_NETWORK_H__
#define __LOGJAM_NETWORK_H__

#include <livejournal/livejournal.h>
#include <livejournal/verb.h>

/* network -- medium-level interface to lj protocol.
 * takes a hash of request values, returns hash of lj
 * response, optionally with a "progress" dialog that
 * can be cancelled.
 */

GQuark net_error_quark(void);
#define NET_ERROR net_error_quark()

/* CANCELLED is used so we don't pop up a dialog saying "cancelled"
 * when they click cancel.
 * PROTOCOL is for protocol-level errors; if getchallenge fails at a lower
 * level, subsequent requests won't work either.
 */
typedef enum {
	NET_ERROR_GENERIC,
	NET_ERROR_PROTOCOL,
	NET_ERROR_CANCELLED
} NetErrorCode;

typedef struct _NetContext NetContext;
/* the internals of netcontext shouldn't be used by casual callers. */
struct _NetContext {
	void (*title)(NetContext *ctx, const char *title);
	void (*progress_str)(NetContext *ctx, const char *text);
	void (*progress_int)(NetContext *ctx, int cur, int max);
	void (*error)(NetContext *ctx, GError *err);
	GString* (*http)(NetContext *ctx, const char *url, GSList *headers, GString *post, GError **err);
};

extern NetContext *network_ctx_cmdline;
extern NetContext *network_ctx_silent;
gboolean net_run_verb_ctx(LJVerb *verb, NetContext *ctx, GError **err);
#ifdef HAVE_GTK
#include <gtk/gtkwindow.h>
NetContext* net_ctx_gtk_new(GtkWindow *parent, const char *title);
void        net_ctx_gtk_free(NetContext* ctx);
#endif

#ifdef HAVE_GTK
/* lower-level window-based network interface.
 * used by syncitems to have a custom progress dialog. */
#include "progress.h"

gboolean   net_window_run_verb(ProgressWindow *pw, LJVerb *verb);
gboolean   net_window_run_post(ProgressWindow *pw, const char *url,
                               GSList *headers,
                               GString *request, GString *response,
                               GError **err);
GString*   net_get_run(const char *title, GtkWindow *parent,
                       const char *url);

#endif

#endif /* __LOGJAM_NETWORK_H__ */
