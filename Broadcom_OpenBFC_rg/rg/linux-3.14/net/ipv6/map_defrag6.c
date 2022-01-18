/* ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <m-asama@ginzado.co.jp> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return Masakazu Asama
 * ----------------------------------------------------------------------------
 */
/*	MAP IPv6 packet defragment(reassemble) function
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

static struct kmem_cache *dn_kmem __read_mostly;
static int dn_kmem_alloced;

static inline u32
map_defrag6_dn_hash(struct in6_addr *saddr, struct in6_addr *daddr, __be32 id)
{
	/* XXX: atode yoku kanngaeru */
	u32 h = id
	      + saddr->s6_addr32[0]
	      + saddr->s6_addr32[1]
	      + saddr->s6_addr32[2]
	      + saddr->s6_addr32[3]
	      + daddr->s6_addr32[0]
	      + daddr->s6_addr32[1]
	      + daddr->s6_addr32[2]
	      + daddr->s6_addr32[3];
	h ^= (h >> 20);
	h ^= (h >> 10);
	h ^= (h >> 5);
	h &= (MAP_DEFRAG6_HASH_SIZE - 1);
	/* pr_notice("map_defrag6_dn_hash: %08x\n", h); */
	return h;
}

static int
map_defrag6_collect(struct hlist_head *dnlist, struct in6_addr *saddr,
		    struct in6_addr *daddr, __be32 id, struct map *m)
{
	struct map_defrag6_node *dn, *dntmp, *dntmp2;
	u32 h = map_defrag6_dn_hash(saddr, daddr, id);

	hlist_for_each_entry(dn, &m->defrag6_hash[h], dn_hash) {
		if (ipv6_addr_equal(saddr, dn->saddr) &&
		    ipv6_addr_equal(saddr, dn->saddr) &&
		    id == dn->id) {

#ifdef MAP_DEBUG
			pr_notice("map_defrag6_collect: match: %08x%08x%08x%08x %08x%08x%08x%08x %08x %d:%d %d\n",
				ntohl(dn->saddr->s6_addr32[0]),
				ntohl(dn->saddr->s6_addr32[1]),
				ntohl(dn->saddr->s6_addr32[2]),
				ntohl(dn->saddr->s6_addr32[3]),
				ntohl(dn->daddr->s6_addr32[0]),
				ntohl(dn->daddr->s6_addr32[1]),
				ntohl(dn->daddr->s6_addr32[2]),
				ntohl(dn->daddr->s6_addr32[3]),
				ntohl(dn->id),
				(ntohs(dn->frag_off) & 0xfff8),
				ntohs(dn->payload_len),
				(ntohs(dn->frag_off) & 0x7));
#endif
			dntmp = NULL;
			dntmp2 = NULL;
			hlist_for_each_entry(dntmp, dnlist,
					dn_pending) {
				dntmp2 = dntmp;
				if ((ntohs(dn->frag_off) & 0xfff8) <
				    (ntohs(dntmp->frag_off) & 0xfff8)) {
					break;
				}
			}
			dntmp = dntmp2;
			if (dntmp) {
#ifdef MAP_DEBUG
				pr_notice("map_defrag6_collect: dntmp: %08x%08x%08x%08x %08x%08x%08x%08x %08x %d:%d %d\n",
					ntohl(dntmp->saddr->s6_addr32[0]),
					ntohl(dntmp->saddr->s6_addr32[1]),
					ntohl(dntmp->saddr->s6_addr32[2]),
					ntohl(dntmp->saddr->s6_addr32[3]),
					ntohl(dntmp->daddr->s6_addr32[0]),
					ntohl(dntmp->daddr->s6_addr32[1]),
					ntohl(dntmp->daddr->s6_addr32[2]),
					ntohl(dntmp->daddr->s6_addr32[3]),
					ntohl(dntmp->id),
					(ntohs(dntmp->frag_off) & 0xfff8),
					ntohs(dntmp->payload_len),
					(ntohs(dntmp->frag_off) & 0x7));
#endif
				if ((ntohs(dn->frag_off) & 0xfff8) <
				    (ntohs(dntmp->frag_off) & 0xfff8))
					hlist_add_before(&dn->dn_pending,
						&dntmp->dn_pending);
				else
					hlist_add_after(&dntmp->dn_pending,
						&dn->dn_pending);
			} else
				hlist_add_head(&dn->dn_pending, dnlist);
		}
	}

#ifdef MAP_DEBUG
	{
		int i = 1;
		hlist_for_each_entry(dntmp, dnlist, dn_pending) {
			pr_notice("map_defrag6_collect: %2d : %08x%08x%08x%08x %08x%08x%08x%08x %08x %d:%d %d\n",
				i,
				ntohl(dntmp->saddr->s6_addr32[0]),
				ntohl(dntmp->saddr->s6_addr32[1]),
				ntohl(dntmp->saddr->s6_addr32[2]),
				ntohl(dntmp->saddr->s6_addr32[3]),
				ntohl(dntmp->daddr->s6_addr32[0]),
				ntohl(dntmp->daddr->s6_addr32[1]),
				ntohl(dntmp->daddr->s6_addr32[2]),
				ntohl(dntmp->daddr->s6_addr32[3]),
				ntohl(dntmp->id),
				(ntohs(dntmp->frag_off) & 0xfff8),
				ntohs(dntmp->payload_len),
				(ntohs(dntmp->frag_off) & 0x7));
			++i;
		}
	}
#endif

	return 0;
}

static inline int
map_defrag6_dn_expired(struct map_defrag6_node *dn)
{
	return time_is_before_jiffies(dn->received + MAP_DEFRAG6_EXPIRES);
}

static inline void
map_defrag6_dn_destroy(struct map_defrag6_node *dn)
{
	hlist_del(&dn->dn_hash);
	list_del(&dn->dn_list);
	kfree_skb(dn->skb);
	kmem_cache_free(dn_kmem, dn);
	--dn_kmem_alloced;
}

static inline void
map_defrag6_dn_gc(struct map *m)
{
	struct map_defrag6_node *dn, *dn_node;
	list_for_each_entry_safe(dn, dn_node, &m->defrag6_list, dn_list) {
		if (!map_defrag6_dn_expired(dn))
			break;
		map_defrag6_dn_destroy(dn);
	}
	m->defrag6_last_gc = jiffies;
}

static struct map_defrag6_node *
map_defrag6_dn_create(struct sk_buff *skb, struct map *m)
{
	struct map_defrag6_node *dn;
	struct ipv6hdr *ipv6h;
	struct frag_hdr *fragh;

	ipv6h = ipv6_hdr(skb);
	fragh = (struct frag_hdr *)(ipv6h + 1);

	dn = kmem_cache_alloc(dn_kmem, GFP_KERNEL);
	if (!dn)
		return NULL;

	map_defrag6_dn_gc(m);
	++dn_kmem_alloced;
	dn->skb = skb;
	dn->saddr = &ipv6h->saddr;
	dn->daddr = &ipv6h->daddr;
	dn->id = fragh->identification;
	dn->payload_len = ipv6h->payload_len;
	dn->frag_off = fragh->frag_off;
	dn->h = map_defrag6_dn_hash(dn->saddr, dn->daddr, dn->id);
	dn->received = jiffies;
	hlist_add_head(&dn->dn_hash, &m->defrag6_hash[dn->h]);
	list_add_tail(&dn->dn_list, &m->defrag6_list);
	INIT_HLIST_NODE(&dn->dn_pending);

	return dn;
}

static int
map_defrag6_complete(struct hlist_head *dnlist)
{
	struct map_defrag6_node *dn, *dntmp = NULL;
	int frag_off, total_len = 0;
	hlist_for_each_entry(dn, dnlist, dn_pending) {
		dntmp = dn;
		frag_off = ntohs(dn->frag_off) & 0xfff8;
		if (frag_off != total_len)
			return 0;
		total_len += ntohs(dn->payload_len) - sizeof(struct frag_hdr);
	}
	dn = dntmp;

	if (dn->frag_off & htons(IP6_MF))
		return 0;

	return total_len;
}

static struct sk_buff *
map_defrag6_rebuild_skb(struct hlist_head *dnlist, int total_len)
{
	struct map_defrag6_node *dn;
	struct hlist_node *n;
	struct sk_buff *nskb = NULL;
	struct ipv6hdr *ipv6h = NULL;
	struct frag_hdr *fragh = NULL;
	void *ptr = NULL;
	int len;
	int offset;

	nskb = alloc_skb(total_len + sizeof(struct ipv6hdr) + LL_MAX_HEADER,
		GFP_ATOMIC);
	if (!nskb) {
		pr_notice("map_defrag6_rebuild_skb: alloc_skb failed.\n");
		goto err;
	}
	skb_reserve(nskb, LL_MAX_HEADER);
	skb_put(nskb, total_len + sizeof(struct ipv6hdr));

	dn = hlist_entry(dnlist->first, struct map_defrag6_node, dn_pending);
	nskb->dev = dn->skb->dev;
	ipv6h = ipv6_hdr(dn->skb);
	fragh = (struct frag_hdr *)(ipv6h + 1);
	ptr = ipv6h;
	skb_copy_to_linear_data(nskb, ptr, sizeof(struct ipv6hdr));
	nskb->protocol = htons(ETH_P_IPV6);
	skb_reset_network_header(nskb);
	ipv6h = ipv6_hdr(nskb);
	ipv6h->nexthdr = fragh->nexthdr;
	ipv6h->payload_len = htons(total_len);
	hlist_for_each_entry_safe(dn, n, dnlist, dn_pending) {
		ptr = ipv6_hdr(dn->skb);
		ptr += sizeof(struct ipv6hdr) + sizeof(struct frag_hdr);
		len = ntohs(dn->payload_len) - sizeof(struct frag_hdr);
		offset = (ntohs(dn->frag_off) & 0xfff8) +
			sizeof(struct ipv6hdr);
		skb_copy_to_linear_data_offset(nskb, offset, ptr, len);
	}

err:
	hlist_for_each_entry_safe(dn, n, dnlist, dn_pending) {
		hlist_del_init(&dn->dn_pending);
		map_defrag6_dn_destroy(dn);
	}

	return nskb;
}

struct sk_buff *
map_defrag6(struct sk_buff *skb, struct map *m)
{
	struct ipv6hdr *ipv6h;
	struct frag_hdr *fragh;
	struct map_defrag6_node *dn;
	struct hlist_node *node, *n;
	struct hlist_head dnlist;
	int total_len;

#ifdef MAP_DEBUG
	pr_notice("map_defrag6:\n");
#endif

	ipv6h = ipv6_hdr(skb);
	fragh = (struct frag_hdr *)(ipv6h + 1);

	if (ipv6h->nexthdr != IPPROTO_FRAGMENT) {
#ifdef MAP_DEBUG
		pr_notice("map_defrag6: ipv6h->nexthdr != IPPROTO_FRAGMENT\n");
#endif
		return skb;
	}

	write_lock_bh(&m->defrag6_lock);

	dn = map_defrag6_dn_create(skb, m);
	if (!dn) {
		pr_notice("map_defrag6: map_defrag6_dn_create failed.\n");
		write_unlock_bh(&m->defrag6_lock);
		goto err_kfree_skb;
	}

	INIT_HLIST_HEAD(&dnlist);
	map_defrag6_collect(&dnlist, &ipv6h->saddr, &ipv6h->daddr,
		fragh->identification, m);

	total_len = map_defrag6_complete(&dnlist);
	if (total_len > 0) {
		skb = map_defrag6_rebuild_skb(&dnlist, total_len);
	} else {
		hlist_for_each_entry_safe(dn, n, &dnlist, dn_pending) {
			hlist_del_init(&dn->dn_pending);
		}
		skb = NULL;
	}

	write_unlock_bh(&m->defrag6_lock);

	return skb;

err_kfree_skb:
	kfree_skb(skb);

	return NULL;
}

int
map_defrag6_init(void)
{
	dn_kmem = kmem_cache_create("map_defrag6_node",
		sizeof(struct map_defrag6_node), 0, SLAB_HWCACHE_ALIGN, NULL);
	if (!dn_kmem)
		return -1;

	return 0;
}

void
map_defrag6_exit(void)
{
	kmem_cache_destroy(dn_kmem);
}
