/* ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <m-asama@ginzado.co.jp> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return Masakazu Asama
 * ----------------------------------------------------------------------------
 */
/*	MAP A+P NAPT function
 *
 *	Authors:
 *	Masakazu Asama		<m-asama@ginzado.co.jp>
 */

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/if.h>
#include <linux/if_map.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include <linux/if_arp.h>
#include <linux/random.h>
#include <linux/route.h>
#include <linux/rtnetlink.h>
#include <linux/netfilter_ipv6.h>

#include <net/ip.h>
#include <net/ipv6.h>
#include <net/icmp.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/xfrm.h>
#include <net/dsfield.h>
#include <net/inet_ecn.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <net/map.h>

#define CSUM_CO_DEC(x) { if (x <= 0) { --x; x &= 0xffff; } }
#define CSUM_CO_INC(x) { if (x & 0x10000) { ++x; x &= 0xffff; } }

static struct kmem_cache *nn_kmem __read_mostly;
static int nn_kmem_alloced;

int
map_napt_hairpin(struct sk_buff *skb, struct map *m, __be32 *daddrp,
	__be16 *dportp, struct in6_addr *saddr6, int fb)
{
	struct iphdr *iph;
	__be32 *saddrp = NULL;
	__be16 *sportp = NULL;
	__sum16 *checkp = NULL;
	__u16 port;
	__u16 mask;
	__u16 psid;
	__u8 psid_offset;

	if (m->p.role != MAP_ROLE_CE || !m->bmr ||
	    (m->psid_length <= 0 && m->p.napt_always == MAP_NAPT_ALWAYS_F))
		goto out;

	read_lock(&m->rule_lock);
	if (!m->bmr) {
		read_unlock(&m->rule_lock);
		return 0;
	}
	psid_offset = m->bmr->p.psid_offset;
	read_unlock(&m->rule_lock);

	if (*daddrp != m->laddr4)
		goto out;

	iph = ip_hdr(skb);
	port = ntohs(*dportp);

	if (m->psid_length == 32)
		mask = 0xffff;
	else {
		mask = (1 << m->psid_length) - 1;
		mask <<= 16 - psid_offset - m->psid_length;
	}
	psid = m->psid;
	psid <<= 16 - psid_offset - m->psid_length;
	if ((port & mask) == psid) {
		pr_notice("map_napt_hairpin: hairpinning!\n");
		if (!map_napt(iph, 1, m, &saddrp, &sportp, &checkp, saddr6,
			      fb)) {
			/* XXX: */
			skb->rxhash = 0;
			skb_set_queue_mapping(skb, 0);
			skb_dst_drop(skb);
			nf_reset(skb);
			netif_rx(skb);
		}
		return 1;
	}

out:
	return 0;
}

static inline __be16
map_napt_generate_port_random(struct map *m)
{
	u32 t;
	int i;

	if (!m->port_range)
		return 0;

	read_lock(&m->port_range_lock);
	i = prandom_u32() % m->port_range_length;
	t = m->port_range[i].max - m->port_range[i].min;
	if (t)
		t = prandom_u32() % t;
	t = m->port_range[i].min + t;
	read_unlock(&m->port_range_lock);

	return htons(t);
}

static inline __be16
map_napt_generate_port_next(__be16 p, struct map *m)
{
	int i;
	u32 t;

	if (!m->port_range)
		return 0;

	t = ntohs(p) + 1;

	read_lock(&m->port_range_lock);
	for (i = 0; i < m->port_range_length; i++) {
		if (ntohs(p) == m->port_range[i].max) {
			t = m->port_range[(i + 1) % m->port_range_length].min;
			break;
		}
	}
	read_unlock(&m->port_range_lock);

	return htons(t);
}

static inline u32
map_napt_nn_hash_lookup(__be32 addr, __be16 port, __u8 proto)
{
	/* XXX: atode yoku kanngaeru */
	u32 h = ntohl(addr) | ntohs(port) | proto;
	h ^= (h >> 20);
	h ^= (h >> 10);
	h ^= (h >> 5);
	h &= (MAP_NAPT_HASH_LOOKUP_SIZE - 1);
	return h;
}

static inline u32
map_napt_nn_hash_create(__be32 addr, __u8 proto)
{
	/* XXX: atode yoku kanngaeru */
	u32 h = ntohl(addr) | proto;
	h ^= (h >> 20);
	h ^= (h >> 10);
	h ^= (h >> 5);
	h &= (MAP_NAPT_HASH_CREATE_SIZE - 1);
	return h;
}

static inline void
map_napt_nn_debug(char *h, char *f, struct map_napt_node *nn)
{
	pr_info("%s: proto = %d laddr6 = %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x laddr = %d.%d.%d.%d lport = %d maddr = %d.%d.%d.%d mport = %d raddr = %d.%d.%d.%d rport = %d last_used = %lu nn_kmem_alloced = %d %s\n",
		h,
		nn->proto,
		ntohs(nn->laddr6.s6_addr16[0]),
		ntohs(nn->laddr6.s6_addr16[1]),
		ntohs(nn->laddr6.s6_addr16[2]),
		ntohs(nn->laddr6.s6_addr16[3]),
		ntohs(nn->laddr6.s6_addr16[4]),
		ntohs(nn->laddr6.s6_addr16[5]),
		ntohs(nn->laddr6.s6_addr16[6]),
		ntohs(nn->laddr6.s6_addr16[7]),
		ntohl(nn->laddr) >> 24,
		(ntohl(nn->laddr) >> 16) & 0xff,
		(ntohl(nn->laddr) >> 8) & 0xff,
		ntohl(nn->laddr) & 0xff,
		ntohs(nn->lport),
		ntohl(nn->maddr) >> 24,
		(ntohl(nn->maddr) >> 16) & 0xff,
		(ntohl(nn->maddr) >> 8) & 0xff,
		ntohl(nn->maddr) & 0xff,
		ntohs(nn->mport),
		ntohl(nn->raddr) >> 24,
		(ntohl(nn->raddr) >> 16) & 0xff,
		(ntohl(nn->raddr) >> 8) & 0xff,
		ntohl(nn->raddr) & 0xff,
		ntohs(nn->rport),
		nn->last_used,
		nn_kmem_alloced,
		f);
}

static inline int
map_napt_nn_expired(struct map_napt_node *nn)
{
	if (nn->proto == IPPROTO_TCP)
		return time_is_before_jiffies(nn->last_used +
			MAP_NAPT_EXPIRES_TCP);
	else
		return time_is_before_jiffies(nn->last_used +
			MAP_NAPT_EXPIRES_OTHER);
}

static inline void
map_napt_nn_destroy(char *f, struct map_napt_node *nn)
{
#ifdef MAP_DEBUG
	map_napt_nn_debug("before destroy", f, nn);
#endif
	hlist_del(&nn->nn_hash_lup0);
	hlist_del(&nn->nn_hash_lup1);
	hlist_del(&nn->nn_hash_crat);
	list_del_init(&nn->nn_list);
	list_del_init(&nn->nn_gc_list);
	kmem_cache_free(nn_kmem, nn);
	--nn_kmem_alloced;
}

static inline int
map_napt_nn_est(__u8 flags)
{
	if ((flags & MAP_NAPT_F_EST) == MAP_NAPT_F_EST)
		return 1;
	return 0;
}

static inline int
map_napt_nn_finrst(__u8 flags)
{
	if ((flags & MAP_NAPT_F_FIN) == MAP_NAPT_F_FIN)
		return 1;
	if ((flags & MAP_NAPT_F_RST) == MAP_NAPT_F_RST)
		return 2;
	return 0;
}

void
map_napt_nn_gc(struct map *m)
{
	int ret = 0;
	struct map_napt_node *nn, *nn_node;
	char *reason = "???";
	unsigned long min_expires, gc_threshold;

	min_expires = (MAP_NAPT_EXPIRES_TCP < MAP_NAPT_EXPIRES_OTHER)
		? MAP_NAPT_EXPIRES_TCP
		: MAP_NAPT_EXPIRES_OTHER;
	gc_threshold = MAP_NAPT_GC_THRESHOLD;

	list_for_each_entry_safe(nn, nn_node, &m->napt_list, nn_list) {
		if (time_is_after_jiffies(nn->last_used + min_expires))
			break;
		if (map_napt_nn_expired(nn))
			map_napt_nn_destroy("exp", nn);
	}

	list_for_each_entry_safe(nn, nn_node, &m->napt_gc_list, nn_gc_list) {
		if (time_is_after_jiffies(nn->last_used + gc_threshold))
			break;
		ret = map_napt_nn_finrst(nn->flags);
		if (ret)
			reason = ret ? "fin" : "rst";
		else {
			if (!map_napt_nn_est(nn->flags))
				reason = "syn";
		}
		map_napt_nn_destroy(reason, nn);
	}

	m->napt_last_gc = jiffies;
}

static struct map_napt_node*
map_napt_nn_create(__be32 saddr, __be16 sport, __be32 daddr, __be16 dport,
		   __u8 proto, struct in6_addr *saddr6, __be32 paddr,
		   struct map *m)
{
	struct map_napt_node *nn, *nn_node;
	u32 hl0, hl1;
	u32 h = map_napt_nn_hash_create(daddr, proto);
	__be16 p = map_napt_generate_port_random(m);
	__be16 origp = p;
	int first = 1;

	map_napt_nn_gc(m);
	hlist_for_each_entry(nn, &m->napt_hash_crat[h], nn_hash_crat)
		if (nn->proto == proto
		 && nn->raddr == daddr && nn->rport == dport
		 && nn->laddr == saddr && nn->lport == sport
		 && (!saddr6 || ipv6_addr_equal(&nn->laddr6, saddr6))) {
			pr_info("map_napt_nn_create: napt node found.\n");
			goto out;
	}

	hl0 = map_napt_nn_hash_lookup(saddr, sport, proto);
	hlist_for_each_entry(nn, &m->napt_hash_lup0[hl0], nn_hash_lup0) {
		if (nn->proto == proto
		 && nn->laddr == saddr && nn->lport == sport
		 && (!saddr6 || ipv6_addr_equal(&nn->laddr6, saddr6))) {
			pr_notice("map_napt_nn_create: Endpoint-Independent Mapping:\n");
			map_napt_nn_debug(
				"map_napt_nn_create: recycle e-i mapping:",
				 "", nn);
			p = nn->mport;
			origp = p;
		}
	}

try_next:
	if (!first && p == origp) {
		pr_crit("map_napt_nn_create: source port for %d.%d.%d.%d(%d) exhausted.\n",
			ntohl(nn->raddr) >> 24,
			(ntohl(nn->raddr) >> 16) & 0xff,
			(ntohl(nn->raddr) >> 8) & 0xff,
			ntohl(nn->raddr) & 0xff,
			nn->proto);
		list_for_each_entry_safe(nn, nn_node, &m->napt_list, nn_list) {
			if (nn->proto == proto
			 && nn->raddr == daddr && nn->rport == dport) {
				if (proto == IPPROTO_TCP
				 && m->p.napt_force_recycle
					== MAP_NAPT_FORCE_RECYCLE_F
				 && !map_napt_nn_finrst(nn->flags)
				 && map_napt_nn_est(nn->flags))
					continue;
				pr_crit("map_napt_nn_create: recycle oldest.\n");
				map_napt_nn_debug(
					"map_napt_nn_create: recycle oldest:",
					"", nn);
				p = nn->mport;
				map_napt_nn_destroy(
					"map_napt_nn_create: recycle oldest:",
					nn);
				goto recycle;
			}
		}
		pr_crit("map_napt_nn_create: recycle faild.\n");
		nn = NULL;
		goto out;
	}
	hlist_for_each_entry(nn, &m->napt_hash_crat[h], nn_hash_crat) {
		if (nn->proto == proto
		  && nn->raddr == daddr
		  && nn->rport == dport
		  && nn->mport == p) {
			p = map_napt_generate_port_next(p, m);
			first = 0;
			goto try_next;
		}
	}

recycle:
	nn = kmem_cache_alloc(nn_kmem, GFP_KERNEL);
	if (!nn) {
		pr_info("map_napt_nn_create: kmem_cache_alloc fail.\n");
		goto out;
	}

	++nn_kmem_alloced;
	nn->proto = proto;
	nn->raddr = daddr;
	nn->rport = dport;
	nn->laddr = saddr;
	nn->lport = sport;
	nn->maddr = paddr;
	nn->mport = p;
	if (saddr6)
		memcpy(&nn->laddr6, saddr6, sizeof(*saddr6));
	else
		memset(&nn->laddr6, 0, sizeof(*saddr6));
	nn->flags = 0;
	nn->last_used = jiffies;
	hl0 = map_napt_nn_hash_lookup(nn->laddr, nn->lport, proto);
	hl1 = map_napt_nn_hash_lookup(nn->maddr, nn->mport, proto);
	hlist_add_head(&nn->nn_hash_lup0, &m->napt_hash_lup0[hl0]);
	hlist_add_head(&nn->nn_hash_lup1, &m->napt_hash_lup1[hl1]);
	hlist_add_head(&nn->nn_hash_crat, &m->napt_hash_crat[h]);
	list_add_tail(&nn->nn_list, &m->napt_list);
	if (proto == IPPROTO_TCP)
		list_add_tail(&nn->nn_gc_list, &m->napt_gc_list);
	else
		INIT_LIST_HEAD(&nn->nn_gc_list);

out:
#ifdef MAP_DEBUG
	if (nn)
		map_napt_nn_debug("after create", "", nn);
#endif
	return nn;
}

/**
 *   @dir: 1 = in; 0 = out;
 **/

static struct map_napt_node*
map_napt_nn_lookup(__be32 saddr, __be16 sport, __be32 waddr, __be16 wport,
		   __u8 proto, struct in6_addr *saddr6, int dir, struct map *m)
{
	struct map_napt_node *nn;
	__be32 sa, wa;
	__be16 sp, wp;
	u32 h;

#ifdef MAP_DEBUG
	pr_notice("map_napt_nn_lookup (%s):\n", dir ? "in" : "out");
#endif

	h = map_napt_nn_hash_lookup(saddr, sport, proto);

	if (dir)
		hlist_for_each_entry(nn, &m->napt_hash_lup1[h],
				nn_hash_lup1) {
			wa = nn->raddr; wp = nn->rport;
			sa = nn->maddr; sp = nn->mport;
			if (nn->proto == proto
			  && wa == waddr && wp == wport
			  && sa == saddr && sp == sport
			  && !map_napt_nn_expired(nn)) {
#ifdef MAP_DEBUG
				pr_notice("map_napt_nn_lookup (%s): match!\n",
					  dir ? "in" : "out");
#endif
				return nn;
			}
		}
	else
		hlist_for_each_entry(nn, &m->napt_hash_lup0[h],
				nn_hash_lup0) {
			wa = nn->raddr; wp = nn->rport;
			sa = nn->laddr; sp = nn->lport;
			if (nn->proto == proto
			  && wa == waddr && wp == wport
			  && sa == saddr && sp == sport
			  && (!saddr6 || ipv6_addr_equal(&nn->laddr6, saddr6))
			  && !map_napt_nn_expired(nn)) {
#ifdef MAP_DEBUG
				pr_notice("map_napt_nn_lookup (%s): match!\n",
					  dir ? "in" : "out");
#endif
				return nn;
			}
		}

#ifdef MAP_DEBUG
	pr_notice("map_napt_nn_lookup (%s): miss!\n",
		dir ? "in" : "out");
#endif

	return 0;
}

static __sum16
map_napt_update_csum(__sum16 check, __be32 oaddr, __be16 oport,
		__be32 naddr, __be16 nport, __u8 proto, int nested_icmp)
{
	long csum = ntohs(check);

	if (proto == IPPROTO_UDP && csum == 0)
		return htons(csum);

	csum = ~csum & 0xffff;

	if (proto != IPPROTO_ICMP || nested_icmp) {
		csum -= ntohl(oaddr) & 0xffff;
		CSUM_CO_DEC(csum)
		csum -= (ntohl(oaddr) >> 16) & 0xffff;
		CSUM_CO_DEC(csum)
	}
	csum -= ntohs(oport) & 0xffff;
	CSUM_CO_DEC(csum)

	if (proto != IPPROTO_ICMP || nested_icmp) {
		csum += ntohl(naddr) & 0xffff;
		CSUM_CO_INC(csum)
		csum += (ntohl(naddr) >> 16) & 0xffff;
		CSUM_CO_INC(csum)
	}
	csum += ntohs(nport) & 0xffff;
	CSUM_CO_INC(csum)

	csum = ~csum & 0xffff;

	return htons(csum);
}

static void
map_napt_set_flags(struct map_napt_node *nn, __u8 flags, int dir)
{
	if (dir) {
		if ((flags & MAP_NAPT_TCP_F_SYN) == MAP_NAPT_TCP_F_SYN)
			nn->flags |= MAP_NAPT_F_I_SYN_ACK;
		if ((flags & MAP_NAPT_TCP_F_FIN) == MAP_NAPT_TCP_F_FIN)
			nn->flags |= MAP_NAPT_F_I_FIN;
		if ((nn->flags & MAP_NAPT_F_O_FIN) == MAP_NAPT_F_O_FIN
		 && (flags & MAP_NAPT_TCP_F_ACK) == MAP_NAPT_TCP_F_ACK)
			nn->flags |= MAP_NAPT_F_I_FIN_ACK;
	} else {
		if ((flags & MAP_NAPT_TCP_F_SYN) == MAP_NAPT_TCP_F_SYN)
			nn->flags |= MAP_NAPT_F_O_SYN;
		if ((flags & MAP_NAPT_TCP_F_ACK) == MAP_NAPT_TCP_F_ACK)
			nn->flags |= MAP_NAPT_F_O_ACK;
		if ((flags & MAP_NAPT_TCP_F_FIN) == MAP_NAPT_TCP_F_FIN)
			nn->flags |= MAP_NAPT_F_O_FIN;
		if ((nn->flags & MAP_NAPT_F_I_FIN) == MAP_NAPT_F_I_FIN
		 && (flags & MAP_NAPT_TCP_F_ACK) == MAP_NAPT_TCP_F_ACK)
			nn->flags |= MAP_NAPT_F_O_FIN_ACK;
	}
	if ((flags & MAP_NAPT_TCP_F_RST) == MAP_NAPT_TCP_F_RST)
		nn->flags |= MAP_NAPT_F_RST;
}

/**
 *   @dir: 1 = in; 0 = out;
 **/

static int
map_napt_update(__be32 *saddrp, __be16 *sportp, __be32 waddr, __be16 wport,
		__u8 proto, struct in6_addr *saddr6, __be32 paddr,
		__sum16 *checkp, __be32 *icmpaddr, int dir,
		__u8 flags, int nested_icmp, struct map *m)
{
	__be32 naddr = 0;
	__be16 nport = 0;
	struct map_napt_node *nn;
	__u8 orig_flags;

#ifdef MAP_DEBUG
	pr_notice("map_napt_update (%s):\n", dir ? "in" : "out");
#endif

	if (proto == IPPROTO_ICMP)
		wport = 0;

#ifdef MAP_DEBUG
	pr_notice("map_napt_update: saddr %d.%d.%d.%d waddr %d.%d.%d.%d\n",
			ntohl(*saddrp) >> 24,
			(ntohl(*saddrp) >> 16) & 0xff,
			(ntohl(*saddrp) >> 8) & 0xff,
			ntohl(*saddrp) & 0xff,
			ntohl(waddr) >> 24,
			(ntohl(waddr) >> 16) & 0xff,
			(ntohl(waddr) >> 8) & 0xff,
			ntohl(waddr) & 0xff);
	pr_notice("map_napt_update: sport %d wport %d\n",
			ntohs(*sportp), ntohs(wport));
#endif

	write_lock_bh(&m->napt_lock);
	nn = map_napt_nn_lookup(*saddrp, *sportp, waddr, wport, proto, saddr6,
				dir, m);
	if (nn) {
		orig_flags = nn->flags;
		map_napt_set_flags(nn, flags, dir);
		if (dir) {
			naddr = nn->laddr; nport = nn->lport;
			if (saddr6)
				memcpy(saddr6, &nn->laddr6, sizeof(nn->laddr6));
		} else {
			naddr = nn->maddr; nport = nn->mport;
		}
		list_move_tail(&nn->nn_list, &m->napt_list);
		if (nn->proto == IPPROTO_TCP
		 && !map_napt_nn_est(orig_flags) && map_napt_nn_est(nn->flags)
		 && !map_napt_nn_finrst(nn->flags)
		 && nn->nn_gc_list.next != &nn->nn_gc_list)
				list_del_init(&nn->nn_gc_list);
		if (nn->proto == IPPROTO_TCP
		 && !map_napt_nn_finrst(orig_flags)
		 && map_napt_nn_finrst(nn->flags)
		 && nn->nn_gc_list.next == &nn->nn_gc_list)
				list_add_tail(&nn->nn_gc_list,
					&m->napt_gc_list);
		if (nn->nn_gc_list.next != &nn->nn_gc_list)
			list_move_tail(&nn->nn_gc_list, &m->napt_gc_list);
	} else {
#ifdef MAP_DEBUG
		pr_notice("map_napt_update: map_napt_nn_lookup return null");
#endif
		if (dir) {
			write_unlock_bh(&m->napt_lock);
			return -1;
		}
		/* if (proto == IPPROTO_TCP
		 *	&& (flags & MAP_NAPT_TCP_F_SYN) != MAP_NAPT_TCP_F_SYN) {
		 *	write_unlock_bh(&m->napt_lock);
		 *	return -2;
		 * }
		 */
		nn = map_napt_nn_create(*saddrp, *sportp, waddr, wport,
			proto, saddr6, paddr, m);
		if (!nn) {
#ifdef MAP_DEBUG
			pr_notice("map_napt: map_napt_nn_create return null");
#endif
			write_unlock_bh(&m->napt_lock);
			return -3;
		}
		map_napt_set_flags(nn, flags, dir);
		naddr = nn->maddr; nport = nn->mport;
	}
	nn->last_used = jiffies;
	write_unlock_bh(&m->napt_lock);

#ifdef MAP_DEBUG
	pr_notice("map_napt_update: saddr %d.%d.%d.%d -> %d.%d.%d.%d\n",
			ntohl(*saddrp) >> 24,
			(ntohl(*saddrp) >> 16) & 0xff,
			(ntohl(*saddrp) >> 8) & 0xff,
			ntohl(*saddrp) & 0xff,
			ntohl(naddr) >> 24,
			(ntohl(naddr) >> 16) & 0xff,
			(ntohl(naddr) >> 8) & 0xff,
			ntohl(naddr) & 0xff);
	pr_notice("map_napt_update: sport %d -> %d\n",
			ntohs(*sportp), ntohs(nport));
#endif

	*checkp = map_napt_update_csum(*checkp, *saddrp, *sportp, naddr,
		nport, proto, nested_icmp);
	*saddrp = naddr;
	*sportp = nport;

	if (icmpaddr)
		*icmpaddr = naddr;

	return 0;
}

static inline int
map_napt_first_pool(__be32 *first, struct map *m)
{
	struct map_pool *mp;
	if (m->p.role == MAP_ROLE_CE) {
		*first = m->laddr4;
		return 0;
	}
	if (m->p.pool_num > 0) {
		mp = list_first_entry(&m->pool_list, struct map_pool, list);
		*first = mp->p.pool_prefix;
		return 0;
	}
	return -1;
}

static inline int
map_napt_next_pool(__be32 cur, __be32 *next, struct map *m)
{
	struct map_pool *mp;
	__u32 mask;
	if (m->p.role == MAP_ROLE_CE || m->p.pool_num == 0)
		return -1;
	read_lock_bh(&m->pool_lock);
	list_for_each_entry(mp, &m->pool_list, list) {
		mask = 0xffffffff << (32 - mp->p.pool_prefix_length);
		if ((ntohl(cur) & mask) == ntohl(mp->p.pool_prefix)) {
			if (((ntohl(cur) + 1) & mask) ==
				ntohl(mp->p.pool_prefix)) {
				*next = htonl(ntohl(cur) + 1);
				read_unlock_bh(&m->pool_lock);
				return 0;
			}
			if (!list_is_last(&mp->list, &m->pool_list)) {
				mp = list_entry(mp->list.next, struct map_pool,
						list);
				*next = mp->p.pool_prefix;
				read_unlock_bh(&m->pool_lock);
				return 0;
			}
		}
	}
	read_unlock_bh(&m->pool_lock);
	return -1;
}

void
map_napt_debug_pool(struct map *m)
{
	__be32 paddr;
	if (!map_napt_first_pool(&paddr, m)) {
		pr_notice("map_napt_debug_pool: %d.%d.%d.%d\n",
			ntohl(paddr) >> 24,
			(ntohl(paddr) >> 16 & 0xff),
			(ntohl(paddr) >> 8 & 0xff),
			(ntohl(paddr) & 0xff));
	} else {
		pr_notice("map_napt_debug_pool: error\n");
		return;
	}
	while (!map_napt_next_pool(paddr, &paddr, m)) {
		pr_notice("map_napt_debug_pool: %d.%d.%d.%d\n",
			ntohl(paddr) >> 24,
			(ntohl(paddr) >> 16 & 0xff),
			(ntohl(paddr) >> 8 & 0xff),
			(ntohl(paddr) & 0xff));
	}
}

static inline int
map_napt_needed(struct map *m, int fb)
{
	if (m->p.role == MAP_ROLE_BR && m->p.pool_num > 0 && fb)
		return MAP_ROLE_BR;
	if (m->p.role == MAP_ROLE_CE && m->bmr &&
	    (m->psid_length > 0 || m->p.napt_always == MAP_NAPT_ALWAYS_T))
		return MAP_ROLE_CE;
	return 0;
}

/**
 *   @dir: 1 = in; 0 = out;
 **/

int
map_napt(struct iphdr *iph, int dir, struct map *m, __be32 **waddrpp,
	 __be16 **wportpp, __sum16 **checkpp, struct in6_addr *saddr6, int fb)
{
	__be32 *saddrp = NULL;
	__be16 *sportp = NULL;
	__u8 proto;
	__be32 *icmpaddr = NULL;
	__be32 paddr;
	u8 flags = 0;
	u8 *ptr;
	int err = 0;
	struct iphdr *icmpiph = NULL;
	struct tcphdr *tcph, *icmptcph;
	struct udphdr *udph, *icmpudph;
	struct icmphdr *icmph, *icmpicmph;
	int nested_icmp = 0;

	if (dir) {
		saddrp = &iph->daddr;
		*waddrpp = &iph->saddr;
	} else {
		saddrp = &iph->saddr;
		*waddrpp = &iph->daddr;
	}

	ptr = (u8 *)iph;
	ptr += iph->ihl * 4;
	switch (iph->protocol) {
	case IPPROTO_TCP:
		proto = IPPROTO_TCP;
		tcph = (struct tcphdr *)ptr;
		if (dir) {
			sportp = &tcph->dest;
			*wportpp = &tcph->source;
		} else {
			sportp = &tcph->source;
			*wportpp = &tcph->dest;
		}
		*checkpp = &tcph->check;
		if (tcph->syn)
			flags |= MAP_NAPT_TCP_F_SYN;
		if (tcph->ack)
			flags |= MAP_NAPT_TCP_F_ACK;
		if (tcph->fin)
			flags |= MAP_NAPT_TCP_F_FIN;
		if (tcph->rst)
			flags |= MAP_NAPT_TCP_F_RST;
		break;
	case IPPROTO_UDP:
		proto = IPPROTO_UDP;
		udph = (struct udphdr *)ptr;
		if (dir) {
			sportp = &udph->dest;
			*wportpp = &udph->source;
		} else {
			sportp = &udph->source;
			*wportpp = &udph->dest;
		}
		*checkpp = &udph->check;
		break;
	case IPPROTO_ICMP:
		proto = IPPROTO_ICMP;
		icmph = (struct icmphdr *)ptr;
		*checkpp = &icmph->checksum;
		switch (icmph->type) {
		case ICMP_DEST_UNREACH:
		case ICMP_SOURCE_QUENCH:
		case ICMP_TIME_EXCEEDED:
		case ICMP_PARAMETERPROB:
			ptr = (u8 *)icmph;
			ptr += sizeof(struct icmphdr);
			icmpiph = (struct iphdr *)ptr;
			if (ntohs(iph->tot_len) < icmpiph->ihl * 4 + 12) {
				err = -1;
				pr_notice("map_napt: ???\n");
				goto out;
			}
			if (dir) {
				saddrp = &icmpiph->saddr;
				*waddrpp = &icmpiph->daddr;
			} else {
				saddrp = &icmpiph->daddr;
				*waddrpp = &icmpiph->saddr;
			}
			ptr += icmpiph->ihl * 4;
			switch (icmpiph->protocol) {
			case IPPROTO_TCP:
				proto = IPPROTO_TCP;
				icmptcph = (struct tcphdr *)ptr;
				if (dir) {
					sportp = &icmptcph->source;
					*wportpp = &icmptcph->dest;
					icmpaddr = &iph->daddr;
				} else {
					sportp = &icmptcph->dest;
					*wportpp = &icmptcph->source;
					icmpaddr = &iph->saddr;
				}
				break;
			case IPPROTO_UDP:
				proto = IPPROTO_UDP;
				icmpudph = (struct udphdr *)ptr;
				if (dir) {
					sportp = &icmpudph->source;
					*wportpp = &icmpudph->dest;
					icmpaddr = &iph->daddr;
				} else {
					sportp = &icmpudph->dest;
					*wportpp = &icmpudph->source;
					icmpaddr = &iph->saddr;
				}
				break;
			case IPPROTO_ICMP:
				nested_icmp = 1;
				proto = IPPROTO_ICMP;
				icmpicmph = (struct icmphdr *)ptr;
				if (dir) {
					sportp = &icmpicmph->un.echo.id;
					*wportpp = &icmpicmph->un.echo.id;
					icmpaddr = &iph->daddr;
				} else {
					sportp = &icmpicmph->un.echo.id;
					*wportpp = &icmpicmph->un.echo.id;
					icmpaddr = &iph->saddr;
				}
				break;
			default:
				err = -1;
				pr_notice("map_napt: unknown proto in icmp.\n");
				goto out;
			}
			break;
		default:
			sportp = &icmph->un.echo.id;
			*wportpp = &icmph->un.echo.id;
			break;
		}
		break;
	default:
		err = -1;
		pr_notice("map_napt: unknown proto.\n");
		goto out;
	}

	if (saddrp && sportp && map_napt_needed(m, fb)) {
		err = map_napt_first_pool(&paddr, m);
		if (err) {
			pr_notice("map_napt: map_napt_first_pool err.\n");
			goto out;
		}
retry:
		err = map_napt_update(saddrp, sportp, **waddrpp,
			**wportpp, proto, saddr6, paddr, *checkpp, icmpaddr,
			dir, flags, nested_icmp, m);
		if (err) {
			if (!map_napt_next_pool(paddr, &paddr, m))
				goto retry;
			pr_notice("map_napt: map_napt_update failed(2). dir = %d err = %d\n",
				  dir, err);
			pr_notice("map_napt: s=%d.%d.%d.%d:%d(%04x) w=%d.%d.%d.%d:%d(%04x) proto=%d\n",
				((ntohl(*saddrp) >> 24) & 0xff),
				((ntohl(*saddrp) >> 16) & 0xff),
				((ntohl(*saddrp) >> 8) & 0xff),
				((ntohl(*saddrp)) & 0xff),
				ntohs(*sportp),
				ntohs(*sportp),
				((ntohl(**waddrpp) >> 24) & 0xff),
				((ntohl(**waddrpp) >> 16) & 0xff),
				((ntohl(**waddrpp) >> 8) & 0xff),
				((ntohl(**waddrpp)) & 0xff),
				ntohs(**wportpp),
				ntohs(**wportpp),
				proto);
			goto out;
		}
		/* XXX: */
		if (icmpiph) {
			__sum16 ocheck, ncheck;
			long csum;
			ocheck = icmpiph->check;
			icmpiph->check = 0;
			icmpiph->check = ip_fast_csum(
				(unsigned char *)icmpiph, icmpiph->ihl);
			ncheck = icmpiph->check;
			csum = ntohs(**checkpp);
			csum = ~csum & 0xffff;
			csum -= ntohs(ocheck) & 0xffff;
			CSUM_CO_DEC(csum)
			csum += ntohs(ncheck) & 0xffff;
			CSUM_CO_INC(csum)
			csum = ~csum & 0xffff;
			**checkpp = htons(csum);
		}
	}

out:
	return err;
}

int
map_napt_init(void)
{
	nn_kmem = kmem_cache_create("map_napt_node",
		sizeof(struct map_napt_node), 0, SLAB_HWCACHE_ALIGN, NULL);
	if (!nn_kmem)
		return -1;

	return 0;
}

void
map_napt_exit(void)
{
	kmem_cache_destroy(nn_kmem);
}
