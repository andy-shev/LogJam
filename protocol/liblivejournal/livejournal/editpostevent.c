/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <config.h>

#include <glib.h>
#include <stdlib.h> /* atoi */

#include "editpostevent.h"

static void
parse_result(LJVerb *verb) {
	/* XXX get itemid and anum, if we need it. */
}

LJEditPostEvent*
lj_editpostevent_new(LJUser *user, const char *usejournal, gboolean edit, LJEntry *entry) {
	LJEditPostEvent *editpostevent = g_new0(LJEditPostEvent, 1);
	LJVerb *verb = (LJVerb*)editpostevent;

	lj_verb_init(verb, user, edit ? "editevent" : "postevent", FALSE, parse_result);
	if (usejournal)
		lj_request_add(verb->request, "usejournal", usejournal);
	lj_entry_set_request_fields(entry, verb->request);

	return editpostevent;
}

void
lj_editpostevent_free(LJEditPostEvent *editpostevent) {
	/*if (editpostevent->friends) {
		if (includefriends)
			g_hash_table_foreach(editpostevent->friends,
					(GHFunc)hash_friend_free_cb, NULL);
		g_hash_table_destroy(editpostevent->friends);
	}*/
	g_free(editpostevent);
}

