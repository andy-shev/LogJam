/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Gaal Yahas <gaal@forum2.org>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef _jam_account_
#define _jam_account_

#include <glib.h>
#include <glib-object.h>
#include <livejournal/livejournal.h>
#include <libxml/tree.h>

#include "network.h"  /* need netcontext struct. */

#define JAM_TYPE_ACCOUNT (jam_account_get_type())
#define JAM_ACCOUNT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), JAM_TYPE_ACCOUNT, JamAccount))
#define JAM_ACCOUNT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), JAM_TYPE_ACCOUNT, JamAccountClass))

typedef struct _JamAccountClass JamAccountClass;
typedef struct _JamAccount JamAccount;

#define JAM_TYPE_HOST (jam_host_get_type())
#define JAM_HOST(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), JAM_TYPE_HOST, JamHost))
#define JAM_HOST_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), JAM_TYPE_HOST, JamHostClass))

typedef struct _JamHostClass JamHostClass;
typedef struct _JamHost JamHost;

struct _JamAccount {
	GObject obj;

	JamHost *host;

	gboolean remember_user;
	gboolean remember_password;
};

struct _JamAccountClass {
	GObjectClass parent_class;

	void         (*set_username)(JamAccount *acc, const char *username);
	const gchar* (*get_username)(JamAccount *acc);
	void         (*set_password)(JamAccount *acc, const char *password);
	const gchar* (*get_password)(JamAccount *acc);
};

typedef struct _JamAccountLJ JamAccountLJ;

#define JAM_TYPE_ACCOUNT_LJ (jam_account_lj_get_type())
#define JAM_ACCOUNT_LJ(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), JAM_TYPE_ACCOUNT_LJ, JamAccountLJ))
#define JAM_ACCOUNT_IS_LJ(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), JAM_TYPE_ACCOUNT_LJ))

struct _JamAccountLJ {
	JamAccount account;
	LJUser *user;
	guint32 cfmask;
	time_t lastupdate;
};

#ifdef blogger_punted_for_this_release
typedef struct _JamAccountBlogger JamAccountBlogger;

#define JAM_TYPE_ACCOUNT_BLOGGER (jam_account_blogger_get_type())
#define JAM_ACCOUNT_BLOGGER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), JAM_TYPE_ACCOUNT_BLOGGER, JamAccountBlogger))
#define JAM_ACCOUNT_IS_BLOGGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), JAM_TYPE_ACCOUNT_BLOGGER))

struct _JamAccountBlogger {
	JamAccount account;
	char *username, *password;
};
#endif /* blogger_punted_for_this_release */

typedef struct _JamHostLJ JamHostLJ;
#define JAM_TYPE_HOST_LJ (jam_host_lj_get_type())
#define JAM_HOST_LJ(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), JAM_TYPE_HOST_LJ, JamHostLJ))
#define JAM_HOST_IS_LJ(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), JAM_TYPE_HOST_LJ))

#ifdef blogger_punted_for_this_release
typedef struct _JamHostBlogger JamHostBlogger;
#define JAM_TYPE_HOST_BLOGGER (jam_host_blogger_get_type())
#define JAM_HOST_BLOGGER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), JAM_TYPE_HOST_BLOGGER, JamHostBlogger))
#define JAM_HOST_IS_BLOGGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), JAM_TYPE_HOST_BLOGGER))
#endif /* blogger_punted_for_this_release */

struct _JamHost {
	GObject obj;

	char *name;
	GSList *accounts;
	JamAccount *lastaccount;
};

struct _JamHostClass {
	GObjectClass parent_class;

	const char* (*get_stock_icon)(void);
	void        (*save_xml)(JamHost *host, xmlNodePtr servernode);
	gboolean    (*load_xml)(JamHost *host, xmlDocPtr doc, xmlNodePtr servernode);
	JamAccount* (*make_account)(JamHost *host, const char *username);

	gboolean    (*do_post)(JamHost *host,   NetContext *ctx, void *doc, GError **err);
	gboolean    (*do_edit)(JamHost *host,   NetContext *ctx, void *doc, GError **err);
	gboolean    (*do_delete)(JamHost *host, NetContext *ctx, void *doc, GError **err);
};

void        jam_account_logjam_init(void);
GType       jam_account_get_type(void);
JamAccount* jam_account_from_xml(xmlDocPtr doc, xmlNodePtr node, JamHost *host);
JamAccount* jam_account_make(LJUser *u);
JamAccount* jam_account_new_from_names(const gchar *username, const gchar *servername);
gchar*      jam_account_id_strdup_from_names(const gchar *username, const gchar *servername);
gchar*      jam_account_id_strdup(JamAccount *acc);
JamAccount* jam_account_lookup(gchar *id);
JamAccount* jam_account_lookup_by_user(LJUser *u);

void jam_account_connect(JamAccount *acc, gboolean connect);
void jam_account_connect_all(gboolean connect);

const gchar* jam_account_get_username  (JamAccount *acc);
const gchar* jam_account_get_password  (JamAccount *acc);
JamHost*     jam_account_get_host      (JamAccount *acc);

void         jam_account_set_username(JamAccount *acc, const char *username);
void         jam_account_set_password(JamAccount *acc, const char *password);
void         jam_account_set_remember  (JamAccount *acc, gboolean u, gboolean p);
void         jam_account_get_remember  (JamAccount *acc, gboolean *u, gboolean *p);
gboolean     jam_account_get_remember_password(JamAccount *acc);

gboolean     jam_account_write(JamAccount *account, GError **err);

GType        jam_account_lj_get_type(void);
JamAccount*  jam_account_lj_new(LJServer *server, const char *username);
JamAccount*  jam_account_lj_from_xml(xmlDocPtr doc, xmlNodePtr node, JamHostLJ *host);
void         jam_account_lj_write(JamAccountLJ *account, xmlNodePtr node);

LJUser*      jam_account_lj_get_user  (JamAccountLJ *acc);
LJServer*    jam_account_lj_get_server(JamAccountLJ *acc);
guint32      jam_account_lj_get_cfmask(JamAccountLJ *acc);
void         jam_account_lj_set_cfmask(JamAccountLJ *acc, guint32 mask);
gboolean     jam_account_lj_get_checkfriends(JamAccount *acc);

#ifdef blogger_punted_for_this_release
GType        jam_account_blogger_get_type(void);
JamAccount*  jam_account_blogger_new(const char *username);
JamAccount*  jam_account_blogger_from_xml(xmlDocPtr doc, xmlNodePtr node, JamHostBlogger *host);
#endif /* blogger_punted_for_this_release */


typedef enum {
	JAM_HOST_SUBMITTING_ENTRY,
	JAM_HOST_SAVING_CHANGES,
	JAM_HOST_DELETING_ENTRY,
} JamHostActionTitle;

GType       jam_host_get_type(void);
const char* jam_host_get_stock_icon(JamHost *host);

JamAccount* jam_host_get_account_by_username(JamHost *host, const char *username, gboolean create);
void        jam_host_add_account(JamHost *host, JamAccount *acc);

JamHost*    jam_host_from_xml(xmlDocPtr doc, xmlNodePtr node, void *data);
gboolean    jam_host_write(JamHost *host, GError **err);

gboolean    jam_host_do_post(JamHost *host, NetContext *ctx, void *doc, GError **err);
gboolean    jam_host_do_edit(JamHost *host, NetContext *ctx, void *doc, GError **err);
gboolean    jam_host_do_delete(JamHost *host, NetContext *ctx, void *doc, GError **err);


LJServer*   jam_host_lj_get_server(JamHostLJ *host);

JamHostLJ*  jam_host_lj_new(LJServer *s);
GType       jam_host_lj_get_type(void);


#ifdef blogger_punted_for_this_release
char*       jam_host_blogger_get_rpcurl(JamHostBlogger *host);
void        jam_host_blogger_set_rpcurl(JamHostBlogger *host, const char* url);

JamHostBlogger* jam_host_blogger_new(void);
GType           jam_host_blogger_get_type(void);
#endif /* blogger_punted_for_this_release */

#endif /* _jam_account_ */
