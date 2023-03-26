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

ModuleApiV1 *sfwbar_module_api;
gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 1;
typedef gchar *(*SysctlParseFunc)( gchar *, void *, size_t );

typedef struct {
  gint *oid;
  gsize len;
  guint type;
  SysctlParseFunc func;
} SysctlVar;

static gchar *sysctl_clockinfo ( gchar *old, void *buf, size_t len )
{
  struct clockinfo *ci;

  if(len != sizeof(struct clockinfo))
    return g_strdup("clockinfo: invalid data");

  ci = buf;

  return g_strdup_printf("%s { hz = %d, tick = %d, profhz = %d, stathz = %d }",
      old, ci->hz, ci->tick, ci->profhz, ci->stathz);
}

static gchar *sysctl_timeval ( gchar *old, void *buf, size_t len )
{
  struct timeval *tv;

  if(len != sizeof(struct timeval))
    return g_strdup("timeval: invalid data");

  tv = buf;

  return g_strdup_printf("%s { sec = %jd, usec = %lu } %s",
      old, tv->tv_sec, tv->tv_usec, ctime(&(tv->tv_sec)));
}

static gchar *sysctl_loadavg ( gchar *old, void *buf, size_t len )
{
  struct loadavg *la;

  if(len != sizeof(struct loadavg))
    return g_strdup("loadavg: invalid data");

  la = buf;

  return g_strdup_printf("%s { %.2f %.2f %.2f }",old,
      (double)la->ldavg[0]/(double)la->fscale,
      (double)la->ldavg[1]/(double)la->fscale,
      (double)la->ldavg[2]/(double)la->fscale);
}

static gchar *sysctl_vmtotal ( gchar *old, void *buf, size_t len )
{
  struct vmtotal *vm;
  int kpp;

  if(len != sizeof(struct vmtotal))
    return g_strdup("vmtotal: invalid data");

  vm = buf;
  kpp = getpagesize()/1024;

  return g_strdup_printf("%s\nProcesses:\t\t"
      "(RUNQ: %hd Disk Wait: %hd Page Wait: %hd Sleep: %hd)\n"
      "Virtual Memory:\t\t(Total %ldK Active %ldK)\n"
      "Real Memory:\t\t(Total %ldK Active %ldK)\n"
      "Shared Virtual Memory:\t(Total %ldK Active %ldK)\n"
      "Shared Real Memory:\t(Total %ldK Active %ldK)\n"
      "Free Memory:\t%ldK\n",old,
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
  void *ptr;
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
    tmp = res;
    switch(var->type)
    {
      case CTLTYPE_STRING:
        res = g_strconcat(tmp," ",ptr,NULL);
        ilen = nlen;
        break;
      case CTLTYPE_INT:
        res = g_strdup_printf("%s %d",tmp,*(int *)ptr);
        ilen=sizeof(int);
        break;
      case CTLTYPE_UINT:
        res = g_strdup_printf("%s %u",tmp,*(u_int *)ptr);
        ilen=sizeof(u_int);
        break;
      case CTLTYPE_LONG:
        res = g_strdup_printf("%s %ld",tmp,*(long *)ptr);
        ilen=sizeof(long);
        break;
      case CTLTYPE_ULONG:
        res = g_strdup_printf("%s %lu",tmp,*(u_long *)ptr);
        ilen=sizeof(u_long);
        break;
      case CTLTYPE_S8:
        res = g_strdup_printf("%s %d",tmp,*(int8_t *)ptr);
        ilen=sizeof(int8_t);
        break;
      case CTLTYPE_U8:
        res = g_strdup_printf("%s %u",tmp,*(uint8_t *)ptr);
        ilen=sizeof(uint8_t);
        break;
      case CTLTYPE_S16:
        res = g_strdup_printf("%s %d",tmp,*(int16_t *)ptr);
        ilen=sizeof(int16_t);
        break;
      case CTLTYPE_U16:
        res = g_strdup_printf("%s %u",tmp,*(uint16_t *)ptr);
        ilen=sizeof(uint16_t);
        break;
      case CTLTYPE_S32:
        res = g_strdup_printf("%s %d",tmp,*(int32_t *)ptr);
        ilen=sizeof(int32_t);
        break;
      case CTLTYPE_U32:
        res = g_strdup_printf("%s %u",tmp,*(uint32_t *)ptr);
        ilen=sizeof(uint32_t);
        break;
      case CTLTYPE_S64:
        res = g_strdup_printf("%s %ld",tmp,*(int64_t *)ptr);
        ilen=sizeof(int64_t);
        break;
      case CTLTYPE_U64:
        res = g_strdup_printf("%s %lu",tmp,*(uint64_t *)ptr);
        ilen=sizeof(uint64_t);
        break;
      case CTLTYPE_OPAQUE:
        if(var->func)
          res = g_strconcat(tmp," ",var->func(tmp,buf,nlen),NULL);
        ilen=nlen;
        break;
    default:
        res = g_strconcat(tmp," [unknown]",NULL);
        ilen=nlen;
        break;
    }
    if(tmp!=res)
      g_free(tmp);
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

void *bsdctl_func ( void **params )
{
  SysctlVar *var;

  if(!params || !params[0])
    return g_strdup("");

  var = sysctl_var_get(params[0]);
  return sysctl_query(var);
}

ModuleExpressionHandlerV1 handler1 = {
  .flags = 0,
  .name = "BSDCtl",
  .parameters = "S",
  .function = bsdctl_func
};

ModuleExpressionHandlerV1 *sfwbar_expression_handlers[] = {
  &handler1,
  NULL
};
