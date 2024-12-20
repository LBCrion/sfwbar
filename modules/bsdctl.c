/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2023- Sfwbar maintainers
 */

#include "../src/module.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/vmmeter.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <glib.h>

gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = MODULE_API_VERSION;
typedef gchar *(*SysctlParseFunc)( void *, size_t );

typedef struct {
  gint *oid;
  gsize len;
  guint type;
  SysctlParseFunc func;
} SysctlVar;

static gchar *sysctl_clockinfo ( void *buf, size_t len )
{
  struct clockinfo *ci;

  if(len != sizeof(struct clockinfo))
    return g_strdup("clockinfo: invalid data");

  ci = buf;

  return g_strdup_printf("{ hz = %d, tick = %d, profhz = %d, stathz = %d }",
      ci->hz, ci->tick, ci->profhz, ci->stathz);
}

static gchar *sysctl_timeval ( void *buf, size_t len )
{
  struct timeval *tv;

  if(len != sizeof(struct timeval))
    return g_strdup("timeval: invalid data");

  tv = buf;

  return g_strdup_printf("{ sec = %jd, usec = %lu } %s",
      tv->tv_sec, tv->tv_usec, ctime(&(tv->tv_sec)));
}

static gchar *sysctl_loadavg ( void *buf, size_t len )
{
  struct loadavg *la;

  if(len != sizeof(struct loadavg))
    return g_strdup("loadavg: invalid data");

  la = buf;

  return g_strdup_printf("{ %.2f %.2f %.2f }",
      (double)la->ldavg[0]/(double)la->fscale,
      (double)la->ldavg[1]/(double)la->fscale,
      (double)la->ldavg[2]/(double)la->fscale);
}

static gchar *sysctl_vmtotal ( void *buf, size_t len )
{
  struct vmtotal *vm;
  int kpp;

  if(len != sizeof(struct vmtotal))
    return g_strdup("vmtotal: invalid data");

  vm = buf;
  kpp = getpagesize()/1024;

  return g_strdup_printf("Processes:\t\t"
      "(RUNQ: %hd Disk Wait: %hd Page Wait: %hd Sleep: %hd)\n"
      "Virtual Memory:\t\t(Total %ldK Active %ldK)\n"
      "Real Memory:\t\t(Total %ldK Active %ldK)\n"
      "Shared Virtual Memory:\t(Total %ldK Active %ldK)\n"
      "Shared Real Memory:\t(Total %ldK Active %ldK)\n"
      "Free Memory:\t%ldK\n",
      vm->t_rq,vm->t_dw,vm->t_pw,vm->t_sl,
      vm->t_vm * kpp, vm->t_avm * kpp, vm->t_rm * kpp, vm->t_arm * kpp,
      vm->t_vmshr * kpp, vm->t_avmshr * kpp,
      vm->t_rmshr * kpp, vm->t_armshr * kpp, vm->t_free * kpp);
}

static SysctlVar *sysctl_var_new ( const gchar *name )
{
  int mib[CTL_MAXNAME];
  int qoid[CTL_MAXNAME+2];
  guchar buf[BUFSIZ];
  size_t len = CTL_MAXNAME;
  size_t qlen = BUFSIZ;
  SysctlVar *var;

  var = g_malloc0(sizeof(SysctlVar));

  if(sysctlnametomib(name,mib,&len)<0)
    return var;
  qoid[0] = 0;
  qoid[1] = 4;
  memcpy(qoid+2,mib,len*sizeof(int));
  if(sysctl(qoid,len+2,buf,&qlen,0,0)<0)
    return var;

  var->len = len;
  var->oid = g_memdup2(mib,len*sizeof(int));
  var->type = *(u_int *)buf & CTLTYPE;

  if(var->type == CTLTYPE_OPAQUE)
  {
    var->func = NULL;
    if(!g_strcmp0((char *)buf+sizeof(u_int),"S,clockinfo"))
      var->func = sysctl_clockinfo;
    else if(!g_strcmp0((char *)buf+sizeof(u_int),"S,timeval"))
      var->func = sysctl_timeval;
    else if(!g_strcmp0((char *)buf+sizeof(u_int),"S,loadavg"))
      var->func = sysctl_loadavg;
    else if(!g_strcmp0((char *)buf+sizeof(u_int),"S,vmtotal"))
      var->func = sysctl_vmtotal;
  }

  return var;
}

static gchar *sysctl_query ( SysctlVar *var )
{
  u_char buf[1024];
  void *ptr,*vptr;
  size_t ilen, nlen = sizeof(buf);
  gchar *res, *tmp;

  if(!var || !var->oid)
    return g_strdup("Sysctl invalid variable");

  if(sysctl(var->oid,var->len,buf,&nlen,0,0)<0)
    return g_strdup("Unsuccessful sysctl call");
  ptr = buf;

  res = g_strdup("");

  while((int)nlen>0)
  {
    vptr = NULL;
    switch(var->type)
    {
#ifdef CTLTYPE_STRING
      case CTLTYPE_STRING:
        vptr = g_strndup(ptr,nlen);
        ilen = nlen;
        break;
#endif
#ifdef CTLTYPE_INT
      case CTLTYPE_INT:
        if(nlen>=sizeof(int))
          vptr = g_strdup_printf("%d",*(int *)ptr);
        ilen=sizeof(int);
        break;
#endif
#ifdef CTLTYPE_UINT
      case CTLTYPE_UINT:
        if(nlen>=sizeof(u_int))
          vptr = g_strdup_printf("%u",*(u_int *)ptr);
        ilen=sizeof(u_int);
        break;
#endif
#ifdef CTLTYPE_LONG
      case CTLTYPE_LONG:
        if(nlen>=sizeof(long))
          vptr = g_strdup_printf("%ld",*(long *)ptr);
        ilen=sizeof(long);
        break;
#endif
#ifdef CTLTYPE_ULONG
      case CTLTYPE_ULONG:
        if(nlen>=sizeof(u_long))
          vptr = g_strdup_printf("%lu",*(u_long *)ptr);
        ilen=sizeof(u_long);
        break;
#endif
#ifdef CTLTYPE_S8
      case CTLTYPE_S8:
        if(nlen>=sizeof(int8_t))
          vptr = g_strdup_printf("%d",*(int8_t *)ptr);
        ilen=sizeof(int8_t);
        break;
#endif
#ifdef CTLTYPE_U8
      case CTLTYPE_U8:
        if(nlen>=sizeof(uint8_t))
          vptr = g_strdup_printf("%u",*(uint8_t *)ptr);
        ilen=sizeof(uint8_t);
        break;
#endif
#ifdef CTLTYPE_S16
      case CTLTYPE_S16:
        if(nlen>=sizeof(int16_t))
          vptr = g_strdup_printf("%d",*(int16_t *)ptr);
        ilen=sizeof(int16_t);
        break;
#endif
#ifdef CTLTYPE_U16
      case CTLTYPE_U16:
        if(nlen>=sizeof(uint16_t))
          vptr = g_strdup_printf("%u",*(uint16_t *)ptr);
        ilen=sizeof(uint16_t);
        break;
#endif
#ifdef CTLTYPE_S32
      case CTLTYPE_S32:
        if(nlen>=sizeof(int32_t))
          vptr = g_strdup_printf("%d",*(int32_t *)ptr);
        ilen=sizeof(int32_t);
        break;
#endif
#ifdef CTLTYPE_U32
      case CTLTYPE_U32:
        if(nlen>=sizeof(uint32_t))
          vptr = g_strdup_printf("%u",*(uint32_t *)ptr);
        ilen=sizeof(uint32_t);
        break;
#endif
#ifdef CTLTYPE_S64
      case CTLTYPE_S64:
        if(nlen>=sizeof(int64_t))
          vptr = g_strdup_printf("%ld",*(int64_t *)ptr);
        ilen=sizeof(int64_t);
        break;
#endif
#ifdef CTLTYPE_U64
      case CTLTYPE_U64:
        if(nlen>=sizeof(uint64_t))
          vptr = g_strdup_printf("%lu",*(uint64_t *)ptr);
        ilen=sizeof(uint64_t);
        break;
#endif
#ifdef CTLTYPE_OPAQUE
      case CTLTYPE_OPAQUE:
        if(var->func)
          vptr = var->func(buf,nlen);
        ilen=nlen;
        break;
#endif
    default:
        vptr = g_strdup("[unknown]");
        ilen=nlen;
        break;
    }
    if(vptr)
    {
      tmp = g_strconcat(res,*res?" ":"",vptr,NULL);
      g_free(vptr);
      g_free(res);
      res = tmp;
    }
    nlen-=ilen;
    ptr+=ilen;
  }

  return res;
}

static SysctlVar *sysctl_var_get ( gchar *name )
{
  static GHashTable *index;
  SysctlVar *var;

  if(!index)
    index = g_hash_table_new(g_str_hash, g_str_equal);
  var = g_hash_table_lookup(index, name);
  if(!var)
  {
    var = sysctl_var_new(name);
    g_hash_table_insert(index,name,var);
  }
  return var;
}

static value_t bsdctl_func ( vm_t *vm, value_t p[], gint np )
{
  SysctlVar *var;

  vm_param_check_np(vm, np, 1, "BSDCtl");
  vm_param_check_string(vm, p, 0, "BSDCtl");

  var = sysctl_var_get(value_get_string(p[0]));
  return value_new_string(sysctl_query(var));
}

gboolean sfwbar_module_init ( void )
{
  vm_func_add("BSDCtl", bsdctl_func, FALSE);
  return TRUE;
}
