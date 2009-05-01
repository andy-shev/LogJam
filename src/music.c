/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"

#include "conf.h"
#include "music.h"

const CommandList music_commands[] = {
	{ N_("None"), NULL },
	{ "Music Player Daemon", "sh -c \"mpc | grep -v '^volume: .* repeat: .* random: .*'\"" },
	{ "Rhythmbox", "rhythmbox-client --print-playing" },
	{ "XMMS / Beep", "logjam-xmms-client" },
	{ "amaroK", "dcop amarok player nowPlaying" },
	{ NULL, NULL }
};

MusicSource
music_current_source(void) {
	int i;

	if (!conf.music_command)
		return MUSIC_SOURCE_NONE;

	for (i = 0; i < MUSIC_SOURCE_COUNT; i++) {
		if (music_commands[i].label && music_commands[i].command &&
				(strcmp(music_commands[i].command, conf.music_command) == 0))
			return i;
	}
	return MUSIC_SOURCE_CUSTOM;
}

GQuark
music_error_quark(void) {
	static GQuark quark = 0;
	if (quark == 0)
		quark = g_quark_from_static_string("music-error-quark");
	return quark;
}

#ifndef G_OS_WIN32
static gboolean
music_command_can_detect(GError **err) {
	if (!conf.music_command || !conf.music_command[0]) {
		g_set_error(err, MUSIC_ERROR, MUSIC_COMMAND_LINE_ERROR,
				_("No music command line is set."));
		return FALSE;
	}
	return TRUE;
}

static char*
music_command_detect(const char *command, GError **err) {
	gchar *std_out, *std_err, *p, *cnv;
	gint exit_status;

	if (!g_spawn_command_line_sync(command,
			&std_out, &std_err, &exit_status, err))
		return NULL;

	if (exit_status != 0) {
		g_set_error(err, MUSIC_ERROR, MUSIC_COMMAND_ERROR, std_err);
		g_free(std_err);
		return NULL;
	}
	g_free(std_err);

	/* we take the first line of this output. */
	for (p = std_out; *p; p++) {
		if (*p == '\n') {
			*p = 0;
			break;
		}
	}
	if (p == std_out) {
		g_set_error(err, MUSIC_ERROR, MUSIC_COMMAND_ERROR,
				_("Command produced no output."));
		g_free(std_out);
		return NULL;
	}
	/* Check if result is correct UTF-8 */
	if (g_utf8_validate(std_out, -1, NULL))
		return std_out;
	/* Try converting from the current locale to UTF-8 */
	cnv = g_locale_to_utf8(std_out, -1, NULL, NULL, err);
	g_free(std_out);
	return cnv;
}

gboolean
music_can_detect(GError **err) {
	return music_command_can_detect(err);
}

char*
music_detect(GError **err) {
	if (conf.music_command)
		return music_command_detect(conf.music_command, err);
	return NULL;
}
#else
gboolean music_can_detect(GError **err) {
	g_set_error(err, MUSIC_ERROR, MUSIC_UNIMPLEMENTED_ERROR,
			_("Music detection is not implemented on this platform."));
	return FALSE;
}
char* music_detect(GError **err) {
	g_set_error(err, MUSIC_ERROR, MUSIC_UNIMPLEMENTED_ERROR,
			_("Music detection is not implemented on this platform."));
	return FALSE;
}
#endif /* G_OS_WIN32 */

