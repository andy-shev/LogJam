/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_EDITPOSTEVENT_H__
#define __LIVEJOURNAL_EDITPOSTEVENT_H__

#include <livejournal/verb.h>
#include <livejournal/entry.h>

typedef struct _LJEditPostEvent {
	LJVerb verb;
} LJEditPostEvent;

LJEditPostEvent* lj_editpostevent_new(LJUser *user, const char *usejournal, gboolean edit, LJEntry *entry);
void             lj_editpostevent_free(LJEditPostEvent *editpostevent);

#endif /* __LIVEJOURNAL_EDITPOSTEVENT_H__ */

