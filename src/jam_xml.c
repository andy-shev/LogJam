/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include <libxml/tree.h>

#include "jam_xml.h"

char*
jam_xmlGetString(xmlDocPtr doc, xmlNodePtr node) {
	/* we can't guarantee that xmlFree and g_free do the same thing,
	 * so we must copy this string into a glib-allocated string. */
	xmlChar *str = xmlNodeListGetString(doc, node->xmlChildrenNode, TRUE);
	char *ret = g_strdup((char*)str);
	xmlFree(str);
	return ret;
}

guint32
jam_xmlGetUInt32(xmlDocPtr doc, xmlNodePtr node) {
	guint32 ret = 0;
	xmlChar *value = xmlNodeListGetString(doc, node->xmlChildrenNode, TRUE);
	if (value) {
		ret = atol((char*)value);
		xmlFree(value);
	}
	return ret;
}

gboolean
jam_xmlGetInt(xmlDocPtr doc, xmlNodePtr node, int *ret) {
	xmlChar *value = xmlNodeListGetString(doc, node->xmlChildrenNode, TRUE);
	if (value) {
		*ret = atoi((char*)value);
		xmlFree(value);
		return TRUE;
	}
	return FALSE;
}

xmlNodePtr
jam_xmlAddInt(xmlNodePtr node, char *name, int val) {
	char buf[20];
	sprintf(buf, "%d", val);
	return xmlNewTextChild(node, NULL, BAD_CAST name, BAD_CAST buf);
}

void
jam_xmlNewDoc(xmlDocPtr *doc, xmlNodePtr *node, char *name) {
	*doc = xmlNewDoc(BAD_CAST "1.0");
	*node = xmlNewDocNode(*doc, NULL, BAD_CAST name, NULL);
	xmlDocSetRootElement(*doc, *node);
}

void
jam_xmlSetIntProp(xmlNodePtr node, const char *name, int value) {
	char buf[20];
	g_snprintf(buf, 20, "%d", value);
	xmlSetProp(node, BAD_CAST name, BAD_CAST buf);
}

gboolean
jam_xmlGetIntProp(xmlNodePtr node, const char *name, int *value) {
	xmlChar *prop;
	prop = xmlGetProp(node, BAD_CAST name);
	if (prop) {
		*value = atoi((char*)prop);
		xmlFree(prop);
		return TRUE;
	}
	return FALSE;
}

