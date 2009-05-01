
#ifndef __LIVEJOURNAL_GETTAGS_H__
#define __LIVEJOURNAL_GETTAGS_H__

#include <livejournal/verb.h>

typedef struct {
	LJVerb verb;
	GHashTable *tags;
} LJGetTags;

LJGetTags* lj_gettags_new(LJUser *user, const gchar *journal);
void       lj_gettags_free(LJGetTags *gettags, gboolean includetags);

#endif /* __LIVEJOURNAL_GETTAGS_H__ */

