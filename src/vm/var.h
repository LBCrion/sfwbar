#ifndef __VAR_H__
#define __VAR_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define VAR_TYPE ivm_var_get_type()
G_DECLARE_DERIVABLE_TYPE (VmVar, vm_var, VM, VAR, GObject)

struct _VmVarClass {
  GObjectClass parent_class;
};

struct _VmVarPrivate {
  value_t val;
};

G_END_DECLS

#endif
