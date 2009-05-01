/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <config.h>
#include <glib.h>
#include <livejournal/types.h>
#include <livejournal/serveruser.h>

LJUser*
lj_user_new(LJServer *server) {
	LJUser *u = g_new0(LJUser, 1);
	u->server = server;
	return u;
}

gint
lj_user_compare(gconstpointer a, gconstpointer b) {
	return g_ascii_strcasecmp(((LJUser*)a)->username, ((LJUser*)b)->username);
}

LJServer*
lj_server_new(const char *url) {
	LJServer *s = g_new0(LJServer, 1);
	s->protocolversion = 1; /* default to protocol version 1. */
	if (url)
		s->url = g_strdup(url);
	return s;
}

int 
lj_server_get_last_cached_moodid(LJServer *server) {
	GSList *l;
	int max = 0;
	for (l = server->moods; l != NULL; l = l->next) {
		if (((LJMood*)l->data)->id > max) 
			max = ((LJMood*)l->data)->id;
	}
	return max;
}

static void
webmenuitem_free(LJWebMenuItem *wmi) {
	g_free(wmi->text);
	g_free(wmi->url);
	g_slist_foreach(wmi->subitems, (GFunc)webmenuitem_free, NULL);
	g_slist_free(wmi->subitems);
	g_free(wmi);
}

void
lj_webmenu_free(GSList *l) {
	if (!l) return;
	g_slist_foreach(l, (GFunc)webmenuitem_free, NULL);
	g_slist_free(l);
}

