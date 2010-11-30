/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <config.h>

#include <glib.h>
#include <stdlib.h> /* atoi */

#include "protocol.h"
#include "serveruser.h"
#include "friends.h"
#include "login.h"

static void
read_moods(LJResult *result, LJServer *s) {
	LJMood *m;
	char *id, *name;
	int moodcount;
	int i;

	/* grab moods */
	moodcount = lj_result_get_int(result, "mood_count");

	for (i = 1; i < moodcount+1; i++) { /* 1-based list */
		id   = lj_result_getf(result, "mood_%d_id", i);
		if (!id) continue;
		name = lj_result_getf(result, "mood_%d_name", i);

		/* work around LiveJournal bug: sometimes returns unnamed moods. */
		if (!name || !name[0]) continue;

		m = lj_mood_new();
		m->id   = atoi(id);
		m->name = g_strdup(name);
		m->parentid = lj_result_getf_int(result, "mood_%d_parent", i);

		s->moods = g_slist_insert_sorted(s->moods, m,
				(GCompareFunc)lj_mood_compare);
	}
}

static void
read_pickws(LJResult *result, LJUser *u) {
	int pickwcount;
	int i;

	/* first, free all existing pickws. */
	if (u->pickws) {
		g_slist_foreach(u->pickws, (GFunc)g_free, NULL);
		g_slist_free(u->pickws);
		u->pickws = NULL;
	}

	/* grab picture keywords */
	pickwcount = lj_result_get_int(result, "pickw_count");

	for (i = 1; i < pickwcount+1; i++) { /* 1-based list */
		char *kw = lj_result_getf(result, "pickw_%d", i);
		if (!kw) continue;

		if (lj_server_unicode(u->server))
			kw = g_utf8_normalize(kw, -1, G_NORMALIZE_DEFAULT);
		else
			kw = g_strdup(kw);

		u->pickws = g_slist_insert_sorted(u->pickws, kw,
				(GCompareFunc)g_utf8_collate);
	}
}

static void
read_friendgroups(LJResult *result, LJUser *u) {
	int fgmax;
	int i;
	char *fgname;
	LJFriendGroup *fg;
	char *pub;

	/* first, free all existing friendgroups. */
	if (u->friendgroups) {
		g_slist_foreach(u->friendgroups, (GFunc)lj_friend_group_free, NULL);
		g_slist_free(u->friendgroups);
		u->friendgroups = NULL;
	}

	/* then read the new ones. */
	fgmax = lj_result_get_int(result, "frgrp_maxnum");

	for (i = 1; i <= fgmax; i++) {
		fgname = lj_result_getf(result, "frgrp_%d_name", i);
		if (!fgname) continue;

		fg = lj_friendgroup_new();
		fg->id = i;
		fg->name = g_strdup(fgname);
		pub = lj_result_getf(result, "frgrp_%d_public", i);
		fg->ispublic = pub && !(g_ascii_strcasecmp(pub, "1"));

		u->friendgroups = g_slist_append(u->friendgroups, fg);
	}
}

static void
read_usejournals(LJResult *result, LJUser *u) {
	int i, accesscount;
	char *journal;

	/* first, free all existing usejournals. */
	if (u->usejournals) {
		g_slist_foreach(u->usejournals, (GFunc)g_free, NULL);
		g_slist_free(u->usejournals);
		u->usejournals = NULL;
	}

	/* then read the new ones. */
	accesscount = lj_result_get_int(result, "access_count");

	for (i = 1; i < accesscount+1; i++) {
		journal = lj_result_getf(result, "access_%d", i);
		if (!journal) continue;

		u->usejournals = g_slist_append(u->usejournals, g_strdup(journal));
	}
}

static LJWebMenuItem* webmenuitem_build(LJResult *result, int base, int i);

static GSList*
webmenu_build_submenu(LJResult *result, int base) {
	char *str;
	int i, count;
	LJWebMenuItem *wmi;
	GSList *list = NULL;

	str = lj_result_getf(result, "menu_%d_count", base);
	if (str == NULL) {
		g_warning("menu with no elements?");
		return NULL;
	}
	count = atoi(str);

	for (i = 1; i <= count; i++) {
		wmi = webmenuitem_build(result, base, i);
		if (wmi)
			list = g_slist_append(list, wmi);
	}
	return list;
}

static LJWebMenuItem*
webmenuitem_build(LJResult *result, int base, int i) {
	LJWebMenuItem *wmi;
	char *text, *url, *sub;

	text = lj_result_getf(result, "menu_%d_%d_text", base, i);
	if (text == NULL) {
		g_warning("menu item has no text?");
		return NULL;
	}
	url = lj_result_getf(result, "menu_%d_%d_url", base, i);
	sub = lj_result_getf(result, "menu_%d_%d_sub", base, i);

	wmi = g_new0(LJWebMenuItem, 1);

	if (text[0] == '-' && text[1] == 0) { /* separator */
		; /* empty item is separator. */
	} else if (url != NULL) { /* url menu item */
		wmi->text = g_strdup(text);
		wmi->url = g_strdup(url);
	} else if (sub != NULL) { /* submenu */
		int subbase;
		subbase = atoi(sub);
		wmi->text = g_strdup(text);
		wmi->subitems = webmenu_build_submenu(result, subbase);
	} else {
		g_warning("unknown menu item type...");
		g_free(wmi);
		return NULL;
	}
	return wmi;
}

static void
read_webmenu(LJResult *result, LJUser *u) {
	lj_webmenu_free(u->webmenu);
	u->webmenu = webmenu_build_submenu(result, 0);
}

static void
read_message(LJLogin *login, LJResult *result) {
	char *value;
	if ((value = lj_result_get(result, "message")) != NULL)
		login->message = g_strdup(value);
}

static void
read_user(LJResult *result, LJUser *u) {
	char *value;
	read_pickws(result, u);
	read_friendgroups(result, u);
	read_usejournals(result, u);
	read_webmenu(result, u);
	if ((value = lj_result_get(result, "name"))) {
		g_free(u->fullname);
		u->fullname = g_strdup(value);
	}
}

static void
parse_result(LJVerb *verb) {
	LJLogin *login = (LJLogin*)verb;
	LJUser *u = lj_request_get_user(verb->request);
	LJServer *s = u->server;
	LJResult *result = verb->result;
	read_moods(result, s);
	read_user(result, u);
	read_message(login, result);
}

LJLogin*
lj_login_new(LJUser *user, const char *clientversion) {
	LJLogin *login = g_new0(LJLogin, 1);
	LJVerb *verb = (LJVerb*)login;
	LJRequest *request;

	lj_verb_init(verb, user, "login", FALSE, parse_result);
	request = verb->request;

	lj_request_add(request, "clientversion", clientversion);
	lj_request_add_int(request, "getmoods", lj_server_get_last_cached_moodid(user->server));
	lj_request_add_int(request, "getmenus", 1);
	lj_request_add_int(request, "getpickws", 1);

	return login;
}

void
lj_login_free(LJLogin *login) {
	lj_verb_free_contents((LJVerb*)login);
	g_free(login->message);
	g_free(login);
}

