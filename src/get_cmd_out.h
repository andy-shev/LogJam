/* logjam - a GTK client for LiveJournal.
 *
 * Functions to get the output of external command.
 * Copyright (C) 2004, Kir Kolyshkin <kir@sacred.ru>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __GET_CMD_OUT_H__
#define __GET_CMD_OUT_H__

GString * get_command_output(const char *command, GError **err,
		GtkWindow *parent);

#endif /* __GET_CMD_OUT_H__ */
