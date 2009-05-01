/*
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef html_markup_h
#define html_markup_h

#include "jamdoc.h"

void html_mark_bold      (JamDoc *doc);
void html_mark_italic    (JamDoc *doc);
void html_mark_underline (JamDoc *doc);
void html_mark_strikeout (JamDoc *doc);
void html_mark_monospaced(JamDoc *doc);

#endif /* html_markup_h */
