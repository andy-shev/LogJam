/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <config.h>

#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#include "protocol.h"
#include "md5.h"

struct _LJRequest {
	LJUser *user;
	GHashTable *hash;
};

struct _LJResult {
	/* in a result, only one string is allocated,
	 * and the rest of the hash table is pointers within that string.
	 * we need to keep a pointer to that string around for when we free the
	 * result. */
	char *string;
	GHashTable *hash;
};

static inline gboolean
needsescape(char c) {
	return !isalnum(c) && c != '_';
}

char*
lj_urlencode(const char *string) {
	int escapecount = 0;
	const char *src;
	char *dest;
	char *newstr;
	
	char hextable[] = { '0', '1', '2', '3', '4', '5', '6', '7',
	                    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'  };
	
	if (string == NULL) return NULL;

	for (src = string; *src != 0; src++) 
		if (needsescape(*src)) escapecount++;
	
	newstr = g_new(char, (src-string) - escapecount + (escapecount * 3) + 1);
	
	src = string;
	dest = newstr;
	while (*src != 0) {
		if (needsescape(*src)) {
			unsigned char c = *src;
			*dest++ = '%';
			*dest++ = hextable[c >> 4];
			*dest++ = hextable[c & 0x0F];
			src++;
		} else {
			*dest++ = *src++;
		}
	}
	*dest = 0;

	return newstr;
}

char* 
lj_urldecode(const char *string) {
	int destlen = 0;
	const char *src;
	char *dest;
	char *newstr;
	
	if (string == NULL) return NULL;

	for (src = string; *src != 0; src++) {
		if (*src == '%') { src+=2; } /* FIXME: this isn't robust. should check
										the next two chars for 0 */
		destlen++;
	}
	
	newstr = g_new(char, destlen + 1);
	src = string;
	dest = newstr;
	while (*src != 0) {
		if (*src == '%') {
			char h = (char)toupper(src[1]);
			char l = (char)toupper(src[2]);
			char vh, vl;
			vh = isalpha(h) ? (10+(h-'A')) : (h-'0');
			vl = isalpha(l) ? (10+(l-'A')) : (l-'0');
			*dest++ = ((vh<<4)+vl);
			src += 3;
		} else if (*src == '+') {
			*dest++ = ' ';
			src++;
		} else {
			*dest++ = *src++;
		}
	}
	*dest = 0;

	return newstr;
}

/* retrieve a pointer to the current line, modifying src in place. */
static char *
get_line_inplace(char *src, char **dest) {
	*dest = src;
	while (*src != 0 && *src != '\n')
		src++;

	if (*src == '\n') {
		*src = 0;
		src++;
	}

	return src;
}

static LJResult* 
lj_result_new(void) {
	return g_new0(LJResult, 1);
}

LJResult* 
lj_result_new_from_response(const char *res) {
	char *origsrc, *src;
	char *key;
	char *val;
	LJResult *result;
	GHashTable *hash;
	int read_keys = 0;

	/* shortcut the common error case. */
	if (res == NULL || *res == 0)
		return NULL;

	/* make our own copy so we can modify it. */
	origsrc = src = g_strdup(res);
	hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

	while (*src != 0) {
		src = get_line_inplace(src, &key);
		if (*src) {
			src = get_line_inplace(src, &val);
			g_hash_table_insert(hash, key, val);
			read_keys++;
		}
	}
	if (read_keys == 0) { /* error. */
		g_free(origsrc);
		g_hash_table_destroy(hash);
		return NULL;
	}

	result = lj_result_new();
	result->string = origsrc;
	result->hash = hash;
	return result;
}

gboolean 
lj_result_succeeded(LJResult *result) {
	char *error;

	if (result == NULL) return FALSE;

	error = lj_result_get(result, "success");
	if (error == NULL) return FALSE;

	return (g_ascii_strcasecmp(error, "OK") == 0);
}

void
lj_request_add(LJRequest *request, const char *key, const char *val) {
	g_hash_table_insert(request->hash, g_strdup(key), g_strdup(val));
}
void
lj_request_add_int(LJRequest *request, const char *key, int val) {
	g_hash_table_insert(request->hash, g_strdup(key), g_strdup_printf("%d", val));
}

LJRequest*
lj_request_new_without_auth(LJUser *user, const char *mode) {
	LJRequest *request = g_new0(LJRequest, 1);

	request->user = user;
	request->hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	lj_request_add(request, "mode", mode);

	lj_request_add_int(request, "ver", user->server->protocolversion);

	return request;
}

LJRequest*
lj_request_new(LJUser *user, const char *mode) {
	LJRequest *request = lj_request_new_without_auth(user, mode);
	char buf[33];
	lj_request_add(request, "user", user->username);
	lj_md5_hash(user->password, buf);
	lj_request_add(request, "hpassword", buf);
	return request;
}

void
lj_request_use_challenge(LJRequest *request, LJChallenge challenge) {
	char passhash[33];
	char *chalpass;
	char fullhash[33];

	/* make sure we're not passing a password. */
	g_hash_table_remove(request->hash, "password");
	g_hash_table_remove(request->hash, "hpassword");

	lj_request_add(request, "auth_method", "challenge");
	lj_request_add(request, "auth_challenge", challenge);
	lj_md5_hash(request->user->password, passhash);
	chalpass = g_strconcat(challenge, passhash, NULL);
	lj_md5_hash(chalpass, fullhash);
	g_free(chalpass);
	lj_request_add(request, "auth_response", fullhash);
}

LJUser*
lj_request_get_user(LJRequest *request) {
	return request->user;
}

static void
hash_append_str_encoded(gpointer key, gpointer value, gpointer data) {
	GString *string = data;
	gchar *en_key, *en_value;

	if (key == NULL || value == NULL) return;
	en_key = lj_urlencode(key);
	en_value = lj_urlencode(value);

	if (string->len > 0)
		g_string_append_c(string, '&');
	g_string_append_printf(string, "%s=%s", en_key, en_value);

	g_free(en_key);
	g_free(en_value);
}

GString*
lj_request_to_string(LJRequest* request) {
	GString *str = g_string_sized_new(2048);
	g_hash_table_foreach(request->hash, hash_append_str_encoded, str);
	return str;
}

void
lj_request_free(LJRequest *request) {
	g_hash_table_destroy(request->hash);
	g_free(request);
}

#define SHOWCOUNT 6
char *showfirst[] = { "mode", "user", "password", "hpassword", "usejournal", "ver" };
static void
hash_dump(gpointer key, gpointer value, gpointer data) {
	int i;
	for (i = 0; i < SHOWCOUNT; i++)
		if (g_ascii_strcasecmp((char*)key, showfirst[i]) == 0)
			return;
	g_print("\t%s: %s\n", (char*)key, (char*)value);
}

void
lj_request_dump(LJRequest *request) {
	int i;
	char *value;
	for (i = 0; i < SHOWCOUNT; i++) {
		value = g_hash_table_lookup(request->hash, showfirst[i]);
		if (!value) continue;
		g_print("\t%s: %s\n", showfirst[i], value);
	}
	g_hash_table_foreach(request->hash, hash_dump, NULL);
}

char*
lj_result_get(LJResult *result, const char *key) {
	return g_hash_table_lookup(result->hash, key);
}

char*
lj_result_getf(LJResult* result, const char *key, ...) {
	char buf[100];
	va_list ap;

	va_start(ap, key);
	g_vsnprintf(buf, 100, key, ap);
	va_end(ap);
	return lj_result_get(result, buf); 
}

int
lj_result_get_int(LJResult *result, const char *key) {
	char *val;
	val = lj_result_get(result, key);
	if (val)
		return atoi(val);
	return 0;
}

int
lj_result_getf_int(LJResult* result, const char *key, ...) {
	char buf[100];
	va_list ap;

	va_start(ap, key);
	g_vsnprintf(buf, 100, key, ap);
	va_end(ap);
	return lj_result_get_int(result, buf); 
}

void
lj_result_free(LJResult *result) {
	if (!result) return;  /* NULL result is the error result. */
	g_free(result->string);
	g_hash_table_destroy(result->hash);
	g_free(result);
}

