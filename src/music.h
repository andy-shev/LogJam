/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef music_h
#define music_h

#include "conf.h" /* CommandList */

typedef enum {
	MUSIC_SOURCE_NONE,
	MUSIC_SOURCE_BANSHEE,
	MUSIC_SOURCE_MPD,
	MUSIC_SOURCE_RHYTHMBOX,
	MUSIC_SOURCE_XMMS,
	MUSIC_SOURCE_AMAROK,
	MUSIC_SOURCE_CUSTOM,
	MUSIC_SOURCE_COUNT
} MusicSource;

extern const CommandList music_commands[];

typedef enum {
	MUSIC_COMMAND_ERROR,
	MUSIC_COMMAND_LINE_ERROR,
	MUSIC_UNIMPLEMENTED_ERROR
} MusicError;

gboolean music_can_detect(GError **err);
char*    music_detect(GError **err);

GQuark   music_error_quark(void);
#define MUSIC_ERROR music_error_quark()

MusicSource music_current_source(void);

#endif /* music_h */
