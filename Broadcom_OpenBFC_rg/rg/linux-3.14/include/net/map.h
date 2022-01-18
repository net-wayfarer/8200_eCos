/****************************************************************************
*
* Portions of this software (c) 2016 Broadcom. All rights reserved.
* The term “Broadcom” refers to Broadcom Limited and/or its subsidiaries.
*
****************************************************************************
*
* Filename: map.h
*
****************************************************************************
* Description:
* MAP device implementation
*
****************************************************************************/
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <m-asama@ginzado.co.jp> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return Masakazu Asama
 * ----------------------------------------------------------------------------
 */

#ifndef _MAP_H_
#define _MAP_H_

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/if.h>
#include <linux/if_map.h>
#include <linux/in6.h>

#define MAP_NAPT_HASH_LOOKUP_SIZE	(1<<9)
#define MAP_NAPT_HASH_CREATE_SIZE	(1<<7)
#define MAP_NAPT_EXPIRES_TCP		(3*24*60*60*HZ)
#define MAP_NAPT_EXPIRES_OTHER		(3*60*60*HZ)
#define MAP_NAPT_GC_THRESHOLD		(60*HZ)

#define MAP_DEFRAG6_HASH_SIZE		(1<<7)
#define MAP_DEFRAG6_EXPIRES		(60*HZ)

#define MAP_NAPT_F_O_SYN		(1<<0)
#define MAP_NAPT_F_I_SYN_ACK		(1<<1)
#define MAP_NAPT_F_O_ACK		(1<<2)
#define MAP_NAPT_F_O_FIN		(1<<3)
#define MAP_NAPT_F_I_FIN_ACK		(1<<4)
#define MAP_NAPT_F_I_FIN		(1<<5)
#define MAP_NAPT_F_O_FIN_ACK		(1<<6)
#define MAP_NAPT_F_RST			(1<<7)
#define MAP_NAPT_F_FIN			(MAP_NAPT_F_I_FIN | \
					 MAP_NAPT_F_I_FIN_ACK | \
					 MAP_NAPT_F_O_FIN | \
					 MAP_NAPT_F_O_FIN_ACK)
#define MAP_NAPT_F_EST			(MAP_NAPT_F_O_SYN | \
					 MAP_NAPT_F_I_SYN_ACK | \
					 MAP_NAPT_F_O_ACK)

#define MAP_NAPT_TCP_F_SYN		(1<<0)
#define MAP_NAPT_TCP_F_ACK		(1<<1)
#define MAP_NAPT_TCP_F_FIN		(1<<2)
#define MAP_NAPT_TCP_F_RST		(1<<3)

struct map_defrag6_node {
	struct hlist_node	dn_hash;
	struct list_head	dn_list;
	struct hlist_node	dn_pending;
	struct sk_buff		*skb;
	struct in6_addr		*saddr;
	struct in6_addr		*daddr;
	__be32			id;
	__be16			payload_len;
	__be16			frag_off;
	u32			h;
	unsigned long		received;
};

/*
struct map_napt_node_parm {
	__be32			raddr, laddr, maddr;
	__be16			rport, lport, mport;
	struct in6_addr		laddr6;
	__u8			proto;
	__u8			flags;
	struct timespec		last_used;
};

struct map_napt_parm {
	struct timespec		current_time;
	unsigned long		napt_node_num;
	struct map_napt_node_parm napt_node[0];
};

struct map_napt_block {
	__u16			min, max;
};
*/

struct map_napt_node {
	struct hlist_node	nn_hash_lup0, nn_hash_lup1, nn_hash_crat;
	struct list_head	nn_list, nn_gc_list;
	__be32			raddr, laddr, maddr;
	__be16			rport, lport, mport;
	struct in6_addr		laddr6;
	__u8			proto;
	__u8			flags;
	unsigned long		last_used;
};

struct mrtree_node {
	__u32			val[4];
	int			len;
	struct map_rule		*mr;
	struct mrtree_node	*children[2];
	struct mrtree_node	*parent;
};

/*
struct map_rule_parm {
	struct in6_addr		ipv6_prefix;
	__u8			ipv6_prefix_length;
	__be32			ipv4_prefix;
	__u8			ipv4_prefix_length;
	__u16			psid_prefix;
	__u8			psid_prefix_length;
	__u8			ea_length;
	__u8			psid_offset;
	__u8			forwarding_mode;
	__u8			forwarding_rule;
};
*/

struct map_rule {
	struct list_head	list;
	struct mrtree_node	*mrtn_ipv6addr;
	struct mrtree_node	*mrtn_ipv4addrport;
	struct map_rule_parm	p;
};

/*
struct map_pool_parm {
	__be32			pool_prefix;
	__u8			pool_prefix_length;
};
*/

struct map_pool {
	struct list_head	list;
	struct map_pool_parm	p;
};

/*
struct map_parm {
	char			name[IFNAMSIZ];
	int			tunnel_source;
	struct in6_addr		br_address;
	__u8			br_address_length;
	__u8			role;
	__u8			default_forwarding_mode;
	__u8			default_forwarding_rule;
	int			ipv6_fragment_size;
	__u8			ipv4_fragment_inner;
	__u8			napt_always;
	__u8			napt_force_recycle;
	unsigned long		rule_num;
	unsigned long		pool_num;
	struct map_rule_parm	rule[0];
	struct map_pool_parm	pool[0];
};
*/

struct map {
	struct list_head	list;
	struct map_parm		p;
	struct net_device	*dev;
	struct list_head	rule_list;
	struct mrtree_node	*mrtn_root_ipv6addr;
	struct mrtree_node	*mrtn_root_ipv4addrport;
	rwlock_t		rule_lock;
	struct list_head	pool_list;
	rwlock_t		pool_lock;
	struct map_rule		*bmr;
	struct in6_addr		map_ipv6_address;
	__u8			map_ipv6_address_length;
	__be32			laddr4;
	__u16			psid;
	int			psid_length;
	struct map_napt_block	*port_range;
	rwlock_t		port_range_lock;
	int			port_range_length;
	/*
	int			ipv6_fragment_size;
	int			ipv4_fragment_size;
	*/
	struct hlist_head	napt_hash_lup0[MAP_NAPT_HASH_LOOKUP_SIZE];
	struct hlist_head	napt_hash_lup1[MAP_NAPT_HASH_LOOKUP_SIZE];
	struct hlist_head	napt_hash_crat[MAP_NAPT_HASH_CREATE_SIZE];
	struct list_head	napt_list;
	struct list_head	napt_gc_list;
	rwlock_t		napt_lock;
	unsigned long		napt_last_gc;
	struct hlist_head	defrag6_hash[MAP_DEFRAG6_HASH_SIZE];
	struct list_head	defrag6_list;
	rwlock_t		defrag6_lock;
	unsigned long		defrag6_last_gc;
	int			psid_offset_nums[17];
	rwlock_t		psid_offset_nums_lock;
};

/*
struct map_current_parm {
	int			has_bmr;
	struct map_rule_parm	bmrp;
	struct in6_addr		map_ipv6_address;
	__u8			map_ipv6_address_length;
	__be32			laddr4;
	__u16			psid;
	int			psid_length;
	int			port_range_length;
	struct map_napt_block	port_range[0];
};
*/

struct map_net {
	struct net_device	*map_fb_dev;
	struct list_head	map_list;
	rwlock_t		map_list_lock;
};

int map_gen_addr6(struct in6_addr *addr6, __be32 addr4, __be16 port4,
		  struct map_rule *mr, int trans);
int map_get_addrport(struct iphdr *iph, __be32 *saddr4, __be32 *daddr4,
		     __be16 *sport4, __be16 *dport4, __u8 *proto, int *icmperr);
int map_get_map_ipv6_address(struct map_rule *mr, struct in6_addr *ipv6addr,
			     struct in6_addr *map_ipv6_address);

struct map_rule *map_rule_find_by_ipv6addr(struct map *m,
					   struct in6_addr *ipv6addr);
struct map_rule *map_rule_find_by_ipv4addrport(struct map *m, __be32 *ipv4addr,
					       __be16 *port, int fro);
int map_rule_free(struct map *m, struct map_rule *mr);
int map_rule_add(struct map *m, struct map_rule_parm *mrp);
int map_rule_change(struct map *m, struct map_rule_parm *mrp);
int map_rule_delete(struct map *m, struct map_rule_parm *mrp);
void mrtree_node_dump(struct mrtree_node *root);
int map_rule_init(void);
void map_rule_exit(void);

int map_trans_validate_src(struct sk_buff *skb, struct map *m, __be32 *saddr4,
			   int *fb);
int map_trans_validate_dst(struct sk_buff *skb, struct map *m, __be32 *daddr4);
int map_trans_forward_v6v4(struct sk_buff *skb, struct map *m, __be32 *saddr4,
			   __be32 *daddr4, int fb, int frag);
int map_trans_forward_v4v6(struct sk_buff *skb, struct map *m,
			   struct map_rule *mr, int fb, int df);

int map_encap_validate_src(struct sk_buff *skb, struct map *m, __be32 *saddr4,
			   int *fb);
int map_encap_validate_dst(struct sk_buff *skb, struct map *m, __be32 *daddr4);
int map_encap_forward_v6v4(struct sk_buff *skb, struct map *m, __be32 *saddr4,
			   __be32 *daddr4, int fb);
int map_encap_forward_v4v6(struct sk_buff *skb, struct map *m,
			   struct map_rule *mr, int fb);

int map_napt_hairpin(struct sk_buff *skb, struct map *m, __be32 *daddrp,
		     __be16 *dportp, struct in6_addr *saddr6, int fb);
void map_napt_nn_gc(struct map *m);
int map_napt(struct iphdr *iph, int dir, struct map *m, __be32 **saddrpp,
	     __be16 **sportpp, __sum16 **checkpp, struct in6_addr *saddr6,
	     int fb);
int map_napt_init(void);
void map_napt_exit(void);

struct sk_buff *map_defrag6(struct sk_buff *skb, struct map *m);
int map_defrag6_init(void);
void map_defrag6_exit(void);

void map_napt_debug_pool(struct map *m);

static inline void map_debug_print_skb(const char *func, struct sk_buff *skb)
{
	struct iphdr *iph = NULL;
	struct ipv6hdr *ipv6h = NULL;
	struct tcphdr *tcph = NULL;
	struct udphdr *udph = NULL;
	struct icmphdr *icmph = NULL;
	struct icmp6hdr *icmp6h = NULL;
	__u8 nexthdr;
	u8 *ptr;

	if (!skb) {
		pr_notice("%s: skb == NULL\n", func);
		return;
	}

	switch (ntohs(skb->protocol)) {
	case ETH_P_IP:
		iph = ip_hdr(skb);
		pr_notice("%s: ipv4 src:%d.%d.%d.%d dst:%d.%d.%d.%d\n",
			func,
			((ntohl(iph->saddr) >> 24) & 0xff),
			((ntohl(iph->saddr) >> 16) & 0xff),
			((ntohl(iph->saddr) >> 8) & 0xff),
			((ntohl(iph->saddr)) & 0xff),
			((ntohl(iph->daddr) >> 24) & 0xff),
			((ntohl(iph->daddr) >> 16) & 0xff),
			((ntohl(iph->daddr) >> 8) & 0xff),
			((ntohl(iph->daddr)) & 0xff));
		switch (iph->protocol) {
		case IPPROTO_TCP:
			tcph = (struct tcphdr *)(((u8 *)iph) + iph->ihl * 4);
			break;
		case IPPROTO_UDP:
			udph = (struct udphdr *)(((u8 *)iph) + iph->ihl * 4);
			break;
		case IPPROTO_ICMP:
			icmph = (struct icmphdr *)(((u8 *)iph) + iph->ihl * 4);
			break;
		default:
			pr_notice("%s: unknown transport\n", func);
			return;
		}
		break;
	case ETH_P_IPV6:
		ipv6h = ipv6_hdr(skb);
		pr_notice("%s: ipv6 src:%08x%08x%08x%08x dst:%08x%08x%08x%08x\n",
			func,
			(ntohl(ipv6h->saddr.s6_addr32[0])),
			(ntohl(ipv6h->saddr.s6_addr32[1])),
			(ntohl(ipv6h->saddr.s6_addr32[2])),
			(ntohl(ipv6h->saddr.s6_addr32[3])),
			(ntohl(ipv6h->daddr.s6_addr32[0])),
			(ntohl(ipv6h->daddr.s6_addr32[1])),
			(ntohl(ipv6h->daddr.s6_addr32[2])),
			(ntohl(ipv6h->daddr.s6_addr32[3])));
		ptr = (u8 *)ipv6h;
		nexthdr = ipv6h->nexthdr;
		ptr += sizeof(struct ipv6hdr);
		if (nexthdr == IPPROTO_FRAGMENT) {
			pr_notice("%s: IPPROTO_FRAGMENT\n", func);
			nexthdr = ((struct frag_hdr *)ptr)->nexthdr;
			ptr += sizeof(struct frag_hdr);
		}
		switch (nexthdr) {
		case IPPROTO_TCP:
			tcph = (struct tcphdr *)(((u8 *)ipv6h) +
						 sizeof(struct ipv6hdr));
			break;
		case IPPROTO_UDP:
			udph = (struct udphdr *)(((u8 *)ipv6h) +
						 sizeof(struct ipv6hdr));
			break;
		case IPPROTO_ICMPV6:
			icmp6h = (struct icmp6hdr *)(((u8 *)ipv6h) +
						     sizeof(struct ipv6hdr));
			break;
		default:
			pr_notice("%s: unknown transport\n", func);
			return;
		}
		break;
	default:
		pr_notice("%s: skb->protocol unknown\n", func);
		return;
	}

	if (tcph) {
		pr_notice("%s: tcp src:%d(0x%04x) dst:%d(0x%04x)\n",
			func,
			ntohs(tcph->source),
			ntohs(tcph->source),
			ntohs(tcph->dest),
			ntohs(tcph->dest));
	}

	if (udph) {
		pr_notice("%s: udp src:%d(0x%04x) dst:%d(0x%04x)\n",
			func,
			ntohs(udph->source),
			ntohs(udph->source),
			ntohs(udph->dest),
			ntohs(udph->dest));
	}

	if (icmph) {
		pr_notice("%s: icmp type:%d code:%d id:%d(0x%04x)\n",
			func,
			icmph->type,
			icmph->code,
			ntohs(icmph->un.echo.id),
			ntohs(icmph->un.echo.id));
	}

	if (icmp6h) {
		pr_notice("%s: icmpv6 type:%d code:%d id:%d(0x%04x)\n",
			func,
			icmp6h->icmp6_type,
			icmp6h->icmp6_code,
			ntohs(icmp6h->icmp6_dataun.u_echo.identifier),
			ntohs(icmp6h->icmp6_dataun.u_echo.identifier));
	}
}

#endif /* _MAP_H_ */
