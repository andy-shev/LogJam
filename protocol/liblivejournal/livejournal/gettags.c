
#include <config.h>

#include <glib.h>

#include "tags.h"
#include "gettags.h"

static void
read_tags(LJResult *result, GHashTable *tags) {
	LJTag *t;
	int i, count;
	guint32 mask;
	char *value;

	count = lj_result_get_int(result, "tag_count");
	for (i = 1; i <= count; i++) {
		t = lj_tag_new();
		t->tag = g_strdup(lj_result_getf(result, "tag_%d_name", i));

        value = lj_result_getf(result, "tag_%d_uses", i);
		if (value) t->count = lj_color_to_int(value);

		g_hash_table_insert(tags, t->tag, t);
	}
}

static void
parse_result(LJVerb *verb) {
	GHashTable *tags;

	tags = g_hash_table_new(g_str_hash, g_str_equal);

	read_tags(verb->result, tags);
	((LJGetTags*)verb)->tags = tags;
}

LJGetTags*
lj_gettags_new(LJUser *user, const gchar *journal) {
	LJGetTags *gettags = g_new0(LJGetTags, 1);
	LJVerb *verb = (LJVerb*)gettags;

	lj_verb_init(verb, user, "getusertags", FALSE, parse_result);
    if (journal)
	    lj_request_add(verb->request, "usejournal", journal);

	return gettags;
}

static void
hash_tag_free_cb(gpointer key, LJTag *t, gpointer data) {
	lj_tag_free(t);
}

void
lj_gettags_free(LJGetTags *gettags, gboolean includetags) {
	if (gettags->tags) {
		if (includetags)
			g_hash_table_foreach(gettags->tags,
					(GHFunc)hash_tag_free_cb, NULL);
		g_hash_table_destroy(gettags->tags);
	}
	g_free(gettags);
}

