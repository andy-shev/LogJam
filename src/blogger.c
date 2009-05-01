/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include <blogger.h>

#include "jamdoc.h"
#include "jam_xml.h"
#include "network.h"

#include "account.h"

struct _JamHostBlogger {
	JamHost host;
	char *rpcurl; /* RPC URL -- yay acronyms! */
};

typedef struct {
	NetContext *ctx;
	char *rpcurl;
} JamGXRCtxInfo;

static gboolean
jam_gxr_callback(gpointer data,
                 GString *request, GString *response,
                 GError **err) {
	JamGXRCtxInfo *info = data;
	GSList *headers = NULL;
	GString *resp;

	headers = g_slist_append(headers, "Content-Type: text/xml");
	resp = info->ctx->http(info->ctx, info->rpcurl, headers, request, err);
	/* XXX fix gxr so we don't need this copy */
	if (resp) {
		g_string_append_len(response, resp->str, resp->len);
		g_string_free(resp, TRUE);
	} else {
		response = NULL;
	}
	g_slist_free(headers);

	return response != NULL;
}

char*
jam_host_blogger_get_rpcurl(JamHostBlogger *host) {
	return host->rpcurl;
}

void
jam_host_blogger_set_rpcurl(JamHostBlogger *host, const char* url) {
	g_free(host->rpcurl);
	host->rpcurl = g_strdup(url);
}

static const char*
jam_host_blogger_get_stock_icon(void) {
	return "logjam-blogger";
}

static gboolean
jam_host_blogger_load_xml(JamHost *host, xmlDocPtr doc, xmlNodePtr node) {
	JamHostBlogger *hostb = JAM_HOST_BLOGGER(host);

	XML_GET_STR("rpcurl", hostb->rpcurl)
	/* else: */ return FALSE;

	return TRUE;
}

static void
jam_host_blogger_save_xml(JamHost *host, xmlNodePtr servernode) {
	JamHostBlogger *hostb = JAM_HOST_BLOGGER(host);

	xmlSetProp(servernode, "protocol", "blogger");
	xmlNewTextChild(servernode, NULL, "rpcurl", hostb->rpcurl);
}

static JamAccount*
jam_host_blogger_make_account(JamHost *host, const char *username) {
	JamAccount *acc;
	acc = jam_account_blogger_new(username);
	jam_host_add_account(host, acc);
	return acc;
}

static gboolean
jam_host_blogger_do_post(JamHost *host, NetContext *ctx, void *doc, GError **err) {
	GXRContext *gxrctx;
	JamGXRCtxInfo info_stack = {0}, *info = &info_stack;
	JamAccount *acc;
	GError *myerr = NULL;

	acc = jam_doc_get_account(doc);

	info->rpcurl = jam_host_blogger_get_rpcurl(JAM_HOST_BLOGGER(host));
	info->ctx = ctx;

	gxrctx = gxr_context_new(jam_gxr_callback, info);
	blogger_newPost(gxrctx, "appkey",
			jam_doc_get_usejournal(doc),
			jam_account_get_username(acc),
			jam_account_get_password(acc),
			"this is my post",
			TRUE,
			&myerr);
	gxr_context_free(gxrctx);

	if (myerr)
		g_propagate_error(err, myerr);
	return myerr != NULL;
}

static gboolean
jam_host_blogger_do_edit(JamHost *host, NetContext *ctx, void *doc, GError **err) {
	return FALSE;
}

static gboolean
jam_host_blogger_do_delete(JamHost *host, NetContext *ctx, void *doc, GError **err) {
	return FALSE;
}

static void
jam_host_blogger_class_init(JamHostClass *klass) {
	klass->get_stock_icon = jam_host_blogger_get_stock_icon;
	klass->save_xml = jam_host_blogger_save_xml;
	klass->load_xml = jam_host_blogger_load_xml;
	klass->make_account = jam_host_blogger_make_account;
	klass->do_post = jam_host_blogger_do_post;
	klass->do_edit = jam_host_blogger_do_edit;
	klass->do_delete = jam_host_blogger_do_delete;
}

GType
jam_host_blogger_get_type(void) {
	static GType new_type = 0;
	if (!new_type) {
		const GTypeInfo new_info = {
			sizeof(JamHostClass),
			NULL,
			NULL,
			(GClassInitFunc) jam_host_blogger_class_init,
			NULL,
			NULL,
			sizeof(JamHostBlogger),
			0,
			NULL
		};
		new_type = g_type_register_static(JAM_TYPE_HOST,
				"JamHostBlogger", &new_info, 0);
	}
	return new_type;
}

JamHostBlogger*
jam_host_blogger_new(void) {
	JamHostBlogger *b = JAM_HOST_BLOGGER(g_object_new(jam_host_blogger_get_type(), NULL));
	return b;
}


