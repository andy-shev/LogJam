/* liblivejournal - a client library for LiveJournal.
 * Copyright (C) 2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LIVEJOURNAL_TAGS_H__
#define __LIVEJOURNAL_TAGS_H__

typedef struct _LJTag {
	char *tag;
	guint32 count;
} LJTag;

LJTag *lj_tag_new(void);
void   lj_tag_free(LJTag *t);

#endif /* __LIVEJOURNAL_TAGS_H__ */
