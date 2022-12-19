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

MODULE_TRIGGER_DECL
gboolean invalid;

GMutex mutex;
struct in_addr ip, mask, bcast, gateway;
struct in6_addr ip6, mask6, bcast6, gateway6;
guint32 rx_packets, tx_packets, rx_bytes, tx_bytes;
guint32 prx_packets, ptx_packets, prx_bytes, ptx_bytes;
gint64 last_time, time_diff;
gchar *interface;
gchar *essid;
gint qual, level, noise;
guint32 seq;

void net_update_essid ( void );

void net_update_address ( void )
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
        case AF_INET6:
          ip6 = ((struct sockaddr_in6 *)(iter->ifa_addr))->sin6_addr;
          if(iter->ifa_netmask)
            mask6 = ((struct sockaddr_in6 *)(iter->ifa_netmask))->sin6_addr;
          if(iter->ifa_broadaddr)
            bcast6 = ((struct sockaddr_in6 *)(iter->ifa_broadaddr))->sin6_addr;
          break;
      }
  freeifaddrs(addrs);
}

void net_set_interface ( gint32 iidx, struct in_addr gate,
    struct in6_addr gate6 )
{
  gchar ifname[IF_NAMESIZE];

  if(iidx)
  {
    if_indextoname(iidx,ifname);
    g_mutex_lock(&mutex);
    g_free(interface);
    interface = g_strdup(ifname);
    gateway = gate;
    gateway6 = gate6;
    g_mutex_unlock(&mutex);
    net_update_essid();
    net_update_address();
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
    memset(&gateway6,0,sizeof(gateway6));
    mask.s_addr = 0;
    g_mutex_unlock(&mutex);
  }
  MODULE_TRIGGER_EMIT("network");
}

gchar *net_getaddr ( void *ina, gint type  )
{
  gchar addr[INET6_ADDRSTRLEN];

  return g_strdup(inet_ntop(type,ina,addr,INET_ADDRSTRLEN));
}

#if defined(__linux__)

#include <linux/rtnetlink.h>
#include <linux/wireless.h>

void net_update_traffic ( void )
{
  gint64 ctime;
  struct ifaddrs *addrs, *iter;

  if(!invalid)
    return;

  prx_packets = rx_packets;
  ptx_packets = tx_packets;
  prx_bytes = rx_bytes;
  ptx_bytes = tx_bytes;

  getifaddrs(&addrs);
  for(iter=addrs;iter;iter=iter->ifa_next)
    if(!g_strcmp0(interface,iter->ifa_name) && iter->ifa_addr &&
        iter->ifa_addr->sa_family == AF_PACKET)
    {
      rx_packets = ((struct rtnl_link_stats *)(iter->ifa_data))->rx_packets;
      tx_packets = ((struct rtnl_link_stats *)(iter->ifa_data))->tx_packets;
      rx_bytes = ((struct rtnl_link_stats *)(iter->ifa_data))->rx_bytes;
      tx_bytes = ((struct rtnl_link_stats *)(iter->ifa_data))->tx_bytes;
    }
  freeifaddrs(addrs);

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
    (void)g_strlcpy(wreq.ifr_name,interface,sizeof(wreq.ifr_name));
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

gint net_rt_connect ( void )
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

gint net_rt_request ( gint sock )
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

gboolean net_rt_parse ( GIOChannel *chan, GIOCondition cond, gpointer data )
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
    if(hdr->nlmsg_type == RTM_DELADDR && !g_strcmp0(interface,if_indextoname(
            ((struct ifinfomsg *)NLMSG_DATA(hdr))->ifi_index,ifname)))
    {
      net_set_interface(0,gate,gate6);
      break;
    }
    else if(hdr->nlmsg_type == RTM_NEWLINK)
    {
      rta = IFLA_RTA(NLMSG_DATA(hdr));
      rtl = IFLA_PAYLOAD(hdr);
      if(((struct ifinfomsg *)NLMSG_DATA(hdr))->ifi_change!=0)
        for(;rtl && RTA_OK(rta,rtl);rta=RTA_NEXT(rta,rtl))
          if(rta->rta_type==IFLA_WIRELESS)
            net_update_essid();
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

void net_update_traffic ( void )
{
  gint64 ctime;
  struct ifaddrs *addrs, *iter;

  if(!invalid)
    return;

  prx_packets = rx_packets;
  ptx_packets = tx_packets;
  prx_bytes = rx_bytes;
  ptx_bytes = tx_bytes;

  getifaddrs(&addrs);
  for(iter=addrs;iter;iter=iter->ifa_next)
    if(iter->ifa_addr && iter->ifa_addr->sa_family==AF_LINK)
    {
      rx_packets = ((struct if_data *)(iter->ifa_data))->ifi_ipackets;
      tx_packets = ((struct if_data *)(iter->ifa_data))->ifi_opackets;
      rx_bytes = ((struct if_data *)(iter->ifa_data))->ifi_ibytes;
      tx_bytes = ((struct if_data *)(iter->ifa_data))->ifi_obytes;
    }
  freeifaddrs(addrs);
  ctime = g_get_monotonic_time();
  time_diff = ctime - last_time;
  last_time = ctime;
}

#if defined(__FreeBSD__)
#include <net80211/ieee80211_ioctl.h>

void net_update_essid ( void )
{
  struct ieee80211req req;
  gchar lessid[IEEE80211_NWID_LEN+1];
  gint sock;
  gint rssi = -100;

  if(!interface)
  {
    g_mutex_lock(&mutex);
    g_free(essid);
    essid = NULL;
    g_mutex_unlock(&mutex);
    return;
  }

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
    g_mutex_lock(&mutex);
    g_free(essid);
    essid = g_strdup(lessid);
    g_mutex_unlock(&mutex);
  }
  else
  {
    g_mutex_lock(&mutex);
    g_free(essid);
    essid = NULL;
    g_mutex_unlock(&mutex);
  }
  close(sock);
}

gdouble net_get_signal ( void )
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

gdouble net_get_signal ( void )
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

void net_update_essit ( void )
{
  (void)net_get_signal();
}

#endif

gint net_rt_connect ( void )
{
  return socket(AF_ROUTE, SOCK_RAW,0);
}

gboolean net_rt_request ( gint sock )
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

gboolean net_rt_parse ( GIOChannel *chan, GIOCondition cond, gpointer data )
{
  gint len ,i;
  gint sock;
  gchar buff[4096];
  struct rt_msghdr *hdr;
  struct sockaddr_in *sin;
  struct sockaddr *sa;
  struct in_addr gate, dest, mask;
  struct in6_addr gate6;
  gchar ifname[IF_NAMESIZE];
  gchar addrstr[INET6_ADDRSTRLEN];

  sock = g_io_channel_unix_get_fd(chan);
  len = recv(sock,&buff,sizeof(buff),0);
  if(len<=0)
    return TRUE;

  hdr = (struct rt_msghdr *)buff;
  gate.s_addr = 0;
  mask.s_addr = 1;
  dest.s_addr = 1;
  memset(&gate6,0,sizeof(gate6));

  if(hdr->rtm_type == RTM_DELADDR && !g_strcmp0(interface,if_indextoname(
          ((struct ifa_msghdr *)hdr)->ifam_index,ifname)))
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

gchar *net_getaddr ( void *ina, int type )
{
  return g_strdup("");
}
void net_update_traffic ( void )
{
}

gdouble net_get_signal ( void )
{
  return 0.0;
}

void network_init ( void * )
{
}

#endif

void sfwbar_module_init ( GMainContext *gmc )
{
  int sock;
  GIOChannel *chan;

  g_mutex_init(&mutex);
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
  invalid = TRUE;
}

void *sfwbar_expr_func ( void **params )
{
  gchar *result;

  if(!params || !params[0])
    return g_strdup("");

  g_mutex_lock(&mutex);
  if(!g_ascii_strcasecmp(params[0],"ip"))
    result = net_getaddr(&ip,AF_INET);
  else if(!g_ascii_strcasecmp(params[0],"mask"))
    result = net_getaddr(&mask,AF_INET);
  else if(!g_ascii_strcasecmp(params[0],"ip6"))
    result = net_getaddr(&ip6,AF_INET6);
  else if(!g_ascii_strcasecmp(params[0],"mask6"))
    result = net_getaddr(&mask6,AF_INET6);
  else if(!g_ascii_strcasecmp(params[0],"gateway"))
    result = net_getaddr(&gateway,AF_INET);
  else if(!g_ascii_strcasecmp(params[0],"signal"))
    result = g_strdup_printf("%lf",net_get_signal());
  else if(!g_ascii_strcasecmp(params[0],"rxrate"))
  {
    net_update_traffic();
    result = g_strdup_printf("%lf",(gdouble)(rx_bytes - prx_bytes)*
        1000000/time_diff);
  }
  else if(!g_ascii_strcasecmp(params[0],"txrate"))
  {
    net_update_traffic();
    result = g_strdup_printf("%lf",(gdouble)(tx_bytes - ptx_bytes)*
        1000000/time_diff);
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

ModuleExpressionHandler *sfwbar_expression_handlers[] = {
  &handler1,
  NULL
};
