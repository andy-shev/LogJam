
#ifndef __logjam_marshal_MARSHAL_H__
#define __logjam_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* BOOLEAN:STRING,POINTER (marshalers.list:1) */
extern void logjam_marshal_BOOLEAN__STRING_POINTER (GClosure     *closure,
                                                    GValue       *return_value,
                                                    guint         n_param_values,
                                                    const GValue *param_values,
                                                    gpointer      invocation_hint,
                                                    gpointer      marshal_data);

/* VOID:INT,BOOLEAN (marshalers.list:2) */
extern void logjam_marshal_VOID__INT_BOOLEAN (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

G_END_DECLS

#endif /* __logjam_marshal_MARSHAL_H__ */

