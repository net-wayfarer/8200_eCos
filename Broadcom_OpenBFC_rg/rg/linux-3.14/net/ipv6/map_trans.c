/* ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <m-asama@ginzado.co.jp> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return Masakazu Asama
 * ----------------------------------------------------------------------------
 */
/*	MAP-T function
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

#define CSUM_CO_DEC(x) { if (x <= 0) { --x; x &= 0xffff; } }
#define CSUM_CO_INC(x) { if (x & 0x10000) { ++x; x &= 0xffff; } }

static inline __sum16
map_trans_update_csum_v6v4(__sum16 check, struct in6_addr *osaddr,
	struct in6_addr *odaddr, __be32 nsaddr, __be32 ndaddr)
{
	long csum = ntohs(check);
	int i;

	csum = ~csum & 0xffff;

	for (i = 0; i < 4; ++i) {
		csum -= ntohl(osaddr->s6_addr32[i]) & 0xffff;
		CSUM_CO_DEC(csum)
		csum -= (ntohl(osaddr->s6_addr32[i]) >> 16) & 0xffff;
		CSUM_CO_DEC(csum)

		csum -= ntohl(odaddr->s6_addr32[i]) & 0xffff;
		CSUM_CO_DEC(csum)
		csum -= (ntohl(odaddr->s6_addr32[i]) >> 16) & 0xffff;
		CSUM_CO_DEC(csum)
	}

	csum += ntohl(nsaddr) & 0xffff;
	CSUM_CO_INC(csum)
	csum += (ntohl(nsaddr) >> 16) & 0xffff;
	CSUM_CO_INC(csum)

	csum += ntohl(ndaddr) & 0xffff;
	CSUM_CO_INC(csum)
	csum += (ntohl(ndaddr) >> 16) & 0xffff;
	CSUM_CO_INC(csum)

	csum = ~csum & 0xffff;

	return htons(csum);
}

static inline __sum16
map_trans_update_csum_v4v6(__sum16 check, __be32 osaddr, __be32 odaddr,
	struct in6_addr *nsaddr, struct in6_addr *ndaddr)
{
	long csum = ntohs(check);
	int i;

	csum = ~csum & 0xffff;

	csum -= ntohl(osaddr) & 0xffff;
	CSUM_CO_DEC(csum)
	csum -= (ntohl(osaddr) >> 16) & 0xffff;
	CSUM_CO_DEC(csum)

	csum -= ntohl(odaddr) & 0xffff;
	CSUM_CO_DEC(csum)
	csum -= (ntohl(odaddr) >> 16) & 0xffff;
	CSUM_CO_DEC(csum)

	for (i = 0; i < 4; ++i) {
		csum += ntohl(nsaddr->s6_addr32[i]) & 0xffff;
		if (csum & 0x10000) {
			++csum;
			csum &= 0xffff;
		}
		csum += (ntohl(nsaddr->s6_addr32[i]) >> 16) & 0xffff;
		if (csum & 0x10000) {
			++csum;
			csum &= 0xffff;
		}

		csum += ntohl(ndaddr->s6_addr32[i]) & 0xffff;
		if (csum & 0x10000) {
			++csum;
			csum &= 0xffff;
		}
		csum += (ntohl(ndaddr->s6_addr32[i]) >> 16) & 0xffff;
		if (csum & 0x10000) {
			++csum;
			csum &= 0xffff;
		}
	}

	csum = ~csum & 0xffff;

	return htons(csum);
}

static inline int
map_trans_icmp_typecode_v6v4(__u8 otype, __u8 ocode, __u8 *ntype, __u8 *ncode)
{
	switch (otype) {
	case ICMPV6_ECHO_REQUEST:
		*ntype = ICMP_ECHO;
		break;
	case ICMPV6_ECHO_REPLY:
		*ntype = ICMP_ECHOREPLY;
		break;
	case ICMPV6_MGM_QUERY:
	case ICMPV6_MGM_REPORT:
	case ICMPV6_MGM_REDUCTION:
		return -1;
	case ICMPV6_DEST_UNREACH:
		/* XXX: */
		*ntype = ICMP_DEST_UNREACH;
		switch (ocode) {
		case ICMPV6_NOROUTE:
		case ICMPV6_NOT_NEIGHBOUR:
		case ICMPV6_ADDR_UNREACH:
			*ncode = ICMP_HOST_UNREACH;
			break;
		case ICMPV6_ADM_PROHIBITED:
			*ncode = ICMP_HOST_ANO;
			break;
		case ICMPV6_PORT_UNREACH:
			*ncode = ICMP_PORT_UNREACH;
			break;
		default:
			return -1;
		}
		break;
	case ICMPV6_PKT_TOOBIG:
		/* XXX: */
		*ntype = ICMP_DEST_UNREACH;
		*ncode = ICMP_FRAG_NEEDED;
		break;
	case ICMPV6_TIME_EXCEED:
		/* XXX: */
		*ntype = ICMP_TIME_EXCEEDED;
		break;
	case ICMPV6_PARAMPROB:
		/* XXX: */
		switch (ocode) {
		case ICMPV6_HDR_FIELD:
			*ntype = ICMP_PARAMETERPROB;
			*ncode = 0;
			break;
		case ICMPV6_UNK_NEXTHDR:
			*ntype = ICMP_DEST_UNREACH;
			*ncode = ICMP_PROT_UNREACH;
			break;
		case ICMPV6_UNK_OPTION:
		default:
			return -1;
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static inline int
map_trans_icmp_typecode_v4v6(__u8 otype, __u8 ocode, __u8 *ntype, __u8 *ncode)
{
	switch (otype) {
	case ICMP_ECHO:
		*ntype = ICMPV6_ECHO_REQUEST;
		break;
	case ICMP_ECHOREPLY:
		*ntype = ICMPV6_ECHO_REPLY;
		break;
	case ICMP_INFO_REQUEST:
	case ICMP_INFO_REPLY:
	case ICMP_TIMESTAMP:
	case ICMP_TIMESTAMPREPLY:
	case ICMP_ADDRESS:
	case ICMP_ADDRESSREPLY:
		return -1;
	case ICMP_DEST_UNREACH:
		/* XXX: */
		*ntype = ICMPV6_DEST_UNREACH;
		switch (ocode) {
		case ICMP_NET_UNREACH:
		case ICMP_HOST_UNREACH:
			*ncode = ICMPV6_NOROUTE;
			break;
		case ICMP_PROT_UNREACH:
			*ntype = ICMPV6_PARAMPROB;
			*ncode = ICMPV6_UNK_NEXTHDR;
			/* XXX: */
			break;
		case ICMP_PORT_UNREACH:
			*ncode = ICMPV6_PORT_UNREACH;
			break;
		case ICMP_FRAG_NEEDED:
			*ntype = ICMPV6_PKT_TOOBIG;
			*ncode = 0;
			break;
		case ICMP_SR_FAILED:
			*ncode = ICMPV6_NOROUTE;
			break;
		case ICMP_NET_UNKNOWN:
		case ICMP_HOST_UNKNOWN:
		case ICMP_HOST_ISOLATED:
			*ncode = ICMPV6_NOROUTE;
			break;
		case ICMP_NET_ANO:
		case ICMP_HOST_ANO:
			*ncode = ICMPV6_ADM_PROHIBITED;
			break;
		case ICMP_NET_UNR_TOS:
			/* XXX: */
			*ncode = ICMPV6_NOROUTE;
			break;
		case ICMP_PKT_FILTERED:
			*ncode = ICMPV6_ADM_PROHIBITED;
			break;
		case ICMP_PREC_VIOLATION:
			return -1;
		case ICMP_PREC_CUTOFF:
			*ncode = ICMPV6_ADM_PROHIBITED;
			break;
		default:
			return -1;
		}
		break;
	case ICMP_REDIRECT:
	case ICMP_SOURCE_QUENCH:
		return -1;
	case ICMP_TIME_EXCEEDED:
		*ntype = ICMPV6_TIME_EXCEED;
		break;
	case ICMP_PARAMETERPROB:
		/* XXX: */
		switch (ocode) {
		default:
			return -1;
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static inline __sum16
map_trans_icmp_csum_v6v4(__sum16 check, __u8 otype, __u8 ocode, __u8 ntype,
	__u8 ncode, __u8 otypein, __u8 ocodein, __sum16 ocsumin, __u8 ntypein,
	__u8 ncodein, __sum16 ncsumin, struct in6_addr *saddr,
	struct in6_addr *daddr, __be16 payload_len, int iphlen,
	struct iphdr *iph, int ipv6hlen, struct ipv6hdr *ipv6h)
{
	long csum = ntohs(check);
	__be32 *sumtmp;
	u32 t;
	int i;

	csum = ~csum & 0xffff;

	t = otype; t <<= 8; t |= ocode;
	csum -= t & 0xffff;
	CSUM_CO_DEC(csum)

	t = ntype; t <<= 8; t |= ncode;
	csum += t & 0xffff;
	CSUM_CO_INC(csum)

	t = otypein; t <<= 8; t |= ocodein;
	csum -= t & 0xffff;
	CSUM_CO_DEC(csum)

	t = ntypein; t <<= 8; t |= ncodein;
	csum += t & 0xffff;
	CSUM_CO_INC(csum)

	csum -= ntohs(ocsumin) & 0xffff;
	CSUM_CO_DEC(csum)

	csum += ntohs(ncsumin) & 0xffff;
	CSUM_CO_INC(csum)

	for (i = 0; i < 4; ++i) {
		csum -= ntohl(saddr->s6_addr32[i]) & 0xffff;
		CSUM_CO_DEC(csum)
		csum -= (ntohl(saddr->s6_addr32[i]) >> 16) & 0xffff;
		CSUM_CO_DEC(csum)
		csum -= ntohl(daddr->s6_addr32[i]) & 0xffff;
		CSUM_CO_DEC(csum)
		csum -= (ntohl(daddr->s6_addr32[i]) >> 16) & 0xffff;
		CSUM_CO_DEC(csum)
	}

	csum -= ntohs(payload_len) & 0xffff;
	CSUM_CO_DEC(csum)

	t = IPPROTO_ICMPV6;
	csum -= t & 0xffff;
	CSUM_CO_DEC(csum)

	if (ipv6hlen) {
		sumtmp = (__be32 *)ipv6h;
		for (i = 0; i < (ipv6hlen / 4); ++i) {
			csum -= ntohl(sumtmp[i]) & 0xffff;
			CSUM_CO_DEC(csum)
			csum -= (ntohl(sumtmp[i]) >> 16) & 0xffff;
			CSUM_CO_DEC(csum)
		}
	}

	if (iphlen) {
		sumtmp = (__be32 *)iph;
		for (i = 0; i < (iphlen / 4); ++i) {
			csum += ntohl(sumtmp[i]) & 0xffff;
			CSUM_CO_INC(csum)
			csum += (ntohl(sumtmp[i]) >> 16) & 0xffff;
			CSUM_CO_INC(csum)
		}
	}

	csum = ~csum & 0xffff;

	return htons(csum);
}

static inline __sum16
map_trans_icmp_csum_v4v6(__sum16 check, __u8 otype, __u8 ocode, __u8 ntype,
	__u8 ncode, __u8 otypein, __u8 ocodein, __sum16 ocsumin, __u8 ntypein,
	__u8 ncodein, __sum16 ncsumin, struct in6_addr *saddr,
	struct in6_addr *daddr, __be16 payload_len, int iphlen,
	struct iphdr *iph, int ipv6hlen, struct ipv6hdr *ipv6h)
{
	long csum = ntohs(check);
	__be32 *sumtmp;
	u32 t;
	int i;

	csum = ~csum & 0xffff;

	t = otype; t <<= 8; t |= ocode;
	csum -= t & 0xffff;
	CSUM_CO_DEC(csum)

	t = ntype; t <<= 8; t |= ncode;
	csum += t & 0xffff;
	CSUM_CO_INC(csum)

	t = otypein; t <<= 8; t |= ocodein;
	csum -= t & 0xffff;
	CSUM_CO_DEC(csum)

	t = ntypein; t <<= 8; t |= ncodein;
	csum += t & 0xffff;
	CSUM_CO_INC(csum)

	csum -= ntohs(ocsumin) & 0xffff;
	CSUM_CO_DEC(csum)

	csum += ntohs(ncsumin) & 0xffff;
	CSUM_CO_INC(csum)

	for (i = 0; i < 4; ++i) {
		csum += ntohl(saddr->s6_addr32[i]) & 0xffff;
		CSUM_CO_INC(csum)
		csum += (ntohl(saddr->s6_addr32[i]) >> 16) & 0xffff;
		CSUM_CO_INC(csum)
		csum += ntohl(daddr->s6_addr32[i]) & 0xffff;
		CSUM_CO_INC(csum)
		csum += (ntohl(daddr->s6_addr32[i]) >> 16) & 0xffff;
		CSUM_CO_INC(csum)
	}

	csum += ntohs(payload_len) & 0xffff;
	CSUM_CO_INC(csum)

	t = IPPROTO_ICMPV6;
	csum += t & 0xffff;
	CSUM_CO_INC(csum)

	if (iphlen) {
		sumtmp = (__be32 *)iph;
		for (i = 0; i < (iphlen / 4); ++i) {
			csum -= ntohl(sumtmp[i]) & 0xffff;
			CSUM_CO_DEC(csum)
			csum -= (ntohl(sumtmp[i]) >> 16) & 0xffff;
			CSUM_CO_DEC(csum)
		}
	}

	if (ipv6hlen) {
		sumtmp = (__be32 *)ipv6h;
		for (i = 0; i < (ipv6hlen / 4); ++i) {
			csum += ntohl(sumtmp[i]) & 0xffff;
			CSUM_CO_DEC(csum)
			csum += (ntohl(sumtmp[i]) >> 16) & 0xffff;
			CSUM_CO_DEC(csum)
		}
	}

	csum = ~csum & 0xffff;

	return htons(csum);
}

static int
map_trans_icmp_v6v4(struct sk_buff **skb, struct icmphdr *icmph,
	struct in6_addr *saddr, struct in6_addr *daddr, __be16 *payload_len,
	struct map *m)
{
	__u8 otype = 0, ocode = 0;
	__u8 ntype = 0, ncode = 0;
	__u8 otypein = 0, ocodein = 0;
	__u8 ntypein = 0, ncodein = 0;
	__sum16 ocsumin = 0, ncsumin = 0;
	struct icmphdr *icmpinh;
	__sum16 check = icmph->checksum;
	u8 *buf = NULL;
	struct iphdr *iph = NULL;
	struct ipv6hdr *ipv6h = NULL;
	struct frag_hdr *fragh;
	u8 *ptr, *datas, *datad;
	int len, ipv6hlen = 0, iphlen = 0;
	__u8 nexthdr;
	__be16 orig_payload_len = *payload_len;
	struct map_rule *mr = NULL;
	struct in6_addr tmpsaddr6 = {}, tmpdaddr6 = {};
	int ret;

	otype = icmph->type; ocode = icmph->code;
	ret = map_trans_icmp_typecode_v6v4(otype, ocode, &ntype, &ncode);
	icmph->type = ntype; icmph->code = ncode;
	if (ret)
		return ret;

	switch (otype) {
	case ICMPV6_DEST_UNREACH:
	case ICMPV6_PKT_TOOBIG:
	case ICMPV6_TIME_EXCEED:
	case ICMPV6_PARAMPROB:
		ptr = (u8 *)icmph;
		ptr += sizeof(struct icmp6hdr);
		len = skb_tail_pointer(*skb) - ptr;
		buf = kmalloc(len, GFP_KERNEL);
		if (!buf) {
			pr_notice("map_trans_icmp_v6v4: buf malloc failed.\n");
			return -1;
		}
		memcpy(buf, ptr, len);

		iph = (struct iphdr *)ptr;
		datad = (u8 *)iph;
		datad += sizeof(struct iphdr);
		iphlen = sizeof(struct iphdr);

		iph->version = 4;
		iph->ihl = 5;
		iph->tos = 0;

		ipv6h = (struct ipv6hdr *)buf;
		datas = (u8 *)ipv6h;
		datas += sizeof(struct ipv6hdr);
		len -= sizeof(struct ipv6hdr);
		ipv6hlen = sizeof(struct ipv6hdr);
		nexthdr = ipv6h->nexthdr;
		if (ipv6h->nexthdr == IPPROTO_FRAGMENT) {
			fragh = (struct frag_hdr *)datas;
			datas += sizeof(struct frag_hdr);
			len -= sizeof(struct frag_hdr);
			ipv6hlen += sizeof(struct frag_hdr);
			nexthdr = fragh->nexthdr;

			iph->tot_len = htons(ntohs(ipv6h->payload_len) - 8 +
				sizeof(struct iphdr));
			iph->id = htons(ntohl(fragh->identification) & 0xffff);
			iph->frag_off = htons(ntohs(fragh->frag_off) & 0xfff8);
			if (ntohs(fragh->frag_off) & IP6_MF)
				iph->frag_off |= htons(IP_MF);
		} else {
			iph->tot_len = htons(ntohs(ipv6h->payload_len) +
				sizeof(struct iphdr));
			iph->id = 0;
			iph->frag_off = htons(IP_DF);
		}
		iph->ttl = ipv6h->hop_limit;
		mr = map_rule_find_by_ipv6addr(m, &ipv6h->saddr);
		if (mr)
			map_get_map_ipv6_address(mr, &ipv6h->saddr, &tmpsaddr6);
		if (ipv6_addr_equal(&ipv6h->saddr, &tmpsaddr6)) {
			iph->saddr = htonl(ntohl(ipv6h->saddr.s6_addr32[2])
					   << 16);
			iph->saddr |= htonl(ntohl(ipv6h->saddr.s6_addr32[3])
					    >> 16);
		} else {
			if (m->p.br_address_length > 64)
				iph->saddr = ipv6h->saddr.s6_addr32[3];
			else {
				iph->saddr = htonl(ntohl(
					ipv6h->saddr.s6_addr32[2]) << 8);
				iph->saddr |= htonl(ntohl(
					ipv6h->saddr.s6_addr32[3]) >> 24);
			}
		}
		mr = map_rule_find_by_ipv6addr(m, &ipv6h->daddr);
		if (mr)
			map_get_map_ipv6_address(mr, &ipv6h->daddr, &tmpdaddr6);
		if (ipv6_addr_equal(&ipv6h->daddr, &tmpdaddr6)) {
			iph->daddr = htonl(ntohl(ipv6h->daddr.s6_addr32[2])
						 << 16);
			iph->daddr |= htonl(ntohl(ipv6h->daddr.s6_addr32[3])
						  >> 16);
		} else {
			if (m->p.br_address_length > 64)
				iph->daddr = ipv6h->daddr.s6_addr32[3];
			else {
				iph->daddr = htonl(ntohl(
					ipv6h->daddr.s6_addr32[2]) << 8);
				iph->daddr |= htonl(ntohl(
					ipv6h->daddr.s6_addr32[3]) >> 24);
			}
		}
		memcpy(datad, datas, len);
		skb_trim(*skb, (*skb)->len - ipv6hlen + iphlen);
		*payload_len = htons(ntohs(*payload_len) - ipv6hlen + iphlen);
		if (nexthdr == IPPROTO_ICMPV6) {
			iph->protocol = IPPROTO_ICMP;
			ptr = (u8 *)iph;
			ptr += iph->ihl * 4;
			icmpinh = (struct icmphdr *)ptr;
			otypein = icmpinh->type;
			ocodein = icmpinh->code;
			ocsumin = icmpinh->checksum;
			ret = map_trans_icmp_v6v4(skb, icmpinh, &ipv6h->saddr,
				&ipv6h->daddr, &ipv6h->payload_len, m);
			ntypein = icmpinh->type;
			ncodein = icmpinh->code;
			ncsumin = icmpinh->checksum;
#ifdef MAP_DEBUG
			pr_notice("map_trans_icmp_v4v6: otin:%d ocin:%d ntin:%d ncin:%d\n",
				otypein, ocodein, ntypein, ncodein);
#endif
			if (ret) {
				pr_notice("map_trans_icmp_v6v4: innter func err.\n");
				kfree(buf);
				return ret;
			}
		} else
			iph->protocol = nexthdr;
	}

	icmph->checksum = map_trans_icmp_csum_v6v4(check, otype, ocode, ntype,
		ncode, otypein, ocodein, ocsumin, ntypein, ncodein, ncsumin,
		saddr, daddr, orig_payload_len, iphlen, iph, ipv6hlen, ipv6h);

	kfree(buf);

	return 0;
}

static int
map_trans_icmp_v4v6(struct sk_buff **skb, struct icmp6hdr *icmp6h,
	struct in6_addr *saddr, struct in6_addr *daddr, __be16 *payload_len,
	struct map *m)
{
	__u8 otype = 0, ocode = 0;
	__u8 ntype = 0, ncode = 0;
	__u8 otypein = 0, ocodein = 0;
	__u8 ntypein = 0, ncodein = 0;
	__sum16 ocsumin = 0, ncsumin = 0;
	struct icmp6hdr *icmp6inh;
	__sum16 check = icmp6h->icmp6_cksum;
	struct in6_addr icmpsaddr, icmpdaddr;
	u8 *buf = NULL;
	struct iphdr *iph = NULL;
	struct ipv6hdr *ipv6h = NULL;
	u8 *ptr, *datas, *datad;
	int len, ipv6hlen = 0, iphlen = 0;
	struct map_rule *mr = NULL;
	__be32 saddr4, daddr4;
	__be16 sport4, dport4;
	__u8 proto;
	int icmperr;
	int ret;

	otype = icmp6h->icmp6_type; ocode = icmp6h->icmp6_code;
	ret = map_trans_icmp_typecode_v4v6(otype, ocode, &ntype, &ncode);
	icmp6h->icmp6_type = ntype; icmp6h->icmp6_code = ncode;
#ifdef MAP_DEBUG
	pr_notice("map_trans_icmp_v4v6: ot:%d oc:%d nt:%d nc:%d\n",
		otype, ocode, ntype, ncode);
#endif
	if (ret) {
		pr_notice("map_trans_icmp_v4v6: map_trans_icmp_typecode_v4v6 err.\n");
		return ret;
	}

	switch (otype) {
	case ICMP_DEST_UNREACH:
	case ICMP_TIME_EXCEEDED:
		ptr = (u8 *)icmp6h;
		ptr += sizeof(struct icmphdr);
		len = skb_tail_pointer(*skb) - ptr;
		buf = kmalloc(len, GFP_KERNEL);
		if (!buf) {
			pr_notice("map_trans_icmp_v4v6: buf malloc failed.\n");
			return -1;
		}
		memcpy(buf, ptr, len);

		ipv6h = (struct ipv6hdr *)ptr;
		datad = (u8 *)ipv6h;
		datad += sizeof(struct ipv6hdr);
		ipv6hlen = sizeof(struct ipv6hdr);

		iph = (struct iphdr *)buf;
		datas = (u8 *)iph;
		datas += iph->ihl * 4;
		len -= iph->ihl * 4;
		iphlen = iph->ihl * 4;

		if (map_get_addrport(iph, &saddr4, &daddr4, &sport4, &dport4,
		    &proto, &icmperr)) {
			pr_notice("map_trans_icmp_v4v6: map_get_addrport error\n");
			return -1;
		}

		mr = map_rule_find_by_ipv4addrport(m, &iph->saddr, &sport4, 1);
		if (mr) {
			map_gen_addr6(&icmpsaddr, iph->saddr, sport4, mr, 1);
		} else {
			icmpsaddr.s6_addr32[0] = m->p.br_address.s6_addr32[0];
			icmpsaddr.s6_addr32[1] = m->p.br_address.s6_addr32[1];
			if (m->p.br_address_length > 64) {
				icmpsaddr.s6_addr32[2] =
					m->p.br_address.s6_addr32[1];
				icmpsaddr.s6_addr32[3] = iph->saddr;
			} else {
				icmpsaddr.s6_addr32[2] =
					htonl(ntohl(iph->saddr) >> 8);
				icmpsaddr.s6_addr32[3] =
					htonl(ntohl(iph->saddr) << 24);
			}
		}

		mr = map_rule_find_by_ipv4addrport(m, &iph->daddr, &dport4, 1);
		if (mr) {
			map_gen_addr6(&icmpsaddr, iph->daddr, dport4, mr, 1);
		} else {
			icmpdaddr.s6_addr32[0] = m->p.br_address.s6_addr32[0];
			icmpdaddr.s6_addr32[1] = m->p.br_address.s6_addr32[1];
			if (m->p.br_address_length > 64) {
				icmpdaddr.s6_addr32[2] =
					m->p.br_address.s6_addr32[2];
				icmpdaddr.s6_addr32[3] = iph->daddr;
			} else {
				icmpdaddr.s6_addr32[2] =
					htonl(ntohl(iph->daddr) >> 8);
				icmpdaddr.s6_addr32[3] =
					htonl(ntohl(iph->daddr) << 24);
			}
		}

		skb_put(*skb, ipv6hlen - iphlen); /* XXX: */
		*payload_len = htons(ntohs(*payload_len) + ipv6hlen - iphlen);

		ipv6h->version = 6;
		ipv6h->priority = 0; /* XXX: */
		ipv6h->flow_lbl[0] = 0;
		ipv6h->flow_lbl[1] = 0;
		ipv6h->flow_lbl[2] = 0;
		ipv6h->payload_len = htons(ntohs(iph->tot_len) - iph->ihl * 4);
		ipv6h->hop_limit = iph->ttl;
		memcpy(&ipv6h->saddr, &icmpsaddr, sizeof(struct in6_addr));
		memcpy(&ipv6h->daddr, &icmpdaddr, sizeof(struct in6_addr));
		memcpy(datad, datas, len);
		if (iph->protocol == IPPROTO_ICMP) {
			ipv6h->nexthdr = IPPROTO_ICMPV6;
			ptr = (u8 *)ipv6h;
			ptr += sizeof(struct ipv6hdr);
			icmp6inh = (struct icmp6hdr *)ptr;
			otypein = icmp6inh->icmp6_type;
			ocodein = icmp6inh->icmp6_code;
			ocsumin = icmp6inh->icmp6_cksum;
			ret = map_trans_icmp_v4v6(skb, icmp6inh, &ipv6h->saddr,
				&ipv6h->daddr, &ipv6h->payload_len, m);
			ntypein = icmp6inh->icmp6_type;
			ncodein = icmp6inh->icmp6_code;
			ncsumin = icmp6inh->icmp6_cksum;
#ifdef MAP_DEBUG
			pr_notice("map_trans_icmp_v4v6: otin:%d ocin:%d ntin:%d ncin:%d\n",
				otypein, ocodein, ntypein, ncodein);
#endif
			if (ret) {
				pr_notice("map_trans_icmp_v4v6: innter func err.\n");
				kfree(buf);
				return ret;
			}
		} else
			ipv6h->nexthdr = iph->protocol;
	}

	icmp6h->icmp6_cksum = map_trans_icmp_csum_v4v6(check, otype, ocode,
		ntype, ncode, otypein, ocodein, ocsumin, ntypein, ncodein,
		ncsumin, saddr, daddr, *payload_len, iphlen, iph, ipv6hlen,
		ipv6h);

	kfree(buf);

	return 0;
}

/* XXX: */

int
map_trans_validate_src(struct sk_buff *skb, struct map *m, __be32 *saddr4,
		       int *fb)
{
	struct ipv6hdr *ipv6h = ipv6_hdr(skb);
	struct map_rule *mr;
	u8 *ptr;
	struct iphdr *icmp6iph;
	struct tcphdr *tcph, *icmp6tcph;
	struct udphdr *udph, *icmp6udph;
	struct icmp6hdr *icmp6h;
	struct icmphdr *icmpicmph;
	__u8 proto;
	__be32 saddr;
	__be16 sport;
	struct in6_addr addr6;
	int src_prefix_length;
	int err = 0;

	proto = ipv6h->nexthdr;
	ptr = (u8 *)ipv6h;
	ptr += sizeof(struct ipv6hdr);
	if (proto == IPPROTO_FRAGMENT) {
		proto = ((struct frag_hdr *)ptr)->nexthdr;
		ptr += sizeof(struct frag_hdr);
	}

	if (proto != IPPROTO_ICMPV6 &&
	    proto != IPPROTO_TCP &&
	    proto != IPPROTO_UDP) {
		pr_notice("map_trans_validate_src: is this transed?\n");
		err = -1;
		goto err;
	}

	if (m->p.role == MAP_ROLE_CE &&
	    ipv6_prefix_equal(&ipv6h->saddr, &m->p.br_address,
	    m->p.br_address_length)) {
		if (m->p.br_address_length > 64) {
			*saddr4 = ipv6h->saddr.s6_addr32[3];
		} else {
			*saddr4 = htonl(ntohl(ipv6h->saddr.s6_addr32[2]) << 8);
			*saddr4 |=
				htonl(ntohl(ipv6h->saddr.s6_addr32[3]) >> 24);
		}
		return 0;
	}

	saddr = htonl(ntohl(ipv6h->saddr.s6_addr32[2]) << 16);
	saddr |= htonl(ntohl(ipv6h->saddr.s6_addr32[3]) >> 16);

	switch (proto) {
	case IPPROTO_ICMPV6:
		icmp6h = (struct icmp6hdr *)ptr;
		switch (icmp6h->icmp6_type) {
		case ICMPV6_DEST_UNREACH:
		case ICMPV6_PKT_TOOBIG:
			ptr = (u8 *)icmp6h;
			ptr += sizeof(struct icmp6hdr);
			icmp6iph = (struct iphdr *)ptr;
			saddr = icmp6iph->daddr;
			ptr += icmp6iph->ihl * 4;
			switch (icmp6iph->protocol) {
			case IPPROTO_TCP:
				icmp6tcph = (struct tcphdr *)ptr;
				sport = icmp6tcph->dest;
				break;
			case IPPROTO_UDP:
				icmp6udph = (struct udphdr *)ptr;
				sport = icmp6udph->dest;
				break;
			case IPPROTO_ICMP:
				icmpicmph = (struct icmphdr *)ptr;
				sport = icmpicmph->un.echo.id;
				break;
			default:
				pr_notice("map_trans_validate_src: unknown proto transed in icmp error.\n");
				err = -1;
				goto err;
			}
			break;
		default:
			sport = icmp6h->icmp6_dataun.u_echo.identifier;
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
		pr_notice("map_trans_validate_src: unknown encaped.\n");
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

	if (map_gen_addr6(&addr6, saddr, sport, mr, 1)) {
		if (m->p.role == MAP_ROLE_BR) {
			*fb = 1;
			goto fallback;
		} else {
			err = -1;
			goto err;
		}
	}

	if (mr->p.ipv4_prefix_length + mr->p.ea_length < 32)
		src_prefix_length = 80 + mr->p.ipv4_prefix_length +
			mr->p.ea_length;
	else
		src_prefix_length = 128;
	if (!ipv6_prefix_equal(&addr6, &ipv6h->saddr, src_prefix_length)) {
		if (m->p.role == MAP_ROLE_BR) {
			*fb = 1;
			goto fallback;
		} else {
			pr_notice("map_trans_validate_src: validation failed.\n");
			err = -1;
			goto err_icmpv6_send;
		}
	}

fallback:
	if (*fb) {
		if (m->p.br_address_length > 64) {
			*saddr4 = ipv6h->saddr.s6_addr32[3];
		} else {
			*saddr4 = htonl(ntohl(ipv6h->saddr.s6_addr32[2]) << 8);
			*saddr4 |=
				htonl(ntohl(ipv6h->saddr.s6_addr32[3]) >> 24);
		}
	} else {
		*saddr4 = htonl(ntohl(ipv6h->saddr.s6_addr32[2]) << 16);
		*saddr4 |= htonl(ntohl(ipv6h->saddr.s6_addr32[3]) >> 16);
	}

	return 0;

err_icmpv6_send:
	pr_notice("map_trans_validate_src: icmpv6_send(skb, ICMPV6_DEST_UNREACH, 5 /* Source address failed ingress/egress policy */, 0);\n");
	icmpv6_send(skb, ICMPV6_DEST_UNREACH,
		5 /* Source address failed ingress/egress policy */, 0);
err:
	map_debug_print_skb("map_trans_validate_src", skb);
	return err;
}

int
map_trans_validate_dst(struct sk_buff *skb, struct map *m, __be32 *daddr4)
{
	struct ipv6hdr *ipv6h = ipv6_hdr(skb);
	u8 *ptr;
	struct ipv6hdr *icmp6ipv6h;
	struct tcphdr *tcph, *icmp6tcph;
	struct udphdr *udph, *icmp6udph;
	struct icmp6hdr *icmp6h, *icmp6icmp6h;
	__u8 proto, nexthdr;
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

	if (proto != IPPROTO_ICMPV6 &&
	    proto != IPPROTO_TCP &&
	    proto != IPPROTO_UDP) {
		pr_notice("map_trans_validate_dst: is this transed?\n");
		err = -1;
		goto err;
	}

	if (!ipv6_prefix_equal(&ipv6h->daddr, &m->map_ipv6_address,
	    m->map_ipv6_address_length)) {
		pr_notice("map_trans_validate_dst: not match my address.\n");
		err = -1;
		goto err;
	}

	if (m->p.role == MAP_ROLE_BR ||
	    (m->p.role == MAP_ROLE_CE && !m->bmr)) {
		if (m->p.br_address_length > 64) {
			*daddr4 = ipv6h->daddr.s6_addr32[3];
		} else {
			*daddr4 = htonl(ntohl(ipv6h->daddr.s6_addr32[2]) << 8);
			*daddr4 |=
				htonl(ntohl(ipv6h->daddr.s6_addr32[3]) >> 24);
		}
		return 0;
	}

	if (!m->bmr) {
		pr_notice("map_trans_validate_dst: m->bmr is null.\n");
		err = -1;
		goto err;
	}

	daddr = htonl(ntohl(ipv6h->daddr.s6_addr32[2]) << 16);
	daddr |= htonl(ntohl(ipv6h->daddr.s6_addr32[3]) >> 16);

	switch (proto) {
	case IPPROTO_ICMPV6:
		icmp6h = (struct icmp6hdr *)ptr;
		switch (icmp6h->icmp6_type) {
		case ICMPV6_DEST_UNREACH:
		case ICMPV6_PKT_TOOBIG:
		case ICMPV6_TIME_EXCEED:
		case ICMPV6_PARAMPROB:
			ptr = (u8 *)icmp6h;
			ptr += sizeof(struct icmp6hdr);
			icmp6ipv6h = (struct ipv6hdr *)ptr;
			daddr = htonl(ntohl(icmp6ipv6h->saddr.s6_addr32[2])
				      << 16);
			daddr |= htonl(ntohl(icmp6ipv6h->saddr.s6_addr32[3])
				       >> 16);
			nexthdr = icmp6ipv6h->nexthdr;
			ptr += sizeof(struct ipv6hdr);
			if (nexthdr == IPPROTO_FRAGMENT) {
				nexthdr = ((struct frag_hdr *)ptr)->nexthdr;
				ptr += sizeof(struct frag_hdr);
			}
			switch (nexthdr) {
			case IPPROTO_TCP:
				icmp6tcph = (struct tcphdr *)ptr;
				dport = icmp6tcph->source;
				break;
			case IPPROTO_UDP:
				icmp6udph = (struct udphdr *)ptr;
				dport = icmp6udph->source;
				break;
			case IPPROTO_ICMPV6:
				icmp6icmp6h = (struct icmp6hdr *)ptr;
				dport =
				    icmp6icmp6h->icmp6_dataun.u_echo.identifier;
				break;
			default:
				pr_notice("map_trans_validate_dst: unknown proto transed in icmp error.\n");
				err = -1;
				goto err;
			}
			break;
		default:
			dport = icmp6h->icmp6_dataun.u_echo.identifier;
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
		pr_notice("map_trans_validate_dst: unknown encaped.\n");
		err = -1;
		goto err;
	}

	read_lock(&m->rule_lock);
	if (!m->bmr) {
		read_unlock(&m->rule_lock);
		pr_notice("map_trans_validate_dst: bmr is null..\n");
		err = -1;
		goto err;
	}
	if (map_gen_addr6(&addr6, daddr, dport, m->bmr, 1)) {
		read_unlock(&m->rule_lock);
		pr_notice("map_trans_validate_dst: map_gen_addr6 failed.\n");
		err = -1;
		goto err;
	}
	read_unlock(&m->rule_lock);

	if (!ipv6_prefix_equal(&addr6, &ipv6h->daddr,
			       m->map_ipv6_address_length)) {
		pr_notice("map_trans_validate_dst: validation failed.\n");
		pr_notice("map_trans_validate_dst: addr6 = %08x%08x%08x%08x\n",
			ntohl(addr6.s6_addr32[0]),
			ntohl(addr6.s6_addr32[1]),
			ntohl(addr6.s6_addr32[2]),
			ntohl(addr6.s6_addr32[3]));
		pr_notice("map_trans_validate_dst: ipv6h->daddr = %08x%08x%08x%08x\n",
			ntohl(ipv6h->daddr.s6_addr32[0]),
			ntohl(ipv6h->daddr.s6_addr32[1]),
			ntohl(ipv6h->daddr.s6_addr32[2]),
			ntohl(ipv6h->daddr.s6_addr32[3]));
		pr_notice("map_trans_validate_dst: daddr = %d.%d.%d.%d dport = %d(%04x)\n",
			((ntohl(daddr) >> 24) & 0xff),
			((ntohl(daddr) >> 16) & 0xff),
			((ntohl(daddr) >> 8) & 0xff),
			(ntohl(daddr) & 0xff),
			ntohs(dport), ntohs(dport));
		err = -1;
		goto err_icmpv6_send;
	}

	*daddr4 = htonl(ntohl(ipv6h->daddr.s6_addr32[2]) << 16);
	*daddr4 |= htonl(ntohl(ipv6h->daddr.s6_addr32[3]) >> 16);

	return 0;

err_icmpv6_send:
	pr_notice("map_trans_validate_dst: icmpv6_send(skb, ICMPV6_DEST_UNREACH, ICMPV6_ADDR_UNREACH, 0);\n");
	icmpv6_send(skb, ICMPV6_DEST_UNREACH, ICMPV6_ADDR_UNREACH, 0);
err:
	map_debug_print_skb("map_trans_validate_dst", skb);
	return err;
}

int
map_trans_forward_v6v4(struct sk_buff *skb, struct map *m, __be32 *saddr4,
	__be32 *daddr4, int fb, int frag)
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

	if (nexthdr != IPPROTO_ICMPV6 &&
	    nexthdr != IPPROTO_TCP &&
	    nexthdr != IPPROTO_UDP) {
		pr_notice("map_trans_forward_v6v4: this packet is not transed?\n");
		err = -1;
		goto err;
	}

	skb_dst_drop(skb);
	skb_pull(skb, hsize);
	skb_push(skb, sizeof(struct iphdr));
	skb_reset_network_header(skb);
	skb->protocol = htons(ETH_P_IP);
	iph = ip_hdr(skb);

	iph->version = 4;
	iph->ihl = 5;
	iph->tos = 0;
	if (orig_ipv6h.nexthdr == IPPROTO_FRAGMENT) {
		iph->tot_len = htons(ntohs(orig_ipv6h.payload_len) - 8 +
			sizeof(struct iphdr));
		iph->id = htons(ntohl(orig_fragh.identification) & 0xffff);
		iph->frag_off = htons(ntohs(orig_fragh.frag_off) & 0xfff8);
		if (ntohs(orig_fragh.frag_off) & IP6_MF)
			iph->frag_off |= htons(IP_MF);
	} else {
		iph->tot_len = htons(ntohs(orig_ipv6h.payload_len) +
			sizeof(struct iphdr));
		iph->id = 0;
		iph->frag_off = frag ? 0 : htons(IP_DF);
	}
	if (nexthdr == IPPROTO_ICMPV6) {
		__be16 payload_len = orig_ipv6h.payload_len;
		iph->protocol = IPPROTO_ICMP;
		ptr = (u8 *)iph;
		ptr += iph->ihl * 4;
		err = map_trans_icmp_v6v4(&skb, (struct icmphdr *)ptr,
			&orig_ipv6h.saddr, &orig_ipv6h.daddr, &payload_len, m);
		if (err)
			goto err;
		iph->tot_len = htons(ntohs(iph->tot_len) -
			(ntohs(orig_ipv6h.payload_len) - ntohs(payload_len)));
	} else
		iph->protocol = nexthdr;
	iph->ttl = orig_ipv6h.hop_limit;
	iph->saddr = *saddr4;
	iph->daddr = *daddr4;

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
		pr_notice("map_trans_forward_v6v4: saddr:%d.%d.%d.%d daddr:%d.%d.%d.%d\n",
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

	if (iph->protocol != IPPROTO_ICMP)
		*checkp = map_trans_update_csum_v6v4(*checkp,
			&orig_ipv6h.saddr, &orig_ipv6h.daddr, *saddr4, *daddr4);

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
	map_debug_print_skb("map_trans_forward_v6v4", skb);
out:
	return err;
}

int
map_trans_forward_v4v6(struct sk_buff *skb, struct map *m, struct map_rule *mr,
		       int fb, int df)
{
	struct flowi6 fl6;
	struct in6_addr saddr6, daddr6;
	struct iphdr orig_iph;
	unsigned int max_headroom;
	struct sk_buff *oskb;
	struct dst_entry *dst;
	struct net *net = dev_net(m->dev);
	struct ipv6hdr *ipv6h;
	int pkt_len;
	struct iphdr *iph;
	__be32 *daddrp = NULL;
	__be16 *dportp = NULL;
	__sum16 *checkp = NULL;
	struct icmphdr *icmph;
	u8 *ptr;
	int err = 0;

	iph = ip_hdr(skb);
	if (iph->protocol == IPPROTO_ICMP) {
		ptr = (u8 *)iph;
		ptr += iph->ihl * 4;
		icmph = (struct icmphdr *)ptr;
		if (((icmph->type == ICMP_DEST_UNREACH) ||
		    (icmph->type == ICMP_TIME_EXCEEDED)) &&
		    (skb_tailroom(skb) < sizeof(struct ipv6hdr))) {
			oskb = skb;
			skb = skb_copy_expand(skb, LL_MAX_HEADER,
				sizeof(struct ipv6hdr), GFP_ATOMIC);
			kfree_skb(oskb);
		}
	}

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
	/* XXXXX: */
	if (m->p.role == MAP_ROLE_BR || !m->bmr ||
	    (m->psid_length <= 0 && m->p.napt_always == MAP_NAPT_ALWAYS_F)) {
		if (m->psid_length <= 0 &&
		    m->p.napt_always == MAP_NAPT_ALWAYS_F) {
			saddr6.s6_addr32[2] = htonl(ntohl(iph->saddr) >> 16);
			saddr6.s6_addr32[3] = htonl(ntohl(iph->saddr) << 16);
		} else if (m->p.br_address_length > 64) {
			saddr6.s6_addr32[2] = m->map_ipv6_address.s6_addr32[2];
			saddr6.s6_addr32[3] = iph->saddr;
		} else {
			saddr6.s6_addr32[2] = htonl(ntohl(iph->saddr) >> 8);
			saddr6.s6_addr32[3] = htonl(ntohl(iph->saddr) << 24);
		}
	} else {
		saddr6.s6_addr32[2] = m->map_ipv6_address.s6_addr32[2];
		saddr6.s6_addr32[3] = m->map_ipv6_address.s6_addr32[3];
	}

	if (mr) {
		map_gen_addr6(&daddr6, iph->daddr, *dportp, mr, 1);
	} else {
		daddr6.s6_addr32[0] = m->p.br_address.s6_addr32[0];
		daddr6.s6_addr32[1] = m->p.br_address.s6_addr32[1];
		if (m->p.br_address_length > 64) {
			daddr6.s6_addr32[2] = m->p.br_address.s6_addr32[2];
			daddr6.s6_addr32[3] = iph->daddr;
		} else {
			daddr6.s6_addr32[2] = htonl(ntohl(iph->daddr) >> 8);
			daddr6.s6_addr32[3] = htonl(ntohl(iph->daddr) << 24);
		}
	}

	if (m->p.role == MAP_ROLE_BR && fb) {
		err = map_napt(iph, 1, m, &daddrp, &dportp, &checkp, &daddr6,
			       fb);
		iph->check = 0;
		iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
		if (err)
			goto err;
	}

	if (iph->protocol != IPPROTO_ICMP)
		*checkp = map_trans_update_csum_v4v6(*checkp, iph->saddr,
			iph->daddr, &saddr6, &daddr6);

	memset(&fl6, 0, sizeof(fl6));
	fl6.saddr = saddr6;
	fl6.daddr = daddr6;
	fl6.flowi6_oif = m->dev->ifindex;
	fl6.flowlabel = 0;

	dst = ip6_route_output(net, NULL, &fl6);
	/* dst_metric_set(dst, RTAX_MTU, 1280); */
	if (dst_mtu(dst) > m->p.ipv6_fragment_size)
		dst_metric_set(dst, RTAX_MTU, m->p.ipv6_fragment_size);

	max_headroom = sizeof(struct ipv6hdr) + sizeof(struct frag_hdr) -
		       sizeof(struct iphdr) + 20;

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
	skb_dst_set(skb, dst);

	memcpy(&orig_iph, iph, sizeof(orig_iph));
	skb_pull(skb, orig_iph.ihl * 4);
	skb_push(skb, sizeof(struct ipv6hdr));
	skb_reset_network_header(skb);
	skb->protocol = htons(ETH_P_IPV6);
	ipv6h = ipv6_hdr(skb);

	ipv6h->version = 6;
	ipv6h->priority = 0; /* XXX: */
	ipv6h->flow_lbl[0] = 0;
	ipv6h->flow_lbl[1] = 0;
	ipv6h->flow_lbl[2] = 0;
	ipv6h->payload_len = htons(ntohs(orig_iph.tot_len) - orig_iph.ihl * 4);
	ipv6h->hop_limit = orig_iph.ttl;
	memcpy(&ipv6h->saddr, &fl6.saddr, sizeof(struct in6_addr));
	memcpy(&ipv6h->daddr, &fl6.daddr, sizeof(struct in6_addr));
	if (orig_iph.protocol == IPPROTO_ICMP) {
		ipv6h->nexthdr = IPPROTO_ICMPV6;
		ptr = (u8 *)ipv6h;
		ptr += sizeof(*ipv6h);
		err = map_trans_icmp_v4v6(&skb, (struct icmp6hdr *)ptr,
			&ipv6h->saddr, &ipv6h->daddr, &ipv6h->payload_len, m);
		if (err)
			goto tx_err_dst_release;
	} else
		ipv6h->nexthdr = orig_iph.protocol;

	pkt_len = skb->len;

	skb->local_df = 1;
#ifdef MAP_DEBUG
	map_debug_print_skb("map_trans_forward_v4v6 before sending", skb);
#endif
	if (df)
		err = ip6_local_out(skb);
	else
		err = ip6_fragment(skb, ip6_local_out);

	return 0;

tx_err_dst_release:
	pr_notice("map_trans_forward_v4v6: tx_err_dst_release:\n");
	/* dst_release(dst); */ /* XXX: */
err:
	map_debug_print_skb("map_trans_forward_v4v6 ERROR", skb);
out:
	return err;
}
