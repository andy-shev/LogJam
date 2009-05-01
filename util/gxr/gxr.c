/* gxr - an XMLRPC library for libxml.
 * Copyright (C) 2000-2003 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include <string.h>

#include "gxr-internal.h"

GXRContext*
gxr_context_new(GXRRunRequestFunc func, gpointer data) {
	GXRContext *ctx;
	ctx = g_new0(GXRContext, 1);
	ctx->run_request = func;
	ctx->user_data = data;
	return ctx;
}

void
gxr_context_set_debug(GXRContext *ctx, gboolean debug) {
	ctx->debug = debug;
}

void
gxr_context_free(GXRContext *ctx) {
	g_free(ctx);
}

void
gxr_make_doc(xmlDocPtr *pdoc, xmlNodePtr *pnparams, const char *methodname) {
	xmlDocPtr doc;
	xmlNodePtr nmethod, nparams;

	doc = xmlNewDoc("1.0");
	doc->children = nmethod = xmlNewDocNode(doc, NULL, "methodCall", NULL);
	xmlNewTextChild(nmethod, NULL, "methodName", methodname);
	nparams = xmlNewChild(nmethod, NULL, "params", NULL);

	*pdoc = doc;
	*pnparams = nparams;
}

static void
gxr_parse_value(xmlDocPtr doc, xmlNodePtr nvalue, GXRValueType *type, char **pvalue) {
	xmlNodePtr ntype = nvalue->xmlChildrenNode;

	if (type) {
		*type = GXR_VALUE_UNKNOWN;
		if (strcmp(ntype->name, "int") == 0) {
			*type = GXR_VALUE_INT;
		} else if (strcmp(ntype->name, "string") == 0) {
			*type = GXR_VALUE_STRING;
		} else if (strcmp(ntype->name, "boolean") == 0) {
			*type = GXR_VALUE_BOOLEAN;
		} else {
			g_warning("unknown value type %s\n", ntype->name);
		}
	}

	*pvalue = xmlNodeListGetString(doc, ntype->xmlChildrenNode, 1);
}

static gboolean
gxr_parse_member(xmlDocPtr doc, xmlNodePtr nmember, char **pname, char **pvalue) {
	xmlNodePtr cur;
	gboolean ret = FALSE;
	*pname = *pvalue = NULL;
	if (strcmp(nmember->name, "member") != 0) goto out;
	for (cur = nmember->xmlChildrenNode; cur; cur = cur->next) {
		if (strcmp(cur->name, "name") == 0) {
			*pname = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		} else if (strcmp(cur->name, "value") == 0) {
			gxr_parse_value(doc, cur, NULL, pvalue);
		}
	}

	if (*pname && *pvalue)
		ret = TRUE;

out:
	if (!ret) {
		if (*pname) xmlFree(*pname);
		if (*pvalue) xmlFree(*pvalue);
	}
	return ret;
}

#define PARSE_ASSERT(x, dom, code, text) if (!(x)) { g_set_error(err, dom, code, text); goto out; }
#define PARSE_ASSERT_XML(x) PARSE_ASSERT(x, 0, 0, "Error parsing response XML.")

static void
gxr_parse_fault(xmlDocPtr doc, xmlNodePtr nfault, GError **err) {
	xmlNodePtr cur, n;
	char *faultString = NULL, *faultCode = NULL;

	cur = nfault;
	PARSE_ASSERT_XML(strcmp(cur->name, "fault") == 0);
	cur = cur->xmlChildrenNode;
	PARSE_ASSERT_XML(strcmp(cur->name, "value") == 0);
	cur = cur->xmlChildrenNode;
	PARSE_ASSERT_XML(strcmp(cur->name, "struct") == 0);
	cur = cur->xmlChildrenNode;
	for (n = cur; n; n = n->next) {
		char *name, *value;
		PARSE_ASSERT_XML(gxr_parse_member(doc, n, &name, &value));
		if (strcmp(name, "faultString") == 0) {
			faultString = value; xmlFree(name);
		} else if (strcmp(name, "faultCode") == 0) {
			faultCode = value; xmlFree(name);
		} else {
			xmlFree(name); xmlFree(value);
		}
	}
	g_set_error(err, 0, 0, "Fault: %s [%s]",
		faultString ? faultString : "(no faultString)",
		faultCode ? faultCode : "(no faultCode)");
out:
	if (faultString) xmlFree(faultString);
	if (faultCode) xmlFree(faultCode);
}

static gboolean
gxr_parse_response(GString *response, GXRValueType wanttype, char **value, GError **err) {
	xmlDocPtr doc = NULL;
	xmlNodePtr cur;
	GXRValueType type;
	gboolean ret = FALSE;

	/* XXX xmlParseMemory errors? */
	doc = xmlParseMemory(response->str, response->len);
	cur = xmlDocGetRootElement(doc);
	PARSE_ASSERT_XML(strcmp(cur->name, "methodResponse") == 0);
	cur = cur->xmlChildrenNode;

	if (strcmp(cur->name, "fault") == 0) {
		gxr_parse_fault(doc, cur, err);
		goto out;
	}
	PARSE_ASSERT_XML(strcmp(cur->name, "params") == 0);
	cur = cur->xmlChildrenNode;
	PARSE_ASSERT_XML(strcmp(cur->name, "param") == 0);
	cur = cur->xmlChildrenNode;
	PARSE_ASSERT_XML(strcmp(cur->name, "value") == 0);
	gxr_parse_value(doc, cur, &type, value);
	if (type != wanttype) {
		g_set_error(err, 0, 0, "Return value is of wrong type.");
		goto out;
	}
	ret = TRUE;
out:
	xmlFreeDoc(doc);
	return ret;
}

gboolean
gxr_run_request(GXRContext *ctx, xmlDocPtr doc, GXRValueType wanttype, char **retval, GError **err) {
	xmlChar *mem = NULL; int len;
	GString *request = NULL, *response = NULL;

	*retval = NULL;

	xmlDocDumpMemory(doc, &mem, &len);
	request = g_string_new_len(mem, len);

	response = g_string_new(NULL);
	if (!ctx->run_request(ctx->user_data, request, response, err))
		goto out;

	if (!gxr_parse_response(response, wanttype, retval, err))
		goto out;

out:
	if (request) g_string_free(request, TRUE);
	if (response) g_string_free(response, TRUE);
	return (*retval != NULL);
}

void
gxr_add_param(xmlNodePtr nparent, const char *type, const char *value) {
	xmlNodePtr nparam, nvalue, ntype;
	nparam = xmlNewChild(nparent, NULL, "param", NULL);
	nvalue = xmlNewChild(nparam, NULL, "value", NULL);
	ntype = xmlNewChild(nvalue, NULL, type, value);
}

void
gxr_add_param_string(xmlNodePtr nparent, const char *value) {
	gxr_add_param(nparent, "string", value);
}

void
gxr_add_param_int(xmlNodePtr nparent, int value) {
	char *string = g_strdup_printf("%d", value);
	gxr_add_param(nparent, "int", string);
	g_free(string);
}

void
gxr_add_param_boolean(xmlNodePtr nparent, gboolean value) {
	char *string = g_strdup_printf("%d", value);
	gxr_add_param(nparent, "boolean", string);
	g_free(string);
}

