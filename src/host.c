/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Gaal Yahas <gaal@forum2.org>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "glib-all.h"

#include <string.h>
#include "jamdoc.h"
#include "account.h"
#include "jam_xml.h"
#include "conf_xml.h"

const char*
jam_host_get_stock_icon(JamHost *host) {
	return JAM_HOST_GET_CLASS(host)->get_stock_icon();
}

JamAccount*
jam_host_get_account_by_username(JamHost *host,
                                 const char *username, gboolean create) {
	JamAccount *acc = NULL;
	GSList *l;
	for (l = host->accounts; l != NULL; l = l->next) {
		acc = l->data;
		if (strcmp(username, jam_account_get_username(acc)) == 0)
			return acc;
	}
	/* this is a previously-unknown account. */
	if (create)
		return JAM_HOST_GET_CLASS(host)->make_account(host, username);
	else
		return NULL;
}

void
jam_host_add_account(JamHost *host, JamAccount *acc) {
	acc->host = host;
	host->accounts = g_slist_append(host->accounts, acc);
}

static void*
parseuserdir(const char *dirname, void *host) {
	return conf_parsedirxml(dirname, (conf_parsedirxml_fn)jam_account_from_xml, host);
}

JamHost*
jam_host_from_xml(xmlDocPtr doc, xmlNodePtr node, void *data) {
	JamHost *host = NULL;
	char *username = NULL;
	char *userspath;
	JamHostClass *klass;

	xmlChar *protocol = xmlGetProp(node, BAD_CAST "protocol");
	if (!protocol || xmlStrcmp(protocol, BAD_CAST "livejournal") == 0) {
		host = JAM_HOST(jam_host_lj_new(lj_server_new(NULL)));
#ifdef blogger_punted_for_this_release
	} else if (xmlStrcmp(protocol, BAD_CAST "blogger") == 0) {
		host = JAM_HOST(jam_host_blogger_new());
#endif
	} else {
		g_error("unknown protocol '%s'\n", protocol);
	}
	if (protocol) xmlFree(protocol);
	klass = JAM_HOST_GET_CLASS(host);

	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
		XML_GET_STR("name", host->name)
		XML_GET_STR("currentuser", username)
		XML_GET_SUB(host, klass->load_xml)
		XML_GET_END("jam_host_from_xml")
	}

	userspath = g_build_filename(data, "users", NULL);
	host->accounts = conf_parsedirlist(userspath, parseuserdir, host);
	g_free(userspath);

	/* after we've parsed all of the users, scan for the current one. */
	if (username) {
		host->lastaccount = jam_host_get_account_by_username(host, username, FALSE);
		g_free(username);
	}

	return host;
}

gboolean
jam_host_write(JamHost *host, GError **err) {
	xmlDocPtr doc = NULL;
	GSList *l;
	char *path;
	xmlNodePtr servernode;
	GError *terr = NULL;
	gboolean ret = FALSE;

	path = g_build_filename(app.conf_dir, "servers", host->name, "conf.xml", NULL);
	if (!verify_path(path, FALSE, err))
		goto out;

	jam_xmlNewDoc(&doc, &servernode, "server");

	xmlNewTextChild(servernode, NULL, BAD_CAST "name", BAD_CAST host->name);

	for (l = host->accounts; l != NULL; l = l->next) {
		JamAccount *acc = l->data;
		if (!jam_account_write(acc, &terr)) {
			g_printerr("Error writing account: %s\n", terr->message);
			g_error_free(terr);
			terr = NULL;
		}
	}

	if (host->lastaccount)
		xmlNewTextChild(servernode, NULL, BAD_CAST "currentuser",
		                BAD_CAST jam_account_get_username(host->lastaccount));

	JAM_HOST_GET_CLASS(host)->save_xml(host, servernode);

	if (xmlSaveFormatFile(path, doc, TRUE) < 0) {
		g_set_error(err, 0, 0, "xmlSaveFormatFile error saving to %s.\n", path);
		goto out;
	}
	ret = TRUE;

out:
	if (path) g_free(path);
	if (doc) xmlFreeDoc(doc);

	return ret;
}

GType
jam_host_get_type(void) {
	static GType new_type = 0;
	if (!new_type) {
		const GTypeInfo new_info = {
			sizeof(JamHostClass),
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			sizeof(JamHost),
			0,
			NULL
		};
		new_type = g_type_register_static(G_TYPE_OBJECT,
				"JamHost", &new_info, G_TYPE_FLAG_ABSTRACT);
	}
	return new_type;
}


/* protocol functions. */
gboolean
jam_host_do_post(JamHost *host, NetContext *ctx, void *doc, GError **err) {
	if (ctx->title)
		ctx->title(ctx, _("Submitting Entry"));
	return JAM_HOST_GET_CLASS(host)->do_post(host, ctx, doc, err);
}

gboolean
jam_host_do_edit(JamHost *host, NetContext *ctx, void *doc, GError **err) {
	if (ctx->title)
		ctx->title(ctx, _("Saving Changes"));
	return JAM_HOST_GET_CLASS(host)->do_edit(host, ctx, doc, err);
}

gboolean
jam_host_do_delete(JamHost *host, NetContext *ctx, void *doc, GError **err) {
	if (ctx->title)
		ctx->title(ctx, _("Deleting Entry"));
	return JAM_HOST_GET_CLASS(host)->do_delete(host, ctx, doc, err);
}


