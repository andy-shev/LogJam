/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <config.h>
#include <glib.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>  /* atoi. */
#include <string.h>  /* strchr. */

#include "entry.h"
#include "friends.h"
#include "tags.h"
#include "types.h"

char*
_lj_nid_by_id(GSList *l, int id) {
	for ( ; l; l = l->next) {
		if (((_LJNameIDHash*)l->data)->id == id) 
			return ((_LJNameIDHash*)l->data)->name;
	}
	return NULL;
}
int
_lj_nid_by_name(GSList *l, const char* name) {
	for ( ; l; l = l->next) {
		if (strcmp(((_LJNameIDHash*)l->data)->name, name) == 0) 
			return ((_LJNameIDHash*)l->data)->id;
	}
	return -1;
}
gint
_lj_nid_compare_alpha(_LJNameIDHash *a, _LJNameIDHash *b) {
	return g_ascii_strcasecmp(a->name, b->name);
}

void
_lj_nid_free(_LJNameIDHash *nid) {
	g_free(nid->name);
	g_free(nid);
}

LJFriend*
lj_friend_new(void) {
	LJFriend *f = g_new0(LJFriend, 1);
	f->foreground = 0x000000;
	f->background = 0xFFFFFF;
	return f;
}

LJFriendType
lj_friend_type_from_str(char *str) {
	if (str) {
		if (strcmp(str, "community") == 0) 
			return LJ_FRIEND_TYPE_COMMUNITY;
	} 
	return LJ_FRIEND_TYPE_USER;
}

void
lj_friend_free(LJFriend *f) {
	g_free(f->username);
	g_free(f->fullname);
	g_free(f);
}

gint
lj_friend_compare_username(gconstpointer a, gconstpointer b) {
	const LJFriend *fa = a;
	const LJFriend *fb = b;

	return strcmp(fa->username, fb->username);
}

LJTag*
lj_tag_new(void) {
    LJTag *t = g_new0(LJTag, 1);
    return t;
}

void
lj_tag_free(LJTag *t) {
   g_free(t->tag);
   g_free(t);
}

void 
lj_security_append_to_request(LJSecurity *security, LJRequest *request) {
	char *text = NULL;

	switch (security->type) {
		case LJ_SECURITY_PUBLIC:
			text = "public"; break;
		case LJ_SECURITY_PRIVATE:
			text = "private"; break;
		case LJ_SECURITY_FRIENDS:
		case LJ_SECURITY_CUSTOM:
			text = "usemask"; break;
	}
	lj_request_add(request, "security", text);

	if (security->type == LJ_SECURITY_FRIENDS) {
		lj_request_add_int(request, "allowmask", LJ_SECURITY_ALLOWMASK_FRIENDS);
	} else if (security->type == LJ_SECURITY_CUSTOM) {
		lj_request_add_int(request, "allowmask", security->allowmask);
	}
}

void
lj_security_from_strings(LJSecurity *security, const char *sectext, const char *allowmask) {
	if (!sectext) {
		security->type = LJ_SECURITY_PUBLIC;
		return;
	}

	if (g_ascii_strcasecmp(sectext, "public") == 0) {
		security->type = LJ_SECURITY_PUBLIC;
	} else if (g_ascii_strcasecmp(sectext, "private") == 0) {
		security->type = LJ_SECURITY_PRIVATE;
	} else if (g_ascii_strcasecmp(sectext, "friends") == 0) {
		security->type = LJ_SECURITY_FRIENDS;
	} else if (g_ascii_strcasecmp(sectext, "usemask") == 0 ||
			   g_ascii_strcasecmp(sectext, "custom") == 0) {
		unsigned int am = 0;
		if (allowmask)
			am = atoi(allowmask);

		switch (am) {
			case 0:
				security->type = LJ_SECURITY_PRIVATE; break;
			case LJ_SECURITY_ALLOWMASK_FRIENDS: 
				security->type = LJ_SECURITY_FRIENDS; break;
			default:
				security->type = LJ_SECURITY_CUSTOM;
				security->allowmask = am;
		}
	} else {
		g_warning("security: '%s' unhandled", sectext);
	}
}

#if 0
void 
security_load_from_result(LJSecurity *security, NetResult *result) {
	char *sectext, *allowmask;

	sectext = net_result_get(result, "security");
	if (sectext) {
		allowmask = net_result_get(result, "allowmask");
		security_load_from_strings(security, sectext, allowmask);
	}
}
#endif

void
lj_security_to_strings(LJSecurity *security, char **sectext, char **allowmask) {
	char *type = NULL;
	switch (security->type) {
		case LJ_SECURITY_PUBLIC: return;
		case LJ_SECURITY_PRIVATE: type = "private"; break;
		case LJ_SECURITY_FRIENDS: type = "friends"; break;
		case LJ_SECURITY_CUSTOM: type = "custom"; break;
	}
	if (sectext)
		*sectext = g_strdup(type);

	if (allowmask && security->type == LJ_SECURITY_CUSTOM)
		*allowmask = g_strdup_printf("%ud", security->allowmask);
}

time_t
lj_timegm(struct tm *tm) {
#ifdef HAVE_TIMEGM
	return timegm(tm);
#elif defined(WIN32_FAKE_TIMEGM)
	/* this is only for non-cygwin builds.
	 * windows getenv/putenv works in a wacky way. */
	time_t ret;
	char *tz;
	
	tz = getenv("TZ");
	putenv("TZ=UTC");
	tzset();
	ret = mktime(tm);
	if (tz) {
		char *putstr = g_strdup_printf("TZ=%s", tz);
		putenv(putstr);
		g_free(putstr);
	} else {
		putenv("TZ=");
	}
	tzset();
	return ret;
#else
	/* on systems that lack timegm, we can fake it with environment trickery.
	 * this is taken from the timegm(3) manpage. */
	time_t ret;
	char *tz;

	tz = getenv("TZ");
	setenv("TZ", "", 1);
	tzset();
	ret = mktime(tm);
	if (tz)
		setenv("TZ", tz, 1);
	else
		unsetenv("TZ");
	tzset();
	return ret;
#endif
}

gboolean
lj_ljdate_to_tm(const char *ljtime, struct tm *ptm) {
	gboolean ret = TRUE;
	if (sscanf(ljtime, "%4d-%2d-%2d %2d:%2d:%2d",
		&ptm->tm_year, &ptm->tm_mon, &ptm->tm_mday,
		&ptm->tm_hour, &ptm->tm_min, &ptm->tm_sec) < 5)
		ret = FALSE;
	ptm->tm_year -= 1900;
	ptm->tm_mon -= 1;
	ptm->tm_isdst = -1; /* -1: "the information is not available" */
	return ret;
}

char*
lj_tm_to_ljdate(struct tm *ptm) {
	return g_strdup_printf("%04d-%02d-%02d %02d:%02d:%02d",
		ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday,
		ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
}

char*
lj_tm_to_ljdate_noseconds(struct tm *ptm) {
	return g_strdup_printf("%04d-%02d-%02d %02d:%02d",
		ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday,
		ptm->tm_hour, ptm->tm_min);
}

guint32
lj_color_to_int(const char *color) {
	if (color[0] != '#')
		return 0;
	return strtol(color+1, NULL, 16);
}

void
lj_int_to_color(guint32 l, char *color) {
	sprintf(color, "#%06X", l);
}

