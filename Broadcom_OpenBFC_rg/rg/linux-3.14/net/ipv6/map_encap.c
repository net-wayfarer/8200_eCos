/* ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <m-asama@ginzado.co.jp> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return Masakazu Asama
 * ----------------------------------------------------------------------------
 */
/*	MAP-E function
 *
 *	Authors:
 *	Masakazu Asama		<m-asama@ginzado.co.jp>
 */

#include <linux/skbuff.h>
#include <linux/in6.h>
#include <linux/ipv6.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>

#include <net/ip.h>
#include <net/ipv6.h>
#include <net/icmp.h>
#include <net/route.h>
#include <net/ip6_route.h>
#include <net/map.h>

/* XXX: */

int
map_encap_validate_src(struct sk_buff *skb, struct map *m, __be32 *saddr4,
		       int *fb)
{
	struct ipv6hdr *ipv6h = ipv6_hdr(skb);
	struct map_rule *mr;
	u8 *ptr;
	struct iphdr *iph, *icmpiph;
	struct tcphdr *tcph, *icmptcph;
	struct udphdr *udph, *icmpudph;
	struct icmphdr *icmph, *icmpicmph;
	__u8 proto;
	__be32 saddr;
	__be16 sport;
	struct in6_addr addr6;
	int err = 0;

	proto = ipv6h->nexthdr;
	ptr = (u8 *)ipv6h;
	ptr += sizeof(struct ipv6hdr);
	if (proto == IPPROTO_FRAGMENT) {
		proto = ((struct frag_hdr *)ptr)->nexthdr;
		ptr += sizeof(struct frag_hdr);
	}

	if (proto != IPPROTO_IPIP) {
		pr_notice("map_encap_validate_src: is this encaped?\n");
		err = -1;
		goto err;
	}

	iph = (struct iphdr *)ptr;

	if (m->p.role == MAP_ROLE_CE &&
	    ipv6_addr_equal(&ipv6h->saddr, &m->p.br_address)) {
		*saddr4 = iph->saddr;
		return 0;
	}

	saddr = iph->saddr;
	ptr += iph->ihl * 4;

	switch (iph->protocol) {
	case IPPROTO_ICMP:
		icmph = (struct icmphdr *)ptr;
		switch (icmph->type) {
		case ICMP_DEST_UNREACH:
		case ICMP_SOURCE_QUENCH:
		case ICMP_TIME_EXCEEDED:
		case ICMP_PARAMETERPROB:
			ptr = (u8 *)icmph;
			ptr += sizeof(struct icmphdr);
			icmpiph = (struct iphdr *)ptr;
			saddr = icmpiph->daddr;
			ptr += icmpiph->ihl * 4;
			switch (icmpiph->protocol) {
			case IPPROTO_TCP:
				icmptcph = (struct tcphdr *)ptr;
				sport = icmptcph->dest;
				break;
			case IPPROTO_UDP:
				icmpudph = (struct udphdr *)ptr;
				sport = icmpudph->dest;
				break;
			case IPPROTO_ICMP:
				icmpicmph = (struct icmphdr *)ptr;
				sport = icmpicmph->un.echo.id;
				break;
			default:
				pr_notice("map_encap_validate_src: unknown proto encaped in icmp error.\n");
				err = -1;
				goto err;
			}
			break;
		default:
			sport = icmph->un.echo.id;
			break;
		}
		break;
	case IPPROTO_TCP:
		tcph = (struct tcphdr *)ptr;
		sport = tcph->source;
		break;
	case IPPROTO_UDP:
		udph = (struct udphdr *)ptr;
		sport = udph->source;
		break;
	default:
		pr_notice("map_encap_validate_src: unknown encaped.\n");
		err = -1;
		goto err;
	}

	mr = map_rule_find_by_ipv6addr(m, &ipv6h->saddr);
	if (!mr) {
		if (m->p.role == MAP_ROLE_BR) {
			*fb = 1;
			goto fallback;
		} else {
			err = -1;
			goto err;
		}
	}

	if (map_gen_addr6(&addr6, saddr, sport, mr, 0)) {
		if (m->p.role == MAP_ROLE_BR) {
			*fb = 1;
			goto fallback;
		} else {
			err = -1;
			goto err;
		}
	}

	if (!ipv6_addr_equal(&addr6, &ipv6h->saddr)) {
		if (m->p.role == MAP_ROLE_BR) {
			*fb = 1;
			goto fallback;
		} else {
			pr_notice("map_encap_validate_src: validation failed.\n");
			err = -1;
			goto err_icmpv6_send;
		}
	}

fallback:
	*saddr4 = iph->saddr;

	return 0;

err_icmpv6_send:
	pr_notice("map_encap_validate_src: icmpv6_send(skb, ICMPV6_DEST_UNREACH, 5 /* Source address failed ingress/egress policy */, 0);\n");
	icmpv6_send(skb, ICMPV6_DEST_UNREACH,
		5 /* Source address failed ingress/egress policy */, 0);
err:
	map_debug_print_skb("map_encap_validate_src", skb);
	return err;
}

int
map_encap_validate_dst(struct sk_buff *skb, struct map *m, __be32 *daddr4)
{
	struct ipv6hdr *ipv6h = ipv6_hdr(skb);
	u8 *ptr;
	struct iphdr *iph, *icmpiph;
	struct tcphdr *tcph, *icmptcph;
	struct udphdr *udph, *icmpudph;
	struct icmphdr *icmph, *icmpicmph;
	__u8 proto;
	__be32 daddr;
	__be16 dport;
	struct in6_addr addr6;
	int err = 0;

	proto = ipv6h->nexthdr;
	ptr = (u8 *)ipv6h;
	ptr += sizeof(struct ipv6hdr);
	if (proto == IPPROTO_FRAGMENT) {
		proto = ((struct frag_hdr *)ptr)->nexthdr;
		ptr += sizeof(struct frag_hdr);
	}

	if (proto != IPPROTO_IPIP) {
		pr_notice("map_encap_validate_dst: is this encaped?\n");
		err = -1;
		goto err;
	}

	iph = (struct iphdr *)ptr;

	if (!ipv6_addr_equal(&ipv6h->daddr, &m->map_ipv6_address)) {
		pr_notice("map_encap_validate_dst: not match my address.\n");
		err = -1;
		goto err;
	}

	if (m->p.role == MAP_ROLE_BR || (m->p.role == MAP_ROLE_CE && !m->bmr)) {
		*daddr4 = iph->daddr;
		return 0;
	}

	if (!m->bmr) {
		pr_notice("map_encap_validate_dst: m->bmr is null.\n");
		err = -1;
		goto err;
	}

	daddr = iph->daddr;
	ptr += iph->ihl * 4;
	switch (iph->protocol) {
	case IPPROTO_ICMP:
		icmph = (struct icmphdr *)ptr;
		switch (icmph->type) {
		case ICMP_DEST_UNREACH:
		case ICMP_SOURCE_QUENCH:
		case ICMP_TIME_EXCEEDED:
		case ICMP_PARAMETERPROB:
			ptr = (u8 *)icmph;
			ptr += sizeof(struct icmphdr);
			icmpiph = (struct iphdr *)ptr;
			daddr = icmpiph->saddr;
			ptr += icmpiph->ihl * 4;
			switch (icmpiph->protocol) {
			case IPPROTO_TCP:
				icmptcph = (struct tcphdr *)ptr;
				dport = icmptcph->source;
				break;
			case IPPROTO_UDP:
				icmpudph = (struct udphdr *)ptr;
				dport = icmpudph->source;
				break;
			case IPPROTO_ICMP:
				icmpicmph = (struct icmphdr *)ptr;
				dport = icmpicmph->un.echo.id;
				break;
			default:
				pr_notice("map_encap_validate_dst: unknown proto encaped in icmp error.\n");
				err = -1;
				goto err;
			}
			break;
		default:
			dport = icmph->un.echo.id;
			break;
		}
		break;
	case IPPROTO_TCP:
		tcph = (struct tcphdr *)ptr;
		dport = tcph->dest;
		break;
	case IPPROTO_UDP:
		udph = (struct udphdr *)ptr;
		dport = udph->dest;
		break;
	default:
		pr_notice("map_encap_validate_dst: unknown encaped.\n");
		err = -1;
		goto err;
	}

	read_lock(&m->rule_lock);
	if (!m->bmr) {
		read_unlock(&m->rule_lock);
		pr_notice("map_encap_validate_dst: bmr is null..\n");
		err = -1;
		goto err;
	}
	if (map_gen_addr6(&addr6, daddr, dport, m->bmr, 0)) {
		read_unlock(&m->rule_lock);
		pr_notice("map_encap_validate_dst: map_gen_addr6 failed.\n");
		err = -1;
		goto err;
	}
	read_unlock(&m->rule_lock);

	if (!ipv6_addr_equal(&addr6, &ipv6h->daddr)) {
		pr_notice("map_encap_validate_dst: validation failed.\n");
		pr_notice("map_encap_validate_dst: addr6 = %08x%08x%08x%08x\n",
			ntohl(addr6.s6_addr32[0]),
			ntohl(addr6.s6_addr32[1]),
			ntohl(addr6.s6_addr32[2]),
			ntohl(addr6.s6_addr32[3]));
		pr_notice("map_encap_validate_dst: ipv6h->daddr = %08x%08x%08x%08x\n",
			ntohl(ipv6h->daddr.s6_addr32[0]),
			ntohl(ipv6h->daddr.s6_addr32[1]),
			ntohl(ipv6h->daddr.s6_addr32[2]),
			ntohl(ipv6h->daddr.s6_addr32[3]));
		pr_notice("map_encap_validate_dst: daddr = %d.%d.%d.%d dport = %d(%04x)\n",
			((ntohl(daddr) >> 24) & 0xff),
			((ntohl(daddr) >> 16) & 0xff),
			((ntohl(daddr) >> 8) & 0xff),
			(ntohl(daddr) & 0xff),
			ntohs(dport), ntohs(dport));
		err = -1;
		goto err_icmpv6_send;
	}

	*daddr4 = iph->daddr;

	return 0;

err_icmpv6_send:
	pr_notice("map_encap_validate_dst: icmpv6_send(skb, ICMPV6_DEST_UNREACH, ICMPV6_ADDR_UNREACH, 0);\n");
	icmpv6_send(skb, ICMPV6_DEST_UNREACH, ICMPV6_ADDR_UNREACH, 0);
err:
	map_debug_print_skb("map_encap_validate_dst", skb);
	return err;
}

int
map_encap_forward_v6v4(struct sk_buff *skb, struct map *m, __be32 *saddr4,
	__be32 *daddr4, int fb)
{
	struct ipv6hdr orig_ipv6h = {}, *ipv6h;
	struct frag_hdr orig_fragh = {}, *fragh;
	int hsize;
	__u8 nexthdr;
	struct iphdr *iph;
	__be32 *saddrp = NULL;
	__be16 *sportp = NULL;
	__sum16 *checkp = NULL;
	struct in6_addr *saddr6;
	u8 *ptr;
	int err = 0;

	ipv6h = ipv6_hdr(skb);

	memcpy(&orig_ipv6h, ipv6h, sizeof(orig_ipv6h));
	saddr6 = &orig_ipv6h.saddr;
	hsize = sizeof(orig_ipv6h);
	nexthdr = orig_ipv6h.nexthdr;
	if (orig_ipv6h.nexthdr == IPPROTO_FRAGMENT) {
		ptr = (u8 *)ipv6h;
		ptr += sizeof(*ipv6h);
		fragh = (struct frag_hdr *)ptr;
		memcpy(&orig_fragh, fragh, sizeof(orig_fragh));
		hsize += sizeof(orig_fragh);
		nexthdr = orig_fragh.nexthdr;
	}

	if (nexthdr != IPPROTO_IPIP) {
		pr_notice("map_encap_forward_v6v4: this packet is not ipip.\n");
		err = -1;
		goto err;
	}

	skb_dst_drop(skb);
	skb_pull(skb, hsize);
	skb_reset_network_header(skb);
	skb->protocol = htons(ETH_P_IP);
	iph = ip_hdr(skb);

	if (m->p.role == MAP_ROLE_BR && fb) {
		err = map_napt(iph, 0, m, &saddrp, &sportp, &checkp, saddr6,
			       fb);
		if (err)
			goto err;
		/* NAPT Hairpinning */
		if (map_napt_hairpin(skb, m, saddrp, sportp, saddr6, fb))
			goto out;
	} else
		err = map_napt(iph, 1, m, &saddrp, &sportp, &checkp, NULL, fb);
	if (err) {
		pr_notice("map_encap_forward_v6v4: saddr:%d.%d.%d.%d daddr:%d.%d.%d.%d\n",
			((ntohl(iph->saddr) >> 24) & 0xff),
			((ntohl(iph->saddr) >> 16) & 0xff),
			((ntohl(iph->saddr) >> 8) & 0xff),
			((ntohl(iph->saddr)) & 0xff),
			((ntohl(iph->daddr) >> 24) & 0xff),
			((ntohl(iph->daddr) >> 16) & 0xff),
			((ntohl(iph->daddr) >> 8) & 0xff),
			((ntohl(iph->daddr)) & 0xff));
		goto err;
	}

	iph->check = 0;
	iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);

	skb->dev = m->dev;
	skb->rxhash = 0;
	skb_set_queue_mapping(skb, 0);
	skb_dst_drop(skb);
	nf_reset(skb);

	netif_rx(skb);

	return 0;

err:
	map_debug_print_skb("map_encap_forward_v6v4", skb);
out:
	return err;
}

struct map_encap_forward_v4v6_arg {
	struct flowi6 *fl6;
	struct dst_entry *dst;
};

static int
map_encap_forward_v4v6_finish(struct sk_buff *skb, void *arg)
{
	struct map_encap_forward_v4v6_arg *a = arg;
	struct flowi6 *fl6 = a->fl6;
	struct dst_entry *dst = a->dst;
	struct iphdr *iph;
	struct ipv6hdr *ipv6h;
	int pkt_len;
	unsigned int max_headroom;
	struct iphdr orig_iph;
	int err = 0;

	max_headroom = sizeof(struct ipv6hdr) + sizeof(struct frag_hdr) + 20;

	if (skb_headroom(skb) < max_headroom || skb_shared(skb) ||
	    (skb_cloned(skb) && !skb_clone_writable(skb, 0))) {
		struct sk_buff *new_skb;
		new_skb = skb_realloc_headroom(skb, max_headroom);
		if (!new_skb)
			goto tx_err_dst_release;

		if (skb->sk)
			skb_set_owner_w(new_skb, skb->sk);
		kfree_skb(skb);
		skb = new_skb;
	}
	skb_dst_drop(skb);
	skb_dst_set_noref(skb, dst);

	iph = ip_hdr(skb);

	memcpy(&orig_iph, iph, sizeof(orig_iph));
	skb_push(skb, sizeof(struct ipv6hdr));
	skb_reset_network_header(skb);
	skb->protocol = htons(ETH_P_IPV6);
	ipv6h = ipv6_hdr(skb);

	ipv6h->version = 6;
	ipv6h->priority = 0; /* XXX: */
	ipv6h->flow_lbl[0] = 0;
	ipv6h->flow_lbl[1] = 0;
	ipv6h->flow_lbl[2] = 0;
	ipv6h->payload_len = orig_iph.tot_len;
	ipv6h->hop_limit = orig_iph.ttl - 1; /* XXX: */
	memcpy(&ipv6h->saddr, &fl6->saddr, sizeof(struct in6_addr));
	memcpy(&ipv6h->daddr, &fl6->daddr, sizeof(struct in6_addr));
	ipv6h->nexthdr = IPPROTO_IPIP;

	pkt_len = skb->len;

	skb->local_df = 1;

	err = ip6_local_out(skb);

	return 0;

tx_err_dst_release:
	pr_notice("map_encap_forward_v4v6: tx_err_dst_release:\n");
	/* dst_release(dst); */ /* XXX: */
	return err;
}

int
map_encap_forward_v4v6(struct sk_buff *skb, struct map *m, struct map_rule *mr,
		       int fb)
{
	struct flowi6 fl6;
	struct in6_addr saddr6, daddr6;
	struct net *net = dev_net(m->dev);
	struct dst_entry *dst;
	struct iphdr *iph;
	__be32 *daddrp = NULL;
	__be16 *dportp = NULL;
	__sum16 *checkp = NULL;
	int ipv4_fragment_size;
	int ret;
	int err = 0;
	struct map_encap_forward_v4v6_arg arg;

	iph = ip_hdr(skb);

	err = map_napt(iph, 0, m, &daddrp, &dportp, &checkp, NULL, 0);
	if (err)
		goto err;
	/* NAPT Hairpinning */
	if (map_napt_hairpin(skb, m, daddrp, dportp, NULL, 0))
		goto out;

	iph->check = 0;
	iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);

	saddr6.s6_addr32[0] = m->map_ipv6_address.s6_addr32[0];
	saddr6.s6_addr32[1] = m->map_ipv6_address.s6_addr32[1];
	saddr6.s6_addr32[2] = m->map_ipv6_address.s6_addr32[2];
	saddr6.s6_addr32[3] = m->map_ipv6_address.s6_addr32[3];

	if (mr) {
		map_gen_addr6(&daddr6, *daddrp, *dportp, mr, 0);
	} else {
		daddr6.s6_addr32[0] = m->p.br_address.s6_addr32[0];
		daddr6.s6_addr32[1] = m->p.br_address.s6_addr32[1];
		daddr6.s6_addr32[2] = m->p.br_address.s6_addr32[2];
		daddr6.s6_addr32[3] = m->p.br_address.s6_addr32[3];
	}

	if (m->p.role == MAP_ROLE_BR && fb) {
		err = map_napt(iph, 1, m, &daddrp, &dportp, &checkp, &daddr6,
			       fb);
		iph->check = 0;
		iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
		if (err)
			goto err;
	}

	memset(&fl6, 0, sizeof(fl6));
	fl6.saddr = saddr6;
	fl6.daddr = daddr6;
	fl6.flowi6_oif = m->dev->ifindex;
	fl6.flowlabel = 0;

	dst = ip6_route_output(net, NULL, &fl6);
	/* dst_metric_set(dst, RTAX_MTU, 1280); */
	if (dst_mtu(dst) > m->p.ipv6_fragment_size)
		dst_metric_set(dst, RTAX_MTU, m->p.ipv6_fragment_size);

	arg.fl6 = &fl6;
	arg.dst = dst;

	ipv4_fragment_size = dst_mtu(dst) - sizeof(struct ipv6hdr);
	if (m->p.ipv4_fragment_inner == MAP_IPV4_FRAG_INNER_T &&
	    skb->len > ipv4_fragment_size) {
		skb_dst(skb)->dev->mtu = ipv4_fragment_size -
					 sizeof(struct frag_hdr);
		ret = ip_fragment(skb, map_encap_forward_v4v6_finish, &arg);
	} else {
		ret = map_encap_forward_v4v6_finish(skb, &arg);
	}
	dst_release(dst);

	return ret;

err:
	map_debug_print_skb("map_encap_forward_v4v6", skb);
out:
	return err;
}
