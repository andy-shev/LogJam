/*
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef html_markup_h
#define html_markup_h

#include "jamdoc.h"

void html_mark_bold        (JamDoc *doc);
void html_mark_italic      (JamDoc *doc);
void html_mark_em          (JamDoc *doc);
void html_mark_underline   (JamDoc *doc);
void html_mark_strikeout   (JamDoc *doc);
void html_mark_monospaced  (JamDoc *doc);
void html_mark_para        (JamDoc *doc);
void html_mark_smallcaps   (JamDoc *doc);
void html_mark_blockquote  (JamDoc *doc);
void html_mark_big         (JamDoc *doc);
void html_mark_small       (JamDoc *doc);
void html_mark_superscript (JamDoc *doc);
void html_mark_subscript   (JamDoc *doc);
void html_mark_ulist       (JamDoc *doc);
void html_mark_olist       (JamDoc *doc);
void html_mark_listitem    (JamDoc *doc);
void html_mark_h1          (JamDoc *doc);
void html_mark_h2          (JamDoc *doc);
void html_mark_h3          (JamDoc *doc);
void html_mark_h4          (JamDoc *doc);

#endif /* html_markup_h */
