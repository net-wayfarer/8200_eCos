/* dnsmasq is Copyright (c) 2000 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

/* See RFC1035 for details of the protocol this code talks. */

/* Author's email: simon@thekelleys.org.uk */

#include "dnsmasq.h"
#include "errno.h"

static struct server *last_server;  
static struct frec *ftab;
static int sighup, sigusr1;
static int forward_4to6 = 0, forward_match = 0;

#ifdef HAVE_GETOPT_LONG
static struct option opts[] = { 
  {"version", no_argument, 0, 'v'},
  {"no-hosts", no_argument, 0, 'h'},
  {"no-poll", no_argument, 0, 'n'},
  {"help", no_argument, 0, '?'},
  {"no-daemon", no_argument, 0, 'd'},
  {"user", required_argument, 0, 'u'},
  {"resolv-file", required_argument, 0, 'r'},
  {"mx-host", required_argument, 0, 'm'},
  {"mx-target", required_argument, 0, 't'},
  {"cache-size", required_argument, 0, 'c'},
  {"port", required_argument, 0, 'p'},
  {"dhcp-lease", required_argument, 0, 'l'},
  {"domain-suffix", required_argument, 0, 's'},
  {"interface", required_argument, 0, 'i'},
  {"listen-address", required_argument, 0, 'a'},
  {"bogus-priv", no_argument, 0, 'b'},
  {"filterwin2k", no_argument, 0, 'f'},
  {"pid-file", required_argument, 0, 'x'},
  {"forward-match", no_argument, 0, 'j'},
  {"forward-4to6", no_argument, 0, 'k'}
};
#endif

static char *usage =
"Usage: dnsmasq [options]\n"
"\nValid options are :\n"
"-v, --version               Display dnsmasq version.\n"
"-d, --no-daemon             Do NOT fork into the background.\n"
"-h, --no-hosts              Do NOT load " HOSTSFILE " file.\n"
"-n, --no-poll               Do NOT poll " RESOLVFILE " file, reload only on SIGHUP.\n"
"-u, --user=username         Change to this user after startup. (defaults to " CHUSER ").\n"  
"-b, --bogus-priv            Fake reverse lookups for RFC1918 private address ranges.\n"
"-f, --filterwin2k           Don't forward spurious DNS requests from Windows hosts.\n"
"-r, --resolv-file=path      Specify path to resolv.conf (defaults to " RESOLVFILE ").\n"
"-x, --pid-file=path         Specify path of PID file. (defaults to " RUNFILE ").\n"
"-p, --port=number           Specify port to listen for DNS requests on (defaults to 53).\n"
"-m, --mx-host=host_name     Specify the MX name to reply to.\n"
"-t, --mx-target=host_name   Specify the host in an MX reply.\n"
"-c, --cache-size=cachesize  Specify the size of the cache in entries (defaults to %d).\n"
"-l, --dhcp-lease=path       Specify the path to the DHCP lease file.\n"
"-s, --domain-suffix=domain  Specify the domain suffix which DHCP entries will use.\n"
"-i, --interface=interface   Specify interface(s) to listen on.\n"
"-a, --listen-address=ipaddr Specify local address(es) to listen on.\n"
"-j, --forward-match         Forward DNS requests in the same IP type.\n"
"-k, --forward-4to6       IPv4 DNS requests are forwarded in IPv6 (override --foreard-match).\n"
"-?, --help                  Display this message.\n"
"\n";

static void forward_query(int udpfd, 
			  int peerfd,
			  int peerfd6,
			  union mysockaddr *udpaddr, 
			  HEADER *header,
			  int plen);
static void reply_query(int fd, char *packet, int cachesize);
static void reload_servers(char *fname, struct irec *interfaces,
			   int peerfd, int peerfd6);
static void *safe_malloc(int size);
static char *safe_string_alloc(char *cp);
static int sa_len(union mysockaddr *addr);
static struct irec *find_all_interfaces(struct iname *names, 
					struct iname *addrs,
					int fd, int port);
static int sockaddr_isequal(union mysockaddr *s1, union mysockaddr *s2);
static void sig_handler(int sig);
static struct frec *get_new_frec(time_t now);
static struct frec *lookup_frec(unsigned short id);
static struct frec *lookup_frec_by_sender(unsigned short id,
					  union mysockaddr *addr);
static unsigned short get_id(void);


int main (int argc, char **argv)
{
  int i, cachesize = CACHESIZ;
  int port = NAMESERVER_PORT;
  int peerfd, peerfd6, option; 
  int use_hosts = 1, daemon = 1, do_poll = 1, first_loop = 1;
  char *resolv = RESOLVFILE;
  char *runfile = RUNFILE;
  time_t resolv_changed = 0;
  off_t lease_file_size = (off_t)0;
  ino_t lease_file_inode = (ino_t)0;
  struct irec *iface, *interfaces = NULL;
  char *mxname = NULL;
  char *mxtarget = NULL;
  char *lease_file = NULL;
  char *domain_suffix = NULL;
  char *username = CHUSER;
  int boguspriv = 0;
  int filterwin2k = 0;
  struct iname *if_names = NULL;
  struct iname *if_addrs = NULL;

  sighup = 1; /* init cache the first time through */
  sigusr1 = 0; /* but don't dump */
  signal(SIGUSR1, sig_handler);
  signal(SIGHUP, sig_handler);

  last_server = NULL;
  
  opterr = 0;

  while (1)
    {
#ifdef HAVE_GETOPT_LONG
      option = getopt_long(argc, argv, "fnbvhdjkr:m:p:c:l:s:i:t:u:a:x:", opts, NULL);
#else
      option = getopt(argc, argv, "fnbvhdjkr:m:p:c:l:s:i:t:u:a:x:");
#endif

      if (option == 'b')
	  boguspriv = 1;
	
      if (option == 'f')
          filterwin2k = 1;
          
      if (option == 'v')
	{
	  fprintf(stderr, "dnsmasq version %s\n", VERSION);
	  exit(0);
	}

      if (option == 'h')
	use_hosts = 0;

      if (option == 'n')
	do_poll = 0;

      if (option == 'd')
	daemon = 0;
      
      if (option == 'x')
	runfile = safe_string_alloc(optarg);

      if (option == 'r')
	resolv = safe_string_alloc(optarg);
	
      if (option == 'm')
	mxname = safe_string_alloc(optarg);

      if (option == 't')
	mxtarget = safe_string_alloc(optarg);
      
      if (option == 'l')
	lease_file = safe_string_alloc(optarg);
      
      if (option == 's')
	domain_suffix = safe_string_alloc(optarg);

      if (option == 'u')
	username = safe_string_alloc(optarg);
      
      if (option == 'i')
	{
	  struct iname *new = safe_malloc(sizeof(struct iname));
	  new->next = if_names;
	  if_names = new;
	  new->name = safe_string_alloc(optarg);
	  new->found = 0;
	}

      if (option == 'a')
	{
	  struct iname *new = safe_malloc(sizeof(struct iname));
	  new->next = if_addrs;
	  if_addrs = new;
	  new->found = 0;
	  if (inet_pton(AF_INET, optarg, &new->addr.in.sin_addr))
	    new->addr.sa.sa_family = AF_INET;
#ifndef NO_IPV6
	  else if (inet_pton(AF_INET6, optarg, &new->addr.in6.sin6_addr))
	    new->addr.sa.sa_family = AF_INET6;
#endif
	  else
	    option = '?'; /* error */
	}

      if (option == 'c')
	{
	  cachesize = atoi(optarg);
	  /* zero is OK, and means no caching.
	     Very low values cause prolems with  hosts
	     with many A records. */
	  
	  if (cachesize < 0)
	    option = '?'; /* error */
	  else if ((cachesize > 0) && (cachesize < 20))
	    cachesize = 20;
	  else if (cachesize > 1000)
	    cachesize = 1000;
	}

      if (option == 'p')
	port = atoi(optarg);
      
      if (option == '?')
	{ 
	  fprintf (stderr, usage,  CACHESIZ);
          exit (0);
	}

      if (option == 'j')
         forward_match = 1;

      if (option == 'k')
         forward_4to6 = 1;

      if (option == -1)
	break;
    }

  /* port might no be known when the address is parsed - fill in here */
  if (if_addrs)
    {  
      struct iname *tmp;
      for(tmp = if_addrs; tmp; tmp = tmp->next)
#ifndef NO_IPV6
	if (tmp->addr.sa.sa_family == AF_INET6)
	  { 
#ifdef HAVE_SOCKADDR_SA_LEN
	    tmp->addr.in6.sin6_len = sizeof(struct sockaddr_in6);
#endif
	    tmp->addr.in6.sin6_port = htons(port);
	    tmp->addr.in6.sin6_flowinfo = htonl(0);
	  }
	else
#endif
	  {
#ifdef HAVE_SOCKADDR_SA_LEN
	    tmp->addr.in.sin_len = sizeof(struct sockaddr_in);
#endif
	    tmp->addr.in.sin_port = htons(port);
	  }
    }

  /* only one of these need be specified: the other defaults to the
     host-name */
  if (mxname || mxtarget)
    {
      static char hostname[MAXDNAME];
      if (gethostname(hostname, MAXDNAME) == -1)
	{
	  perror("dnsmasq: cannot get host-name");
	  exit(1);
	}
      
      if (!mxname)
	mxname = safe_string_alloc(hostname);
      
      if (!mxtarget)
	mxtarget = safe_string_alloc(hostname);
    }

  /* peerfd is not bound to a low port
     so that we can send queries out on it without them getting
     blocked at firewalls */
  
  if ((peerfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1 && 
      errno != EAFNOSUPPORT &&
      errno != EINVAL)
    {
      perror("dnsmasq: cannot create socket");
      exit(1);
    }
  
  if ((peerfd6 = socket(AF_INET6, SOCK_DGRAM, 0)) == -1 && 
      errno != EAFNOSUPPORT &&
      errno != EINVAL)
    {
      perror("dnsmasq: cannot create IPv6 socket");
      exit(1);
    }
     
  if (peerfd == -1 && peerfd6 == -1)
    {
      fprintf(stderr, "dnsmasq: no kernel support for IPv4 _or_ IPv6.\n");
      exit(1);
    }

  interfaces = find_all_interfaces(if_names, if_addrs,
				   peerfd == -1 ? peerfd6 : peerfd, port);
  
  /* open a socket bound to NS port on each local interface.
     this is necessary to ensure that our replies originate from
     the address they were sent to. See Stevens page 531 */
  for (iface = interfaces; iface; iface = iface->next)
    {
      if ((iface->fd = socket(iface->addr.sa.sa_family, SOCK_DGRAM, 0)) == -1)
	{
	  perror("dnsmasq: cannot create socket");
	  exit(1);
	}

    /* interface list contains all address, including temporary and
     * not ready ones.
     * retry bind() if return with EADDRNOTAVAIL, address might being going
     * thru DAD process */
    int bind_retry = 5;
    while (bind(iface->fd, &iface->addr.sa, sa_len(&iface->addr)))
    {
      if (errno == EADDRNOTAVAIL)
      {
        bind_retry--;
        if (bind_retry)
        {
          sleep(1);
          continue;  // try again
        }
      }
      perror("dnsmasq: bind failed");
      char addrbuff[INET6_ADDRSTRLEN];
#ifndef NO_IPV6
      if (iface->addr.sa.sa_family == AF_INET6)
        inet_ntop(AF_INET6, &iface->addr.in6.sin6_addr,
          addrbuff, INET6_ADDRSTRLEN);
      else
#endif
        inet_ntop(AF_INET, &iface->addr.in.sin_addr,
          addrbuff, INET_ADDRSTRLEN);
      fprintf(stderr, "dnsmasq: %s\n", addrbuff);

      exit(1);
    }
  }

  ftab = safe_malloc(FTABSIZ*sizeof(struct frec));
  for (i=0; i<FTABSIZ; i++)
    ftab[i].new_id = 0;
  
  cache_init(cachesize);

  setbuf(stdout,NULL);

  if (daemon)
    {
      FILE *pidfile;
      struct passwd *ent_pw;
        
      /* The following code "daemonizes" the process. 
	 See Stevens section 12.4 */

#ifndef NO_FORK
      if (fork() != 0 )
	exit(0);
      
      setsid();
      
      if (fork() != 0)
	exit(0);
#endif
      
      chdir("/");
      umask(022); /* make pidfile 0644 */
      
      /* write pidfile _after_ forking ! */
      if ((pidfile = fopen(runfile, "w")))
      	{
	  fprintf(pidfile, "%d\n", (int) getpid());
	  fclose(pidfile);
	}
      
      umask(0);

      for (i=0; i<64; i++)
	{
	  if (i == peerfd || i == peerfd6)
	    continue;
	  for (iface = interfaces; iface; iface = iface->next)
	    if (iface->fd == i)
	      break;
	  if (!iface)
	    close(i);
	}

      /* Change uid and gid for security */
      if ((ent_pw = getpwnam(username)))
	{
	  gid_t dummy;
	  struct group *gp;
	  /* remove all supplimentary groups */
	  setgroups(0, &dummy);
	  /* change group to "dip" if it exists, for /etc/ppp/resolv.conf 
	     otherwise get the group for "nobody" */
	  if ((gp = getgrnam("dip")) || (gp = getgrgid(ent_pw->pw_gid)))
	    setgid(gp->gr_gid); 
	  /* finally drop root */
	  setuid(ent_pw->pw_uid);
	}
    }
  
  openlog("dnsmasq", LOG_PID, LOG_USER);
  
  if (cachesize)
    syslog(LOG_INFO, "started, version %s cachesize %d", VERSION, cachesize);
  else
    syslog(LOG_INFO, "started, version %s cache disabled", VERSION);
  
  if (mxname)
    syslog(LOG_INFO, "serving MX record for mailhost %s target %s", 
	   mxname, mxtarget);
  
  if (getuid() == 0 || geteuid() == 0)
    syslog(LOG_WARNING, "failed to drop root privs");
  
  while (1)
    {
      /* Size: we check after adding each record, so there must be 
	 memory for the largest packet, and the largest record */
      static char packet[PACKETSZ+MAXDNAME+RRFIXEDSZ];
      int ready, maxfd = peerfd > peerfd6 ? peerfd : peerfd6;
      fd_set rset;
      HEADER *header;
      struct stat statbuf;
      
      if (first_loop)
	/* do init stuff only first time round. */
	{
	  first_loop = 0;
	  ready = 0;
	}
      else
	{
	  FD_ZERO(&rset);
	  if (peerfd != -1)
	    FD_SET(peerfd, &rset);
	  if (peerfd6 != -1)
	    FD_SET(peerfd6, &rset);
	  for (iface = interfaces; iface; iface = iface->next)
	    {
	      FD_SET(iface->fd, &rset);
	      if (iface->fd > maxfd)
		maxfd = iface->fd;
	    }
	  
	  ready = select(maxfd+1, &rset, NULL, NULL, NULL);
	  
	  if (ready == -1)
	    {
	      if (errno == EINTR)
		ready = 0; /* do signal handlers */
	      else
		continue;
	    }
	}

      if (sighup)
	{
	  signal(SIGHUP, SIG_IGN);
	  cache_reload(use_hosts, cachesize);
	  if (!do_poll)
	    reload_servers(resolv, interfaces, peerfd, peerfd6);
	  sighup = 0;
	  signal(SIGHUP, sig_handler);
	}

      if (sigusr1)
	{
	  signal(SIGUSR1, SIG_IGN);
	  dump_cache(daemon, cachesize);
	  sigusr1 = 0;
	  signal(SIGUSR1, sig_handler);
	}

      if (do_poll &&
	  (stat(resolv, &statbuf) == 0) && 
	  (statbuf.st_mtime > resolv_changed) &&
	  (statbuf.st_mtime < time(NULL) || resolv_changed == 0))
	{
	  resolv_changed = statbuf.st_mtime;
	  reload_servers(resolv, interfaces, peerfd, peerfd6);
	}

      if (lease_file && (stat(lease_file, &statbuf) == 0) &&
	  ((lease_file_size == (off_t)0) ||
	   (statbuf.st_size > lease_file_size) ||
	   (statbuf.st_ino != lease_file_inode)))
	{
	  lease_file_size = statbuf.st_size;
	  lease_file_inode = statbuf.st_ino;
	  load_dhcp(lease_file, domain_suffix);
	}
		
      if (ready == 0)
	continue; /* no sockets ready */

      if (peerfd != -1 && FD_ISSET(peerfd, &rset))
	reply_query(peerfd, packet, cachesize);
      if (peerfd6 != -1 && FD_ISSET(peerfd6, &rset))
	reply_query(peerfd6, packet, cachesize);
      
      for (iface = interfaces; iface; iface = iface->next)
	{
	  if (FD_ISSET(iface->fd, &rset))
	    {
	      /* request packet, deal with query */
	      union mysockaddr udpaddr;
	      socklen_t udplen = sizeof(udpaddr);
	      int m, n = recvfrom(iface->fd, packet, PACKETSZ, 0, &udpaddr.sa, &udplen); 
	      udpaddr.sa.sa_family = iface->addr.sa.sa_family;
#ifndef NO_IPV6
	      if (udpaddr.sa.sa_family == AF_INET6)
		udpaddr.in6.sin6_flowinfo = htonl(0);
#endif
	      
	      header = (HEADER *)packet;
	      if (n >= (int)sizeof(HEADER) && !header->qr)
		{
		  unsigned int add;
		  add = ((struct sockaddr_in *)&udpaddr)->sin_addr.s_addr;
		  m = answer_request (header, ((char *) header) + PACKETSZ, n, mxname, mxtarget, boguspriv, filterwin2k);
		  if (m >= 1)
		    {
		      /* answered from cache, send reply */
		      sendto(iface->fd, (char *)header, m, 0, 
			     &udpaddr.sa, sa_len(&udpaddr));
		    }
		  else 
		    {
		      /* cannot answer from cache, send on to real nameserver */
		      forward_query(iface->fd, peerfd, peerfd6, &udpaddr, header, n);
		    }
		}
	      
	    }
	}
    }
  
  return 0;
}

/* for use during startup */
static void *safe_malloc(int size)
{
  void *ret = malloc(size);
  
  if (!ret)
    {
      fprintf(stderr, "dnsmasq: could not get memory\n");
      exit(1);
    }
 
  return ret;
}
    
static char *safe_string_alloc(char *cp)
{
  char *ret = safe_malloc(strlen(cp)+1);
  strcpy(ret, cp);
  return ret;
}

static int sa_len(union mysockaddr *addr)
{
#ifdef HAVE_SOCKADDR_SA_LEN
  return addr->sa.sa_len;
#else
#ifndef NO_IPV6
  if (addr->sa.sa_family == AF_INET)
#endif
    return sizeof(addr->in);
#ifndef NO_IPV6
  else
    return sizeof(addr->in6); 
#endif
#endif /*HAVE_SOCKADDR_SA_LEN*/
}

static struct irec *add_iface(struct irec *list, unsigned int flags, 
			      char *name, union mysockaddr *addr, 
			      struct iname *names, struct iname *addrs)
{
  struct irec *iface;

  /* we may need to check the whitelist */
  if (names)
    { 
      struct iname *tmp;
      for(tmp = names; tmp; tmp = tmp->next)
	if (strcmp(tmp->name, name) == 0)
	  {
	    tmp->found = 1;
	    break;
	  }
      if (!(flags & IFF_LOOPBACK) && !tmp) 
	/* not on whitelist and not loopback */
	return list;
    }
  
  if (addrs)
    { 
      struct iname *tmp;
      for(tmp = addrs; tmp; tmp = tmp->next)
	if (sockaddr_isequal(&tmp->addr, addr))
	  {
	    tmp->found = 1;
	    break;
	  }
      
      if (!tmp) 
	/* not on whitelist */
	return list;
    }
  
  /* check whether the interface IP has been added already 
     it is possible to have multiple interfaces with the same address. */
  for (iface = list; iface; iface = iface->next) 
    if (sockaddr_isequal(&iface->addr, addr))
      break;
  if (iface) 
    return list;
  
  /* If OK, add it to the head of the list */
  iface = safe_malloc(sizeof(struct irec));
  iface->addr = *addr;
  iface->next = list;
  return iface;
}

static struct irec *find_all_interfaces(struct iname *names,
					struct iname *addrs,
					int fd, int port)
{
  /* this code is adapted from Stevens, page 434. It finally
     destroyed my faith in the C/unix API */
  int len = 100 * sizeof(struct ifreq);
  int lastlen = 0;
  char *buf, *ptr;
  struct ifconf ifc;
  struct irec *ret = NULL;

  while (1)
    {
      buf = safe_malloc(len);
      ifc.ifc_len = len;
      ifc.ifc_buf = buf;
      if (ioctl(fd, SIOCGIFCONF, &ifc) < 0)
	{
	  if (errno != EINVAL || lastlen != 0)
	    {
	      perror("dnsmasq: ioctl error while enumerating interfaces");
	      exit(1);
	    }
	}
      else
	{
	  if (ifc.ifc_len == lastlen)
	    break; /* got a big enough buffer now */
	  lastlen = ifc.ifc_len;
	}
      len += 10* sizeof(struct ifreq);
      free(buf);
    }

  for (ptr = buf; ptr < buf + ifc.ifc_len; )
    {
      struct ifreq *ifr = (struct ifreq *) ptr;
#ifdef HAVE_SOCKADDR_SA_LEN
      ptr += ifr->ifr_addr.sa_len + IF_NAMESIZE;
#else
      ptr += sizeof(struct ifreq);
#endif

      if (ifr->ifr_addr.sa_family == AF_INET || ifr->ifr_addr.sa_family == AF_INET6)
	{
	  /* copy since getting flags overwrites */
	  union mysockaddr addr;
#ifndef NO_IPV6
	  if (ifr->ifr_addr.sa_family == AF_INET6)
	    {
#ifdef HAVE_BROKEN_SOCKADDR_IN6
	      addr.in6 = *((struct my_sockaddr_in6 *) &ifr->ifr_addr);
#else
	      addr.in6 = *((struct sockaddr_in6 *) &ifr->ifr_addr);
#endif
	      addr.in6.sin6_port = htons(port);
	      addr.in6.sin6_flowinfo = htonl(0);
	    }
	  else
#endif
	    {
	      addr.in = *((struct sockaddr_in *) &ifr->ifr_addr);
	      addr.in.sin_port = htons(port);
	    }
	  /* get iface flags, since we need to distinguish loopback interfaces */
	  if (ioctl(fd, SIOCGIFFLAGS, ifr) < 0)
	    {
	      perror("dnsmasq: ioctl error getting interface flags");
	      exit(1);
	    }
	  
	  ret = add_iface(ret, ifr->ifr_flags, ifr->ifr_name, &addr, 
			  names, addrs);
	}
    }
  free(buf);

#ifndef NO_IPV6
#ifdef HAVE_LINUX_IPV6_PROC
  /* IPv6 addresses don't seem to work with SIOCGIFCONF. Barf */
  /* This code snarfed from net-tools 1.60 and certainly linux specific, though
     it shouldn't break on other Unices, and their SIOGIFCONF might work. */
  {
    FILE *f = fopen(IP6INTERFACES, "r");
    
    if (f)
      {
	union mysockaddr addr;
	unsigned int plen, scope, flags, if_idx;
	char devname[20], addrstring[32];
	
	while (fscanf(f, "%32s %02x %02x %02x %02x %20s\n",
		      addrstring, &if_idx, &plen, &scope, &flags, devname) != EOF) 
	  {
	    int i;
	    unsigned char *addr6p = (unsigned char *) &addr.in6.sin6_addr;
	    memset(&addr, 0, sizeof(addr));
	    addr.sa.sa_family = AF_INET6;
	    for (i=0; i<16; i++)
	      {
		unsigned int byte;
		sscanf(addrstring+i+i, "%02x", &byte);
		addr6p[i] = byte;
	      }
#ifdef HAVE_SOCKADDR_SA_LEN 
	    /* For completeness - should never be defined on Linux. */
	    addr.in6.sin6_len = sizeof(struct sockaddr_in6);
#endif
	    addr.in6.sin6_port = htons(port);
	    addr.in6.sin6_flowinfo = htonl(0);

       unsigned int scope_id = if_nametoindex(devname);
       if (scope_id == 0)
       {
          perror("if_nametoindex failed, scope_id");
          printf("******if_nametoindex failed, scope_id=%d, devname=%s", scope_id, devname);
       }
	    addr.in6.sin6_scope_id = scope_id;
	    
	    ret = add_iface(ret, flags, devname, &addr, names, addrs);
	  }
	
	fclose(f);
      }
  }
#endif /*NO_IPV6*/
#endif /* LINUX */

  /* if a whitelist provided, make sure the if names on it were OK */
  while(names)
    {
      if (!names->found)
	{
	  fprintf(stderr, "dnsmasq: unknown interface %s\n", names->name);
	  exit(1);
	}
      names = names->next;
    }
   
  while(addrs)
    {
      if (!addrs->found)
	{
	  char addrbuff[INET6_ADDRSTRLEN];
#ifndef NO_IPV6
	  if (addrs->addr.sa.sa_family == AF_INET6)
	    inet_ntop(AF_INET6, &addrs->addr.in6.sin6_addr,
		      addrbuff, INET6_ADDRSTRLEN);
	  else
#endif
	    inet_ntop(AF_INET, &addrs->addr.in.sin_addr,
		      addrbuff, INET_ADDRSTRLEN);
	  fprintf(stderr, "dnsmasq: no interface with address %s\n", addrbuff);
	  exit(1);
	}
      addrs = addrs->next;
    }
    
  return ret;
}

static void sig_handler(int sig)
{
  if (sig == SIGHUP)
    sighup = 1;
  else if (sig == SIGUSR1)
    sigusr1 = 1;
}

static void reload_servers(char *fname, struct irec *interfaces,
			   int peerfd, int peerfd6)
{
  FILE *f;
  char *line, buff[MAXLIN];
  int i;

  f = fopen(fname, "r");
  if (!f)
    {
      syslog(LOG_ERR, "failed to read %s: %m", fname);
      return;
    }
  
  syslog(LOG_INFO, "reading %s", fname);

  /* forward table rules reference servers, so have to blow 
     them away */
  for (i=0; i<FTABSIZ; i++)
    ftab[i].new_id = 0;
  
  /* delete existing ones */
  if (last_server)
    {
      struct server *s = last_server;
      while (1)
	{
	  struct server *tmp = s->next;
	  free(s);
	  if (tmp == last_server)
	    break;
	  s = tmp;
	}
      last_server = NULL;
    }
	
  while ((line = fgets(buff, MAXLIN, f)))
    {
      union  mysockaddr addr;
      char *token = strtok(line, " \t\n");
      struct server *serv;
      struct irec *iface;

      if (!token || strcmp(token, "nameserver") != 0)
	continue;
      if (!(token = strtok(NULL, " \t\n")))
	continue;
      
      if (inet_pton(AF_INET, token, &addr.in.sin_addr))
	{
	  if (peerfd == -1)
	    {
	      syslog(LOG_WARNING, 
		     "ignoring nameserver %s - no IPv4 kernel support", token);
	      continue;
	    }
#ifdef HAVE_SOCKADDR_SA_LEN
	  addr.in.sin_len = sizeof(struct sockaddr_in);
#endif
	  addr.in.sin_family = AF_INET;
	  addr.in.sin_port = htons(NAMESERVER_PORT);
	}
#ifndef NO_IPV6
      else if (inet_pton(AF_INET6, token, &addr.in6.sin6_addr))
	{
	  if (peerfd6 == -1)
	    {
	      syslog(LOG_WARNING, 
		     "ignoring nameserver %s - no IPv6 kernel support", token);
	      continue;
	    }
#ifdef HAVE_SOCKADDR_SA_LEN
	  addr.in6.sin6_len = sizeof(struct sockaddr_in6);
#endif
	  addr.in6.sin6_family = AF_INET6;
	  addr.in6.sin6_port = htons(NAMESERVER_PORT);
	  addr.in6.sin6_flowinfo = htonl(0);
	}
#endif
      else
	continue;
      
      /* Avoid loops back to ourself */
      for (iface = interfaces; iface; iface = iface->next)
	if (sockaddr_isequal(&addr, &iface->addr))
	  {
	    syslog(LOG_WARNING, "ignoring nameserver %s - local interface", token);
	    break;
	  }
      if (iface)
	continue;
    
      if (!(serv = malloc(sizeof (struct server))))
	continue;

      if (last_server)
	last_server->next = serv;
      else
	last_server = serv;
      
      serv->next = last_server;
      last_server = serv;
      serv->addr = addr;
      syslog(LOG_INFO, "using nameserver %s", token); 
    }
  
  fclose(f);
}

static int sockaddr_isequal(union mysockaddr *s1, union mysockaddr *s2)
{
  if (s1->sa.sa_family == s2->sa.sa_family)
    { 
      if (s1->sa.sa_family == AF_INET &&
	  s1->in.sin_port == s2->in.sin_port &&
	  memcmp(&s1->in.sin_addr, &s2->in.sin_addr, sizeof(struct in_addr)) == 0)
	return 1;
      
#ifndef NO_IPV6
      if (s1->sa.sa_family == AF_INET6 &&
	  s1->in6.sin6_port == s2->in6.sin6_port &&
	  s1->in6.sin6_flowinfo == s2->in6.sin6_flowinfo &&
	  memcmp(&s1->in6.sin6_addr, &s2->in6.sin6_addr, sizeof(struct in6_addr)) == 0)
	return 1;
#endif
    }
  return 0;
}

	
static void forward_query(int udpfd,
			  int peerfd,
			  int peerfd6,
			  union mysockaddr *udpaddr, 
			  HEADER *header,
			  int plen)
{
  time_t now = time(NULL);
  struct frec *forward;

   /* may be no available servers or recursion not speced */
  if (!last_server || !header->rd)
    forward = NULL;
  else if ((forward = lookup_frec_by_sender(ntohs(header->id), udpaddr)))
    {
      /* retry on existing query, send to next server */
      forward->sentto = forward->sentto->next;
      header->id = htons(forward->new_id);
    }
  else
    {
      /* new query, pick nameserver and send */
      forward = get_new_frec(now);
      forward->source = *udpaddr;
      forward->new_id = get_id();
      forward->fd = udpfd;
      forward->orig_id = ntohs(header->id);
      header->id = htons(forward->new_id);
      forward->sentto = last_server;
      last_server = last_server->next;
    }

  /* check for sendto errors here (no route to host) 
     if we fail to send to all nameservers, send back an error
     packet straight away (helps modem users when offline) */
  
  if (forward)
    {
      struct server *firstsentto = forward->sentto;
      int send_attempted = 0;
      int loop_forward_4to6 = forward_4to6;
      int loop_forward_match = forward_match;
      while (1)
      {
        int skip_server = 0;
        /* skip server if one of the following
         *   running in 4to6 mode,  request is ipv4, but server is not ipv6.
         *   running in match mode, request IP type is different from server IP type.
         */
        if ((loop_forward_4to6 &&
              udpaddr->sa.sa_family == AF_INET &&
              forward->sentto->addr.sa.sa_family != AF_INET6) ||
            (loop_forward_match &&
              udpaddr->sa.sa_family != forward->sentto->addr.sa.sa_family))
        {
          skip_server = 1;
        }

        if (!skip_server)
        {
          send_attempted = 1;
          int fd = forward->sentto->addr.sa.sa_family == AF_INET ? peerfd : peerfd6;
          if (sendto(fd, (char *)header, plen, 0,
                &forward->sentto->addr.sa,
                sa_len(&forward->sentto->addr)) != -1)
          return;
        }

        /* fail to send, try next server */
        forward->sentto = forward->sentto->next;

        /* check if we tried all without success */
        if (forward->sentto == firstsentto)
        {
          if (!send_attempted)
          {
            /* try again without enforcing forward_4to6 or forward_match */
            loop_forward_match = 0;
            loop_forward_4to6 = 0;
            continue;
          }
          break;
        }
      }

      /* could not send on, prepare to return */ 
      header->id = htons(forward->orig_id);
      forward->new_id = 0; /* cancel */
    }	  
  
  /* could not send on, return empty answer */
  header->qr = 1; /* response */
  header->aa = 0; /* authoritive - never */
  header->ra = 1; /* recursion if available */
  header->tc = 0; /* not truncated */
  header->rcode = NOERROR; /* no error */
  header->ancount = htons(0); /* no answers */
  header->nscount = htons(0);
  header->arcount = htons(0);
  sendto(udpfd, (char *)header, plen, 0, &udpaddr->sa, sa_len(udpaddr));
}

static void reply_query(int fd, char *packet, int cachesize)
{
  /* packet from peer server, extract data for cache, and send to
     original requester */
  struct frec *forward;
  HEADER *header;
  int n = recvfrom(fd, packet, PACKETSZ, 0, NULL, NULL);
  
  header = (HEADER *)packet;
  if (n >= (int)sizeof(HEADER) && header->qr)
    {
      if ((forward = lookup_frec(ntohs(header->id))))
	{
	  if (header->rcode == NOERROR || header->rcode == NXDOMAIN)
	    {
	      last_server = forward->sentto; /* known good */
	      if (cachesize != 0 && header->opcode == QUERY)
		extract_addresses(header, n);
	    }
	  
	  header->id = htons(forward->orig_id);
	  /* There's no point returning an upstream reply marked as truncated,
	     since that will prod the resolver into moving to TCP - which we
	     don't support. */
	  header->tc = 0; /* goodbye truncate */
	  sendto(forward->fd, packet, n, 0, 
		 &forward->source.sa, sa_len(&forward->source));
	  forward->new_id = 0; /* cancel */
	}
    }
}
      

static struct frec *get_new_frec(time_t now)
{
  int i;
  struct frec *oldest = &ftab[0];
  time_t oldtime = now;

  for(i=0; i<FTABSIZ; i++)
    {
      struct frec *f = &ftab[i];
      if (f->time <= oldtime)
	{
	  oldtime = f->time;
	  oldest = f;
	}
      if (f->new_id == 0)
	{
	  f->time = now;
	  return f;
	}
    }

  /* table full, use oldest */

  oldest->time = now;
  return oldest;
}
 
static struct frec *lookup_frec(unsigned short id)
{
  int i;
  for(i=0; i<FTABSIZ; i++)
    {
      struct frec *f = &ftab[i];
      if (f->new_id == id)
	return f;
    }
  return NULL;
}

static struct frec *lookup_frec_by_sender(unsigned short id,
					  union mysockaddr *addr)
{
  int i;
  for(i=0; i<FTABSIZ; i++)
    {
      struct frec *f = &ftab[i];
      if (f->new_id &&
	  f->orig_id == id && 
	  sockaddr_isequal(&f->source, addr))
	return f;
    }
  return NULL;
}


/* return unique ids between 1 and 65535 */
/* These are now random, FSVO random, to frustrate DNS spoofers */
/* code adapted from glibc-2.1.3 */ 
static unsigned short get_id(void)
{
#ifndef HAVE_ARC4RANDOM
  struct timeval now;
  static int salt = 0;
#endif
  unsigned short ret = 0;

  /* salt stops us spinning wasting cpu on a host with
     a low resolution clock and avoids sending requests
     with the same id which are close in time. */

  while (ret == 0)
    {
#ifdef HAVE_ARC4RANDOM
      ret = (unsigned short) arc4random();
#else
      gettimeofday(&now, NULL);
      ret = salt-- ^ now.tv_sec ^ now.tv_usec ^ getpid();
#endif
      
      /* scrap ids already in use */
      if ((ret != 0) && lookup_frec(ret))
	ret = 0;
    }

  return ret;
}





