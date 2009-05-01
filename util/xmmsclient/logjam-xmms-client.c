/* logjam-xmms-client: xmms helper.
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 * 
 * part of:
 * logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 */

#include "config.h"

/* we don't use glib because xmms is glib1.2 and logjam is glib2.0.
 * mixing the two is ok but weird, and we're fine with just the libc. */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <xmmsctrl.h>

/* XXX translate messages. */
#define _(x) x

int
main(int argc, char *argv[]) {
	int session = 0;
	int pos;
	char *text;

	if (argc > 1) {
		if (isdigit(argv[1][0])) {
			session = atoi(argv[1]);
		} else {
			/* translators: this is the logjam-xmms-client command line. */
			fprintf(stderr, _("usage: %s [session_number]\n"), argv[0]);
			return -1;
		}
	}

	pos = xmms_remote_get_playlist_pos(session);
	text = xmms_remote_get_playlist_title(session, pos);

	if (!text) {
		fprintf(stderr, _("XMMS error.  Is XMMS running?\n"));
		return -1;
	}
	printf("%s\n", text);
	return 0;
}

