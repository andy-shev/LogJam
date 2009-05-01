#ifndef __DASHBOARD_FRONTEND_H__
#define __DASHBOARD_FRONTEND_H__

int dashboard_connect_with_timeout (int *fd, long timeout_usecs);
void dashboard_send_raw_cluepacket (const char *rawcluepacket);
void dashboard_send_raw_cluepacket_sync (const char *rawcluepacket);
char * dashboard_xml_quote (const char *str);
char * dashboard_build_clue (const char *text, const char *type, int relevance);
char * dashboard_build_cluepacket_from_cluelist (const char *frontend, gboolean focused, const char *context, GList *clues);
char * dashboard_build_cluepacket_v (const char *frontend, gboolean focused, const char *context, va_list args);
char * dashboard_build_cluepacket (const char *frontend, gboolean focused, const char *context, ...);
char * dashboard_build_cluepacket_then_free_clues (const char *frontend, gboolean focused, const char *context, ...);

#endif /* __DASHBOARD_FRONTEND_H__ */
