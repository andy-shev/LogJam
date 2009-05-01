/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <string.h>

#include "glib-all.h"

#include <stdlib.h> /* atoi */
#include "conf.h"
#include "conf_xml.h"
#include "jam_xml.h"

/* XXX win32 gross hack.  why does the normal xmlFree cause segfaults? */
#ifdef G_OS_WIN32
#define xmlFree(x) g_free(x)
#endif

#define xmlGetString jam_xmlGetString
#define xmlAddTN(node, name, value) xmlNewTextChild(node, NULL, BAD_CAST name, BAD_CAST value)

/*static xmlNodePtr
ljxmlAddTxt(xmlNodePtr node, char *name, char *val) {
	if (val)
		return xmlNewTextChild(node, NULL, name, val);
	return NULL;
}*/

GSList*
conf_parsedirlist(const char *base, void* (*fn)(const char*, void*), void *arg) {
	GSList *list = NULL;
	GDir *dir;
	const char *dirname;
	char *path;
	void *data;

	dir = g_dir_open(base, 0, NULL);
	if (!dir)
		return NULL;

	dirname = g_dir_read_name(dir);
	while (dirname) {
		path = g_build_filename(base, dirname, NULL);
		data = fn(path, arg);
		if (data)
			list = g_slist_append(list, data);
		g_free(path);
		dirname = g_dir_read_name(dir);
	}
	g_dir_close(dir);

	return list;
}

void*
conf_parsedirxml(const char *dirname, void* (*fn)(xmlDocPtr, xmlNodePtr, void*), void *data) {
	xmlDocPtr doc;
	xmlNodePtr node;
	void *ret;
	char *path;

	path = g_build_filename(dirname, "conf.xml", NULL);

	if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
		g_free(path);
		return NULL;
	}

	doc = xmlParseFile(path);
	g_free(path);
	if (doc == NULL) {
		fprintf(stderr, _("error parsing configuration file.\n"));
		return NULL;
	}

	/* we get the root element instead of the doc's first child
	 * because XML allows comments outside of the root element. */
	node = xmlDocGetRootElement(doc);
	if (node == NULL) {
		fprintf(stderr, _("empty document.\n"));
		xmlFreeDoc(doc);
		return NULL;
	}

	ret = fn(doc, node, data);
	xmlFreeDoc(doc);

	return ret;
}


/* we use a bunch of macro magic to make this simpler.
 * (see xml_macros.h) 
 */

/* this should match the enum in conf.h */
static char* geometry_names[] = {
	"main",
	"login",
	"friends",
	"friendgroups",
	"console",
	"manager",
	"cffloat",
	"offline",
	"preview"
};

static void
parsegeometry(Geometry *geom, xmlDocPtr doc, xmlNodePtr node) {
	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
		XML_GET_INT("x", geom->x)
		XML_GET_INT("y", geom->y)
		XML_GET_INT("width", geom->width)
		XML_GET_INT("height", geom->height)
		XML_GET_INT("panedpos", geom->panedpos)
		XML_GET_END("parsegeometry")
	}
}

static void
parsegeometries(Configuration *c, xmlDocPtr doc, xmlNodePtr node) {
	xmlChar *window;
	int i;
	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
		if (xmlStrcmp(node->name, BAD_CAST "geometry") == 0) {
			window = xmlGetProp(node, BAD_CAST "window");
			if (!window) continue;

			for (i = 0; i < GEOM_COUNT; i++) {
				if (g_ascii_strcasecmp((char*)window, geometry_names[i]) == 0) {
					parsegeometry(&c->geometries[i], doc, node);
					break;
				}
			}
			xmlFree(window);
		}
	}
}

static void*
parsehostdir(const char *dirname, void *data) {
	return conf_parsedirxml(dirname, (conf_parsedirxml_fn)jam_host_from_xml, (void*)dirname);
}
static GSList*
parsehostsdir(char *base) {
	return conf_parsedirlist(base, parsehostdir, NULL);
}

#ifdef HAVE_GTK
static void
parseshowmeta(Options *options, xmlDocPtr doc, xmlNodePtr node) {
	JamViewMeta meta;
	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
		if (jam_view_meta_from_name((char*)node->name, &meta))
			options->showmeta[meta] = TRUE;
	}
}
#endif /* HAVE_GTK */

static void
parseoptions(Configuration *c, xmlDocPtr doc, xmlNodePtr node) {
	Options *options = &c->options;

#define READOPTION(x) XML_GET_BOOL(#x, options->x)
	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
		READOPTION(netdump)
		READOPTION(nofork)
		READOPTION(useproxy)
		READOPTION(useproxyauth)
#ifdef HAVE_GTK
#ifdef HAVE_GTKSPELL
		READOPTION(usespellcheck)
#endif
		READOPTION(revertusejournal)
		READOPTION(autosave)
		READOPTION(cfautostart)
		READOPTION(cfusemask)
		READOPTION(close_when_send)
		READOPTION(docklet)
		READOPTION(start_in_dock)
		READOPTION(cffloat)
		READOPTION(cffloatraise)
		READOPTION(friends_hidestats)
		READOPTION(allowmultipleinstances)
		READOPTION(smartquotes)

		XML_GET_IF("showmeta", parseshowmeta(options, doc, node);)

		/* backward compatibility. */
		XML_GET_BOOL("cfautofloat", options->cffloat)
		XML_GET_BOOL("cfautofloatraise", options->cffloatraise)
#endif /* HAVE_GTK */
		XML_GET_END("parseoptions")
	}
}

static void
parsesecurity(Configuration *c, xmlDocPtr doc, xmlNodePtr node) {
	xmlChar *typestr;
	typestr = xmlGetProp(node, BAD_CAST "type");
	if (!typestr) return;
	lj_security_from_strings(&c->defaultsecurity, (char*)typestr, NULL);
	xmlFree(typestr);
}

#ifdef HAVE_GTK
static GSList*
parsequietdlgs(xmlDocPtr doc, xmlNodePtr node) {
	GSList *quiet_dlgs = NULL;
	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
		XML_GET_LIST("quiet_dlg", quiet_dlgs, xmlGetString)
		XML_GET_END("parsequietdlgs")
	}
	return quiet_dlgs;
}
#endif /* HAVE_GTK */

#ifndef G_OS_WIN32
static void
parseproxyauth(Configuration *c, xmlDocPtr doc, xmlNodePtr node) {
	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
		XML_GET_STR("username", c->proxyuser)
		XML_GET_STR("password", c->proxypass)
		XML_GET_END("parseproxyauth")
	}
}
#endif

static void*
parseconf(xmlDocPtr doc, xmlNodePtr node, void *data) {
	char *hostname = NULL;
	char *hostspath;
	Configuration *c = data;

#define XML_GET_CONF(key, func) XML_GET_IF(key, func(c, doc, node);) 
	node = node->xmlChildrenNode;
	for (; node != NULL; node = node->next) {
		XML_GET_STR("currentserver", hostname)
		XML_GET_CONF("geometries", parsegeometries)
		XML_GET_CONF("options", parseoptions)
		XML_GET_CONF("defaultsecurity", parsesecurity)
		XML_GET_STR("uifont", c->uifont)
#ifdef HAVE_GTKSPELL
		XML_GET_STR("spell_language", c->spell_language)
#endif /* HAVE_GTKSPELL */
#ifndef G_OS_WIN32
		XML_GET_STR("proxy", c->proxy)
		XML_GET_CONF("proxyauth", parseproxyauth)
		XML_GET_STR("spawncommand", c->spawn_command)
		XML_GET_STR("musiccommand", c->music_command)
#endif /* G_OS_WIN32 */
		XML_GET_INT("cfuserinterval", c->cfuserinterval)
		XML_GET_INT("cfthreshold", c->cfthreshold)
#ifdef HAVE_GTK
		XML_GET_FUNC("quiet_dlgs", app.quiet_dlgs, parsequietdlgs)
#endif
		XML_GET_END("conf_xml_read")
	}

#ifdef HAVE_GTKSPELL
	if (!c->spell_language || strlen(c->spell_language) < 2) {
		g_free(c->spell_language);
		c->spell_language = g_strdup("en_US");
	}
#endif /* HAVE_GTKSPELL */
	hostspath = g_build_filename(app.conf_dir, "servers", NULL);
	c->hosts = parsehostsdir(hostspath);
	g_free(hostspath);

	if (hostname) {
		c->lasthost = conf_host_by_name(c, hostname);
		g_free(hostname);
	}
	return NULL;
}

int
conf_read(Configuration *c, char *path) {
	conf_parsedirxml(path, parseconf, c);
	return 0;
}

static void
writegeometry(Geometry *geometry, char *name, xmlNodePtr node) {
	node = xmlNewChild(node, NULL, BAD_CAST "geometry", NULL);
	xmlSetProp(node, BAD_CAST "window", BAD_CAST name);
	jam_xmlAddInt(node, "x", geometry->x);
	jam_xmlAddInt(node, "y", geometry->y);
	jam_xmlAddInt(node, "width",  geometry->width);
	jam_xmlAddInt(node, "height", geometry->height);
	if (geometry->panedpos > 0)
		jam_xmlAddInt(node, "panedpos", geometry->panedpos);
}

#ifdef HAVE_GTK
static void
writeshowmeta(Options *options, xmlNodePtr node) {
	xmlNodePtr showmeta = xmlNewChild(node, NULL, BAD_CAST "showmeta", NULL);
	int i;
	for (i = JAM_VIEW_META_FIRST; i <= JAM_VIEW_META_LAST; i++) {
		if (options->showmeta[i])
			xmlNewChild(showmeta, NULL, BAD_CAST jam_view_meta_to_name(i), NULL);
	}
}
#endif /* HAVE_GTK */

static void
writeoptions(Options *options, xmlNodePtr node) {
#define WRITEOPTION(x) if (options->x) xmlNewChild(node, NULL, BAD_CAST #x, NULL);
	WRITEOPTION(netdump);
	WRITEOPTION(nofork);
	WRITEOPTION(useproxy);
	WRITEOPTION(useproxyauth);
#ifdef HAVE_GTK
#ifdef HAVE_GTKSPELL
	WRITEOPTION(usespellcheck);
#endif
	WRITEOPTION(revertusejournal);
	WRITEOPTION(autosave);
	WRITEOPTION(cfautostart);
	WRITEOPTION(cfusemask);
	WRITEOPTION(close_when_send);
	WRITEOPTION(docklet);
	WRITEOPTION(start_in_dock);
	WRITEOPTION(cffloatraise);
	WRITEOPTION(cffloat);
	WRITEOPTION(friends_hidestats);
	WRITEOPTION(allowmultipleinstances);
	WRITEOPTION(smartquotes);

	writeshowmeta(options, node);
#endif /* HAVE_GTK */
}

int
conf_write(Configuration *c, char *base) {
	GError *err = NULL;
	xmlDocPtr doc;
	xmlNodePtr root, node;
	GSList *l;
	int i;
	char *path;

	jam_xmlNewDoc(&doc, &root, "configuration");
	xmlSetProp(root, BAD_CAST "version", BAD_CAST "1");

	for (l = c->hosts; l != NULL; l = l->next) {
		if (!jam_host_write(l->data, &err)) {
			g_printerr("%s\n", err->message);
			g_error_free(err);
			err = NULL;
		}
	}

	if (c->lasthost)
		xmlAddTN(root, "currentserver", c->lasthost->name);

	node = xmlNewChild(root, NULL, BAD_CAST "geometries", NULL);
	for (i = 0; i < GEOM_COUNT; i++) {
		if (c->geometries[i].width > 0) {
			writegeometry(&c->geometries[i], geometry_names[i], node);
		}
	}

	node = xmlNewChild(root, NULL, BAD_CAST "options", NULL);
	writeoptions(&c->options, node);

	if (c->uifont)
		xmlAddTN(root, "uifont", c->uifont);

#ifdef HAVE_GTKSPELL
	if (c->spell_language)
		xmlAddTN(root, "spell_language", c->spell_language);
#endif
#ifndef G_OS_WIN32
	if (c->proxy)
		xmlAddTN(root, "proxy", c->proxy);
	if (c->proxyuser) {
		node = xmlNewChild(root, NULL, BAD_CAST "proxyauth", NULL);
		xmlAddTN(node, "username", c->proxyuser);
		xmlAddTN(node, "password", c->proxypass);
	}

	if (c->spawn_command)
		xmlAddTN(root, "spawncommand", c->spawn_command);

	if (c->music_command)
		xmlAddTN(root, "musiccommand", c->music_command);
#endif

	if (c->cfuserinterval) {
		char buf[20];
		g_snprintf(buf, 20, "%d", c->cfuserinterval);
		xmlAddTN(root, "cfuserinterval", buf);
	}

	if (c->cfthreshold) {
		char buf[20];
		g_snprintf(buf, 20, "%d", c->cfthreshold);
		xmlAddTN(root, "cfthreshold", buf);
	}

#ifdef HAVE_GTK
	if (app.quiet_dlgs) {
		xmlNodePtr xmlList = xmlNewChild(root, NULL, BAD_CAST "quiet_dlgs", NULL);
		for (l = app.quiet_dlgs; l != NULL; l = l->next) {
			xmlAddTN(xmlList, "quiet_dlg", (char*)l->data);
		}
	}
#endif /* HAVE_GTK */

	if (c->defaultsecurity.type != LJ_SECURITY_PUBLIC) {
		char *type = NULL;
		if (c->defaultsecurity.type == LJ_SECURITY_CUSTOM)
			c->defaultsecurity.type = LJ_SECURITY_PRIVATE;
		lj_security_to_strings(&c->defaultsecurity, &type, NULL);
		if (type) {
			node = xmlNewChild(root, NULL, BAD_CAST "defaultsecurity", NULL);
			xmlSetProp(node, BAD_CAST "type", BAD_CAST type);
			g_free(type);
		}
	}

	path = g_build_filename(base, "conf.xml", NULL);
	if (xmlSaveFormatFile(path, doc, TRUE) < 0) {
		g_printerr("xmlSaveFormatFile error saving to %s.\n", path);
	}
	xmlFreeDoc(doc);
	g_free(path);
	return 0;
}


