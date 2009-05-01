/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2005 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __logjam_util_h__
#define __logjam_util_h__

void string_replace(char **dest, char *src);

gboolean verify_dir(const char *path, GError **err);
gboolean verify_path(char *path, gboolean include_last, GError **err);

void xml_escape(char **text);

#endif /* __logjam_util_h__ */
