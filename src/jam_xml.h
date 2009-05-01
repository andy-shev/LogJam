/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __xml_h__
#define __xml_h__

#include <glib.h>

void jam_xmlNewDoc(xmlDocPtr *doc, xmlNodePtr *node, char *name);

char*    jam_xmlGetString(xmlDocPtr doc, xmlNodePtr node);
guint32  jam_xmlGetUInt32(xmlDocPtr doc, xmlNodePtr node);
gboolean jam_xmlGetInt   (xmlDocPtr doc, xmlNodePtr node, int *value);

xmlNodePtr jam_xmlAddInt(xmlNodePtr node, char *name, int val);

void     jam_xmlSetIntProp(xmlNodePtr node, const char *name, int value);
gboolean jam_xmlGetIntProp(xmlNodePtr node, const char *name, int *value);

/* from conf_xml.c
 * 
 * basically, these XML_* functions run a series of ifs on the node name.
 * be sure to end with XML_GET_END, which prints error information if a node
 * wasn't handled.
 */
#define XML_GET_IF(key, what) \
if (xmlStrcmp(node->name, BAD_CAST key) == 0) {\
	what \
} else
#define XML_GET_FUNC(key, dest, func) XML_GET_IF(key, dest = func(doc, node);)
#define XML_GET_STR(key, dest) XML_GET_FUNC(key, dest, jam_xmlGetString)
#define XML_GET_UINT32(key, dest) XML_GET_FUNC(key, dest, jam_xmlGetUInt32)
#define XML_GET_INT(key, dest) XML_GET_IF(key, jam_xmlGetInt(doc, node, &dest);)
#define XML_GET_BOOL(key, dest) XML_GET_IF(key, dest = TRUE; )
#define XML_GET_LIST(key, dest, func) XML_GET_IF(key, dest = g_slist_append(dest, func(doc, node)); )
#define XML_GET_SUB(dest, func) if (xmlStrcmp(node->name, BAD_CAST "text") != 0 && func(dest, doc, node)) { ; } else
/*#define XML_GET_END(func) \
{ fprintf(stderr, "%s: Unknown node %s.\n", func, node->name); }*/
#define XML_GET_END(func) \
if (xmlStrcmp(node->name, BAD_CAST "text") != 0) { /*fprintf(stderr, "%s: Unknown node %s.\n", func, node->name)*/; }

#endif /* __xml_h__ */
