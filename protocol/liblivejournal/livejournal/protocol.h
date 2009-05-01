/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <livejournal/serveruser.h>

typedef struct _LJRequest LJRequest;

LJRequest* lj_request_new(LJUser *user, const char *mode);
LJRequest* lj_request_new_without_auth(LJUser *user, const char *mode);
void       lj_request_free(LJRequest *request);

void       lj_request_add      (LJRequest *request, const char *key, const char *val);
void       lj_request_add_int  (LJRequest *request, const char *key, int val);

GString*   lj_request_to_string(LJRequest *request);
LJUser*    lj_request_get_user (LJRequest *request);
void       lj_request_dump     (LJRequest *request);
void       lj_request_use_challenge(LJRequest *request,
                                    LJChallenge challenge);

typedef struct _LJResult LJResult;

LJResult* lj_result_new_from_response(const char *res);
void      lj_result_free(LJResult *result);
gboolean  lj_result_succeeded(LJResult *result);
char* lj_result_get(LJResult *result, const char *key);
int   lj_result_get_int(LJResult *result, const char *key);
char* lj_result_getf(LJResult *result, const char *key, ...);
int   lj_result_getf_int(LJResult *result, const char *key, ...);

char* lj_urlencode(const char *string);
char* lj_urldecode(const char *string);

#endif /* PROTOCOL_H */
