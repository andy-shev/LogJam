/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2003 Evan Martin <evan@livejournal.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#include "config.h"

#include "gtk-all.h"
#include <stdlib.h> /* atoi */
#include <string.h>

#include "conf.h"
#include "security.h"
#include "util-gtk.h"
#include "spawn.h"
#include "jam.h"
#include "checkfriends.h"
#include "settings.h"
#include "music.h"
#include "groupedbox.h"
#include "remote.h"
#include "smartquotes.h"
#include "tie.h"
#include "account.h"
#include "jamview.h"

/* what's this?  all of these funny structures in the settings box?
 * well, instead of creating and tearing down all of these widgets, i
 * try to pull out shared functionality.
 *
 * it's loosely inspired by george lebl's pong 
 *   http://www.5z.com/jirka/pong-documentation/
 * but i've never actaully read how that works.
 *
 *
 * we have a collection of SettingsWidgets that track their name and
 * configuration option, and then it's all nicely automated through functions
 * of the form sw_*(). 
 */

typedef enum {
	SW_TOGGLE,
	SW_RADIO,
	SW_TEXT,
	SW_INTEGER,
	SW_COMBO,
	SW_SPIN_INTEGER,
	SW_COMMAND,
	SW_CUSTOM
} SettingsWidgetType;

typedef struct _SettingsWidget SettingsWidget;
struct _SettingsWidget {
	char *name;
	void *conf;
	SettingsWidgetType type;
	char *caption;
	gpointer data; /* extra pointer, to be used as needed. */
	gpointer data2; /* extra pointer for wimps. */
	GtkWidget *subwidget;
	GtkWidget *widget;
};

static void run_cfmask_settings_dlg(SettingsWidget *sw);

static SettingsWidget settingswidgets[] = {
	{ "ui_revertusejournal", &conf.options.revertusejournal, 
		SW_TOGGLE, N_("_Revert to primary journal after posting on a community"), NULL },
	{ "ui_defaultsecurity", &conf.defaultsecurity, 
		SW_CUSTOM, N_("Default _security on posts:") },
	{ "ui_autosave", &conf.options.autosave,
		SW_TOGGLE, N_("Automatically save _drafts (for crash recovery)") },

	{ "ui_allowmultipleinstances", &conf.options.allowmultipleinstances,
		SW_TOGGLE, N_("Allow multiple _instances of LogJam to run simultaneously") },

	{ "ui_keepsaveddrafts", &conf.options.keepsaveddrafts,
		SW_TOGGLE, N_("_Keep saved drafts after posting") },

#ifdef HAVE_GTKSPELL
	{ "ui_spellcheck", &conf.options.usespellcheck, 
		SW_TOGGLE, N_("_Use spell check") },
	{ "ui_spell_language", &conf.spell_language,
		SW_TEXT, N_("Entry _language:") },
#endif
	{ "ui_showloginhistory", &conf.options.showloginhistory,
		SW_TOGGLE, N_("Show login history (number of days since last login)") },
	{ "ui_smartquotes", &conf.options.smartquotes,
		SW_TOGGLE, N_("Automatically change _quotes to matching pairs") },
	{ "ui_font", &conf.uifont,
		SW_CUSTOM, N_("Entry display font:") },
#ifndef G_OS_WIN32
	{ "ui_close_when_send", &conf.options.close_when_send,
		SW_TOGGLE, N_("Close main window after sending") },
	{ "ui_docklet", &conf.options.docklet,
		SW_TOGGLE, N_("Add icon to system _tray (for GNOME/KDE/etc. dock)") },
	{ "ui_start_in_dock", &conf.options.start_in_dock,
		SW_TOGGLE, N_("Start in system tray") },
#endif

#ifndef G_OS_WIN32
	{ "web_spawn", &conf.spawn_command,
		SW_COMMAND, N_("Web browser:"), (gpointer)spawn_commands },

	{ "music_command", &conf.music_command,
		SW_COMMAND, N_("Detect music from:"), (gpointer)music_commands },

	{ "net_useproxy", &conf.options.useproxy, 
		SW_TOGGLE, N_("Use _proxy server") },
	{ "net_proxy", &conf.proxy, 
		SW_TEXT, N_("UR_L:") },

	{ "net_useproxyauth", &conf.options.useproxyauth, 
		SW_TOGGLE, N_("Use proxy _authentication") },
	{ "net_proxyuser", &conf.proxyuser, 
		SW_TEXT, N_("_User:") },
	{ "net_proxypass", &conf.proxypass, 
		SW_TEXT, N_("_Password:") },
#endif

	{ "debug_netdump", &conf.options.netdump, 
		SW_TOGGLE, N_("Dump _network data on stderr") },
#ifndef G_OS_WIN32
	{ "debug_nofork", &conf.options.nofork, 
		SW_TOGGLE, N_("Don't _fork on network requests") },
#else
	{ "debug_nofork", &conf.options.nofork, 
		SW_TOGGLE, N_("Don't run network requests in a separate _thread") },
#endif


	{ "cf_autostart", &conf.options.cfautostart,
		SW_TOGGLE, N_("_Monitor friends list for new entries upon login") },
	{ "cf_usemask", &conf.options.cfusemask,
		SW_TOGGLE, N_("_Limit monitoring to groups of friends") },
	{ "cf_mask", NULL /* set at runtime to that of current user */,
		SW_CUSTOM, "" },
	{ "cf_userinterval", &conf.cfuserinterval,
		SW_SPIN_INTEGER, N_("_Check friends list every " /* X seconds */) },
	{ "cf_threshold", &conf.cfthreshold,
		SW_SPIN_INTEGER, N_("_Notify me after " /* X new entires */) },
	{ "cf_float", &conf.options.cffloat,
		SW_TOGGLE, N_("_Floating indicator") },
	{ "cf_floatraise", &conf.options.cffloatraise,
		SW_TOGGLE, N_("_Raise floating indicator when new entries detected") },
	{ "cf_float_decorate", &conf.options.cffloat_decorate,
		SW_TOGGLE, N_("Show _titlebar on floating indicator") },
	
	{ NULL }
};

static SettingsWidget*
sw_lookup(char *name) {
	SettingsWidget *sw;
	for (sw = settingswidgets; sw->name; sw++) 
		if (strcmp(name, sw->name) == 0) return sw;
	g_error("sw_lookup failed for %s", name);
	return NULL;
}

static void
toggle_enable_cb(GtkWidget *toggle, GtkWidget *target) {
	gtk_widget_set_sensitive(target, 
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)));
}
static void
toggle_tie_enable(GtkWidget *toggle, GtkWidget *target) {
	gtk_widget_set_sensitive(target, 
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)));
	g_signal_connect(G_OBJECT(toggle), "toggled",
			G_CALLBACK(toggle_enable_cb), target);
}

static void
toggle_tie(SettingsWidget *sw) {
	tie_toggle(GTK_TOGGLE_BUTTON(sw->widget), (gboolean*)sw->conf);
}

static void
radio_tie_cb(GtkToggleButton *toggle, SettingsWidget *sw) {
	if (gtk_toggle_button_get_active(toggle))
		*(int*)sw->conf = GPOINTER_TO_INT(sw->data);
}
static void
radio_tie(SettingsWidget *sw) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sw->widget),
			*(gint*)sw->conf == GPOINTER_TO_INT(sw->data));
	g_signal_connect(G_OBJECT(sw->widget), "toggled",
			G_CALLBACK(radio_tie_cb), sw);
}

static void
text_tie(SettingsWidget *sw) {
	tie_text(GTK_ENTRY(sw->widget), (char**)sw->conf);
}

static void
integer_tie_cb(GtkEditable *e, SettingsWidget *sw) {
	char *text = gtk_editable_get_chars(e, 0, -1);
	*(int*)sw->conf = atoi(text);
	g_free(text);
}
static void
integer_tie(SettingsWidget *sw) {
	char buf[30];
	sprintf(buf, "%d", *(int*)sw->conf);
	gtk_entry_set_text(GTK_ENTRY(sw->widget), buf);
	g_signal_connect(G_OBJECT(sw->widget), "changed",
			G_CALLBACK(integer_tie_cb), sw);
}

static void
spin_integer_tie_cb(GtkSpinButton *b, SettingsWidget *sw) {
	*(gint*)sw->conf = (gint)gtk_spin_button_get_value(b);
}
static void
spin_integer_tie(SettingsWidget *sw) {
	g_signal_connect(G_OBJECT(sw->widget), "value-changed",
			G_CALLBACK(spin_integer_tie_cb), sw);
}

static void
combo_tie(SettingsWidget *sw) {
	tie_combo(GTK_COMBO(sw->widget), (char**)sw->conf);
}


static void
command_changed_cb(GtkOptionMenu *omenu, SettingsWidget *sw) {
	CommandList *cmds = sw->data;
	int cur = gtk_option_menu_get_history(GTK_OPTION_MENU(sw->widget));

	jam_widget_set_visible(sw->data2, cmds[cur].label == NULL);
	if (cmds[cur].label != NULL) {
		const char *cmd = cmds[cur].command;
		string_replace((char**)sw->conf, cmd ? g_strdup(cmd) : NULL);
		gtk_entry_set_text(GTK_ENTRY(sw->subwidget), cmd ? cmd : "");
	}
}

static GtkWidget*
command_make(SettingsWidget *sw) {
	CommandList *cmds = sw->data;
	GtkWidget *vbox, *menu;
	GtkSizeGroup *sg;
	char *cmd = *((char**)sw->conf);
	int cur = -1;
	int i;

	menu = gtk_menu_new();
	for (i = 0; cmds[i].label; i++) {
		char *curcmd = cmds[i].command;
		gtk_menu_shell_append(GTK_MENU_SHELL(menu),
				gtk_menu_item_new_with_label(_(cmds[i].label)));
		if (cur == -1 &&
				((cmd == curcmd) ||
				(cmd && curcmd && (strcmp(cmd, curcmd) == 0))))
			cur = i;
	}
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),
			gtk_menu_item_new_with_label(_("Custom Command")));
	if (cur == -1) cur = i;

	sw->widget = gtk_option_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(sw->widget), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(sw->widget), cur);

	sw->subwidget = gtk_entry_new();
	tie_text(GTK_ENTRY(sw->subwidget), (char**)sw->conf);

	g_signal_connect(G_OBJECT(sw->widget), "changed",
			G_CALLBACK(command_changed_cb), sw);
	jam_widget_set_visible(sw->subwidget, cur == i);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	vbox = gtk_vbox_new(FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox),
			labelled_box_new_sg(_(sw->caption), sw->widget, sg),
			FALSE, FALSE, 0);
	sw->data2 = labelled_box_new_sg(NULL, sw->subwidget, sg);  /* ugh. */
	gtk_box_pack_start(GTK_BOX(vbox), 
			sw->data2,
			FALSE, FALSE, 0);
	return vbox;
}

static GtkWidget*
sw_make_sg(char *name, GtkSizeGroup *sg) {
	SettingsWidget *sw = sw_lookup(name);
	switch (sw->type) {
		case SW_TOGGLE:
			sw->widget = gtk_check_button_new_with_mnemonic(_(sw->caption));
			toggle_tie(sw);
			return sw->widget;
		case SW_RADIO:
			sw->widget = gtk_radio_button_new_with_mnemonic(NULL,
					_(sw->caption));
			radio_tie(sw);
			return sw->widget;
		case SW_TEXT:
			sw->widget = gtk_entry_new();
			text_tie(sw);
			return labelled_box_new_sg(_(sw->caption), sw->widget, sg);
		case SW_INTEGER:
			sw->widget = gtk_entry_new();
			gtk_entry_set_width_chars(GTK_ENTRY(sw->widget), 4);
			integer_tie(sw);
			return labelled_box_new(_(sw->caption), sw->widget);
		case SW_SPIN_INTEGER:
			{
				gdouble default_value = *(gint*)sw->conf;
				GtkObject *adj = gtk_adjustment_new(default_value,
						/* client code should override these */
						G_MINDOUBLE, G_MAXDOUBLE, 1.0, 1.0, 0);
				sw->widget = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 1);
			}
			spin_integer_tie(sw);
			return labelled_box_new_expand(_(sw->caption), sw->widget, FALSE);
		case SW_COMBO:
			sw->widget = gtk_combo_new();
			combo_tie(sw);
			return labelled_box_new(_(sw->caption), sw->widget);
		case SW_COMMAND:
			return command_make(sw);
		case SW_CUSTOM:
			if (sw->caption && sw->widget)
				return labelled_box_new(_(sw->caption), sw->widget);
			break;
		default:
			break;
	}
	return NULL;
}
static GtkWidget*
sw_make(char *name) {
	return sw_make_sg(name, NULL);
}

static void
sec_changed_cb(SecMgr *sm, SettingsWidget *sw) {
	secmgr_security_get(sm, (LJSecurity*)sw->conf);
}

static void
run_fontsel_settings_dlg(SettingsWidget *sw) {
	GtkWidget *dlg;
	const gchar *newfont;
	gchar *oldfont;

	dlg = gtk_font_selection_dialog_new(_("Select font"));
	gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(dlg),
			gtk_label_get_text(GTK_LABEL(sw->widget))); 
	
	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
		gtk_label_set_text(GTK_LABEL(sw->widget), 
				gtk_font_selection_dialog_get_font_name(
						GTK_FONT_SELECTION_DIALOG(dlg)));
	}


	newfont = gtk_label_get_text(GTK_LABEL(sw->widget));
	oldfont = pango_font_description_to_string(
			pango_context_get_font_description(
				gtk_widget_get_pango_context(GTK_WIDGET(sw->data))));

	if (newfont && g_ascii_strcasecmp(oldfont, newfont) != 0) {
		string_replace(sw->conf, g_strdup(newfont));
		jam_widget_set_font(sw->widget, newfont);
		jam_widget_set_font(sw->data, newfont);
	}
	g_free(oldfont);
	
	gtk_widget_destroy(dlg); 
}

#ifdef USE_DOCK
void docklet_enable(GtkWidget *win, gboolean enable);
static void
docklet_change_cb(GtkToggleButton *tb, JamWin *jw) {
	docklet_enable(GTK_WIDGET(jw), gtk_toggle_button_get_active(tb));
}
#endif /* USE_DOCK */

static GtkWidget*
uisettings(JamWin *jw) {
	SettingsWidget *sw;
	GtkWidget *vbox, *hbox, *button;
	char *fontname = NULL;
	GtkWidget *post, *entry, *misc;

	vbox = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

	post = groupedbox_new_with_text(_("Posting"));
	gtk_box_pack_start(GTK_BOX(vbox), post, FALSE, FALSE, 0);

	groupedbox_pack(GROUPEDBOX(post),
			sw_make("ui_revertusejournal"), FALSE);

	sw = sw_lookup("ui_defaultsecurity");
	sw->widget = secmgr_new(FALSE);
	secmgr_security_set(SECMGR(sw->widget), (LJSecurity*)sw->conf);
	g_signal_connect(G_OBJECT(sw->widget), "changed",
			G_CALLBACK(sec_changed_cb), sw);
	groupedbox_pack(GROUPEDBOX(post), sw_make("ui_defaultsecurity"), FALSE);

#ifdef USE_DOCK
	groupedbox_pack(GROUPEDBOX(post), sw_make("ui_close_when_send"), FALSE);
#endif /* USE_DOCK */

	entry = groupedbox_new_with_text(_("Entries"));
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

	groupedbox_pack(GROUPEDBOX(entry), sw_make("ui_autosave"), FALSE);

	groupedbox_pack(GROUPEDBOX(entry), sw_make("ui_keepsaveddrafts"), FALSE);

#ifdef HAVE_GTKSPELL
	{
		GtkWidget *toggle = sw_make("ui_spellcheck");
		GtkWidget *label, *box;

		hbox = sw_make("ui_spell_language");
		label = gtk_label_new("        ");
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		gtk_box_reorder_child(GTK_BOX(hbox), label, 0);

		sw = sw_lookup("ui_spell_language");
		gtk_entry_set_width_chars(GTK_ENTRY(sw->widget), 5);

		box = gtk_vbox_new(FALSE, 3);
		gtk_box_pack_start(GTK_BOX(box), toggle, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);

		groupedbox_pack(GROUPEDBOX(entry), box, FALSE);
		toggle_tie_enable(toggle, hbox);
	}
#endif

	groupedbox_pack(GROUPEDBOX(entry), sw_make("ui_showloginhistory"), TRUE);
	groupedbox_pack(GROUPEDBOX(entry),
			sw_make("ui_smartquotes"), FALSE);
	
	sw = sw_lookup("ui_font");
	if (conf.uifont == NULL) {
		fontname = pango_font_description_to_string(
				pango_context_get_font_description(
					gtk_widget_get_pango_context(jw->view)));
		sw->widget = gtk_label_new(fontname ? fontname : _("[gtk default]"));
		if (fontname)
			jam_widget_set_font(sw->widget, fontname);
		g_free(fontname);
	} else {
		sw->widget = gtk_label_new(conf.uifont);
		jam_widget_set_font(sw->widget, conf.uifont);
	}
	button = gtk_button_new_from_stock(GTK_STOCK_SELECT_FONT);
	sw->data = jw->view;

	hbox = labelled_box_new_expand(_(sw->caption), button, FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), sw->widget, TRUE, TRUE, 0);
	gtk_box_reorder_child(GTK_BOX(hbox), sw->widget, 1);

	groupedbox_pack(GROUPEDBOX(entry), hbox, FALSE);
	g_signal_connect_swapped(G_OBJECT(button), "clicked",
			G_CALLBACK(run_fontsel_settings_dlg), sw);

	misc = groupedbox_new_with_text(_("Behavior"));
	gtk_box_pack_start(GTK_BOX(vbox), misc, FALSE, FALSE, 0);

	groupedbox_pack(GROUPEDBOX(misc),
			sw_make("ui_allowmultipleinstances"), FALSE);

#ifdef USE_DOCK
	button = sw_make("ui_docklet");
	g_signal_connect(G_OBJECT(button), "toggled",
			G_CALLBACK(docklet_change_cb), jw);
	groupedbox_pack(GROUPEDBOX(misc), button, FALSE);
	groupedbox_pack(GROUPEDBOX(misc), sw_make("ui_start_in_dock"), FALSE);
#endif /* USE_DOCK */

	return vbox;
}

#ifndef G_OS_WIN32
static GtkWidget*
proxyuserpass() {
	GtkWidget *vbox;
	GtkSizeGroup *sg;

	vbox = gtk_vbox_new(FALSE, 6);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	gtk_box_pack_start(GTK_BOX(vbox),
			sw_make_sg("net_proxyuser", sg), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox),
			sw_make_sg("net_proxypass", sg), FALSE, FALSE, 0);

	return vbox;
}

static void
music_diagnose(GtkWidget *button) {
	GtkWindow *dlg = GTK_WINDOW(gtk_widget_get_toplevel(button));
	GError *err = NULL;
	char *music, *result;
	MusicSource source = music_current_source();

	if (!music_can_detect(&err)) {
		jam_warning(dlg, "%s", err->message);
		g_error_free(err);
		return;
	}

	music = music_detect(&err);
	if (!music) {
		if (source == MUSIC_SOURCE_XMMS &&
				err->domain == G_SPAWN_ERROR && err->code == G_SPAWN_ERROR_NOENT) {
			jam_warning(dlg, _("LogJam XMMS helper not found.  "
						"Did you install LogJam's XMMS support?"));
		} else {
			jam_warning(dlg, _("Error detecting music: %s"), err->message);
		}
		g_error_free(err);
		return;
	}
	result = g_strdup_printf(_("Music detection succeeded.\n\nCurrent music:\n%s"), music);
	jam_messagebox(dlg, _("Music Detection"), result);
	g_free(music);
	g_free(result);
}

static GtkWidget*
proxysettings(void) {
	GtkWidget *group, *wbox, *hbox, *cb, *table, *authgroup;

	group = groupedbox_new_with_text(_("Proxy"));

	cb = sw_make("net_useproxy");
	groupedbox_pack(GROUPEDBOX(group), cb, FALSE);

	wbox = gtk_vbox_new(FALSE, 6);
	toggle_tie_enable(cb, wbox);
	groupedbox_pack(GROUPEDBOX(group), wbox, FALSE);

	hbox = sw_make("net_proxy");
	gtk_box_pack_start(GTK_BOX(wbox), hbox, FALSE, FALSE, 0);

	cb = sw_make("net_useproxyauth");
	gtk_box_pack_start(GTK_BOX(wbox), cb, FALSE, FALSE, 0);

	table = proxyuserpass();
	authgroup = groupedbox_new();
	groupedbox_pack(GROUPEDBOX(authgroup), table, FALSE);
	gtk_box_pack_start(GTK_BOX(wbox), authgroup, FALSE, FALSE, 0);

	toggle_tie_enable(cb, table);

	return group;
}

static GtkWidget*
programsettings(JamWin *jw) {
	GtkWidget *group;
	GtkWidget *button, *hbox;
	SettingsWidget *sw;
	JamView *jv = jam_win_get_cur_view(jw);
	
	group = groupedbox_new_with_text(_("Programs"));
	groupedbox_pack(GROUPEDBOX(group), sw_make("web_spawn"), FALSE);

	groupedbox_pack(GROUPEDBOX(group), sw_make("music_command"), FALSE);
	sw = sw_lookup("music_command");
	g_signal_connect_swapped(G_OBJECT(sw->widget), "changed",
			G_CALLBACK(jam_view_settings_changed), jv);

	button = gtk_button_new_with_mnemonic(_("_Diagnose"));
	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(music_diagnose), NULL);
	hbox = labelled_box_new_expand(_("Diagnose problems detecting music:"),
			button, FALSE);
	groupedbox_pack(GROUPEDBOX(group), hbox, FALSE);

	return group;
}

static GtkWidget*
systemsettings(JamWin *jw) {
	GtkWidget *vbox;

	vbox = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
	gtk_box_pack_start(GTK_BOX(vbox), programsettings(jw), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), proxysettings(), FALSE, FALSE, 0);
	return vbox;
}
#endif /* G_OS_WIN32 */

static GtkWidget*
debugsettings(GtkWidget *dlg) {
	GtkWidget *vbox, *gb;

	vbox = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

	gb = groupedbox_new_with_text(_("Network"));
	gtk_box_pack_start(GTK_BOX(vbox), gb, FALSE, FALSE, 0);

	groupedbox_pack(GROUPEDBOX(gb), sw_make("debug_netdump"), FALSE);
	groupedbox_pack(GROUPEDBOX(gb), sw_make("debug_nofork"), FALSE);

	return vbox;
}

static GtkWidget*
cfriends_general_settings(JamAccountLJ *acc) {
	GtkWidget *general, *b, *w;
	SettingsWidget *sw;
	
	general = groupedbox_new_with_text(_("General"));

	groupedbox_pack(GROUPEDBOX(general), sw_make("cf_autostart"), FALSE);

	b  = sw_make("cf_threshold");
	sw = sw_lookup("cf_threshold");
	w  = sw->widget;
	jam_spin_button_set(GTK_SPIN_BUTTON(w), TRUE /* numeric */,
			1.0, 7.0 /* range */, 1.0, 1.0 /*increments */, 0 /* digits */);
	gtk_box_pack_start(GTK_BOX(b),
			gtk_label_new(_("new entries by my friends")), FALSE, FALSE, 5);
	/* XXX: ugly, because it defaults to "1 entries". Yuck. */
	groupedbox_pack(GROUPEDBOX(general), b, FALSE);

	b  = sw_make("cf_userinterval");
	sw = sw_lookup("cf_userinterval");
	w  = sw->widget;
	jam_spin_button_set(GTK_SPIN_BUTTON(w), TRUE /*numeric*/,
			45.0, 3600.0 /*range*/, 1.0, 10.0 /*increments*/, 0 /*digits*/);
	gtk_box_pack_start(GTK_BOX(b), gtk_label_new(_("seconds")),
			FALSE, FALSE, 5);
	groupedbox_pack(GROUPEDBOX(general), b, FALSE);


	return general;
}

static GtkWidget*
cfriends_filter_settings(JamAccountLJ *acc) {
	GtkWidget *filter, *maskhbox, *l;
	SettingsWidget *sw;
	LJUser *u = jam_account_lj_get_user(acc);
	
	filter = groupedbox_new_with_text(_("Filter"));

	maskhbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(maskhbox), 
			sw_make("cf_usemask"), FALSE, FALSE, 0);
	sw = sw_lookup("cf_mask");
	sw->conf = acc;
	sw->widget = gtk_button_new_with_label(_("Choose filter"));
	gtk_box_pack_start(GTK_BOX(maskhbox), GTK_WIDGET(sw->widget), FALSE, FALSE, 0);
	gtk_widget_set_sensitive(sw->widget, u->friendgroups != NULL);
	g_signal_connect_swapped(G_OBJECT(sw->widget), "clicked",
			G_CALLBACK(run_cfmask_settings_dlg), sw);
	l = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(l),
			_("<small>The filter chosen here only affects the "
				"current user.</small>"));
	gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
	gtk_label_set_justify(GTK_LABEL(l), GTK_JUSTIFY_LEFT);

	groupedbox_pack(GROUPEDBOX(filter), maskhbox, FALSE);
	groupedbox_pack(GROUPEDBOX(filter), l, FALSE);

	return filter;
}

static void
float_change_cb(GtkWidget *w, CFMgr *cfm) {
	cf_app_update_float();
}

static GtkWidget*
cfriends_indicators_settings(CFMgr *cfm) {
	GtkWidget *indicators, *floaters, *b, *w;
	SettingsWidget *sw;
	
	indicators = groupedbox_new_with_text(_("Indicators"));

	b  = sw_make("cf_float");
	g_signal_connect(G_OBJECT(b), "toggled",
			G_CALLBACK(float_change_cb), cfm);
	sw = sw_lookup("cf_float");
	w  = sw->widget;
	groupedbox_pack(GROUPEDBOX(indicators), b, FALSE);

	floaters = groupedbox_new();
	groupedbox_pack(GROUPEDBOX(indicators), floaters, FALSE);
	
	groupedbox_pack(GROUPEDBOX(floaters), sw_make("cf_floatraise"), FALSE);
	b = sw_make("cf_float_decorate");
	g_signal_connect(G_OBJECT(b), "toggled",
			G_CALLBACK(cf_float_decorate_refresh), NULL);
	groupedbox_pack(GROUPEDBOX(floaters), b, FALSE);

	toggle_tie_enable(w, floaters);

	return indicators;
}

static GtkWidget*
cfriendssettings(CFMgr *cfm) {
	GtkWidget *vbox;
	JamAccountLJ *acc = cfmgr_get_account(cfm);

	vbox = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

	/* general */
	gtk_box_pack_start(GTK_BOX(vbox), cfriends_general_settings(acc),
			FALSE, FALSE, 0);

	/* filter */
	gtk_box_pack_start(GTK_BOX(vbox), cfriends_filter_settings(acc),
			FALSE, FALSE, 0);
	
	/* indicators */
	gtk_box_pack_start(GTK_BOX(vbox), cfriends_indicators_settings(cfm),
			FALSE, FALSE, 0);

	return vbox;
}

static void
run_settings_dialog(JamWin *jw) {
	GtkWidget *dlg, *nb;

	dlg = gtk_dialog_new_with_buttons(_("Preferences"),
			GTK_WINDOW(jw), GTK_DIALOG_MODAL,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);

	/* the order of notebook pages created here should match the
	 * SettingsPage enum in settings.h */
	nb = gtk_notebook_new();
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), 
			uisettings(jw), gtk_label_new_with_mnemonic(_("Interface")));
#ifndef G_OS_WIN32
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), 
			systemsettings(jw), gtk_label_new_with_mnemonic(_("System")));
#endif /* G_OS_WIN32 */
	if (JAM_ACCOUNT_IS_LJ(jw->account))
		gtk_notebook_append_page(GTK_NOTEBOOK(nb), 
				cfriendssettings(app.cfmgr), gtk_label_new_with_mnemonic(_("Check Friends")));
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), 
			debugsettings(dlg), gtk_label_new_with_mnemonic(_("Debug")));

	jam_dialog_set_contents(GTK_DIALOG(dlg), nb);

	/* XXX HACK:  set_contents calls show_all, but the "command"
	 * settings widgets sometimes want to have hidden children.
	 * so we let them rehide. */
	{
		SettingsWidget *sw;
		for (sw = settingswidgets; sw->name; sw++) 
			if (sw->type == SW_COMMAND)
				command_changed_cb(GTK_OPTION_MENU(sw->widget), sw);
	}

	gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);
}

void
settings_cf_run(CFMgr *cfm) {
	GtkWidget *dlg;

	dlg = gtk_dialog_new_with_buttons(_("Checkfriends Preferences"),
			NULL /* no parent */, GTK_DIALOG_MODAL,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);

	jam_dialog_set_contents(GTK_DIALOG(dlg), cfriendssettings(cfm));

	gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);
}

void
settings_run(JamWin *jw) {
#ifdef HAVE_GTKSPELL
	gboolean hadspell    = conf.options.usespellcheck;
#endif
	gboolean hadautosave = conf.options.autosave;
	gboolean hadquotes   = conf.options.smartquotes;

	run_settings_dialog(jw);
	
	g_slist_foreach(app.secmgr_list, (GFunc)secmgr_security_set_force,
			&conf.defaultsecurity);

	cf_threshold_normalize(&conf.cfthreshold);

	if (!hadautosave && conf.options.autosave)
		jam_autosave_init(jw);
	if (hadautosave && !conf.options.autosave)
		jam_autosave_stop(jw);

	if (conf.options.allowmultipleinstances) {
		GError *err = NULL;
		if (!logjam_remote_stop_listening(app.remote, &err)) {
			jam_warning(GTK_WINDOW(jw),
					_("Error stopping remote listener: %s."), err->message);
			g_error_free(err);
		}
	}
	if (!conf.options.allowmultipleinstances) {
		GError *err = NULL;
		if (!logjam_remote_listen(app.remote, &err) && err) {
			jam_warning(GTK_WINDOW(jw),
					_("Error starting remote listener: %s."), err->message);
			g_error_free(err);
		}
	}

	if (conf.options.smartquotes && !hadquotes)
		smartquotes_attach(jam_doc_get_text_buffer(jw->doc));
	else if (!conf.options.smartquotes && hadquotes)
		smartquotes_detach(jam_doc_get_text_buffer(jw->doc));

	jam_view_settings_changed(jam_win_get_cur_view(jw));
}

static void
run_cfmask_settings_dlg(SettingsWidget *sw) {
	GtkWindow *window = GTK_WINDOW(gtk_widget_get_toplevel(sw->widget));
	JamAccountLJ *acc = sw->conf;
	jam_account_lj_set_cfmask(acc,
			custom_security_dlg_run(window, jam_account_lj_get_cfmask(acc), acc));
}

