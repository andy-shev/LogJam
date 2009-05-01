/* logjam - a GTK client for LiveJournal.
 * Copyright (C) 2000-2004 Evan Martin <martine@danga.com>
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __LOGJAM_POLL_H__
#define __LOGJAM_POLL_H__

typedef enum {
	PQ_RADIO = 0, /* first three  */
	PQ_COMBO,	  /* must be      */
	PQ_CHECK,	  /* in this order*/
	PQ_TEXT,
	PQ_SCALE
} PQType;

typedef struct {
	PQType type;
	char *question;
} PollQuestion;

typedef struct {
	PollQuestion question;
	GSList *answers;
} PollQuestionMulti;

typedef struct {
	PollQuestion question;
	int size, width;
} PollQuestionText;

typedef struct {
	PollQuestion question;
	int from, to, by;
} PollQuestionScale;

typedef enum {
	PSEC_ALL,
	PSEC_FRIENDS,
	PSEC_NONE
} PollSecurity;

typedef struct {
	char *name;
	PollSecurity viewers, voters;
	GSList *questions;
} Poll;

PollQuestion* pollmultidlg_run(GtkWindow *parent, PollQuestionMulti *pqm);
PollQuestion* polltextdlg_run(GtkWindow *parent, PollQuestionText *pqt);
PollQuestion* pollscaledlg_run(GtkWindow *parent, PollQuestionScale *pqs);

void poll_question_free(PollQuestion *pq);

#endif /* __LOGJAM_POLL_H__ */
