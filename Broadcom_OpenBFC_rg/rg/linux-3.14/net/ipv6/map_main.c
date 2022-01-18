/* ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <m-asama@ginzado.co.jp> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return Masakazu Asama
 * ----------------------------------------------------------------------------
 */
/*	MAP device
 *
 *	Authors:
 *	Masakazu Asama		<m-asama@ginzado.co.jp>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/capability.h>
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
#include <linux/route.h>
#include <linux/rtnetlink.h>
#include <linux/netfilter_ipv6.h>

#include <net/ip.h>
#include <net/ipv6.h>
#include <net/icmp.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
/* #include <net/xfrm.h> */
#include <net/dsfield.h>
#include <net/inet_ecn.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <net/map.h>

MODULE_AUTHOR("Masakazu Asama");
MODULE_DESCRIPTION("MAP device");
MODULE_LICENSE("GPL");
MODULE_ALIAS_NETDEV("mapfb");

static int map_net_id __read_mostly;

struct pcpu_tstats {
	unsigned long		rx_packets;
	unsigned long		rx_bytes;
	unsigned long		tx_packets;
	unsigned long		tx_bytes;
};

static struct net_device_stats *
map_get_stats(struct net_device *dev)
{
	struct pcpu_tstats sum = { 0 };
	int i;

	for_each_possible_cpu(i) {
		const struct pcpu_tstats *tstats = per_cpu_ptr(dev->tstats, i);

		sum.rx_packets += tstats->rx_packets;
		sum.rx_bytes += tstats->rx_bytes;
		sum.tx_packets += tstats->tx_packets;
		sum.tx_bytes += tstats->tx_bytes;
	}

	dev->stats.rx_packets = sum.rx_packets;
	dev->stats.rx_bytes = sum.rx_bytes;
	dev->stats.tx_packets = sum.tx_packets;
	dev->stats.tx_bytes = sum.tx_bytes;

	return &dev->stats;
}

int
map_pool_free(struct map *m, struct map_pool *mp)
{
	list_del(&mp->list);
	kfree(mp);
	return 0;
}

int
map_pool_add(struct map *m, struct map_pool_parm *mpp)
{
	struct map_pool *mp;

	write_lock_bh(&m->pool_lock);
	list_for_each_entry(mp, &m->pool_list, list) {
		if (mp->p.pool_prefix == mpp->pool_prefix &&
		    mp->p.pool_prefix_length == mpp->pool_prefix_length) {
			write_unlock_bh(&m->pool_lock);
			return -1;
		}
	}
	mp = kmalloc(sizeof(*mp), GFP_KERNEL);
	if (!mp) {
		write_unlock_bh(&m->pool_lock);
		return -1;
	}
	mp->p = *mpp;
	list_add_tail(&mp->list, &m->pool_list);
	m->p.pool_num += 1;
	write_unlock_bh(&m->pool_lock);

	return 0;
}

int
map_pool_change(struct map *m, struct map_pool_parm *mpp)
{
	return 0;
}

int
map_pool_delete(struct map *m, struct map_pool_parm *mpp)
{
	struct map_pool *mp;

	write_lock_bh(&m->pool_lock);
	list_for_each_entry(mp, &m->pool_list, list) {
		if (mp->p.pool_prefix == mpp->pool_prefix &&
		    mp->p.pool_prefix_length == mpp->pool_prefix_length) {
			map_pool_free(m, mp);
			break;
		}
	}
	m->p.pool_num -= 1;
	write_unlock_bh(&m->pool_lock);

	return 0;
}

int
map_get_addrport(struct iphdr *iph, __be32 *saddr4, __be32 *daddr4,
	__be16 *sport4, __be16 *dport4, __u8 *proto, int *icmperr)
{
	u8 *ptr;
	struct iphdr *icmpiph = NULL;
	struct tcphdr *tcph, *icmptcph;
	struct udphdr *udph, *icmpudph;
	struct icmphdr *icmph, *icmpicmph;

	*icmperr = 0;
	*saddr4 = iph->saddr;
	*daddr4 = iph->daddr;
	ptr = (u8 *)iph;
	ptr += iph->ihl * 4;
	switch (iph->protocol) {
	case IPPROTO_TCP:
		*proto = IPPROTO_TCP;
		tcph = (struct tcphdr *)ptr;
		*sport4 = tcph->source;
		*dport4 = tcph->dest;
		break;
	case IPPROTO_UDP:
		*proto = IPPROTO_UDP;
		udph = (struct udphdr *)ptr;
		*sport4 = udph->source;
		*dport4 = udph->dest;
		break;
	case IPPROTO_ICMP:
		*proto = IPPROTO_ICMP;
		icmph = (struct icmphdr *)ptr;
		switch (icmph->type) {
		case ICMP_DEST_UNREACH:
		case ICMP_SOURCE_QUENCH:
		case ICMP_TIME_EXCEEDED:
		case ICMP_PARAMETERPROB:
			*icmperr = 1;
			ptr = (u8 *)icmph;
			ptr += sizeof(struct icmphdr);
			icmpiph = (struct iphdr *)ptr;
			*saddr4 = icmpiph->saddr;
			*daddr4 = icmpiph->daddr;
			if (ntohs(iph->tot_len) < icmpiph->ihl * 4 + 12) {
				pr_notice("map_get_addrport: ???\n");
				return -1;
			}
			ptr += icmpiph->ihl * 4;
			switch (icmpiph->protocol) {
			case IPPROTO_TCP:
				*proto = IPPROTO_TCP;
				icmptcph = (struct tcphdr *)ptr;
				*sport4 = icmptcph->source;
				*dport4 = icmptcph->dest;
				break;
			case IPPROTO_UDP:
				*proto = IPPROTO_UDP;
				icmpudph = (struct udphdr *)ptr;
				*sport4 = icmpudph->source;
				*dport4 = icmpudph->dest;
				break;
			case IPPROTO_ICMP:
				*proto = IPPROTO_ICMP;
				icmpicmph = (struct icmphdr *)ptr;
				*sport4 = icmpicmph->un.echo.id;
				*dport4 = icmpicmph->un.echo.id;
				break;
			default:
				pr_notice("map_get_addrport: innter icmp unknown proto(%d).\n",
					icmpiph->protocol);
				return -1;
			}
			break;
		default:
			*sport4 = icmph->un.echo.id;
			*dport4 = icmph->un.echo.id;
		}
		break;
	default:
		pr_notice("map_get_addrport: unknown proto(%d).\n",
			iph->protocol);
		return -1;
	}

	return 0;
}

int
map_gen_addr6(struct in6_addr *addr6, __be32 addr4, __be16 port4,
	struct map_rule *mr, int trans)
{
	int i, pbw0, pbi0, pbi1;
	__u32 addr[4];
	__u32 psid = 0;
	__u32 mask = 0;
	__u32 psid_mask;
	__u32 a = ntohl(addr4);
	__u16 p = ntohs(port4);
	int psid_length = mr->p.ipv4_prefix_length + mr->p.psid_prefix_length
			+ mr->p.ea_length - 32;

	if (!mr)
		return -1;

	if (psid_length < 0)
		a &= 0xffffffff << (psid_length * -1);

	if (psid_length > 0) {
		mask = 0xffffffff >> (32 - psid_length);
		psid = (p >> (16 - psid_length - mr->p.psid_offset)) & mask;
	}

	for (i = 0; i < 4; ++i)
		addr[i] = ntohl(mr->p.ipv6_prefix.s6_addr32[i]);

	if (mr->p.ipv4_prefix_length < 32) {
		pbw0 = mr->p.ipv6_prefix_length >> 5;
		pbi0 = mr->p.ipv6_prefix_length & 0x1f;
		addr[pbw0] |= (a << mr->p.ipv4_prefix_length) >> pbi0;
		pbi1 = pbi0 - mr->p.ipv4_prefix_length;
		if (pbi1 > 0)
			addr[pbw0+1] |= a << (32 - pbi1);
	}

	if ((psid_length - mr->p.psid_prefix_length) > 0) {
		psid_mask = (1 << (psid_length - mr->p.psid_prefix_length)) - 1;
		pbw0 = (mr->p.ipv6_prefix_length + 32
			- mr->p.ipv4_prefix_length - mr->p.psid_prefix_length)
				>> 5;
		pbi0 = (mr->p.ipv6_prefix_length + 32
			- mr->p.ipv4_prefix_length - mr->p.psid_prefix_length)
				& 0x1f;
		addr[pbw0] |= ((psid & psid_mask) << (32 - psid_length))
			>> pbi0;
		pbi1 = pbi0 - (32 - psid_length);
		if (pbi1 > 0)
			addr[pbw0+1] |= (psid & psid_mask) << (32 - pbi1);
	}

	/* XXX: */
	if (trans) {
		addr[2] |= (ntohl(addr4) >> 16);
		addr[3] |= (ntohl(addr4) << 16);
	} else {
		addr[2] |= (a >> 16);
		addr[3] |= (a << 16);
	}
	addr[3] |= psid;

	for (i = 0; i < 4; ++i)
		addr6->s6_addr32[i] = htonl(addr[i]);

	return 0;
}

static int
map_v4v6(struct sk_buff *skb, struct map *m)
{
	struct iphdr *iph = ip_hdr(skb);
	struct map_rule *mr;
	__u8 forwarding_mode;
	struct pcpu_tstats *tstats = this_cpu_ptr(m->dev->tstats);
	int tx_bytes = skb->len;
	int err = 0;
	u32 mtu;
	__be32 saddr4, daddr4;
	__be16 sport4, dport4;
	__u8 proto;
	int icmperr;
	int fb = 0;
	int df = 0;

	if (iph->frag_off & htons(IP_DF))
		df = 1;

#ifdef MAP_DEBUG
	pr_notice("map_v4v6: %s: %d.%d.%d.%d -> %d.%d.%d.%d\n",
		m->dev->name,
		ntohl(iph->saddr) >> 24,
		(ntohl(iph->saddr) >> 16) & 0xff,
		(ntohl(iph->saddr) >> 8) & 0xff,
		ntohl(iph->saddr) & 0xff,
		ntohl(iph->daddr) >> 24,
		(ntohl(iph->daddr) >> 16) & 0xff,
		(ntohl(iph->daddr) >> 8) & 0xff,
		ntohl(iph->daddr) & 0xff);
#endif

	if (ntohs(iph->frag_off) & IP_OFFSET) {
		if (ip_defrag(skb, IP_DEFRAG_MAP46))
			return 0;
		iph = ip_hdr(skb);
	}
	map_get_addrport(iph, &saddr4, &daddr4, &sport4, &dport4, &proto,
		&icmperr);
#ifdef MAP_DEBUG
	pr_notice("map_v4v6: %d.%d.%d.%d:%04x -> %d.%d.%d.%d:%04x %d\n",
		ntohl(saddr4) >> 24,
		(ntohl(saddr4) >> 16) & 0xff,
		(ntohl(saddr4) >> 8) & 0xff,
		ntohl(saddr4) & 0xff,
		ntohs(sport4),
		ntohl(daddr4) >> 24,
		(ntohl(daddr4) >> 16) & 0xff,
		(ntohl(daddr4) >> 8) & 0xff,
		ntohl(daddr4) & 0xff,
		ntohs(dport4),
		proto);
#endif
	if (icmperr)
		mr = map_rule_find_by_ipv4addrport(m, &saddr4, &sport4, 1);
	else
		mr = map_rule_find_by_ipv4addrport(m, &daddr4, &dport4, 1);
	if (mr)
		forwarding_mode = mr->p.forwarding_mode;
	else {
		forwarding_mode = m->p.default_forwarding_mode;
		if (m->p.role == MAP_ROLE_BR)
			fb = 1;
	}

	if ((forwarding_mode != MAP_FORWARDING_MODE_T) &&
	    (forwarding_mode != MAP_FORWARDING_MODE_E)) {
		pr_notice("map_v4v6: unknown forwarding mode.\n");
		err = -1;
		goto err;
	}

	switch (forwarding_mode) {
	case MAP_FORWARDING_MODE_T:
		/* mtu = 1280 - sizeof(struct ipv6hdr) +
		 *	sizeof(struct iphdr);
		 */
		mtu = m->p.ipv6_fragment_size - sizeof(struct ipv6hdr)
			+ sizeof(struct iphdr);
		break;
	case MAP_FORWARDING_MODE_E:
		/* mtu = 1280 - sizeof(struct ipv6hdr); */
		mtu = m->p.ipv6_fragment_size - sizeof(struct ipv6hdr);
		break;
	}

	if ((iph->frag_off & htons(IP_DF)) && skb->len > mtu) {
		pr_notice("map_v4v6: skb->len = %d mtu = %d\n",
			skb->len, mtu);
		icmp_send(skb, ICMP_DEST_UNREACH, ICMP_FRAG_NEEDED, htonl(mtu));
		err = -1;
		goto drp;
	}

	if (ip_defrag(skb, IP_DEFRAG_MAP46))
		return 0;

	switch (forwarding_mode) {
	case MAP_FORWARDING_MODE_T:
		err = map_trans_forward_v4v6(skb, m, mr, fb, df);
		break;
	case MAP_FORWARDING_MODE_E:
		err = map_encap_forward_v4v6(skb, m, mr, fb);
		break;
	}

	if (err) {
		pr_notice("map_v4v6: forwarding error.\n");
		goto err;
	}

	tstats->tx_packets++;
	tstats->tx_bytes += tx_bytes;

	return 0;

err:
	m->dev->stats.tx_errors++;
drp:
	m->dev->stats.tx_dropped++;
	kfree_skb(skb);
	return err;
}

struct sk_buff *
map_defrag4(struct sk_buff *skb, struct map *m)
{
	struct ipv6hdr *ipv6h, ipv6h_orig;
	struct iphdr *iph;
	void *ptr;
	unsigned int max_headroom;

	ipv6h = ipv6_hdr(skb);
	if (ipv6h->nexthdr != IPPROTO_IPIP)
		return skb;

	ptr = ipv6h;
	ptr += sizeof(*ipv6h);
	iph = (struct iphdr *)ptr;
	if (!(iph->frag_off & htons(IP_OFFSET | IP_MF)))
		return skb;

	memcpy(&ipv6h_orig, ipv6h, sizeof(struct ipv6hdr));

	skb_pull(skb, sizeof(struct ipv6hdr));
	skb_reset_network_header(skb);
	skb->protocol = htons(ETH_P_IP);

	if (ip_defrag(skb, IP_DEFRAG_MAP46))
		return NULL;

	max_headroom = sizeof(struct ipv6hdr) + sizeof(struct frag_hdr) + 20;
	if (skb_headroom(skb) < max_headroom || skb_shared(skb) ||
	    (skb_cloned(skb) && !skb_clone_writable(skb, 0))) {
		struct sk_buff *new_skb;
		new_skb = skb_realloc_headroom(skb, max_headroom);
		if (!new_skb) {
			kfree_skb(skb);
			return NULL;
		}
		if (skb->sk)
			skb_set_owner_w(new_skb, skb->sk);
		kfree_skb(skb);
		skb = new_skb;
	}

	skb_push(skb, sizeof(struct ipv6hdr));
	skb_reset_network_header(skb);
	skb->protocol = htons(ETH_P_IPV6);

	ipv6h = ipv6_hdr(skb);
	memcpy(ipv6h, &ipv6h_orig, sizeof(struct ipv6hdr));

	return skb;
}

static int
map_v6v4(struct sk_buff *skb, struct map *m)
{
	struct ipv6hdr *ip6h = ipv6_hdr(skb);
	__u8 forwarding_mode;
	__be32 saddr4, daddr4;
	struct pcpu_tstats *tstats = this_cpu_ptr(m->dev->tstats);
	int rx_bytes = skb->len;
	int fb = 0;
	int frag = 0;
	int err = 0;

	if (ip6h->nexthdr == IPPROTO_FRAGMENT)
		frag = 1;

#ifdef MAP_DEBUG
	pr_notice("map_v6v4: %s: %08x %08x %08x %08x -> %08x %08x %08x %08x\n",
		m->dev->name,
		ntohl(ip6h->saddr.s6_addr32[0]),
		ntohl(ip6h->saddr.s6_addr32[1]),
		ntohl(ip6h->saddr.s6_addr32[2]),
		ntohl(ip6h->saddr.s6_addr32[3]),
		ntohl(ip6h->daddr.s6_addr32[0]),
		ntohl(ip6h->daddr.s6_addr32[1]),
		ntohl(ip6h->daddr.s6_addr32[2]),
		ntohl(ip6h->daddr.s6_addr32[3]));
#endif
	skb = map_defrag6(skb, m);
	if (skb == NULL)
		return 0;

	skb = map_defrag4(skb, m);
	if (skb == NULL)
		return 0;

	read_lock(&m->rule_lock);
	if (m->bmr)
		forwarding_mode = m->bmr->p.forwarding_mode;
	else
		forwarding_mode = m->p.default_forwarding_mode;
	read_unlock(&m->rule_lock);

	switch (forwarding_mode) {
	case MAP_FORWARDING_MODE_T:
		err = map_trans_validate_dst(skb, m, &daddr4);
		if (err)
			goto drp;
		err = map_trans_validate_src(skb, m, &saddr4, &fb);
		if (err && !fb)
			goto drp;
		err = map_trans_forward_v6v4(skb, m, &saddr4, &daddr4, fb,
					     frag);
		if (err) {
			pr_notice("map_v6v4: map_trans_forward_v6v4 error.\n");
			goto err;
		}
		break;
	case MAP_FORWARDING_MODE_E:
		err = map_encap_validate_dst(skb, m, &daddr4);
		if (err)
			goto drp;
		err = map_encap_validate_src(skb, m, &saddr4, &fb);
		if (err && !fb)
			goto drp;
		err = map_encap_forward_v6v4(skb, m, &saddr4, &daddr4, fb);
		if (err) {
			pr_notice("map_v6v4: map_encap_forward_v6v4 error.\n");
			goto err;
		}
		break;
	default:
		pr_notice("map_v6v4: unknown forwarding mode.\n");
		err = -1;
		goto err;
	}

	tstats->rx_packets++;
	tstats->rx_bytes += rx_bytes;

	return 0;

err:
	m->dev->stats.rx_errors++;
drp:
	m->dev->stats.rx_dropped++;
	kfree_skb(skb);
	return err;
}

static netdev_tx_t
map_transmit(struct sk_buff *skb, struct net_device *dev)
{
	struct map *m = netdev_priv(dev);
	struct in6_addr zero_addr = {};
	struct ipv6hdr *ipv6h;
	struct icmp6hdr *icmp6h;

	if (ipv6_addr_equal(&m->map_ipv6_address, &zero_addr))
		return NETDEV_TX_OK;

	switch (ntohs(skb->protocol)) {
	case ETH_P_IP:
		map_v4v6(skb, m);
		break;
	case ETH_P_IPV6:
		ipv6h = ipv6_hdr(skb);
		if (ipv6h->nexthdr == IPPROTO_ICMPV6) {
			icmp6h = (struct icmp6hdr *)(ipv6h + 1);
			if (icmp6h->icmp6_type == ICMPV6_PKT_TOOBIG) {
				ip6_update_pmtu(skb, dev_net(skb->dev),
					ntohl(icmp6h->icmp6_mtu), 0, 0);
			}
		}
		map_v6v4(skb, m);
		break;
	default:
		pr_notice("map_transmit: unknown protocol.\n");
	}

	return NETDEV_TX_OK;
}

#if 0
static int
map_receive(struct sk_buff *skb)
{
	struct map_net *mapn = net_generic(dev_net(skb->dev), map_net_id);
	struct map *m = NULL, *tmp;
	struct ipv6hdr *ip6h = ipv6_hdr(skb);

	pr_notice("map_receive:\n");

	if (ntohs(skb->protocol) != ETH_P_IPV6)
		return 0;

	read_lock(&mapn->map_list_lock);
	list_for_each_entry(tmp, &mapn->map_list, list) {
		if (ipv6_addr_equal(&ip6h->daddr, &tmp->map_ipv6_address)) {
			m = tmp;
			break;
		}
	}
	read_unlock(&mapn->map_list_lock);

	if (m)
		map_v6v4(skb, m);

	return 0;
}

static int
map_error(struct sk_buff *skb, struct inet6_skb_parm *opt, u8 type, u8 code,
	int offset, __be32 info)
{
	return 0;
}
#endif

static int
map_change_mtu(struct net_device *dev, int new_mtu)
{
	return 0;
}

static void
map_uninit(struct net_device *dev)
{
	dev_put(dev);
}

static int map_open(struct net_device *dev);
static int map_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);

static const struct net_device_ops map_netdev_ops = {
	.ndo_uninit	= map_uninit,
	.ndo_open	= map_open,
	.ndo_start_xmit	= map_transmit,
	.ndo_do_ioctl	= map_ioctl,
	.ndo_change_mtu	= map_change_mtu,
	.ndo_get_stats	= map_get_stats,
};

#if 0
static struct xfrm6_tunnel map_handler __read_mostly = {
	.handler	= map_receive,
	.err_handler	= map_error,
	.priority	= 1,
};
#endif

static void
map_debug_dump(struct map_net *mapn)
{
	struct map *m;
	struct map_rule *mr;
	int i, j;

	i = 0;
	read_lock(&mapn->map_list_lock);
	list_for_each_entry(m, &mapn->map_list, list) {
		pr_notice("map[%d]:\n", i);
		pr_notice("    p.name = %s\n", m->p.name);
		pr_notice("    p.tunnel_source = %d\n",
			m->p.tunnel_source);
		pr_notice("    p.br_address = %08x %08x %08x %08x\n",
			ntohl(m->p.br_address.s6_addr32[0]),
			ntohl(m->p.br_address.s6_addr32[1]),
			ntohl(m->p.br_address.s6_addr32[2]),
			ntohl(m->p.br_address.s6_addr32[3]));
		pr_notice("    p.br_address_length = %d\n",
			m->p.br_address_length);
		pr_notice("    p.role = %02x\n", m->p.role);
		pr_notice("    p.default_forwarding_mode = %02x\n",
			m->p.default_forwarding_mode);
		pr_notice("    p.default_forwarding_rule = %02x\n",
			m->p.default_forwarding_rule);
		pr_notice("    p.ipv6_fragment_size = %d\n",
			m->p.ipv6_fragment_size);
		pr_notice("    p.ipv4_fragment_inner = %02x\n",
			m->p.ipv4_fragment_inner);
		pr_notice("    p.napt_always = %02x\n",
			m->p.napt_always);
		pr_notice("    p.napt_force_recycle = %02x\n",
			m->p.napt_force_recycle);
		pr_notice("    p.rule_num = %lu\n", m->p.rule_num);
		pr_notice("    bmr = %p\n", m->bmr);
		pr_notice("    map_ipv6_address = %08x %08x %08x %08x\n",
			ntohl(m->map_ipv6_address.s6_addr32[0]),
			ntohl(m->map_ipv6_address.s6_addr32[1]),
			ntohl(m->map_ipv6_address.s6_addr32[2]),
			ntohl(m->map_ipv6_address.s6_addr32[3]));
		pr_notice("    map_ipv6_address_length = %d\n",
			m->map_ipv6_address_length);
		pr_notice("    laddr4 = %d.%d.%d.%d\n",
			ntohl(m->laddr4) >> 24,
			(ntohl(m->laddr4) >> 16) & 0xff,
			(ntohl(m->laddr4) >> 8) & 0xff,
			ntohl(m->laddr4) & 0xff);
		pr_notice("    psid = 0x%04x\n", m->psid);
		pr_notice("    psid_length = %d\n", m->psid_length);
		for (j = 0; j < m->port_range_length; ++j) {
			pr_notice("    port_range[%4d] = %6d(0x%04x) - %6d(0x%04x)\n",
				j,
				m->port_range[j].min, m->port_range[j].min,
				m->port_range[j].max, m->port_range[j].max);
		}
		for (j = 0; j < 17; ++j) {
			pr_notice("    psid_offset_nums[%2d] = %d\n",
				j, m->psid_offset_nums[j]);
		}
		j = 0;
		read_lock(&m->rule_lock);
		list_for_each_entry(mr, &m->rule_list, list) {
#ifdef MAP_DEBUG
			pr_notice("    map_rule[%d](%p):\n", j, mr);
			pr_notice("        p.ipv6_prefix = %08x %08x %08x %08x\n",
				ntohl(mr->p.ipv6_prefix.s6_addr32[0]),
				ntohl(mr->p.ipv6_prefix.s6_addr32[1]),
				ntohl(mr->p.ipv6_prefix.s6_addr32[2]),
				ntohl(mr->p.ipv6_prefix.s6_addr32[3]));
			pr_notice("        p.ipv6_prefix_length = %d\n",
			mr->p.ipv6_prefix_length);
			pr_notice("        p.ipv4_prefix = %d.%d.%d.%d\n",
				ntohl(mr->p.ipv4_prefix) >> 24,
				(ntohl(mr->p.ipv4_prefix) >> 16) & 0xff,
				(ntohl(mr->p.ipv4_prefix) >> 8) & 0xff,
				ntohl(mr->p.ipv4_prefix) & 0xff);
			pr_notice("        p.ipv4_prefix_length = %d\n",
				mr->p.ipv4_prefix_length);
			pr_notice("        p.psid_prefix = 0x%04x\n",
				mr->p.psid_prefix);
			pr_notice("        p.psid_preix_length = %d\n",
				mr->p.psid_prefix_length);
			pr_notice("        p.ea_length = %d\n",
				mr->p.ea_length);
			pr_notice("        p.psid_offset = %d\n",
				mr->p.psid_offset);
			pr_notice("        p.forwarding_mode = %02x\n",
				mr->p.forwarding_mode);
			pr_notice("        p.forwarding_rule = %02x\n",
				mr->p.forwarding_rule);
#endif
			++j;
		}
#ifdef MAP_DEBUG
		pr_notice("    mrtn_root_ipv6addr\n");
		mrtree_node_dump(m->mrtn_root_ipv6addr);
		pr_notice("    mrtn_root_ipv4addrport\n");
		mrtree_node_dump(m->mrtn_root_ipv4addrport);
#endif
		read_unlock(&m->rule_lock);
		++i;
	}
	read_unlock(&mapn->map_list_lock);
}

static void
map_destroy(struct map_net *mapn)
{
	struct map *m, *q;
	LIST_HEAD(list);

	write_lock_bh(&mapn->map_list_lock);
	list_for_each_entry_safe(m, q, &mapn->map_list, list) {
		unregister_netdevice_queue(m->dev, &list);
	}
	write_unlock_bh(&mapn->map_list_lock);

	unregister_netdevice_many(&list);
}

static inline int
map_get_psid_length(struct map_rule *mr)
{
	return mr->p.ipv4_prefix_length + mr->p.psid_prefix_length
		+ mr->p.ea_length - 32;
}

static inline __u16
map_get_psid(struct map_rule *mr, struct in6_addr *ipv6addr)
{
	int psid_length = map_get_psid_length(mr);
	__u16 psid = 0;
	int pbw0, pbi0, pbi1;
	__u32 d = 0;

	if (psid_length <= 0)
		return 0;

	if (mr->p.psid_prefix_length > 0)
		psid = mr->p.psid_prefix
			<< (psid_length - mr->p.psid_prefix_length);

	if (mr->p.ea_length > 0) {
		pbw0 = (mr->p.ipv6_prefix_length + mr->p.ea_length
			+ mr->p.psid_prefix_length - psid_length) >> 5;
		pbi0 = (mr->p.ipv6_prefix_length + mr->p.ea_length
			+ mr->p.psid_prefix_length - psid_length) & 0x1f;
		d = (ntohl(ipv6addr->s6_addr32[pbw0]) << pbi0)
			>> (32 - (psid_length - mr->p.psid_prefix_length));
		pbi1 = pbi0 - (32 - (psid_length - mr->p.psid_prefix_length));
		if (pbi1 > 0)
			d |= ntohl(ipv6addr->s6_addr32[pbw0+1]) >> (32 - pbi1);
		psid |= d;
	}

	return psid;
}

static inline __be32
map_get_laddr4(struct map_rule *mr, struct in6_addr *ipv6addr)
{
	int psid_length = map_get_psid_length(mr);
	__be32 laddr4 = mr->p.ipv4_prefix;
	int pbw0, pbi0, pbi1;
	__u32 d;

	if (mr->p.ipv4_prefix_length < 32) {
		pbw0 = mr->p.ipv6_prefix_length >> 5;
		pbi0 = mr->p.ipv6_prefix_length & 0x1f;
		d = (ntohl(ipv6addr->s6_addr32[pbw0]) << pbi0) >>
			mr->p.ipv4_prefix_length;
		pbi1 = pbi0 - mr->p.ipv4_prefix_length;
		if (pbi1 > 0)
			d |= ntohl(ipv6addr->s6_addr32[pbw0+1]) >> (32 - pbi1);
		laddr4 |= htonl(d);
	}

	if (psid_length < 0) {
		d = ntohl(laddr4);
		d &= 0xffffffff << (psid_length * -1);
		laddr4 = htonl(d);
	}

	return laddr4;
}

int
map_get_map_ipv6_address(struct map_rule *mr, struct in6_addr *ipv6addr,
	struct in6_addr *map_ipv6_address)
{
	int psid_length = map_get_psid_length(mr);
	__u32 psid = map_get_psid(mr, ipv6addr);
	__u32 psid_mask;
	__be32 laddr4 = map_get_laddr4(mr, ipv6addr);
	int pbw0, pbi0, pbi1;

	memcpy(map_ipv6_address, &mr->p.ipv6_prefix, sizeof(*map_ipv6_address));

	if (mr->p.ipv4_prefix_length < 32) {
		pbw0 = mr->p.ipv6_prefix_length >> 5;
		pbi0 = mr->p.ipv6_prefix_length & 0x1f;
		map_ipv6_address->s6_addr32[pbw0] |= htonl((ntohl(laddr4)
			<< mr->p.ipv4_prefix_length) >> pbi0);
		pbi1 = pbi0 - mr->p.ipv4_prefix_length;
		if (pbi1 > 0)
			map_ipv6_address->s6_addr32[pbw0+1]
				|= htonl(ntohl(laddr4) << (32 - pbi1));
	}

	if ((psid_length - mr->p.psid_prefix_length) > 0) {
		psid_mask = (1 << (psid_length - mr->p.psid_prefix_length)) - 1;
		pbw0 = (mr->p.ipv6_prefix_length + 32
			- mr->p.ipv4_prefix_length - mr->p.psid_prefix_length)
				>> 5;
		pbi0 = (mr->p.ipv6_prefix_length + 32
			- mr->p.ipv4_prefix_length - mr->p.psid_prefix_length)
				& 0x1f;
		map_ipv6_address->s6_addr32[pbw0] |= htonl(((psid & psid_mask)
			<< (32 - psid_length)) >> pbi0);
		pbi1 = pbi0 - (32 - psid_length);
		if (pbi1 > 0)
			map_ipv6_address->s6_addr32[pbw0+1]
				|= htonl((psid & psid_mask) << (32 - pbi1));
	}

	map_ipv6_address->s6_addr32[2] |= htonl(ntohl(laddr4) >> 16);
	map_ipv6_address->s6_addr32[3] |= htonl(ntohl(laddr4) << 16);
	map_ipv6_address->s6_addr32[3] |= htonl(psid);

#ifdef MAP_DEBUG
	pr_notice("* psid = 0x%02x psid_length = %d\n", psid,
		psid_length);
	pr_notice("* laddr4 = %d.%d.%d.%d\n",
		ntohl(laddr4) >> 24,
		(ntohl(laddr4) >> 16) & 0xff,
		(ntohl(laddr4) >> 8) & 0xff,
		ntohl(laddr4) & 0xff);
	pr_notice("* map_ipv6_address = %08x %08x %08x %08x\n",
		ntohl(map_ipv6_address->s6_addr32[0]),
		ntohl(map_ipv6_address->s6_addr32[1]),
		ntohl(map_ipv6_address->s6_addr32[2]),
		ntohl(map_ipv6_address->s6_addr32[3]));
#endif

	return 0;
}

static void
map_route6_del(struct in6_addr *addr, int len, struct map *m)
{
	struct fib6_config cfg = {};
	struct in6_addr prefix, zero_addr = {};
	if (ipv6_addr_equal(addr, &zero_addr))
		return;
	ipv6_addr_prefix(&prefix, addr, len);
	cfg.fc_table = RT6_TABLE_MAIN;
	cfg.fc_ifindex = m->dev->ifindex;
	cfg.fc_metric = IP6_RT_PRIO_USER;
	cfg.fc_dst_len = len;
	cfg.fc_flags = RTF_UP;
	cfg.fc_nlinfo.nl_net = dev_net(m->dev);
	cfg.fc_dst = prefix;
	pr_notice("map_route6_del: %08x%08x%08x%08x/%d\n",
		ntohl(prefix.s6_addr32[0]),
		ntohl(prefix.s6_addr32[1]),
		ntohl(prefix.s6_addr32[2]),
		ntohl(prefix.s6_addr32[3]),
		len);
	ip6_route_del(&cfg);
}

static void
map_route6_add(struct in6_addr *addr, int len, struct map *m)
{
	struct fib6_config cfg = {};
	struct in6_addr prefix, zero_addr = {};
	if (ipv6_addr_equal(addr, &zero_addr))
		return;
	ipv6_addr_prefix(&prefix, addr, len);
	cfg.fc_table = RT6_TABLE_MAIN;
	cfg.fc_ifindex = m->dev->ifindex;
	cfg.fc_metric = IP6_RT_PRIO_USER;
	cfg.fc_dst_len = len;
	cfg.fc_flags = RTF_UP;
	cfg.fc_nlinfo.nl_net = dev_net(m->dev);
	cfg.fc_dst = prefix;
	pr_notice("map_route6_add: %08x%08x%08x%08x/%d\n",
		ntohl(prefix.s6_addr32[0]),
		ntohl(prefix.s6_addr32[1]),
		ntohl(prefix.s6_addr32[2]),
		ntohl(prefix.s6_addr32[3]),
		len);
	ip6_route_add(&cfg);
}

static int
map_update(struct map *m)
{
	struct net *net = dev_net(m->dev);
	struct map_net *mapn = net_generic(net, map_net_id);
	struct net_device *dev = NULL;
	struct inet6_dev *idev = NULL;
	struct inet6_ifaddr *ifa;
	struct map_rule *mr = NULL;
	struct in6_addr map_ipv6_address, orig_map_ipv6_address;
	struct in6_addr first_addr = {};
	int orig_map_ipv6_address_length;

	orig_map_ipv6_address = m->map_ipv6_address;
	orig_map_ipv6_address_length = m->map_ipv6_address_length;

	if (m->p.role == MAP_ROLE_CE)
		dev = dev_get_by_index(net, m->p.tunnel_source);
	if (dev)
		idev = in6_dev_get(dev);
	if (idev) {
		int first = 1;
		read_lock_bh(&idev->lock);
		/* XXX: */
		list_for_each_entry(ifa, &idev->addr_list, if_list) {
			if (first && !ifa->scope) {
				first_addr = ifa->addr;
				first = 0;
			}
			mr = map_rule_find_by_ipv6addr(m, &ifa->addr);
			if (mr) {
				map_get_map_ipv6_address(mr, &ifa->addr,
					&map_ipv6_address);
				break;
			}
		}
		read_unlock_bh(&idev->lock);
	}
	if (idev)
		in6_dev_put(idev);
	if (dev)
		dev_put(dev);

	write_lock_bh(&m->rule_lock);
	if (m->p.role == MAP_ROLE_CE && mr) {
		int i;
		__u16 min, max, p1, p2, p3;

		int port_range_length;
		struct map_napt_block *new_port_range, *old_port_range;
		m->bmr = mr;
		m->laddr4 = map_get_laddr4(mr, &map_ipv6_address);
		m->psid = map_get_psid(mr, &map_ipv6_address);
		m->psid_length = map_get_psid_length(mr);
		memcpy(&m->map_ipv6_address, &map_ipv6_address,
			sizeof(m->map_ipv6_address));
		if (m->bmr->p.forwarding_mode == MAP_FORWARDING_MODE_T &&
		    m->psid_length < 0)
			m->map_ipv6_address_length = 80 +
			m->bmr->p.ipv4_prefix_length + m->bmr->p.ea_length;
		else
			m->map_ipv6_address_length = 128;

		write_lock_bh(&m->port_range_lock);
		if (mr->p.psid_offset == 0)
			port_range_length = 1;
		else
			port_range_length = (1 << mr->p.psid_offset) - 1;
		old_port_range = m->port_range;
		new_port_range = kmalloc(sizeof(struct map_napt_block) *
			port_range_length, GFP_KERNEL);
		if (new_port_range) {
			if (mr->p.psid_offset == 0) {
				if (m->psid_length > 0) {
					p1 = m->psid << (16 - m->psid_length);
					p2 = 0xffff >> m->psid_length;
					min = p1;
					max = p1 | p2;
					new_port_range[0].min = min;
					new_port_range[0].max = max;
				} else {
					new_port_range[0].min = 0x1000;
					new_port_range[0].max = 0xffff;
				}
			} else {
				for (i = 0; i < port_range_length; i++) {
					if (m->psid_length > 0) {
						p1 = (i + 1) << (16 -
							mr->p.psid_offset);
						p2 = m->psid << (16 -
							mr->p.psid_offset -
							m->psid_length);
						p3 = 0xffff >>
							(mr->p.psid_offset +
							m->psid_length);
						min = p1 | p2;
						max = p1 | p2 | p3;
						new_port_range[i].min = min;
						new_port_range[i].max = max;
					} else {
						p1 = (i + 1) << (16 -
							mr->p.psid_offset);
						p3 = 0xffff >>
							(mr->p.psid_offset);
						min = p1;
						max = p1 | p3;
						new_port_range[i].min = min;
						new_port_range[i].max = max;
					}
				}
			}
			m->port_range = new_port_range;
			m->port_range_length = port_range_length;
		} else {
			m->port_range = NULL;
			m->port_range_length = 0;
		}
		kfree(old_port_range);
		write_unlock_bh(&m->port_range_lock);
	} else {
		m->bmr = NULL;
		if (m->p.role == MAP_ROLE_BR) {
			memcpy(&m->map_ipv6_address, &m->p.br_address,
				sizeof(m->map_ipv6_address));
			m->map_ipv6_address_length = m->p.br_address_length;
		} else {
			m->map_ipv6_address.s6_addr32[0]
				= first_addr.s6_addr32[0];
			m->map_ipv6_address.s6_addr32[1]
				= first_addr.s6_addr32[1];
			if (m->p.default_forwarding_mode ==
			    MAP_FORWARDING_MODE_E) {
				m->map_ipv6_address.s6_addr32[2]
					= htonl(0x00c00000);
				m->map_ipv6_address.s6_addr32[3]
					= htonl(0x02000000);
				m->map_ipv6_address_length = 128;
			} else {
				if (m->p.br_address_length > 64) {
					m->map_ipv6_address.s6_addr32[2]
						= htonl(0x00006464);
					m->map_ipv6_address.s6_addr32[3]
						= htonl(0x00000000);
					m->map_ipv6_address_length = 96;
				} else {
					m->map_ipv6_address.s6_addr32[2]
						= htonl(0x00000000);
					m->map_ipv6_address.s6_addr32[3]
						= htonl(0x00000000);
					m->map_ipv6_address_length = 72;
				}
			}
		}
		m->laddr4 = 0;
		m->psid = 0;
		m->psid_length = 0;

		write_lock_bh(&m->port_range_lock);
		kfree(m->port_range);
		m->port_range = kmalloc(sizeof(struct map_napt_block),
					GFP_KERNEL);
		m->port_range[0].min = 0x1000;
		m->port_range[0].max = 0xffff;
		m->port_range_length = 1;
		write_unlock_bh(&m->port_range_lock);
	}
	write_unlock_bh(&m->rule_lock);

	pr_notice("map_update: begin\n");
	map_debug_dump(mapn);
	pr_notice("map_update: end\n");

	if ((!ipv6_addr_equal(&orig_map_ipv6_address, &m->map_ipv6_address) ||
	    orig_map_ipv6_address_length != m->map_ipv6_address_length)) {
		map_route6_del(&orig_map_ipv6_address,
			orig_map_ipv6_address_length, m);
		map_route6_add(&m->map_ipv6_address, m->map_ipv6_address_length,
			m);
	}

	return 0;
}

static void
map_free(struct net_device *dev)
{
	struct map *m = netdev_priv(dev);
	struct map_net *mapn = net_generic(dev_net(dev), map_net_id);
	struct map_rule *mr, *mrq;
	struct map_pool *mp, *mpq;
	struct map_defrag6_node *dn, *dn_node;

	pr_notice("map_free: %s\n", m->dev->name);

	write_lock_bh(&m->defrag6_lock);
	list_for_each_entry_safe(dn, dn_node, &m->defrag6_list, dn_list) {
		kfree_skb(dn->skb);
	}
	write_unlock_bh(&m->defrag6_lock);

	write_lock_bh(&m->port_range_lock);
	kfree(m->port_range);
	write_unlock_bh(&m->port_range_lock);

	write_lock_bh(&m->rule_lock);
	list_for_each_entry_safe(mr, mrq, &m->rule_list, list) {
		/* list_del(&mr->list);
		 * kfree(mr);
		 */
		map_rule_free(m, mr);
		m->p.rule_num -= 1;
	}
	write_unlock_bh(&m->rule_lock);
	m->p.rule_num = 0;

	write_lock_bh(&m->pool_lock);
	list_for_each_entry_safe(mp, mpq, &m->pool_list, list) {
		map_pool_free(m, mp);
		m->p.pool_num -= 1;
	}
	write_unlock_bh(&m->pool_lock);
	m->p.pool_num = 0;

	write_lock_bh(&mapn->map_list_lock);
	list_del(&m->list);
	write_unlock_bh(&mapn->map_list_lock);

	free_percpu(dev->tstats);
	free_netdev(dev);
}

static int
map_change(struct map *m, struct map_parm *p)
{
	int old_rule_num;

	old_rule_num = m->p.rule_num;
	m->p = *p;
	m->p.rule_num = old_rule_num;

	map_update(m);

	return 0;
}

static int
map_init(struct net_device *dev, struct map_parm *p)
{
	int h, i;
	struct map *m = netdev_priv(dev);

	if (p)
		m->p = *p;
	m->p.rule_num = 0;
	m->p.pool_num = 0;
	m->dev = dev;
	INIT_LIST_HEAD(&m->rule_list);
	m->mrtn_root_ipv6addr = NULL;
	m->mrtn_root_ipv4addrport = NULL;
	INIT_LIST_HEAD(&m->pool_list);
	/* */
	rwlock_init(&m->rule_lock);
	rwlock_init(&m->pool_lock);
	rwlock_init(&m->port_range_lock);

	for (h = 0; h < MAP_NAPT_HASH_LOOKUP_SIZE; ++h)
		INIT_HLIST_HEAD(&m->napt_hash_lup0[h]);
	for (h = 0; h < MAP_NAPT_HASH_LOOKUP_SIZE; ++h)
		INIT_HLIST_HEAD(&m->napt_hash_lup1[h]);
	for (h = 0; h < MAP_NAPT_HASH_CREATE_SIZE; ++h)
		INIT_HLIST_HEAD(&m->napt_hash_crat[h]);
	INIT_LIST_HEAD(&m->napt_list);
	INIT_LIST_HEAD(&m->napt_gc_list);
	rwlock_init(&m->napt_lock);
	m->napt_last_gc = jiffies;

	for (h = 0; h < MAP_DEFRAG6_HASH_SIZE; ++h)
		INIT_HLIST_HEAD(&m->defrag6_hash[h]);
	INIT_LIST_HEAD(&m->defrag6_list);
	rwlock_init(&m->defrag6_lock);
	m->defrag6_last_gc = jiffies;

	for (i = 0; i < 17; ++i)
		m->psid_offset_nums[i] = 0;

	rwlock_init(&m->psid_offset_nums_lock);

	dev->tstats = alloc_percpu(struct pcpu_tstats);
	if (!dev->tstats)
		return -ENOMEM;

	return 0;
}

static void
map_setup(struct net_device *dev)
{
	dev->netdev_ops = &map_netdev_ops;
	dev->destructor = map_free;
	dev->type = ARPHRD_TUNNEL6;
	dev->hard_header_len = LL_MAX_HEADER + sizeof(struct ipv6hdr);
	/*dev->mtu = 1280; */
	dev->mtu = ETH_DATA_LEN;
	dev->flags |= IFF_NOARP;
	dev->addr_len = sizeof(struct in6_addr);
	dev->features |= NETIF_F_NETNS_LOCAL;
	dev->priv_flags &= ~IFF_XMIT_DST_RELEASE;
	dev->priv_flags |= IFF_MAP;
}

static struct map *
map_create(struct net *net, struct map_parm *p)
{
	struct net_device *dev;
	struct map *m;
	char name[IFNAMSIZ];
	int err;
	struct map_net *mapn = net_generic(net, map_net_id);

	if (p->name[0])
		strlcpy(name, p->name, IFNAMSIZ);
	else
		sprintf(name, "map%%d");

	dev = alloc_netdev(sizeof(*m), name, map_setup);
	if (dev == NULL)
		goto failed;

	dev_net_set(dev, net);

	m = netdev_priv(dev);

	err = map_init(dev, p);
	if (err < 0)
		goto failed_free;

	err = register_netdevice(dev);
	if (err < 0)
		goto failed_free;

	strcpy(m->p.name, dev->name);

	dev_hold(dev);

	write_lock_bh(&mapn->map_list_lock);
	list_add_tail(&m->list, &mapn->map_list);
	write_unlock_bh(&mapn->map_list_lock);

	return m;

failed_free:
	map_free(dev);
failed:
	return NULL;
}

static struct map *
map_find_or_create(struct net *net, struct map_parm *p, int create)
{
	struct map *m;
	struct map_net *mapn = net_generic(net, map_net_id);

	read_lock(&mapn->map_list_lock);
	list_for_each_entry(m, &mapn->map_list, list) {
		if (!strncmp(m->p.name, p->name, IFNAMSIZ)) {
			read_unlock(&mapn->map_list_lock);
			return m;
		}
	}
	read_unlock(&mapn->map_list_lock);

	if (!create)
		return NULL;

	return map_create(net, p);
}

static int
map_open(struct net_device *dev)
{
	int err = 0;
	struct map *m = netdev_priv(dev);
	if (m->map_ipv6_address_length)
		map_route6_add(&m->map_ipv6_address, m->map_ipv6_address_length,
			m);
	return err;
}

static int
map_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int err = 0;
	struct map_parm p;
	struct map_parm *pp;
	struct map_rule_parm *rpp;
	struct map_pool_parm *ppp;
	struct map_current_parm *cpp;
	struct map_napt_block *nbp;
	struct map_napt_parm *npp;
	struct map_napt_node_parm *nnpp;
	struct map_napt_node *nn;
	unsigned int size = 0;
	int i, j;
	struct map *m = NULL;
	struct map_rule *mr = NULL;
	struct map_pool *mp = NULL;
	struct net *net = dev_net(dev);
	struct map_net *mapn = net_generic(net, map_net_id);
	unsigned long current_time;
	struct timespec timespec_max;

	switch (cmd) {
	case SIOCGETMAP:
		if (dev == mapn->map_fb_dev) {
			if (copy_from_user(&p, ifr->ifr_ifru.ifru_data,
			    sizeof(p))) {
				err = -EFAULT;
				break;
			}
			m = map_find_or_create(net, &p, 0);
		}
		if (m == NULL)
			m = netdev_priv(dev);
		memcpy(&p, &m->p, sizeof(p));
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &p, sizeof(p)))
			err = -EFAULT;
		break;
	case SIOCADDMAP:
	case SIOCCHGMAP:
		if (!capable(CAP_NET_ADMIN)) {
			err = -EPERM;
			break;
		}
		if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p))) {
			err = -EFAULT;
			break;
		}
		if (p.role != MAP_ROLE_BR && p.role != MAP_ROLE_CE) {
			err = -EFAULT;
			break;
		}
		if (p.default_forwarding_mode != MAP_FORWARDING_MODE_T &&
		    p.default_forwarding_mode != MAP_FORWARDING_MODE_E) {
			err = -EFAULT;
			break;
		}
		if (p.default_forwarding_rule != MAP_FORWARDING_RULE_T &&
		    p.default_forwarding_rule != MAP_FORWARDING_RULE_F) {
			err = -EFAULT;
			break;
		}
		if (p.ipv6_fragment_size < 1280) {
			err = -EFAULT;
			break;
		}
		if (p.ipv4_fragment_inner != MAP_IPV4_FRAG_INNER_T &&
		    p.ipv4_fragment_inner != MAP_IPV4_FRAG_INNER_F) {
			err = -EFAULT;
			break;
		}
		if (p.napt_always != MAP_NAPT_ALWAYS_T &&
		    p.napt_always != MAP_NAPT_ALWAYS_F) {
			err = -EFAULT;
			break;
		}
		if (p.napt_force_recycle != MAP_NAPT_FORCE_RECYCLE_T &&
		    p.napt_force_recycle != MAP_NAPT_FORCE_RECYCLE_F) {
			err = -EFAULT;
			break;
		}

		/* if (p.role == MAP_ROLE_BR && p.br_address_length > 64) {
		 *	err = -EFAULT;
		 *	break;
		 * }
		 */
		if (p.br_address_length > 96) {
			err = -EFAULT;
			break;
		}
		m = map_find_or_create(net, &p, cmd == SIOCADDMAP);
		if (dev != mapn->map_fb_dev && cmd == SIOCCHGMAP) {
			if (m != NULL) {
				if (m->dev != dev) {
					err = -EEXIST;
					break;
				}
			} else
				m = netdev_priv(dev);
			synchronize_net(); /* XXX: */
			err = map_change(m, &p);
			netdev_state_change(dev); /* XXX: */
		}
		if (m) {
			if (copy_to_user(ifr->ifr_ifru.ifru_data, &m->p,
			    sizeof(p)))
				err = -EFAULT;
		} else
			err = (cmd == SIOCADDMAP ? -ENOBUFS : -ENOENT);
		map_update(m);
		break;
	case SIOCDELMAP:
		if (!capable(CAP_NET_ADMIN)) {
			err = -EPERM;
			break;
		}
		if (dev == mapn->map_fb_dev) {
			if (copy_from_user(&p, ifr->ifr_ifru.ifru_data,
			    sizeof(p))) {
				err = -EFAULT;
				break;
			}
			m = map_find_or_create(net, &p, 0);
			if (m == NULL) {
				err = -ENOENT;
				break;
			}
			if (m->dev == mapn->map_fb_dev) {
				err = -EPERM;
				break;
			}
			dev = m->dev;
		}
		unregister_netdevice(dev);
		map_update(m);
		break;
	case SIOCGETMAPRULES:
		if (dev == mapn->map_fb_dev) {
			if (copy_from_user(&p, ifr->ifr_ifru.ifru_data,
			    sizeof(p))) {
				err = -EFAULT;
				break;
			}
			m = map_find_or_create(net, &p, 0);
		}
		if (m == NULL)
			m = netdev_priv(dev);
		size = sizeof(*pp) + sizeof(*rpp) * m->p.rule_num;
		pp = kmalloc(size, GFP_KERNEL);
		if (!pp) {
			err = -EFAULT;
			break;
		}
		*pp = m->p;
		rpp = pp->rule;
		read_lock(&m->rule_lock);
		list_for_each_entry(mr, &m->rule_list, list) {
			*rpp = mr->p;
			++rpp;
		}
		read_unlock(&m->rule_lock);
		if (copy_to_user(ifr->ifr_ifru.ifru_data, pp, size))
			err = -EFAULT;
		kfree(pp);
		break;
	case SIOCADDMAPRULES:
	case SIOCCHGMAPRULES:
	case SIOCDELMAPRULES:
		if (!capable(CAP_NET_ADMIN)) {
			err = -EPERM;
			break;
		}
		if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p))) {
			err = -EFAULT;
			break;
		}
		if (dev == mapn->map_fb_dev)
			m = map_find_or_create(net, &p, 0);
		if (m == NULL)
			m = netdev_priv(dev);
		size = sizeof(*pp) + sizeof(*rpp) * p.rule_num;
		pp = kmalloc(size, GFP_KERNEL);
		if (!pp) {
			err = -EFAULT;
			break;
		}
		if (copy_from_user(pp, ifr->ifr_ifru.ifru_data, size)) {
			kfree(pp);
			err = -EFAULT;
			break;
		}
		for (i = 0; i < p.rule_num; i++) {
			rpp = &pp->rule[i];
			if (cmd == SIOCADDMAPRULES)
				if (map_rule_add(m, rpp) < 0)
					err = -EFAULT;
			if (cmd == SIOCCHGMAPRULES)
				if (map_rule_change(m, rpp) < 0)
					err = -EFAULT;
			if (cmd == SIOCDELMAPRULES)
				if (map_rule_delete(m, rpp) < 0)
					err = -EFAULT;
		}
		kfree(pp);
		map_update(m);
		break;
	case SIOCGETMAPCURRNUM:
	case SIOCGETMAPCURR:
		if (dev == mapn->map_fb_dev) {
			err = -EFAULT;
			break;
		}
		m = netdev_priv(dev);
		size = sizeof(*cpp);
		if (cmd == SIOCGETMAPCURR)
			size += sizeof(*nbp) * m->port_range_length;
		cpp = kmalloc(size, GFP_KERNEL);
		if (!cpp) {
			err = -EFAULT;
			break;
		}
		if (copy_from_user(cpp, ifr->ifr_ifru.ifru_data,
				   sizeof(*cpp))) {
			kfree(cpp);
			err = -EFAULT;
			break;
		}
		if (cmd == SIOCGETMAPCURR
		 && cpp->port_range_length < m->port_range_length) {
			size = size - m->port_range_length
				+ cpp->port_range_length;
		}
		if (m->bmr) {
			cpp->has_bmr = 1;
			cpp->bmrp = m->bmr->p;
		} else {
			cpp->has_bmr = 0;
			memset(&cpp->bmrp, 0, sizeof(cpp->bmrp));
		}
		cpp->map_ipv6_address = m->map_ipv6_address;
		cpp->map_ipv6_address_length = m->map_ipv6_address_length;
		cpp->laddr4 = m->laddr4;
		cpp->psid = m->psid;
		cpp->psid_length = m->psid_length;
		if (cmd == SIOCGETMAPCURR) {
			for (i = 0; i < cpp->port_range_length; ++i)
				cpp->port_range[i] = m->port_range[i];
		}
		cpp->port_range_length = m->port_range_length;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, cpp, size))
			err = -EFAULT;
		kfree(cpp);
		break;
	case SIOCGETMAPNAPTNUM:
	case SIOCGETMAPNAPT:
		if (dev == mapn->map_fb_dev) {
			err = -EFAULT;
			break;
		}
		m = netdev_priv(dev);
		write_lock_bh(&m->napt_lock);
		map_napt_nn_gc(m);
		/* write_unlock_bh(&m->napt_lock);
		 * read_lock(&m->napt_lock);
		 */
		i = 0;
		list_for_each_entry(nn, &m->napt_list, nn_list) { ++i; }
		size = sizeof(*npp);
		if (cmd == SIOCGETMAPNAPT)
			size += sizeof(*nnpp) * i;
		npp = kmalloc(size, GFP_KERNEL);
		if (!npp) {
			/* read_unlock(&m->napt_lock); */
			write_unlock_bh(&m->napt_lock);
			err = -EFAULT;
			break;
		}
		if (copy_from_user(npp, ifr->ifr_ifru.ifru_data,
				sizeof(*npp))) {
			/* read_unlock(&m->napt_lock); */
			write_unlock_bh(&m->napt_lock);
			err = -EFAULT;
			break;
		}
		if (cmd == SIOCGETMAPNAPT && npp->napt_node_num < i)
			size = size - i + npp->napt_node_num;

		if (cmd == SIOCGETMAPNAPT) {
			jiffies_to_timespec(ULONG_MAX, &timespec_max);
			current_time = jiffies;
			jiffies_to_timespec(current_time, &npp->current_time);
			npp->current_time = timespec_add(npp->current_time,
				timespec_max);
			j = 0;
			list_for_each_entry_reverse(nn, &m->napt_list,
					nn_list) {
				npp->napt_node[j].raddr = nn->raddr;
				npp->napt_node[j].laddr = nn->laddr;
				npp->napt_node[j].maddr = nn->maddr;
				npp->napt_node[j].rport = nn->rport;
				npp->napt_node[j].lport = nn->lport;
				npp->napt_node[j].mport = nn->mport;
				npp->napt_node[j].laddr6 = nn->laddr6;
				npp->napt_node[j].proto = nn->proto;
				npp->napt_node[j].flags = nn->flags;
				/* npp->napt_node[j].last_used =
				 *	nn->last_used;
				 */
				jiffies_to_timespec(nn->last_used,
					&npp->napt_node[j].last_used);
				if (nn->last_used <= current_time)
					npp->napt_node[j].last_used
					= timespec_add(npp->napt_node[j]
					.last_used, timespec_max);
				++j;
				if (j >= npp->napt_node_num)
					break;
			}
		}
		npp->napt_node_num = i;
		/* read_unlock(&m->napt_lock); */
		write_unlock_bh(&m->napt_lock);
		if (copy_to_user(ifr->ifr_ifru.ifru_data, npp, size))
			err = -EFAULT;
		kfree(npp);
		break;
	case SIOCGETMAPPOOLS:
		if (dev == mapn->map_fb_dev) {
			if (copy_from_user(&p, ifr->ifr_ifru.ifru_data,
			    sizeof(p))) {
				err = -EFAULT;
				break;
			}
			m = map_find_or_create(net, &p, 0);
		}
		if (m == NULL)
			m = netdev_priv(dev);
		size = sizeof(*pp) + sizeof(*ppp) * m->p.pool_num;
		pp = kmalloc(size, GFP_KERNEL);
		if (!pp) {
			err = -EFAULT;
			break;
		}
		*pp = m->p;
		ppp = pp->pool;
		read_lock_bh(&m->pool_lock);
		list_for_each_entry(mp, &m->pool_list, list) {
			*ppp = mp->p;
			++ppp;
		}
		read_unlock_bh(&m->pool_lock);
		if (copy_to_user(ifr->ifr_ifru.ifru_data, pp, size))
			err = -EFAULT;
		kfree(pp);
		break;
	case SIOCADDMAPPOOLS:
	case SIOCDELMAPPOOLS:
	case SIOCCHGMAPPOOLS:
		if (!capable(CAP_NET_ADMIN)) {
			err = -EPERM;
			break;
		}
		if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p))) {
			err = -EFAULT;
			break;
		}
		if (dev == mapn->map_fb_dev)
			m = map_find_or_create(net, &p, 0);
		if (m == NULL)
			m = netdev_priv(dev);
		size = sizeof(*pp) + sizeof(*ppp) * p.pool_num;
		pp = kmalloc(size, GFP_KERNEL);
		if (!pp) {
			err = -EFAULT;
			break;
		}
		if (copy_from_user(pp, ifr->ifr_ifru.ifru_data, size)) {
			kfree(pp);
			err = -EFAULT;
			break;
		}
		for (i = 0; i < p.pool_num; i++) {
			ppp = &pp->pool[i];
			if (cmd == SIOCADDMAPPOOLS)
				if (map_pool_add(m, ppp) < 0)
					err = -EFAULT;
			if (cmd == SIOCCHGMAPPOOLS)
				if (map_pool_change(m, ppp) < 0)
					err = -EFAULT;
			if (cmd == SIOCDELMAPPOOLS)
				if (map_pool_delete(m, ppp) < 0)
					err = -EFAULT;
		}
		kfree(pp);
		map_napt_debug_pool(m); /* XXX: */
		break;
	default:
		pr_notice("map_ioctl: ???\n");
	}

#ifdef MAP_DEBUG
	pr_notice("map_ioctl: begin\n");
	map_debug_dump(mapn);
	pr_notice("map_ioctl: end\n");
#endif

	return err;
}

static int __net_init
map_net_init(struct net *net)
{
	struct map_net *mapn = net_generic(net, map_net_id);
	struct map *m = NULL;
	struct map_parm p;
	int err;

	rwlock_init(&mapn->map_list_lock);
	INIT_LIST_HEAD(&mapn->map_list);

	memset(&p, 0, sizeof(p));
	sprintf(p.name, "mapfb");

	err = -ENOMEM;
	mapn->map_fb_dev = alloc_netdev(sizeof(struct map), p.name, map_setup);
	if (!mapn->map_fb_dev)
		goto err_alloc_dev;

	dev_net_set(mapn->map_fb_dev, net);

	m = netdev_priv(mapn->map_fb_dev);

	err = map_init(mapn->map_fb_dev, &p);
	if (err < 0)
		goto err_register;

	err = register_netdev(mapn->map_fb_dev);
	if (err < 0)
		goto err_register;

	dev_hold(mapn->map_fb_dev);

	write_lock_bh(&mapn->map_list_lock);
	list_add_tail(&m->list, &mapn->map_list);
	write_unlock_bh(&mapn->map_list_lock);

	return 0;

err_register:
	map_free(mapn->map_fb_dev);
err_alloc_dev:
	return err;
}

static void __net_exit
map_net_exit(struct net *net)
{
	struct map_net *mapn = net_generic(net, map_net_id);
	rtnl_lock();
	map_destroy(mapn);
	rtnl_unlock();
}

static struct pernet_operations map_net_ops = {
	.init	= map_net_init,
	.exit	= map_net_exit,
	.id	= &map_net_id,
	.size	= sizeof(struct map_net),
};

static int
map_netdev_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *dev = (struct net_device *)ptr;
	struct net *net = dev_net(dev);
	struct map_net *mapn = net_generic(net, map_net_id);
	struct map *m;

	if (event != NETDEV_UNREGISTER)
		return NOTIFY_DONE;

	read_lock(&mapn->map_list_lock);
	list_for_each_entry(m, &mapn->map_list, list) {
		if (m->p.tunnel_source == dev->ifindex) {
			pr_notice("map_netdev_event: %s\n", m->p.name);
			m->p.tunnel_source = 0;
			map_update(m);
		}
	}
	read_unlock(&mapn->map_list_lock);

	return NOTIFY_DONE;
}

static struct notifier_block map_netdev_notifier = {
	.notifier_call = map_netdev_event,
};

static int
map_inet6addr_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct inet6_ifaddr *ifa = (struct inet6_ifaddr *)ptr;
	struct net_device *dev = ifa->idev->dev;
	struct net *net = dev_net(dev);
	struct map_net *mapn = net_generic(net, map_net_id);
	struct map *m;

	read_lock(&mapn->map_list_lock);
	list_for_each_entry(m, &mapn->map_list, list) {
		if (m->p.tunnel_source == dev->ifindex) {
			pr_notice("map_inet6addr_event: %s\n",
				m->p.name);
			map_update(m);
		}
	}
	read_unlock(&mapn->map_list_lock);

	return NOTIFY_DONE;
}

static struct notifier_block map_inet6addr_notifier = {
	.notifier_call = map_inet6addr_event,
};

static int __init
map_module_init(void)
{
	int err;

	err = map_rule_init();
	if (err < 0) {
		pr_err("map init: can't init rule.\n");
		goto out_rule;
	}

	err = map_defrag6_init();
	if (err < 0) {
		pr_err("map init: can't init defrag6.\n");
		goto out_defrag6;
	}

	err = map_napt_init();
	if (err < 0) {
		pr_err("map init: can't init napt.\n");
		goto out_napt;
	}

	err = register_pernet_device(&map_net_ops);
	if (err < 0) {
		pr_err("map init: can't register pernet.\n");
		goto out_pernet;
	}

	err = register_netdevice_notifier(&map_netdev_notifier);
	if (err < 0) {
		pr_err("map init: can't register netdevice_notifier.\n");
		goto out_netdevice;
	}

	err = register_inet6addr_notifier(&map_inet6addr_notifier);
	if (err < 0) {
		pr_err("map init: can't register inet6addr_notifier.\n");
		goto out_inet6addr;
	}

	/* err = xfrm6_tunnel_register(&map_handler, AF_INET);
	 * if (err < 0) {
	 *	pr_err("map init: can't register tunnel.\n");
	 *	goto out_tunnel;
	 * }
	 */

	return 0;

/* out_tunnel:
 *	unregister_inet6addr_notifier(&map_inet6addr_notifier);
 */
out_inet6addr:
	unregister_netdevice_notifier(&map_netdev_notifier);
out_netdevice:
	unregister_pernet_device(&map_net_ops);
out_pernet:
	map_napt_exit();
out_napt:
	map_defrag6_exit();
out_defrag6:
	map_rule_exit();
out_rule:
	return err;
}

static void __exit
map_module_exit(void)
{
	/* if (xfrm6_tunnel_deregister(&map_handler, AF_INET))
	 *	pr_notice("map close: can't deregister tunnel.\n");
	 */
	if (unregister_inet6addr_notifier(&map_inet6addr_notifier))
		pr_notice("map close: can't deregister inet6addr_notifier.\n");
	if (unregister_netdevice_notifier(&map_netdev_notifier))
		pr_notice("map close: can't deregister netdevice_notifier.\n");
	unregister_pernet_device(&map_net_ops);
	map_napt_exit();
	map_defrag6_exit();
	map_rule_exit();
}

module_init(map_module_init);
module_exit(map_module_exit);
