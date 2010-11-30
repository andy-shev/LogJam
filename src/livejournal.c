/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "glib-all.h"
#include <string.h>
#include <livejournal/editpostevent.h>
#include "jamdoc.h"
#include "account.h"
#include "jam_xml.h"
#include "conf_xml.h"
#include "network.h"

struct _JamHostLJ {
	JamHost host;
	LJServer *server;
};

JamAccount*
jam_account_lj_new(LJServer *server, const char *username) {
	JamAccountLJ *acc;

	acc = JAM_ACCOUNT_LJ(g_object_new(jam_account_lj_get_type(), NULL));
	acc->user = lj_user_new(server);
	if (username)
		acc->user->username = g_strdup(username);

	return JAM_ACCOUNT(acc);
}

static GSList*
parsepickws(xmlDocPtr doc, xmlNodePtr node) {
	GSList *pickws = NULL;
	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
		XML_GET_LIST("pickw", pickws, jam_xmlGetString)
		XML_GET_END("parsepickws")
	}
	return pickws;
}

static GSList*
parseusejournals(xmlDocPtr doc, xmlNodePtr node) {
	GSList *l = NULL;
	char *journal;
	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
		if (xmlStrcmp(node->name, BAD_CAST "usejournal") == 0) {
			journal = jam_xmlGetString(doc, node);
			l = g_slist_append(l, journal);
		} else
		XML_GET_END("usejournals")
	}
	return l;
}

static GSList*
parsefriendgroups(xmlDocPtr doc, xmlNodePtr node) {
	GSList *fgs = NULL;
	int id;
	LJFriendGroup *fg;
	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
		if (xmlStrcmp(node->name, BAD_CAST "friendgroup") == 0) {
			if (jam_xmlGetIntProp(node, "id", &id)) {
				fg = lj_friendgroup_new();

				fg->id = id;
				fg->name = jam_xmlGetString(doc, node);
				jam_xmlGetIntProp(node, "ispublic", &fg->ispublic);
				fgs = g_slist_append(fgs, fg);
			}
		} else
		XML_GET_END("parsefriendgroups")
	}
	return fgs;
}

static GSList*
parsewebmenu(xmlDocPtr doc, xmlNodePtr node) {
	GSList *webmenu = NULL;
	LJWebMenuItem *wmi;
	xmlChar *text, *url;

	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
		if (xmlStrcmp(node->name, BAD_CAST "menuitem") == 0) {
			wmi = lj_webmenuitem_new();
			if ((text = xmlGetProp(node, BAD_CAST "text"))) {
				wmi->text = g_strdup((char*)text);
				if ((url = xmlGetProp(node, BAD_CAST "url"))) {
					wmi->url = g_strdup((char*)url);
					xmlFree(url);
				}
				xmlFree(text);
			}
			if (node->xmlChildrenNode)
				wmi->subitems = parsewebmenu(doc, node);
			webmenu = g_slist_append(webmenu, wmi);
		} else
		XML_GET_END("parsewebmenu")
	}
	return webmenu;
}

static void
writewebmenu(xmlNodePtr parent, GSList *webmenu) {
	GSList *l;
	xmlNodePtr node;
	LJWebMenuItem *wmi;

	for (l = webmenu; l != NULL; l = l->next) {
		wmi = (LJWebMenuItem*)l->data;
		node = xmlNewChild(parent, NULL, BAD_CAST "menuitem", NULL);
		if (wmi->text)
			xmlSetProp(node, BAD_CAST "text", BAD_CAST wmi->text);
		if (wmi->url)
			xmlSetProp(node, BAD_CAST "url", BAD_CAST wmi->url);
		if (wmi->subitems)
			writewebmenu(node, wmi->subitems);
	}
}

void
jam_account_lj_write(JamAccountLJ *account, xmlNodePtr node) {
	LJUser *user = account->user;
	xmlNodePtr xmlList, child;
	GSList *l;
	char buf[10];
	LJFriendGroup *fg;

	if (user->fullname)
		xmlNewTextChild(node, NULL,
		                BAD_CAST "fullname", BAD_CAST user->fullname);

	if (account->lastupdate)
		jam_xmlAddInt(node, "lastupdate", (int)account->lastupdate);
	if (user->checkfriends)
		jam_xmlAddInt(node, "checkfriends", (int)TRUE);
	if (account->cfmask) {
		g_snprintf(buf, 10, "%d", account->cfmask);
		xmlNewTextChild(node, NULL, BAD_CAST "cfmask", BAD_CAST buf);
	}

	if (user->pickws) {
		xmlList = xmlNewChild(node, NULL, BAD_CAST "pickws", NULL);
		for (l = user->pickws; l != NULL; l = l->next) {
			xmlNewTextChild(xmlList, NULL, BAD_CAST "pickw", BAD_CAST l->data);
		}
	}

	if (user->usejournals) {
		xmlList = xmlNewChild(node, NULL, BAD_CAST "usejournals", NULL);
		for (l = user->usejournals; l != NULL; l = l->next) {
			xmlNewTextChild(xmlList, NULL, BAD_CAST "usejournal", BAD_CAST l->data);
		}
	}

	if (user->friendgroups) {
		xmlList = xmlNewChild(node, NULL, BAD_CAST "friendgroups", NULL);
		for (l = user->friendgroups; l != NULL; l = l->next) {
			fg = (LJFriendGroup*)l->data;
			child = xmlNewTextChild(xmlList, NULL,
			                        BAD_CAST "friendgroup", BAD_CAST fg->name);
			g_snprintf(buf, 10, "%d", fg->id);
			xmlSetProp(child, BAD_CAST "id", BAD_CAST buf);
			g_snprintf(buf, 10, "%d", fg->ispublic);
			xmlSetProp(child, BAD_CAST "ispublic", BAD_CAST buf);
		}
	}

	if (user->webmenu) {
		xmlList = xmlNewChild(node, NULL, BAD_CAST "webmenu", NULL);
		writewebmenu(xmlList, user->webmenu);
	}
}


JamAccount*
jam_account_lj_from_xml(xmlDocPtr doc, xmlNodePtr node, JamHostLJ *host) {
	JamAccount *account;
	JamAccountLJ *accountlj;
	LJUser *user;
	guint32 lastupdate = 0;

	account = jam_account_lj_new(jam_host_lj_get_server(host), NULL);
	accountlj = JAM_ACCOUNT_LJ(account);
	user = accountlj->user;
	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
		XML_GET_STR("username", user->username)
		XML_GET_STR("fullname", user->fullname)
		XML_GET_STR("password", user->password)
		XML_GET_BOOL("checkfriends", user->checkfriends)
		XML_GET_UINT32("lastupdate", lastupdate)
		XML_GET_FUNC("pickws", user->pickws, parsepickws)
		XML_GET_FUNC("usejournals", user->usejournals, parseusejournals)
		XML_GET_FUNC("friendgroups", user->friendgroups, parsefriendgroups)
		XML_GET_FUNC("webmenu", user->webmenu, parsewebmenu)
		XML_GET_UINT32("cfmask", accountlj->cfmask)
		XML_GET_END("jam_account_lj_from_xml")
	}

	accountlj->lastupdate = (time_t)lastupdate;

	return account;
}

const gchar*
jam_account_lj_get_username(JamAccount *acc) {
	return JAM_ACCOUNT_LJ(acc)->user->username;
}

const gchar*
jam_account_lj_get_password(JamAccount *acc) {
	return JAM_ACCOUNT_LJ(acc)->user->password;
}

gboolean
jam_account_lj_get_checkfriends(JamAccount *acc) {
	return JAM_ACCOUNT_LJ(acc)->user->checkfriends;
}

static void
jam_account_lj_set_password(JamAccount *acc, const char *password) {
	string_replace(&JAM_ACCOUNT_LJ(acc)->user->password, g_strdup(password));
}

LJUser*
jam_account_lj_get_user(JamAccountLJ *acc) {
	return acc->user;
}

LJServer*
jam_account_lj_get_server(JamAccountLJ *acc) {
	return acc->user->server;
}

guint32
jam_account_lj_get_cfmask(JamAccountLJ *acc) {
	return acc->cfmask;
}

void
jam_account_lj_set_cfmask(JamAccountLJ *acc, guint32 mask) {
	acc->cfmask = mask;
}

static void
jam_account_lj_class_init(JamAccountClass *klass) {
	klass->get_username = jam_account_lj_get_username;
	klass->set_password = jam_account_lj_set_password;
	klass->get_password = jam_account_lj_get_password;
}

GType
jam_account_lj_get_type(void) {
	static GType new_type = 0;
	if (!new_type) {
		const GTypeInfo new_info = {
			sizeof(JamAccountClass),
			NULL,
			NULL,
			(GClassInitFunc) jam_account_lj_class_init,
			NULL,
			NULL,
			sizeof(JamAccountLJ),
			0,
			NULL
		};
		new_type = g_type_register_static(JAM_TYPE_ACCOUNT,
				"JamAccountLJ", &new_info, 0);
	}
	return new_type;
}

LJServer*
jam_host_lj_get_server(JamHostLJ *host) {
	return host->server;
}

static const char*
jam_host_lj_get_stock_icon(void) {
	return "logjam-server";
}

static GSList*
parsemoods(xmlDocPtr doc, xmlNodePtr node) {
	GSList *moods = NULL;
	LJMood *mood;

	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
		if (xmlStrcmp(node->name, BAD_CAST "mood") == 0) {
			xmlChar *idstr;
			char *name;
			idstr = xmlGetProp(node, BAD_CAST "id");
			name = jam_xmlGetString(doc, node);

			/* an older bug causes some bad moods to be stored.
			 * filter them out. */
			if (idstr == NULL || name == NULL) {
				if (idstr) xmlFree(idstr);
				if (name) xmlFree(name);
				continue;
			}
			mood = g_new0(LJMood, 1);
			mood->id = atoi((char*)idstr);
			xmlFree(idstr);
			mood->name = name;
			idstr = xmlGetProp(node, BAD_CAST "parentid");
			if (idstr) {
				mood->parentid = atoi((char*)idstr);
				xmlFree(idstr);
			}

			moods = g_slist_append(moods, mood);
		} else XML_GET_END("parsemoods")
	}
	return moods;
}

static gboolean
jam_host_lj_load_xml(JamHost *host, xmlDocPtr doc, xmlNodePtr node) {
	LJServer *server = JAM_HOST_LJ(host)->server;

	XML_GET_STR("url", server->url)
	XML_GET_INT("protocolversion", server->protocolversion)
	XML_GET_FUNC("moods", server->moods, parsemoods)
	/* else: */ return FALSE;

	return TRUE;
}

static void
jam_host_lj_save_xml(JamHost *host, xmlNodePtr servernode) {
	JamHostLJ *hostlj = JAM_HOST_LJ(host);
	xmlNodePtr node;
	GSList *l;
	LJServer *server = hostlj->server;

	xmlSetProp(servernode, BAD_CAST "protocol", BAD_CAST "livejournal");

	xmlNewTextChild(servernode, NULL, BAD_CAST "url", BAD_CAST server->url);

	jam_xmlAddInt(servernode, "protocolversion", server->protocolversion);

	node = xmlNewChild(servernode, NULL, BAD_CAST "moods", NULL);
	for (l = server->moods; l != NULL; l = l->next) {
		LJMood *mood = (LJMood*)l->data;
		char buf[10];
		xmlNodePtr moodnode = xmlNewTextChild(node, NULL, BAD_CAST "mood",
		                                      BAD_CAST mood->name);

		g_snprintf(buf, 10, "%d", mood->id);
		xmlSetProp(moodnode, BAD_CAST "id", BAD_CAST buf);
		if (mood->parentid) {
			g_snprintf(buf, 10, "%d", mood->parentid);
			xmlSetProp(moodnode, BAD_CAST "parentid", BAD_CAST buf);
		}
	}
}

static JamAccount*
jam_host_lj_make_account(JamHost *host, const char *username) {
	JamAccount *acc;
	acc = jam_account_lj_new(JAM_HOST_LJ(host)->server, username);
	jam_host_add_account(host, acc);
	return acc;
}

static gboolean
jam_host_lj_do_action(JamHost *host, NetContext *ctx, JamDoc *doc, gboolean edit, gboolean delete, GError **err) {
	LJEditPostEvent *editpostevent;
	JamAccount *account = jam_doc_get_account(doc);
	JamAccountLJ *acc = JAM_ACCOUNT_LJ(account);
	LJEntry *entry;
	gboolean ret = FALSE;

	entry = jam_doc_get_entry(doc);
	if (!edit)
		entry->itemid = 0;
	else if (delete) {
		g_free(entry->event);
		entry->event = NULL;
	}

#if 0
	XXX
	if (jam_host_lj_get_server(JAM_HOST_LJ(host))->protocolversion < 1) {
		/* scan the entry and make sure they're sending ASCII. */
		unsigned char *p;
		gboolean invalid = FALSE;

		for (p = entry->event; *p; p++) {
			if (*p > 0x7F) {
				invalid = TRUE;
				break;
			}
		}
		if (invalid) {
			jam_warning(parent, _("Error: Your post contains Unicode "
						"characters, but you're trying to submit them to a server "
						"that doesn't support Unicode."));
			return FALSE;
		}
	}
#endif

	editpostevent = lj_editpostevent_new(jam_account_lj_get_user(acc),
			jam_doc_get_usejournal(doc), edit, entry);

	if (net_run_verb_ctx((LJVerb*)editpostevent, ctx, err))
		ret = TRUE;

	lj_editpostevent_free(editpostevent);
	lj_entry_free(entry);

	return ret;
}

static gboolean
jam_host_lj_do_post(JamHost *host, NetContext *ctx, void *doc, GError **err) {
	return jam_host_lj_do_action(host, ctx, doc, FALSE, FALSE, err);
}

static gboolean
jam_host_lj_do_edit(JamHost *host, NetContext *ctx, void *doc, GError **err) {
	return jam_host_lj_do_action(host, ctx, doc, TRUE, FALSE, err);
}

static gboolean
jam_host_lj_do_delete(JamHost *host, NetContext *ctx, void *doc, GError **err) {
	return jam_host_lj_do_action(host, ctx, doc, TRUE, TRUE, err);
}

static void
jam_host_lj_class_init(JamHostClass *klass) {
	klass->get_stock_icon = jam_host_lj_get_stock_icon;
	klass->save_xml = jam_host_lj_save_xml;
	klass->load_xml = jam_host_lj_load_xml;
	klass->make_account = jam_host_lj_make_account;
	klass->do_post = jam_host_lj_do_post;
	klass->do_edit = jam_host_lj_do_edit;
	klass->do_delete = jam_host_lj_do_delete;
}

GType
jam_host_lj_get_type(void) {
	static GType new_type = 0;
	if (!new_type) {
		const GTypeInfo new_info = {
			sizeof(JamHostClass),
			NULL,
			NULL,
			(GClassInitFunc) jam_host_lj_class_init,
			NULL,
			NULL,
			sizeof(JamHostLJ),
			0,
			NULL
		};
		new_type = g_type_register_static(JAM_TYPE_HOST,
				"JamHostLJ", &new_info, 0);
	}
	return new_type;
}

JamHostLJ*
jam_host_lj_new(LJServer *s) {
	JamHostLJ *h = JAM_HOST_LJ(g_object_new(jam_host_lj_get_type(), NULL));
	h->server = s;
	return h;
}


