/* gxr - an XMLRPC library for libxml.
 * Copyright (C) 2003 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef GXR_H
#define GXR_H

#include <glib.h>

/* this function should be provided by the user of the gxr library:
 * using the information in "ctx", it runs the HTTP post "request" and
 * stores the response in "response".  if there was an HTTP error, it
 * sets "err" and returns FALSE; otherwise, it returns TRUE. */
typedef gboolean (*GXRRunRequestFunc)(gpointer data,
                                      GString *request, GString *response,
                                      GError **err);

typedef struct _GXRContext GXRContext;

GXRContext* gxr_context_new(GXRRunRequestFunc func, gpointer data);
void        gxr_context_set_debug(GXRContext *ctx, gboolean debug);
void        gxr_context_free(GXRContext *ctx);

#endif /* GXR_H */
