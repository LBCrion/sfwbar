/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Sfwbar maintainers
 */

#include "../src/module.h"

MODULE_TRIGGER_DECL
gboolean invalid;

GMutex mutex;
struct in_addr gateway;
struct in_addr ip;
struct in_addr mask;
struct in_addr bcast;
guint32 rx_packets, tx_packets, rx_bytes, tx_bytes;
guint32 prx_packets, ptx_packets, prx_bytes, ptx_bytes;
gint64 last_time, time_diff;
gchar *interface;
gchar *essid;
gint qual, level, noise;

#ifdef __linux__

#include <unistd.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>

guint32 nlseq;

void net_update_ifaces ( void )
{
  struct ifaddrs *addrs, *iter;

  getifaddrs(&addrs);
  for(iter=addrs;iter;iter=iter->ifa_next)
    if(!g_strcmp0(interface,iter->ifa_name) && iter->ifa_addr)
      switch(iter->ifa_addr->sa_family)
      {
        case AF_INET:
          ip = ((struct sockaddr_in *)(iter->ifa_addr))->sin_addr;
          if(iter->ifa_netmask)
            mask = ((struct sockaddr_in *)(iter->ifa_netmask))->sin_addr;
          if(iter->ifa_broadaddr)
            bcast = ((struct sockaddr_in *)(iter->ifa_broadaddr))->sin_addr;
          break;
        case AF_PACKET:
            rx_packets =
              ((struct rtnl_link_stats *)(iter->ifa_data))->rx_packets;
            tx_packets =
              ((struct rtnl_link_stats *)(iter->ifa_data))->tx_packets;
            rx_bytes = ((struct rtnl_link_stats *)(iter->ifa_data))->rx_bytes;
            tx_bytes = ((struct rtnl_link_stats *)(iter->ifa_data))->tx_bytes;
          break;
        case AF_INET6:
          break;
    }
  freeifaddrs(addrs);
}

void net_update_traffic ( void )
{
  gint64 ctime;

  if(!invalid)
    return;

  prx_packets = rx_packets;
  ptx_packets = tx_packets;
  prx_bytes = rx_bytes;
  ptx_bytes = tx_bytes;
  net_update_ifaces();
  ctime = g_get_monotonic_time();
  time_diff = ctime - last_time;
  last_time = ctime;
}

void net_update_essid ( void )
{
  struct iwreq wreq;
  gint sock;
  gchar lessid[IW_ESSID_MAX_SIZE+1];

  if(interface)
  {
    *lessid=0;
    memset(&wreq,0,sizeof(wreq));
    wreq.u.essid.length = IW_ESSID_MAX_SIZE+1;
    wreq.u.essid.pointer = lessid;
    g_strlcpy(wreq.ifr_name,interface,sizeof(wreq.ifr_name));
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock >= 0 && ioctl(sock, SIOCGIWESSID, &wreq) >= 0)
    {
      g_mutex_lock(&mutex);
      g_free(essid);
      essid = g_strdup(lessid);
      g_mutex_unlock(&mutex);
    }
    if(sock >= 0)
      close(sock);
    return;
  }
  g_mutex_lock(&mutex);
  g_free(essid);
  essid = NULL;
  g_mutex_unlock(&mutex);
}

gdouble net_get_signal ( void )
{
  struct iw_statistics wstats;
  struct iwreq wreq;
  gint sock;

  if(!interface)
    return 0.0;

  memset(&wreq,0,sizeof(wreq));
  wreq.u.data.pointer = &wstats;
  wreq.u.data.length = sizeof(wstats);
  wreq.u.data.flags = 1;
  g_strlcpy(wreq.ifr_name,interface,sizeof(wreq.ifr_name));
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(sock >= 0 && ioctl(sock, SIOCGIWSTATS, &wreq) >= 0)
  {
    qual = wstats.qual.qual;
    level = wstats.qual.level-(wstats.qual.updated&IW_QUAL_DBM?0x100:0);
    noise = wstats.qual.noise-(wstats.qual.updated&IW_QUAL_DBM?0x100:0);
  }
  if(sock >= 0)
    close(sock);
  return CLAMP(2*(level+100),0,100);
}

void net_set_interface ( gint32 iidx, struct in_addr gate )
{
  gchar ifname[IF_NAMESIZE];

  if(iidx)
  {
    if_indextoname(iidx,ifname);
    g_mutex_lock(&mutex);
    g_free(interface);
    interface = g_strdup(ifname);
    gateway.s_addr = gate.s_addr;
    g_mutex_unlock(&mutex);
  }
  else
  {
    g_mutex_lock(&mutex);
    g_free(interface);
    interface = NULL;
    g_free(essid);
    essid = NULL;
    gateway.s_addr = 0;
    ip.s_addr = 0;
    gateway.s_addr = 0;
    mask.s_addr = 0;
    g_mutex_unlock(&mutex);
  }
  net_update_essid();
  net_update_ifaces();
  MODULE_TRIGGER_EMIT("network");
}

gint nl_connect ( void )
{
  gint sock;
  struct sockaddr_nl saddr;

  sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if(sock < 0)
    return sock;

  saddr.nl_family = AF_NETLINK;
  saddr.nl_pid = getpid();
  saddr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE;
  if(bind(sock,(struct sockaddr *)&saddr,sizeof(saddr)) >= 0)
    return sock;

  close(sock);
  return -1;
}

gint nl_request ( gint sock, guint16 type )
{
  struct {
    struct nlmsghdr hdr;
    struct rtmsg rtm;
  } nlreq;

  nlreq.hdr.nlmsg_type = type;
  nlreq.hdr.nlmsg_pid = getpid();
  nlreq.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  nlreq.hdr.nlmsg_len = sizeof(nlreq);
  nlreq.hdr.nlmsg_seq = nlseq++;
  nlreq.rtm.rtm_family = AF_INET;
  return send(sock,&nlreq,sizeof(nlreq),0);
}

gboolean nl_parse ( GIOChannel *chan, GIOCondition cond, gpointer data )
{
  struct nlmsghdr *hdr;
  struct rtattr *rta;
  gint sock;
  gint rtl;
  gchar buf[4096];
  gchar ifname[IF_NAMESIZE];
  gint len;
  gint32 iidx;
  struct in_addr dest, gate;

  sock = g_io_channel_unix_get_fd(chan);
  len = recv(sock,buf,sizeof(buf),0);
  hdr = (struct nlmsghdr *)buf;
  if(len<=0)
    return TRUE;

  for(;NLMSG_OK(hdr,len)&&hdr->nlmsg_type!=NLMSG_DONE;hdr=NLMSG_NEXT(hdr,len))
  {
    dest.s_addr = 0;
    gate.s_addr = 0;
    iidx = 0;
    switch(hdr->nlmsg_type)
    {
      case RTM_NEWROUTE:
      case RTM_DELROUTE:
        rta = RTM_RTA(NLMSG_DATA(hdr));
        rtl = RTM_PAYLOAD(hdr);
        for(;rtl && RTA_OK(rta,rtl);rta=RTA_NEXT(rta,rtl))
        {
          if(rta->rta_type==RTA_DST)
            dest = *(struct in_addr *)RTA_DATA(rta);
          if(rta->rta_type==RTA_GATEWAY)
            gate = *(struct in_addr *)RTA_DATA(rta);
          if(rta->rta_type==RTA_OIF)
            iidx  = *(guint32 *)RTA_DATA(rta);
        }
        if(hdr->nlmsg_type==RTM_NEWROUTE && !dest.s_addr && gate.s_addr &&
            iidx && !(((struct rtmsg *)NLMSG_DATA(hdr))->rtm_dst_len))
          net_set_interface(iidx, gate);
        if(hdr->nlmsg_type==RTM_DELROUTE && !dest.s_addr && iidx &&
            !(((struct rtmsg *)NLMSG_DATA(hdr))->rtm_dst_len))
          net_set_interface(0, gateway);
        break;
      case RTM_NEWLINK:
        rta = IFLA_RTA(NLMSG_DATA(hdr));
        rtl = IFLA_PAYLOAD(hdr);
        if(((struct ifinfomsg *)NLMSG_DATA(hdr))->ifi_change!=0)
          for(;rtl && RTA_OK(rta,rtl);rta=RTA_NEXT(rta,rtl))
            if(rta->rta_type==IFLA_WIRELESS)
              net_update_essid();
        break;
      case RTM_DELADDR:
        if(!g_strcmp0(interface,if_indextoname(
                ((struct ifinfomsg *)NLMSG_DATA(hdr))->ifi_index,ifname)))
          net_set_interface(0,gate);
        break;
    }
  }
  return TRUE;
}

void network_init ( GMainContext *gmc )
{
  int sock;
  GIOChannel *chan;

  g_mutex_init(&mutex);
  sock = nl_connect();
  if(sock >= 0 && nl_request(sock, RTM_GETROUTE) >= 0)
  {
    chan = g_io_channel_unix_new(sock);
    g_io_add_watch(chan,G_IO_IN | G_IO_PRI |G_IO_ERR | G_IO_HUP,nl_parse,NULL);
  }
  else if(sock >= 0)
    close(sock);
}
#else

void network_init ( void * )
{
}

#endif /* __linux__ */

void sfwbar_module_init ( GMainContext *gmc )
{
  network_init(gmc);
}

void sfwbar_module_invalidate ( void )
{
  invalid = TRUE;
}

void *sfwbar_expr_func ( void **params )
{
  gchar addr[INET_ADDRSTRLEN];
  gchar *result;

  if(!params || !params[0])
    return g_strdup("");

  g_mutex_lock(&mutex);
  if(!g_ascii_strcasecmp(params[0],"ip"))
    result = g_strdup(inet_ntop(AF_INET,&ip,addr,INET_ADDRSTRLEN));
  else if(!g_ascii_strcasecmp(params[0],"mask"))
    result = g_strdup(inet_ntop(AF_INET,&mask,addr,INET_ADDRSTRLEN));
  else if(!g_ascii_strcasecmp(params[0],"gateway"))
    result = g_strdup(inet_ntop(AF_INET,&gateway,addr,INET_ADDRSTRLEN));
  else if(!g_ascii_strcasecmp(params[0],"signal"))
    result = g_strdup_printf("%lf",net_get_signal());
  else if(!g_ascii_strcasecmp(params[0],"rxrate"))
  {
    net_update_traffic();
    result = g_strdup_printf("%lf",(gdouble)(rx_bytes - prx_bytes)/time_diff);
  }
  else if(!g_ascii_strcasecmp(params[0],"txrate"))
  {
    net_update_traffic();
    result = g_strdup_printf("%lf",(gdouble)(tx_bytes - ptx_bytes)/time_diff);
  }
  else if(!g_ascii_strcasecmp(params[0],"essid"))
    result = g_strdup(essid);
  else if(!g_ascii_strcasecmp(params[0],"interface"))
    result = g_strdup(interface);
  else
    result = g_strdup("invalid query");
  g_mutex_unlock(&mutex);

  return result;
}

gint64 sfwbar_module_signature = 0x73f4d956a1;

ModuleExpressionHandler handler1 = {
  .numeric = FALSE,
  .name = "NetInfo",
  .parameters = "Ss",
  .function = sfwbar_expr_func
};

ModuleExpressionHandler handler2 = {
  .numeric = TRUE,
  .name = "TestNum",
  .parameters = "Ss",
  .function = sfwbar_expr_func
};

ModuleExpressionHandler *sfwbar_expression_handlers[] = {
  &handler1,
  &handler2,
  NULL
};
