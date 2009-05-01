/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef CONF_XML_H
#define CONF_XML_H

#include "conf.h"

int conf_read(Configuration *c, char *path);
int conf_write(Configuration *c, char *path);

typedef void* (*conf_parsedirxml_fn)(xmlDocPtr, xmlNodePtr, void*);
typedef void* (*conf_parsedirlist_fn)(const char*, void*);
void*   conf_parsedirxml(const char *dirname, conf_parsedirxml_fn fn, void *data);
GSList* conf_parsedirlist(const char *base, conf_parsedirlist_fn fn, void *data);

#endif /* CONF_XML_H */
