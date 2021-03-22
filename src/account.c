/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Gaal Yahas <gaal@forum2.org>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include <string.h>
#include "conf.h"
#include "conf_xml.h"
#include "jam_xml.h"
#include "account.h"
#include "util.h"

static GObjectClass *jam_account_parent_class = NULL;

static GHashTable *jam_account_cache = NULL;

void
jam_account_logjam_init(void) {
	/* can't be in jam_account_class_init because we can call
	 * jam_account_lookup before the jam_account type exists! */
	jam_account_cache = g_hash_table_new(g_str_hash, g_str_equal);
}

static void
jam_account_finalize(GObject *object) {
	JamAccount *acc = JAM_ACCOUNT(object);

	/* unregister it */
	g_hash_table_remove(jam_account_cache, acc);

	/* must chain up */
	(* jam_account_parent_class->finalize) (object);
}

static void
jam_account_class_init(GObjectClass *class) {
	jam_account_parent_class = g_type_class_peek_parent (class);
	class->finalize = jam_account_finalize;
}

const gchar*
jam_account_get_username(JamAccount *acc) {
	return JAM_ACCOUNT_GET_CLASS(acc)->get_username(acc);
}

const gchar*
jam_account_get_password(JamAccount *acc) {
	return JAM_ACCOUNT_GET_CLASS(acc)->get_password(acc);
}

void
jam_account_set_username(JamAccount *acc, const char *username) {
	JAM_ACCOUNT_GET_CLASS(acc)->set_username(acc, username);
}

void
jam_account_set_password(JamAccount *acc, const char *password) {
	JAM_ACCOUNT_GET_CLASS(acc)->set_password(acc, password);
}

JamHost*
jam_account_get_host(JamAccount *acc) {
	return acc->host;
}

void
jam_account_set_remember(JamAccount *acc, gboolean u, gboolean p) {
	acc->remember_user = u;
	acc->remember_password = p;
}

void
jam_account_get_remember(JamAccount *acc, gboolean *u, gboolean *p) {
	if (u) *u = acc->remember_user;
	if (p) *p = acc->remember_password;
}

gboolean
jam_account_get_remember_password(JamAccount *acc) {
	return acc->remember_password;
}

gchar*
jam_account_id_strdup_from_names(const gchar *username, const gchar *hostname) {
	return g_strdup_printf("%s@%s", username, hostname);
}

gchar*
jam_account_id_strdup(JamAccount *acc) {
	return jam_account_id_strdup_from_names(
			jam_account_get_username(acc),
			acc->host->name);
}

JamAccount*
jam_account_lookup(gchar *id) {
	return (JamAccount*) g_hash_table_lookup(jam_account_cache, id);
}

#if 0
static void
jam_account_add_to_cache(JamAccount *acc) {
	char *id;
	id = jam_account_id_strdup_from_names(user->username, host->name);
	g_hash_table_insert(jam_account_cache, id, acc);
	g_free(id);
}
#endif

/*JamAccount*
jam_account_new(LJUser *u) {
	gchar *id = jam_account_id_strdup_from_names(u->username, u->server->name);
	JamAccount *acc = jam_account_lookup(id);
	if (acc) {
		g_free(id);
		return acc;
	}
	acc = JAM_ACCOUNT(g_object_new(jam_account_get_type(), NULL));
	acc->user = u;
	g_hash_table_insert(cache, id, acc);
	return acc;
}*/

#if 0
JamAccount*
jam_account_get_from_names(const gchar *username, const gchar *hostname) {
	JamHost *host;
	JamAccount *acc;
	gchar *id;
	LJUser *u;

	id = jam_account_id_strdup_from_names(username, hostname);
	acc = jam_account_lookup(id);

	if (acc) { /* have it already */
		g_free(id);
		return acc;
	}

	// FIXME: create a new server / user if they're missing
	if (!(host = conf_host_by_name(&conf, hostname)))
		g_error(_("host %s not found in conf"), hostname);
	if (!(acc = jam_host_get_account_by_username(host, username)))
		g_error(_("user %s not found in server %s"), username, hostname);

#if 0
	u = lj_user_new(
	acc =
	acc = JAM_ACCOUNT(g_object_new(jam_account_get_type(), NULL));
	acc->user = u;
	acc->host = h;
	acc->user->server = s;
#endif
	g_hash_table_insert(jam_account_cache, id, acc);
	return acc;
}
#endif

/*JamAccount*
jam_account_lookup_by_user(LJUser *u) {
	return jam_account_new(u);
}*/

void
jam_account_rename(JamAccount *acc,
		const gchar *username, const gchar *hostname) {
	gchar *newid = jam_account_id_strdup_from_names(username, hostname);
	gchar *oldid = jam_account_id_strdup(acc);
	if (!g_hash_table_steal(jam_account_cache, oldid))
		g_error("can't rename account: old account with this name not found");
	jam_account_set_username(acc, username);
	string_replace(&(acc->host->name), (gchar*)hostname);
	g_free(oldid);
	g_hash_table_insert(jam_account_cache, newid, acc);
}

GType
jam_account_get_type(void) {
	static GType new_type = 0;
	if (!new_type) {
		const GTypeInfo new_info = {
			sizeof(JamAccountClass),
			NULL,
			NULL,
			(GClassInitFunc) jam_account_class_init,
			NULL,
			NULL,
			sizeof(JamAccount),
			0,
			NULL
		};
		new_type = g_type_register_static(G_TYPE_OBJECT,
				"JamAccount", &new_info, G_TYPE_FLAG_ABSTRACT);
	}
	return new_type;
}

JamAccount*
jam_account_from_xml(xmlDocPtr doc, xmlNodePtr node, JamHost *host) {
	JamAccount *acc;
	xmlChar *protocol = xmlGetProp(node, BAD_CAST "protocol");
	if (!protocol || xmlStrcmp(protocol, BAD_CAST "livejournal") == 0) {
		acc = jam_account_lj_from_xml(doc, node, JAM_HOST_LJ(host));
#ifdef blogger_punted_for_this_release
	} else if (xmlStrcmp(protocol, BAD_CAST "blogger") == 0) {
		acc = jam_account_blogger_from_xml(doc, node, JAM_HOST_BLOGGER(host));
#endif /* blogger_punted_for_this_release */
	} else {
		g_error("unknown protocol '%s'\n", protocol);
		return NULL;
	}
	if (protocol) xmlFree(protocol);

	acc->remember_user = TRUE;
	if (jam_account_get_password(acc))
		acc->remember_password = TRUE;
	acc->host = host;

	return acc;
}

gboolean
jam_account_write(JamAccount *account, GError **err) {
	xmlDocPtr doc = NULL;
	char *path;
	xmlNodePtr node;
	gboolean ret = FALSE;

	if (!account->remember_user)
		return TRUE;

	path = g_build_filename(app.conf_dir, "servers", account->host->name,
			"users", jam_account_get_username(account), "conf.xml", NULL);
	if (!verify_path(path, FALSE, err))
		goto out;

	jam_xmlNewDoc(&doc, &node, "user");

	xmlNewTextChild(node, NULL, BAD_CAST "username",
	                BAD_CAST jam_account_get_username(account));
	if (account->remember_password)
		xmlNewTextChild(node, NULL, BAD_CAST "password",
		                BAD_CAST jam_account_get_password(account));

	if (JAM_ACCOUNT_IS_LJ(account)) { // XXX blogger
		jam_account_lj_write(JAM_ACCOUNT_LJ(account), node);
		xmlSetProp(node, BAD_CAST "protocol", BAD_CAST "livejournal");
	}
#ifdef blogger_punted_for_this_release
	else {
		xmlSetProp(node, BAD_CAST "protocol", BAD_CAST "blogger");
	}
#endif

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

#ifdef blogger_punted_for_this_release
JamAccount*
jam_account_blogger_from_xml(xmlDocPtr doc, xmlNodePtr node, JamHostBlogger *host) {
	JamAccount *account;
	JamAccountBlogger *accountb;

	account = jam_account_blogger_new(NULL);
	accountb = JAM_ACCOUNT_BLOGGER(account);
	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
		XML_GET_STR("username", accountb->username)
		XML_GET_STR("password", accountb->password)
		//XML_GET_FUNC("usejournals", accountb->usejournals, parseusejournals)
		XML_GET_END("jam_account_blogger_from_xml")
	}

	return account;
}

const gchar*
jam_account_blogger_get_username(JamAccount *acc) {
	return JAM_ACCOUNT_BLOGGER(acc)->username;
}

const gchar*
jam_account_blogger_get_password(JamAccount *acc) {
	return JAM_ACCOUNT_BLOGGER(acc)->password;
}

static void
jam_account_blogger_set_password(JamAccount *acc, const char *password) {
	string_replace(&JAM_ACCOUNT_BLOGGER(acc)->password, g_strdup(password));
}

static void
jam_account_blogger_class_init(JamAccountClass *klass) {
	klass->get_username = jam_account_blogger_get_username;
	klass->set_password = jam_account_blogger_set_password;
	klass->get_password = jam_account_blogger_get_password;
}

GType
jam_account_blogger_get_type(void) {
	static GType new_type = 0;
	if (!new_type) {
		const GTypeInfo new_info = {
			sizeof(JamAccountClass),
			NULL,
			NULL,
			(GClassInitFunc) jam_account_blogger_class_init,
			NULL,
			NULL,
			sizeof(JamAccountBlogger),
			0,
			NULL
		};
		new_type = g_type_register_static(JAM_TYPE_ACCOUNT,
				"JamAccountBlogger", &new_info, 0);
	}
	return new_type;
}

JamAccount*
jam_account_blogger_new(const char *username) {
	JamAccountBlogger *acc;

	acc = JAM_ACCOUNT_BLOGGER(g_object_new(jam_account_blogger_get_type(), NULL));
	if (username)
		acc->username = g_strdup(username);

	return JAM_ACCOUNT(acc);
}
#endif /* blogger_punted_for_this_release */


