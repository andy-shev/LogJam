/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2009 Andy Shevchenko <andy.shevchenko@gmail.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 *
 * See http://www.mpris.org/2.0/spec/ for MRPISv2 specification.
 * The version 1.0 is located on http://xmms2.org/wiki/MPRIS
 */

#include <string.h>		/* memset */

#include "lj_dbus.h"

#define MPRIS1_IF			"org.freedesktop.MediaPlayer"
#define MPRIS2_IF_ROOT		"org.mpris.MediaPlayer2"
#define MPRIS2_IF_PLAYER	"org.mpris.MediaPlayer2.Player"
#define DBUS_IF_PROPS		"org.freedesktop.DBus.Properties"

/* Internal prototypes */
static gboolean lj_dbus_open(JamDBus *jd);
static void lj_dbus_predict_player_version(MediaPlayer *player, gchar *dest);
static gboolean lj_dbus_append_player_v1(JamDBus *jd, gchar *dest);
static gboolean lj_dbus_append_player_v2(JamDBus *jd, gchar *dest);
static void lj_dbus_players_clear(JamDBus *jd);
static gboolean lj_dbus_players_find(JamDBus *jd, GError **error);
static gboolean lj_dbus_mpris_update_info_v1(MediaPlayer *player, GError **error);
static gboolean lj_dbus_mpris_update_info_v2(MediaPlayer *player, GError **error);

/* Implementation */
static gboolean
lj_dbus_open(JamDBus *jd) {
	GError *error = NULL;

	jd->bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (jd->bus == NULL) {
		g_printerr("Failed to open connection to bus: %s\n", error->message);
		g_error_free(error);
		return FALSE;
    }
	return TRUE;
}

void lj_dbus_close(JamDBus *jd) {
	if (jd == NULL)
		return;
	lj_dbus_players_clear(jd);
	dbus_g_connection_unref(jd->bus);
}

JamDBus *
lj_dbus_new(void) {
	JamDBus *jd = (JamDBus *) g_malloc0(sizeof(JamDBus));

	if (lj_dbus_open(jd) == FALSE) {
		g_free(jd);
		return NULL;
	}
	return jd;
}

/* Predict version of the player */
static void
lj_dbus_predict_player_version(MediaPlayer *player, gchar *dest) {
	gchar *version;

	version = g_utf8_strchr(player->name, strlen(player->name), ' ');
	player->version = version ? ++version : NULL;

	/* Set workaround for broken player(s) */
	if (g_str_has_suffix(dest, "audacious") &&
	    (g_str_has_prefix(version, "0.") ||
         g_str_has_prefix(version, "1."))) {
		player->hint |= MPRIS_HINT_BAD_STATUS;
	}
}

static gboolean
lj_dbus_append_player_v1(JamDBus *jd, gchar *dest) {
	MediaPlayer *player;
	DBusGProxy *proxy;
	GError *error = NULL;
	gchar *name;

	proxy = dbus_g_proxy_new_for_name(jd->bus, dest, "/", MPRIS1_IF);

	if (!dbus_g_proxy_call(proxy, "Identity", &error, G_TYPE_INVALID,
						   G_TYPE_STRING, &name, G_TYPE_INVALID)) {
		g_printerr("Error: %s\n", error->message);
		g_error_free(error);
		g_object_unref(proxy);
		return FALSE;
	}

	player = (MediaPlayer *) g_malloc0(sizeof(MediaPlayer));
	player->mprisv = MPRIS_V1;
	player->dest = g_strdup(dest);
	player->name = g_strdup(name);

	lj_dbus_predict_player_version(player, dest);

	player->proxy = dbus_g_proxy_new_for_name(jd->bus, dest, "/Player", MPRIS1_IF);

	jd->player = g_list_append(jd->player, (gpointer) player);

	g_free(name);
	g_object_unref(proxy);

	return TRUE;
}

static gboolean
lj_dbus_append_player_v2(JamDBus *jd, gchar *dest) {
	MediaPlayer *player;
	DBusGProxy *proxy;
	GError *error = NULL;
	GValue result = { 0, };

	proxy = dbus_g_proxy_new_for_name(jd->bus, dest, "/org/mpris/MediaPlayer2", DBUS_IF_PROPS);

	if (!dbus_g_proxy_call(proxy, "Get", &error,
			G_TYPE_STRING, MPRIS2_IF_ROOT, G_TYPE_STRING, "Identity", G_TYPE_INVALID,
			G_TYPE_VALUE, &result, G_TYPE_INVALID)) {
		g_printerr("Error: %s\n", error->message);
		g_error_free(error);
		g_object_unref(proxy);
		return FALSE;
	}

	if (!G_VALUE_HOLDS_STRING(&result)) {
		/* TODO: Error messaging */
		g_value_unset(&result);
		g_object_unref(proxy);
		return FALSE;
	}

	player = (MediaPlayer *) g_malloc0(sizeof(MediaPlayer));
	player->mprisv = MPRIS_V2;
	player->dest = g_strdup(dest);
	player->name = g_value_dup_string(&result);

	g_value_unset(&result);

	lj_dbus_predict_player_version(player, dest);

	player->proxy = proxy;

	jd->player = g_list_append(jd->player, (gpointer) player);

	return TRUE;
}

static void
lj_dbus_players_clear(JamDBus *jd) {
	GList *list;

	for (list = g_list_first(jd->player); list; list = g_list_next(list)) {
		MediaPlayer *player = (MediaPlayer *) list->data;
		g_object_unref(player->proxy);
		g_free(player->name);
		g_free(player->dest);
	}
	g_list_free(jd->player);
	jd->player = NULL;
}

static gboolean
lj_dbus_players_find(JamDBus *jd, GError **error) {
	DBusGProxy *proxy;
	gchar **names, **p;

	proxy = dbus_g_proxy_new_for_name(jd->bus,
                                      DBUS_SERVICE_DBUS,
                                      DBUS_PATH_DBUS,
                                      DBUS_INTERFACE_DBUS);

	if (!dbus_g_proxy_call(proxy, "ListNames", error, G_TYPE_INVALID,
						   G_TYPE_STRV, &names, G_TYPE_INVALID)) {
		g_object_unref(proxy);
		return FALSE;
	}

	for (p = names; *p; p++) {
		if (g_str_has_prefix(*p, "org.mpris.MediaPlayer2.")) {
			lj_dbus_append_player_v2(jd, *p);
		} else if (g_str_has_prefix(*p, "org.mpris.")) {
			lj_dbus_append_player_v1(jd, *p);
		}
	}

	g_strfreev(names);
	g_object_unref(proxy);

	return TRUE;
}

gboolean
lj_dbus_mpris_update_list(JamDBus *jd, GError **error) {
	if (jd == NULL)
		return FALSE;
	lj_dbus_players_clear(jd);
	return lj_dbus_players_find(jd, error);
}

#define DBUS_TYPE_MPRIS_STATUS \
	(dbus_g_type_get_struct("GValueArray", G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID))
#define DBUS_TYPE_G_STRING_VALUE_HASHTABLE \
	(dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))

/* TODO: Connect to status change signal */

gboolean
lj_dbus_mpris_update_info(GList *list, GError **error) {
	MediaPlayer *player;

	if (list == NULL)
		return FALSE;

	if ((player = (MediaPlayer *) list->data) == NULL)
		return FALSE;

	memset((void *) &player->info, 0, sizeof(MetaInfo));

	if (player->mprisv == MPRIS_V2) {
		/* If we have MPRISv2 */
		return lj_dbus_mpris_update_info_v2(player, error);
	} else if (player->mprisv == MPRIS_V1) {
		/* If we have MPRISv1 */
		return lj_dbus_mpris_update_info_v1(player, error);
	}

	/* Unknown MPRIS version */
	return FALSE;
}

static gboolean
lj_dbus_mpris_update_info_v1(MediaPlayer *player, GError **error) {
	GValueArray *array = NULL;
	GHashTable *info = NULL;
	GValue *value;

	if (player->hint & MPRIS_HINT_BAD_STATUS) {
		if (!dbus_g_proxy_call(player->proxy, "GetStatus", error, G_TYPE_INVALID,
							   G_TYPE_INT, &player->info.status, G_TYPE_INVALID)) {
			return FALSE;
		}
	} else {
		if (!dbus_g_proxy_call(player->proxy, "GetStatus", error, G_TYPE_INVALID,
							   DBUS_TYPE_MPRIS_STATUS, &array, G_TYPE_INVALID)) {
			return FALSE;
		}

		value = g_value_array_get_nth(array, 0);
		player->info.status = g_value_get_int(value);
		g_value_array_free(array);
	}

	if (player->info.status == MPRIS_STATUS_PLAYING) {
		if (!dbus_g_proxy_call(player->proxy, "GetMetadata", error, G_TYPE_INVALID,
							   DBUS_TYPE_G_STRING_VALUE_HASHTABLE, &info, G_TYPE_INVALID)) {
			return FALSE;
		}

		value = (GValue *) g_hash_table_lookup(info, "artist");
		if (value && G_TYPE_CHECK_VALUE_TYPE(value, G_TYPE_STRV)) {
			g_strlcpy(player->info.artist, g_value_get_string(value), MPRIS_INFO_LEN);
		}

		value = (GValue *) g_hash_table_lookup(info, "album");
		if (value && G_VALUE_HOLDS_STRING(value)) {
			g_strlcpy(player->info.album, g_value_get_string(value), MPRIS_INFO_LEN);
		}

		value = (GValue *) g_hash_table_lookup(info, "title");
		if (value && G_VALUE_HOLDS_STRING(value)) {
			g_strlcpy(player->info.title, g_value_get_string(value), MPRIS_INFO_LEN);
		}
	}

	return TRUE;
}

static gboolean
lj_dbus_mpris_update_info_v2(MediaPlayer *player, GError **error) {
	GHashTable *info;
	GValue *value;
	GValue result = { 0, };
	const gchar *status;

	if (!dbus_g_proxy_call(player->proxy, "Get", error,
			G_TYPE_STRING, MPRIS2_IF_PLAYER, G_TYPE_STRING, "PlaybackStatus", G_TYPE_INVALID,
			G_TYPE_VALUE, &result, G_TYPE_INVALID)) {
			return FALSE;
	}

	if (!G_VALUE_HOLDS_STRING(&result)) {
		/* TODO: Error messaging */
		g_value_unset(&result);
		return FALSE;
	}

	status = g_value_get_string(&result);
	if (strcmp(status, "Playing") == 0) {
		player->info.status = MPRIS_STATUS_PLAYING;
	} else if (strcmp(status, "Paused") == 0) {
		player->info.status = MPRIS_STATUS_PAUSED;
	} else if (strcmp(status, "Stopped") == 0) {
		player->info.status = MPRIS_STATUS_STOPPED;
	} else {
		/* TODO: Error messaging */
		g_value_unset(&result);
		return FALSE;
	}

	g_value_unset(&result);

	if (player->info.status == MPRIS_STATUS_PLAYING) {
		if (!dbus_g_proxy_call(player->proxy, "Get", error,
				G_TYPE_STRING, MPRIS2_IF_PLAYER, G_TYPE_STRING, "Metadata", G_TYPE_INVALID,
				G_TYPE_VALUE, &result, G_TYPE_INVALID)) {
			return FALSE;
		}

		if (!G_TYPE_CHECK_VALUE_TYPE(&result, DBUS_TYPE_G_STRING_VALUE_HASHTABLE)) {
			/* TODO: Error messaging */
			g_value_unset(&result);
			return FALSE;
		}

		info = g_value_get_boxed(&result);

		value = (GValue *) g_hash_table_lookup(info, "xesam:artist");
		if (value && G_TYPE_CHECK_VALUE_TYPE(value, G_TYPE_STRV)) {
			GStrv artists = g_value_get_boxed(value);
			gchar *artist_str = g_strjoinv(", ", artists);

			g_strlcpy(player->info.artist, artist_str, MPRIS_INFO_LEN);
			if (strlen(artist_str) >= MPRIS_INFO_LEN) {
				gchar *delim = g_strrstr(player->info.artist, ", ");
				if (delim)
					*delim = '\0';
			}
			g_free(artist_str);
		}

		value = (GValue *) g_hash_table_lookup(info, "xesam:album");
		if (value && G_VALUE_HOLDS_STRING(value)) {
			g_strlcpy(player->info.album, g_value_get_string(value), MPRIS_INFO_LEN);
		}

		value = (GValue *) g_hash_table_lookup(info, "xesam:title");
		if (value && G_VALUE_HOLDS_STRING(value)) {
			g_strlcpy(player->info.title, g_value_get_string(value), MPRIS_INFO_LEN);
		}

		g_value_unset(&result);
	}

	return TRUE;
}

GQuark
lj_dbus_error_quark(void) {
	static GQuark quark = 0;

	if (quark == 0)
		quark = g_quark_from_static_string("dbus-error-quark");
	return quark;
}

/* TODO: User defined format */

gchar *
lj_dbus_mpris_current_music(JamDBus *jd, GError **error) {
	gchar *music;
	GList *list;

	if (!lj_dbus_mpris_update_list(jd, error))
		return NULL;

	list = jd ? jd->player : NULL;

	if (lj_dbus_mpris_update_info(list, error)) {
		MediaPlayer *player = (MediaPlayer *) list->data;
		if (player->info.status == MPRIS_STATUS_PLAYING) {
			music = g_strdup_printf("%s - %s - %s",
						player->info.artist[0] ? player->info.artist : _("Unknown Artist"),
						player->info.album[0] ? player->info.album: _("Unknown Album"),
						player->info.title[0] ? player->info.title: _("Unknown Track"));
			return music;
		} else {
			g_set_error(error, lj_dbus_error_quark(), MPRIS_ERROR_NOT_PLAYING,
				_("Player is stopped."));
		}
	} else if (error == NULL || *error == NULL) {
		g_set_error(error, lj_dbus_error_quark(), MPRIS_ERROR_NO_PLAYER,
			_("No players found."));
	}
	return NULL;
}

#ifdef LJ_DBUS_CONSOLE_TEST

#include <stdio.h>

int
main()
{
	JamDBus *jd;
	GList *list;
	MediaPlayer *player;
	GError *error = NULL;
	gchar *music;

	/* init.c */
	g_type_init ();

	jd = lj_dbus_new();
	if (!lj_dbus_mpris_update_list(jd, &error)) {
		g_printerr("Error: %s\n", error->message);
		g_error_free(error);
	} else {
		for (list = g_list_first(jd->player); list; list = g_list_next(list)) {
			player = (MediaPlayer *) list->data;
			printf("'%s' '%s'\n", player->name, player->dest);
			if (player->version)
				printf("Version of player: %s\n", player->version);
			jd->player = list;
			lj_dbus_mpris_update_info(jd->player, &error);
			if (error) {
				g_printerr("Error: %s\n", error->message);
				g_error_free(error);
				error = NULL;
				continue;
			}

			player = (MediaPlayer *) jd->player->data;
			printf("%d '%s' '%s' '%s'\n",
				player->info.status,
				player->info.artist,
				player->info.album,
				player->info.title);
		}

		music = lj_dbus_mpris_current_music(jd, &error);
		if (error) {
			g_printerr("Error: %s\n", error->message);
			g_error_free(error);
		} else {
			printf("Now playing: %s\n", music);
		}
		g_free(music);
	}

	lj_dbus_close(jd);
	return 0;
}

#endif /* LJ_DBUS_CONSOLE_TEST */

/* lj_dbus.c */

