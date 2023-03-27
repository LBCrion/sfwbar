/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Sfwbar maintainers
 */

#include "../src/module.h"
#include <unistd.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

typedef struct _iface_info {
  gchar *name;
  GMutex mutex;
  gboolean invalid;
  struct in_addr ip, mask, bcast, gateway;
  struct in6_addr ip6, mask6, bcast6, gateway6;
  guint32 rx_packets, tx_packets, rx_bytes, tx_bytes;
  guint32 prx_packets, ptx_packets, prx_bytes, ptx_bytes;
  gint64 last_time, time_diff;
  gchar *essid;
} iface_info;

iface_info *route;

ModuleApiV1 *sfwbar_module_api;
gint64 sfwbar_module_signature = 0x73f4d956a1;
guint16 sfwbar_module_version = 1;
guint32 seq;
GList *iface_list;
gint qual, level, noise;

static void net_update_essid ( gchar * );

static iface_info *net_iface_from_name ( gchar *name, gboolean create )
{
  GList *iter;
  iface_info *iface;

  for(iter = iface_list;iter;iter=g_list_next(iter))
    if(!g_strcmp0(((iface_info *)iter->data)->name,name))
      return iter->data;

  if(!create)
    return NULL;

  iface = g_malloc0(sizeof(iface_info));
  g_mutex_init(&iface->mutex);
  iface->name = g_strdup(name);
  iface_list = g_list_prepend(iface_list,iface);
  return iface;
}

static gchar *net_get_cidr ( guint32 addr )
{
  gint i;
  guint32 m;

  m = g_ntohl(addr);
  for(i=31;(i>=0)&&(m&0x1<<i);i--);

  return g_strdup_printf("%d",31-i);
}

static void net_update_ifaddrs ( void )
{
  struct ifaddrs *addrs, *iter;
  iface_info *iface;

  getifaddrs(&addrs);
  for(iter=addrs;iter;iter=iter->ifa_next)
  {
    iface = net_iface_from_name(iter->ifa_name,TRUE);
    if(iter->ifa_addr)
      switch(iter->ifa_addr->sa_family)
      {
        case AF_INET:
          iface->ip = ((struct sockaddr_in *)(iter->ifa_addr))->sin_addr;
          if(iter->ifa_netmask)
            iface->mask =
              ((struct sockaddr_in *)(iter->ifa_netmask))->sin_addr;
          if(iter->ifa_broadaddr)
            iface->bcast =
              ((struct sockaddr_in *)(iter->ifa_broadaddr))->sin_addr;
          break;
        case AF_INET6:
          iface->ip6 = ((struct sockaddr_in6 *)(iter->ifa_addr))->sin6_addr;
          if(iter->ifa_netmask)
            iface->mask6 =
              ((struct sockaddr_in6 *)(iter->ifa_netmask))->sin6_addr;
          if(iter->ifa_broadaddr)
            iface->bcast6 =
              ((struct sockaddr_in6 *)(iter->ifa_broadaddr))->sin6_addr;
          break;
      }
  }
  freeifaddrs(addrs);
}

static void net_set_interface ( gint32 iidx, struct in_addr gate,
    struct in6_addr gate6 )
{
  gchar ifname[IF_NAMESIZE];
  iface_info *iface;

  if(!iidx)
  {
    if(route)
    {
      route->gateway.s_addr = 0;
      memset(&route->gateway6,0,sizeof(route->gateway6));
      route = NULL;
      MODULE_TRIGGER_EMIT("network");
    }
    return;
  }

  iface = net_iface_from_name(if_indextoname(iidx,ifname),TRUE);
  g_mutex_lock(&iface->mutex);
  g_free(iface->name);
  iface->name = g_strdup(ifname);
  iface->gateway = gate;
  iface->gateway6 = gate6;
  g_mutex_unlock(&iface->mutex);
  net_update_essid(ifname);
  net_update_ifaddrs();
  route = iface;
  MODULE_TRIGGER_EMIT("network");
}

static gchar *net_getaddr ( void *ina, gint type  )
{
  gchar addr[INET6_ADDRSTRLEN];

  return g_strdup(inet_ntop(type,ina,addr,INET_ADDRSTRLEN));
}

#if defined(__linux__)

#include <linux/rtnetlink.h>
#include <linux/wireless.h>

static void net_update_traffic ( gchar *interface )
{
  gint64 ctime;
  struct ifaddrs *addrs, *iter;
  struct rtnl_link_stats *stats;
  iface_info *iface;

  iface = net_iface_from_name(interface,FALSE);
  if(!iface || !iface->invalid)
    return;

  getifaddrs(&addrs);
  for(iter=addrs;iter;iter=iter->ifa_next)
  {
    if(!g_strcmp0(interface,iter->ifa_name) &&
        iter->ifa_addr->sa_family == AF_PACKET)
    {
      iface->prx_packets = iface->rx_packets;
      iface->ptx_packets = iface->tx_packets;
      iface->prx_bytes = iface->rx_bytes;
      iface->ptx_bytes = iface->tx_bytes;

      stats = ((struct rtnl_link_stats *)(iter->ifa_data));
      iface->rx_packets = stats->rx_packets;
      iface->tx_packets = stats->tx_packets;
      iface->rx_bytes = stats->rx_bytes;
      iface->tx_bytes = stats->tx_bytes;

      ctime = g_get_monotonic_time();
      iface->time_diff = ctime - iface->last_time;
      iface->last_time = ctime;
      iface->invalid = FALSE;
    }
  }
  freeifaddrs(addrs);
}

static void net_update_essid ( gchar *interface )
{
  struct iwreq wreq;
  gint sock;
  gchar lessid[IW_ESSID_MAX_SIZE+1];
  iface_info *iface;

  if(!interface)
    return;

  iface = net_iface_from_name(interface,FALSE);
  if(!iface)
    return;

  *lessid=0;
  memset(&wreq,0,sizeof(wreq));
  wreq.u.essid.length = IW_ESSID_MAX_SIZE+1;
  wreq.u.essid.pointer = lessid;
  (void)g_strlcpy(wreq.ifr_name,interface,sizeof(wreq.ifr_name));
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(sock >= 0 && ioctl(sock, SIOCGIWESSID, &wreq) >= 0)
  {
    g_mutex_lock(&iface->mutex);
    g_free(iface->essid);
    iface->essid = g_strdup(lessid);
    g_mutex_unlock(&iface->mutex);
  }
  if(sock >= 0)
    close(sock);
}

static gdouble net_get_signal ( gchar *interface )
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
  (void)g_strlcpy(wreq.ifr_name,interface,sizeof(wreq.ifr_name));
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

static gint net_rt_connect ( void )
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

static gint net_rt_request ( gint sock )
{
  struct {
    struct nlmsghdr hdr;
    struct rtmsg rtm;
  } nlreq;

  memset(&nlreq,0,sizeof(nlreq));
  nlreq.hdr.nlmsg_type = RTM_GETROUTE;
  nlreq.hdr.nlmsg_pid = getpid();
  nlreq.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  nlreq.hdr.nlmsg_len = sizeof(nlreq);
  nlreq.hdr.nlmsg_seq = seq++;
  nlreq.rtm.rtm_family = AF_INET;
  return send(sock,&nlreq,sizeof(nlreq),0);
}

static gboolean net_rt_parse (GIOChannel *chan, GIOCondition cond, gpointer d)
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
  struct in6_addr gate6;
  struct ifinfomsg *ifmsg;

  sock = g_io_channel_unix_get_fd(chan);
  len = recv(sock,buf,sizeof(buf),0);
  hdr = (struct nlmsghdr *)buf;
  if(len<=0)
    return TRUE;

  for(;NLMSG_OK(hdr,len)&&hdr->nlmsg_type!=NLMSG_DONE;hdr=NLMSG_NEXT(hdr,len))
  {
    dest.s_addr = 0;
    gate.s_addr = 0;
    memset(&gate6,0,sizeof(gate6));
    iidx = 0;
    if(hdr->nlmsg_type == RTM_DELADDR && route &&
        !g_strcmp0(route->name,if_indextoname(
            ((struct ifinfomsg *)NLMSG_DATA(hdr))->ifi_index,ifname)))
    {
      net_set_interface(0,gate,gate6);
      break;
    }
    else if(hdr->nlmsg_type == RTM_NEWLINK)
    {
      rta = IFLA_RTA(NLMSG_DATA(hdr));
      rtl = IFLA_PAYLOAD(hdr);
      ifmsg = (struct ifinfomsg *)NLMSG_DATA(hdr);
      if(ifmsg->ifi_change!=0)
        for(;rtl && RTA_OK(rta,rtl);rta=RTA_NEXT(rta,rtl))
          if(rta->rta_type==IFLA_WIRELESS)
            net_update_essid(if_indextoname(ifmsg->ifi_index,ifname));
      break;
    }
    else if(hdr->nlmsg_type == RTM_NEWROUTE || hdr->nlmsg_type == RTM_DELROUTE)
    {
      rta = RTM_RTA(NLMSG_DATA(hdr));
      rtl = RTM_PAYLOAD(hdr);
      for(;rtl && RTA_OK(rta,rtl);rta=RTA_NEXT(rta,rtl))
        switch(rta->rta_type)
        {
          case RTA_DST:
            if(((struct rtmsg *)NLMSG_DATA(hdr))->rtm_family==AF_INET)
              dest = *(struct in_addr *)RTA_DATA(rta);
            break;
          case RTA_GATEWAY:
            if(((struct rtmsg *)NLMSG_DATA(hdr))->rtm_family==AF_INET)
              gate = *(struct in_addr *)RTA_DATA(rta);
            else if(((struct rtmsg *)NLMSG_DATA(hdr))->rtm_family==AF_INET6)
              gate6 = *(struct in6_addr *)RTA_DATA(rta);
            break;
          case RTA_OIF:
            iidx  = *(guint32 *)RTA_DATA(rta);
            break;
        }
      if(hdr->nlmsg_type==RTM_NEWROUTE && !dest.s_addr && gate.s_addr &&
          iidx && !(((struct rtmsg *)NLMSG_DATA(hdr))->rtm_dst_len))
        net_set_interface(iidx, gate,gate6);
      else if(hdr->nlmsg_type==RTM_DELROUTE && !dest.s_addr && iidx &&
          !(((struct rtmsg *)NLMSG_DATA(hdr))->rtm_dst_len))
        net_set_interface(0, gate,gate6);
    }
  }
  return TRUE;
}

#elif defined(__FreeBSD__) || defined(__OpenBSD__)

#include <net/if_dl.h>
#include <net/route.h>

static void net_update_traffic ( gchar *interface )
{
  gint64 ctime;
  struct ifaddrs *addrs, *iter;
  iface_info *iface;

  iface = net_iface_from_name(interface,FALSE);
  if(!iface || !iface->invalid)
    return;

  getifaddrs(&addrs);
  for(iter=addrs;iter;iter=iter->ifa_next)
  {
    if(!g_strcmp0(interface,iter->ifa_name) && iter->ifa_addr &&
        iter->ifa_addr->sa_family==AF_LINK)
    {
      iface->prx_packets = iface->rx_packets;
      iface->ptx_packets = iface->tx_packets;
      iface->prx_bytes = iface->rx_bytes;
      iface->ptx_bytes = iface->tx_bytes;

      iface->rx_packets = ((struct if_data *)(iter->ifa_data))->ifi_ipackets;
      iface->tx_packets = ((struct if_data *)(iter->ifa_data))->ifi_opackets;
      iface->rx_bytes = ((struct if_data *)(iter->ifa_data))->ifi_ibytes;
      iface->tx_bytes = ((struct if_data *)(iter->ifa_data))->ifi_obytes;

      ctime = g_get_monotonic_time();
      iface->time_diff = ctime - iface->last_time;
      iface->last_time = ctime;
      iface->invalid = FALSE;
    }
  }
  freeifaddrs(addrs);
}

#if defined(__FreeBSD__)
#include <net80211/ieee80211_ioctl.h>

static void net_update_essid ( char *interface )
{
  struct ieee80211req req;
  iface_info *iface;
  gchar lessid[IEEE80211_NWID_LEN+1];
  gint sock;

  iface = net_iface_from_name(interface,FALSE);
  if(!iface)
    return;

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(sock<0)
    return;

  memset(&req,0,sizeof(req));
  req.i_type = IEEE80211_IOC_SSID;
  req.i_data = lessid;
  req.i_len = sizeof(lessid);
  (void)g_strlcpy(req.i_name,interface,sizeof(req.i_name));
  if(ioctl(sock,SIOCG80211,&req) >=0)
  {
    lessid[MIN(req.i_len,IEEE80211_NWID_LEN)]='\0';
    g_mutex_lock(&iface->mutex);
    g_free(iface->essid);
    iface->essid = g_strdup(lessid);
    g_mutex_unlock(&iface->mutex);
  }
  else
  {
    g_mutex_lock(&iface->mutex);
    g_free(iface->essid);
    iface->essid = NULL;
    g_mutex_unlock(&iface->mutex);
  }
  close(sock);
}

static gdouble net_get_signal ( gchar *interface )
{
  struct ieee80211req req;
  union {
    struct ieee80211req_sta_req sreq;
    gchar buf[24*1024];
  } nfo;
  gchar bssid[IEEE80211_ADDR_LEN];
  gint sock;
  gint rssi;

  if(!interface)
    return 0.0;

  memset(&req,0,sizeof(req));
  req.i_type = IEEE80211_IOC_BSSID;
  req.i_data = bssid;
  req.i_len = IEEE80211_ADDR_LEN;
  (void)g_strlcpy(req.i_name,interface,sizeof(req.i_name));
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(sock<0)
    return 0;
  if(ioctl(sock,SIOCG80211,&req) < 0)
  {
    close(sock);
    return 0;
  }
  memset(&nfo,0,sizeof(nfo));
  memcpy(nfo.sreq.is_u.macaddr,bssid,sizeof(bssid));
  memset(&req,0,sizeof(req));
  req.i_type = IEEE80211_IOC_STA_INFO;
  req.i_data = &nfo;
  req.i_len = sizeof(nfo);
  (void)g_strlcpy(req.i_name,interface,sizeof(req.i_name));
  if(ioctl(sock,SIOCG80211,&req) >=0)
    rssi = nfo.sreq.info[0].isi_noise+nfo.sreq.info[0].isi_rssi/2;
  else
    rssi = -100;
  close(sock);
  return CLAMP(2*(rssi+100),0,100);
}

#else  /* OpenBSD */

#include <net/if_media.h>
#include <net80211/ieee80211.h>
#include <sys/select.h>
#include <net80211/ieee80211.h>
#include <stdlib.h>
#include <sys/types.h>

static gdouble net_get_signal ( gchar *interface )
{
  gint sock;
  struct ieee80211_bssid bssid;
  struct ieee80211_nodereq req;

  sock = socket(AF_INET,SOCK_DGRAM,0);
  if(sock <0)
    return;
  memset(&bssid,0,sizeof(bssid));
  (void)g_strlcpy(bssid.i_name,interface,sizeof(bssid.i_name));
  if(ioctl(sock,SIOCG80211BSSID,&bssd) < 0 || !*bssid.i_bssid ||
      !memcmp(bssid.i_bssid,bssid.i_bssid+1,IEEE80211_ADDR_LEN-1))
  {
    close(sock);
    return;
  }
  memset(&req,0,sizeof(req));
  (void)g_strlcpy(req.nr_ifname,interface,sizeof(req.nr_ifname));
  memcpy(&(req.nr_macaddr),bssid.i_bssid,sizeof(req.nr_macaddr));
  if(ioctl(sock,SIOCG80211NODE,&req) >= 0)
  {
    if(g_mutex_trylock(&mutex))
    {
      g_free(essid);
      essid = g_strdup(req.nr_nwid);
      g_mutex_unlock(&mutex);
    }
    if(req.nr_max_nssi)
      level = IEEE80211_NODEREQ_RSSI(&req);
    else
      level = CLAMP(2*(req.nr_rssi+100),0,100);
  }
  close(sock);
  return level;
}

static void net_update_essid ( gchar *interface )
{
  (void)net_get_signal(interface);
}

#endif

static gint net_rt_connect ( void )
{
  return socket(AF_ROUTE, SOCK_RAW,0);
}

static gboolean net_rt_request ( gint sock )
{
  struct {
    struct rt_msghdr hdr;
    struct sockaddr_in sin;
  } rtmsg;

  memset(&rtmsg,0,sizeof(rtmsg));
  rtmsg.hdr.rtm_msglen = sizeof(rtmsg);
  rtmsg.hdr.rtm_type = RTM_GET;
  rtmsg.hdr.rtm_version = RTM_VERSION;
  rtmsg.hdr.rtm_addrs = RTA_DST;
  rtmsg.hdr.rtm_seq = seq++;
  rtmsg.hdr.rtm_pid = getpid();

  rtmsg.sin.sin_len = sizeof(struct sockaddr_in);
  rtmsg.sin.sin_family = AF_INET;
  rtmsg.sin.sin_addr.s_addr = 0;

  if(send(sock,&rtmsg,rtmsg.hdr.rtm_msglen,0) >= 0)
    return sock;

  close(sock);
  return -1;
}

static gboolean net_rt_parse (GIOChannel *chan, GIOCondition cond, gpointer d)
{
  gint len ,i;
  gint sock;
  gchar buff[4096];
  struct rt_msghdr *hdr;
  struct sockaddr *sa;
  struct in_addr gate, dest, mask;
  struct in6_addr gate6;
  gchar ifname[IF_NAMESIZE];

  sock = g_io_channel_unix_get_fd(chan);
  len = recv(sock,&buff,sizeof(buff),0);
  if(len<=0)
    return TRUE;

  hdr = (struct rt_msghdr *)buff;
  gate.s_addr = 0;
  mask.s_addr = 1;
  dest.s_addr = 1;
  memset(&gate6,0,sizeof(gate6));

  if(hdr->rtm_type == RTM_DELADDR && route && !g_strcmp0(route->name,
        if_indextoname(((struct ifa_msghdr *)hdr)->ifam_index,ifname)))
  {
    net_set_interface(0,gate,gate6);
    return TRUE;
  }
  else if(hdr->rtm_type==RTM_ADD || hdr->rtm_type==RTM_DELETE ||
      (hdr->rtm_type == RTM_GET && hdr->rtm_pid == getpid()))
  {
    sa = (struct sockaddr *)(hdr +1);
    for(i=0;i<RTAX_MAX;i++)
      if(hdr->rtm_addrs & 1<<i)
      {
        if(i == RTAX_DST)
            dest = (((struct sockaddr_in *)sa)->sin_addr);
        else if(i == RTAX_NETMASK)
            mask = (((struct sockaddr_in *)sa)->sin_addr);
        else if(i == RTAX_GATEWAY)
            gate = (((struct sockaddr_in *)sa)->sin_addr);
        sa = (void *)sa+((sa->sa_len+sizeof(u_long)-1)&~((sizeof(u_long)-1)));
      }
    if(hdr->rtm_type==RTM_GET)
      net_set_interface(hdr->rtm_index,gate,gate6);
    else if(hdr->rtm_type==RTM_ADD && !dest.s_addr && gate.s_addr &&
        !mask.s_addr && hdr->rtm_index)
      net_set_interface(hdr->rtm_index,gate,gate6);
    else if(hdr->rtm_type==RTM_DELETE && !dest.s_addr && !mask.s_addr &&
        hdr->rtm_index)
      net_set_interface(0,gate,gate6);
  }
  return TRUE;
}

#else /* unknown platform, provide shims */

static gchar *net_getaddr ( void *ina, int type )
{
  return g_strdup("");
}

static void net_update_traffic ( void )
{
}

static gdouble net_get_signal ( gchar *interface )
{
  return 0.0;
}

static void network_init ( void * )
{
}

#endif

void sfwbar_module_init ( ModuleApiV1 *api )
{
  int sock;
  GIOChannel *chan;

  sfwbar_module_api = api;
  sock = net_rt_connect();
  if(sock >= 0 && net_rt_request(sock) >= 0)
  {
    chan = g_io_channel_unix_new(sock);
    g_io_add_watch(chan,G_IO_IN | G_IO_PRI |G_IO_ERR | G_IO_HUP,
        net_rt_parse,NULL);
  }
  else if(sock >= 0)
    close(sock);
}

void sfwbar_module_invalidate ( void )
{
  GList *iter;

  for(iter=iface_list;iter;iter=g_list_next(iter))
    ((iface_info *)iter->data)->invalid = TRUE;
}

void *network_func_netstat ( void **params )
{
  iface_info *iface;
  gdouble *result;

  result = g_malloc0(sizeof(gdouble));
  if(!params || !params[0])
    return result;

  if(params[1] && *((gchar *)params[1]))
    iface = net_iface_from_name(params[1],FALSE);
  else
    iface = route;
  if(!iface)
    return result;

  g_mutex_lock(&iface->mutex);
  if(!g_ascii_strcasecmp(params[0],"signal"))
    *result = net_get_signal(route?route->name:NULL);
  else if(!g_ascii_strcasecmp(params[0],"rxrate"))
  {
    net_update_traffic(iface->name);
    *result = (gdouble)(iface->rx_bytes-iface->prx_bytes)*
        1000000/iface->time_diff;
  }
  else if(!g_ascii_strcasecmp(params[0],"txrate"))
  {
    net_update_traffic(iface->name);
    *result = (gdouble)(iface->tx_bytes-iface->ptx_bytes)*
      1000000/iface->time_diff;
  }
  g_mutex_unlock(&iface->mutex);

  return result;
}

void *network_func_netinfo ( void **params )
{
  gchar *result;
  iface_info *iface;

  if(!params || !params[0])
    return g_strdup("");

  if(params[1] && *((gchar *)params[1]))
    iface = net_iface_from_name(params[1],FALSE);
  else
    iface = route;
  if(!iface)
    return g_strdup("");

  g_mutex_lock(&iface->mutex);
  if(!g_ascii_strcasecmp(params[0],"ip"))
    result = net_getaddr(&iface->ip,AF_INET);
  else if(!g_ascii_strcasecmp(params[0],"mask"))
    result = net_getaddr(&iface->mask,AF_INET);
  else if(!g_ascii_strcasecmp(params[0],"cidr"))
    result = net_get_cidr(iface->mask.s_addr);
  else if(!g_ascii_strcasecmp(params[0],"ip6"))
    result = net_getaddr(&iface->ip6,AF_INET6);
  else if(!g_ascii_strcasecmp(params[0],"mask6"))
    result = net_getaddr(&iface->mask6,AF_INET6);
  else if(!g_ascii_strcasecmp(params[0],"gateway"))
    result = net_getaddr(&iface->gateway,AF_INET);
  else if(!g_ascii_strcasecmp(params[0],"gateway6"))
    result = net_getaddr(&iface->gateway6,AF_INET6);
  else if(!g_ascii_strcasecmp(params[0],"essid"))
    result = g_strdup(iface->essid?iface->essid:"");
  else if(!g_ascii_strcasecmp(params[0],"interface"))
    result = g_strdup(iface->name);
  else
    result = g_strdup("invalid query");
  g_mutex_unlock(&iface->mutex);

  return result;
}

ModuleExpressionHandlerV1 handler1 = {
  .flags = 0,
  .name = "NetInfo",
  .parameters = "Ss",
  .function = network_func_netinfo
};

ModuleExpressionHandlerV1 handler2 = {
  .flags = MODULE_EXPR_NUMERIC,
  .name = "NetStat",
  .parameters = "Ss",
  .function = network_func_netstat
};

ModuleExpressionHandlerV1 *sfwbar_expression_handlers[] = {
  &handler1,
  &handler2,
  NULL
};
