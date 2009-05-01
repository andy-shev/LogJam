/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_TYPES_H__
#define __LIVEJOURNAL_TYPES_H__

#include <time.h>

/* associate numbers<->names in a two-way structure (using a GList). */
#define NID_CONTENT int id; char *name; /* yay for no inheritance in C. */
typedef struct _LJNameIDHash {
	NID_CONTENT
} _LJNameIDHash;
char* _lj_nid_by_id(GSList *l, int id);
int   _lj_nid_by_name(GSList *l, const char* name);
int   _lj_nid_compare_alpha(_LJNameIDHash *a, _LJNameIDHash *b);
void  _lj_nid_free(_LJNameIDHash *nid);

/* convert a tm, in utc, to a time_t.
 * gnu provides timegm, but it isn't portable,
 * and this function is tiny anyway. */
time_t lj_timegm(struct tm *tm);

gboolean lj_ljdate_to_tm(const char *ljtime, struct tm *ptm);
char* lj_tm_to_ljdate(struct tm *ptm);
char* lj_tm_to_ljdate_noseconds(struct tm *ptm);

guint32 lj_color_to_int(const char *color);
void    lj_int_to_color(guint32 l, char *color);

typedef char* LJChallenge;
#define lj_challenge_new(s) g_strdup(s)
#define lj_challenge_free(x) g_free(x)

#endif /* __LIVEJOURNAL_TYPES_H__ */
