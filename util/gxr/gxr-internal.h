/* gxr - an XMLRPC library for libxml.
 * Copyright (C) 2003 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef GXR_INTERNAL_H
#define GXR_INTERNAL_H

#include <glib.h>
#include <libxml/tree.h>
#include "gxr.h"

struct _GXRContext {
	GXRRunRequestFunc run_request;
	gboolean debug;
	gpointer user_data;
};

typedef enum {
	GXR_VALUE_UNKNOWN,
	GXR_VALUE_INT,
	GXR_VALUE_STRING,
	GXR_VALUE_BOOLEAN
} GXRValueType;

void gxr_make_doc(xmlDocPtr *pdoc, xmlNodePtr *pnparams, const char *methodname);

void gxr_add_param_string(xmlNodePtr nparent, const char *value);
void gxr_add_param_int(xmlNodePtr nparent, int value);
void gxr_add_param_boolean(xmlNodePtr nparent, gboolean value);

gboolean gxr_run_request(GXRContext *ctx, xmlDocPtr doc, GXRValueType wanttype, char **retval, GError **err);

#endif /* GXR_INTERNAL_H */
