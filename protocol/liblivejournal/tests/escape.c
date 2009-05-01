/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2005 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <glib.h>
#include <livejournal.h>
#include <assert.h>

gboolean
roundtrip(const char *orig) {
	char *encoded, *decoded;
	encoded = lj_urlencode(orig);
	decoded = lj_urldecode(encoded);

	g_print("Roundtrip: '%s' -> '%s' -> '%s'.\n",
	           orig, encoded, decoded);
	if (strcmp(orig, decoded) == 0)
		return TRUE;
	return FALSE;
}

int main(int argc, char *argv[]) {
	assert(roundtrip("foo bar"));
	assert(roundtrip("foo+bar"));
	assert(roundtrip("foo \377 \370 etc."));
	assert(roundtrip("foo\x00FF etc."));
	g_print("PASS\n");
	return 0;
}
