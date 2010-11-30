/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "glib-all.h"
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#ifdef G_OS_WIN32
#include <direct.h> /* mkdir */
#endif

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include "util.h"

void
string_replace(char **dest, char *src) {
	if (*dest) g_free(*dest);
	*dest = src;
}

gboolean
verify_dir(const char *path, GError **err) {
#ifdef G_OS_WIN32
	if (mkdir(path) < 0 && errno != EEXIST) {
#else
	/* mode 0700 so other people can't peek at passwords! */
	if (mkdir(path, 0700) < 0 && errno != EEXIST) {
#endif
		g_set_error(err, 0, 0, /* FIXME domain */
				_("Failed to create directory '%s': %s"),
				path, g_strerror(errno));
		return FALSE;
	}
	return TRUE;
}

gboolean
verify_path(char *path, int include_last, GError **err) {
	int i, len, reallen;

	len = reallen = (int)strlen(path);
	if (!include_last) {
		for (i = len-1; i > 0; i--)
			if (path[i] == G_DIR_SEPARATOR)
				break;
		if (i > 0) {
			len = i;
			path[len] = 0;
		}
	}
	/* the common case is that the path already exists. */
	if (!verify_dir(path, NULL)) {
		/* otherwise, start creating parent directories until we succeed. */
		for (i = len-1; i > 0; i--) {
			if (path[i] == G_DIR_SEPARATOR) {
				path[i] = 0;
				if (verify_dir(path, NULL)) {
					path[i] = G_DIR_SEPARATOR;
					break;
				}
				path[i] = G_DIR_SEPARATOR;
			}
		}
		/* once a parent dir succeeded, create the subdirectories we needed. */
		i++;
		for ( ; i < len; i++) {
			if (path[i] == G_DIR_SEPARATOR) {
				path[i] = 0;
				if (!verify_dir(path, err))
					return FALSE;
				path[i] = G_DIR_SEPARATOR;
			}
		}
		if (!verify_dir(path, err))
			return FALSE;
	}
	if (!include_last)
		path[len] = G_DIR_SEPARATOR;
	return TRUE;
}

void
xml_escape(char **text) {
	char *esc;
	if (!text || !*text)
		return;
	esc = g_markup_escape_text(*text, -1);
	g_free(*text);
	*text = esc;
}

