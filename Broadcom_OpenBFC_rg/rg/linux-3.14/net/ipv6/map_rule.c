/* ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <m-asama@ginzado.co.jp> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return Masakazu Asama
 * ----------------------------------------------------------------------------
 */
/*	MAP Mapping Rule function
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

static struct kmem_cache *mrtn_kmem __read_mostly;
static struct kmem_cache *mr_kmem __read_mostly;

static int
mrtree_node_init(struct mrtree_node *node, struct map_rule *mr)
{
	if (!node || !mr)
		return -1;

	node->mr = mr;
	node->children[0] = NULL;
	node->children[1] = NULL;
	node->parent = NULL;

	return 0;
}

static int
mrtree_node_init_ipv6addr(struct mrtree_node *node, struct map_rule *mr)
{
	if (!node || !mr)
		return -1;

	mrtree_node_init(node, mr);
	mr->mrtn_ipv6addr = node;
	node->val[0] = ntohl(mr->p.ipv6_prefix.s6_addr32[0]);
	node->val[1] = ntohl(mr->p.ipv6_prefix.s6_addr32[1]);
	node->val[2] = ntohl(mr->p.ipv6_prefix.s6_addr32[2]);
	node->val[3] = ntohl(mr->p.ipv6_prefix.s6_addr32[3]);
	node->len = mr->p.ipv6_prefix_length;

	return 0;
}

static int
mrtree_node_init_ipv4addrport(struct mrtree_node *node, struct map_rule *mr)
{
	if (!node || !mr)
		return -1;

	mrtree_node_init(node, mr);
	mr->mrtn_ipv4addrport = node;
	node->val[0] = ntohl(mr->p.ipv4_prefix);
	node->len = mr->p.ipv4_prefix_length;
	if (mr->p.ipv4_prefix_length == 32) {
		node->val[1] = mr->p.psid_prefix <<
			       (32 - mr->p.psid_prefix_length);
		node->len += mr->p.psid_prefix_length;
	} else {
		node->val[1] = 0;
	}
	node->val[2] = node->val[3] = 0;

	return 0;
}

static int
mrtree_node_1st_is_contained_in_2nd(struct mrtree_node *node1,
				    struct mrtree_node *node2)
{
	int i, pbw, pbi;
	__u32 mask;

	if (!node1 || !node2) {
		pr_notice("mrtree_node_1st_is_contained_in_2nd: !node1 || !node2\n");
		return 0;
	}

	if (node2->len < 0 || node2->len > 128) {
		pr_notice("mrtree_node_1st_is_contained_in_2nd: node2->len < 0 || node2->len > 128\n");
		return 0;
	}

	if (node1->len < node2->len) {
		pr_notice("mrtree_node_1st_is_contained_in_2nd: node1->len < node2->len\n");
		return 0;
	}

	pbw = node2->len >> 5;
	pbi = node2->len & 0x1f;
	for (i = 0; i < pbw; i++)
		if (node1->val[i] != node2->val[i])
			return 0;
	if (node2->len == 128)
		return 1;
	if (pbi > 0) {
		mask = 0xffffffff << (32 - pbi);
		if ((node1->val[pbw] & mask) != node2->val[pbw])
			return 0;
	}

	return 1;
}

static int
mrtree_node_1st_is_equal_to_2nd(struct mrtree_node *node1,
				struct mrtree_node *node2)
{
	int i;

	if (!node1 || !node2) {
		pr_notice("mrtree_node_1st_is_equal_to_2nd: !node1 || !node2\n");
		return 0;
	}

	for (i = 0; i < 4; ++i)
		if (node1->val[i] != node2->val[i])
			return 0;
	if (node1->len != node2->len)
		return 0;

	return 1;
}

static int
mrtree_node_next_index_of_1st_for_2nd(struct mrtree_node *node1,
				      struct mrtree_node *node2)
{
	int pbw, pbi;
	__u32 mask;

	if (!node1 || !node2) {
		pr_notice("mrtree_node_next_index_of_1st_for_2nd: node1 or node2 is null.\n");
		return -1;
	}

	if (node1->len < 0 || node1->len >= 128) {
		pr_notice("mrtree_node_next_index_of_1st_for_2nd: node1->len >= 128.\n");
		return -1;
	}
	if (node2->len <= node1->len)
		return -1;
	pbw = node1->len >> 5;
	pbi = node1->len & 0x1f;
	mask = 0x1 << (31 - pbi);
	if (node2->val[pbw] & mask)
		return 1;
	else
		return 0;
}

static struct mrtree_node *
mrtree_node_next_of_1st_for_2nd(struct mrtree_node *node1,
				struct mrtree_node *node2)
{
	int index;

	if (!node1 || !node2) {
		pr_notice("mrtree_node_next_of_1st_for_2nd: !node1 || !node2\n");
		return NULL;
	}

	index = mrtree_node_next_index_of_1st_for_2nd(node1, node2);
	if (index < 0)
		return NULL;
	return node1->children[index];
}

static int
mrtree_node_same_bits_length(struct mrtree_node *node1,
			     struct mrtree_node *node2)
{
	int i, pbw, pbi;
	__u32 mask;

	if (!node1 || !node2) {
		pr_notice("mrtree_node_same_bits_length: !node1 || !node2\n");
		return -1;
	}

	for (i = 1; i < 128; i++) {
		pbw = i >> 5;
		pbi = i & 0x1f;
		mask = 0x0;
		if (pbi)
			mask = 0xffffffff << (32 - pbi);
		if (pbw && !pbi) {
			if (node1->val[pbw - 1] != node2->val[pbw - 1])
				return i - 1;
		} else {
			if ((node1->val[pbw] & mask) !=
			    (node2->val[pbw] & mask))
				return i - 1;
		}
	}
	return 128;
}

static int
mrtree_node_same_bits(struct mrtree_node *node1, struct mrtree_node *node2,
		      struct mrtree_node *node)
{
	int length, i, pbw, pbi;
	__u32 mask;

	if (!node1 || !node2 || !node) {
		pr_notice("mrtree_node_same_bits: !node1 || !node2 || !node\n");
		return -1;
	}

	length = mrtree_node_same_bits_length(node1, node2);
	if (length < 0 || length > 128) {
		pr_notice("mrtree_node_same_bits: length < 0 || length > 128\n");
		return -1;
	}
	pbw = length >> 5;
	pbi = length & 0x1f;
	for (i = 0; i < pbw; i++)
		node->val[i] = node1->val[i];
	if (pbi) {
		mask = 0xffffffff << (32 - pbi);
		node->val[pbw] = node1->val[pbw] & mask;
	}
	node->len = length;
	return 0;
}

static int
mrtree_node_add(struct mrtree_node *node, struct mrtree_node **root)
{
	struct mrtree_node *cur, *tmp, *child;
	int index, index2;

	if (!node || !root) {
		pr_notice("mrtree_node_add: !node || !root\n");
		return -1;
	}

	/* *root == NULL */
	if (!*root) {
		*root = node;
		return 0;
	}

	cur = *root;
	for (;;) {
		tmp = NULL;
		if (mrtree_node_1st_is_contained_in_2nd(node, cur)) {
			tmp = mrtree_node_next_of_1st_for_2nd(cur, node);
		} else {
			if (cur->parent)
				cur = cur->parent;
		}
		if (!tmp)
			break;
		else
			cur = tmp;
	}

	/* cur == *root */
	if (cur == *root && !mrtree_node_1st_is_contained_in_2nd(node, cur)) {
		tmp = kmem_cache_alloc(mrtn_kmem, GFP_KERNEL);
		if (!tmp) {
			pr_notice("mrtree_node_add: alloc mrtree_node failed.\n");
			return -1;
		}
		memset(tmp, 0, sizeof(*tmp));
		mrtree_node_same_bits(*root, node, tmp);
		index = mrtree_node_next_index_of_1st_for_2nd(tmp, node);
		if (index < 0) {
			pr_notice("mrtree_node_add: index = %d\n", index);
			return -1;
		}
		if (index) {
			tmp->children[0] = *root;
			tmp->children[1] = node;
		} else {
			tmp->children[0] = node;
			tmp->children[1] = *root;
		}
		node->parent = tmp;
		(*root)->parent = tmp;
		*root = tmp;
		return 0;
	}

	/* cur == node */
	if (mrtree_node_1st_is_equal_to_2nd(node, cur)) {
		if (cur->mr) {
			pr_notice("mrtree_node_add: mrtree_node_1st_is_equal_to_2nd dup?\n");
			return -1;
		}
		cur->mr = node->mr;
		if (cur->mr->mrtn_ipv6addr == node)
			cur->mr->mrtn_ipv6addr = cur;
		if (cur->mr->mrtn_ipv4addrport == node)
			cur->mr->mrtn_ipv4addrport = cur;
		kmem_cache_free(mrtn_kmem, node);
		return 0;
	}

	index = mrtree_node_next_index_of_1st_for_2nd(cur, node);
	if (index < 0) {
		pr_notice("mrtree_node_add: index = %d\n", index);
		return -1;
	}

	if (!cur->children[index]) {
		/* child == NULL */
		cur->children[index] = node;
		node->parent = cur;
	} else {
		/* child != NULL */
		child = cur->children[index];
		tmp = kmem_cache_alloc(mrtn_kmem, GFP_KERNEL);
		if (!tmp) {
			pr_notice("mrtree_node_add: alloc mrtree_node failed.\n");
			return -1;
		}
		memset(tmp, 0, sizeof(*tmp));
		mrtree_node_same_bits(child, node, tmp);
		if (tmp->len >= node->len) {
			index2 = mrtree_node_next_index_of_1st_for_2nd(node,
								       child);
			if (index2 < 0) {
				pr_notice("mrtree_node_add: index2 = %d\n",
					  index2);
				return -1;
			}
			if (node->children[index2]) {
				pr_notice("mrtree_node_add: node->children[index2]\n");
				return -1;
			}
			node->children[index2] = child;
			child->parent = node;
			cur->children[index] = node;
			node->parent = cur;
			kmem_cache_free(mrtn_kmem, tmp);
			return 0;
		}
		if (tmp->len >= child->len)
			pr_notice("*** tmp->len >= child->len\n");
		index2 = mrtree_node_next_index_of_1st_for_2nd(tmp, node);
		if (index2 < 0) {
			pr_notice("mrtree_node_add: index2 = %d\n", index2);
			pr_notice("*   cur = %08x:%08x:%08x:%08x:%03d\n",
				cur->val[0], cur->val[1], cur->val[2],
				cur->val[3], cur->len);
			pr_notice("*   tmp = %08x:%08x:%08x:%08x:%03d\n",
				tmp->val[0], tmp->val[1], tmp->val[2],
				tmp->val[3], tmp->len);
			pr_notice("*  node = %08x:%08x:%08x:%08x:%03d\n",
				node->val[0], node->val[1], node->val[2],
				node->val[3], node->len);
			pr_notice("* child = %08x:%08x:%08x:%08x:%03d\n",
				child->val[0], child->val[1], child->val[2],
				child->val[3], child->len);
			if (!(cur->len < node->len && node->len < child->len))
				pr_notice("*** !(cur->len < node->len && node->len < child->len)\n");
			return -1;
		}
		if (index2) {
			tmp->children[0] = child;
			tmp->children[1] = node;
		} else {
			tmp->children[0] = node;
			tmp->children[1] = child;
		}
		node->parent = tmp;
		child->parent = tmp;
		cur->children[index] = tmp;
		tmp->parent = cur;
	}

	return 0;
}

static int
mrtree_node_delete(struct mrtree_node *node, struct mrtree_node **root)
{
	struct mrtree_node *parent;

	if (node->mr && node->mr->mrtn_ipv6addr == node)
		node->mr->mrtn_ipv6addr = NULL;
	if (node->mr && node->mr->mrtn_ipv4addrport == node)
		node->mr->mrtn_ipv4addrport = NULL;
	node->mr = NULL;

	if (node->children[0] && node->children[1]) {
#ifdef MAP_DEBUG
		pr_notice("mrtree_node_delete: node->children[0] && node->children[1]\n");
#endif
		return 0;
	}

	if (!node->children[0] && !node->children[1]) {
#ifdef MAP_DEBUG
		pr_notice("mrtree_node_delete: !node->children[0] && !node->children[1]\n");
#endif
		if (!node->parent)
			*root = NULL;
		if (node->parent && node->parent->children[0] == node)
			node->parent->children[0] = NULL;
		if (node->parent && node->parent->children[1] == node)
			node->parent->children[1] = NULL;
	}

	if (node->children[0]) {
#ifdef MAP_DEBUG
		pr_notice("mrtree_node_delete: node->children[0]\n");
#endif
		if (node->parent && node->parent->children[0] == node) {
#ifdef MAP_DEBUG
			pr_notice("mrtree_node_delete: node->parent && node->parent->children[0] == node\n");
#endif
			node->parent->children[0] = node->children[0];
			node->children[0]->parent = node->parent;
		}
		if (node->parent && node->parent->children[1] == node) {
#ifdef MAP_DEBUG
			pr_notice("mrtree_node_delete: node->parent && node->parent->children[1] == node\n");
#endif
			node->parent->children[1] = node->children[0];
			node->children[0]->parent = node->parent;
		}
		if (!node->parent) {
#ifdef MAP_DEBUG
			pr_notice("mrtree_node_delete: !node->parent\n");
#endif
			*root = node->children[0];
		}
	}

	if (node->children[1]) {
#ifdef MAP_DEBUG
		pr_notice("mrtree_node_delete: node->children[1]\n");
#endif
		if (node->parent && node->parent->children[0] == node) {
#ifdef MAP_DEBUG
			pr_notice("mrtree_node_delete: node->parent && node->parent->children[0] == node\n");
#endif
			node->parent->children[0] = node->children[1];
			node->children[1]->parent = node->parent;
		}
		if (node->parent && node->parent->children[1] == node) {
#ifdef MAP_DEBUG
			pr_notice("mrtree_node_delete: node->parent && node->parent->children[1] == node\n");
#endif
			node->parent->children[1] = node->children[1];
			node->children[1]->parent = node->parent;
		}
		if (!node->parent) {
#ifdef MAP_DEBUG
			pr_notice("mrtree_node_delete: !node->parent\n");
#endif
			*root = node->children[1];
		}
	}

	parent = node->parent;

	kmem_cache_free(mrtn_kmem, node);

	if (parent && !parent->mr)
		mrtree_node_delete(parent, root);

	return 0;
}

struct map_rule *
map_rule_find_by_ipv6addr(struct map *m, struct in6_addr *ipv6addr)
{
	struct map_rule *mr = NULL;
	struct mrtree_node *cur = NULL, *tmp, key;

	key.val[0] = ntohl(ipv6addr->s6_addr32[0]);
	key.val[1] = ntohl(ipv6addr->s6_addr32[1]);
	key.val[2] = ntohl(ipv6addr->s6_addr32[2]);
	key.val[3] = ntohl(ipv6addr->s6_addr32[3]);
	key.len = 128;

	if (!m->mrtn_root_ipv6addr)
		return NULL;

	read_lock(&m->rule_lock);
	/* list_for_each_entry(tmp, &m->rule_list, list) {
	 *	if (ipv6_prefix_equal(&tmp->p.ipv6_prefix, ipv6addr,
	 *	    tmp->p.ipv6_prefix_length)) {
	 *		if (!mr || (tmp->p.ipv6_prefix_length >
	 *		    mr->p.ipv6_prefix_length))
	 *			mr = tmp;
	 *	}
	 * }
	 */
	tmp = m->mrtn_root_ipv6addr;
	for (;;) {
		if (tmp && mrtree_node_1st_is_contained_in_2nd(&key, tmp)) {
			if (tmp->mr && (!cur || tmp->len > cur->len))
				cur = tmp;
			tmp = mrtree_node_next_of_1st_for_2nd(tmp, &key);
			if (!tmp)
				break;
		} else
			break;
	}
	if (cur)
		mr = cur->mr;
	read_unlock(&m->rule_lock);

	return mr;
}

struct map_rule *
map_rule_find_by_ipv4addrport(struct map *m, __be32 *ipv4addr, __be16 *port,
	int fro)
{
	struct map_rule *mr = NULL;
	struct mrtree_node *cur = NULL, *tmp, *tmp2, key;
	/* __u32 amask; */
	/* __u16 pmask; */
	/* int psidrp; */
	int i;

	if (!m->mrtn_root_ipv4addrport)
		return NULL;

	key.val[0] = ntohl(*ipv4addr);
	key.len = 48;

	read_lock(&m->rule_lock);
	/* list_for_each_entry(tmp, &m->rule_list, list) {
	 *	if (fro && tmp->p.forwarding_rule != MAP_FORWARDING_RULE_T)
	 *		continue;
	 *	amask = 0xffffffff << (32 - tmp->p.ipv4_prefix_length);
	 *	if ((ntohl(tmp->p.ipv4_prefix) & amask) !=
	 *	    (ntohl(*ipv4addr) & amask))
	 *		continue;
	 *	if (tmp->p.ipv4_prefix_length == 32 &&
	 *	    tmp->p.psid_prefix_length > 0) {
	 *		pmask = 0xffff;
	 *		psidrp = 16 - tmp->p.psid_offset -
	 *			 tmp->p.psid_prefix_length;
	 *		if (tmp->p.psid_prefix_length < 16)
	 *			pmask = ((1 << tmp->p.psid_prefix_length) - 1)
	 *				<< psidrp;
	 *		if ((ntohs(*port) & pmask) ==
	 *		    (tmp->p.psid_prefix << psidrp))
	 *			if (!mr ||
	 *			    (tmp->p.psid_prefix_length >
	 *			     mr->p.psid_prefix_length))
	 *				mr = tmp;
	 *	} else {
	 *		if (!mr || (tmp->p.ipv4_prefix_length >
	 *			    mr->p.ipv4_prefix_length))
	 *			mr = tmp;
	 *	}
	 * }
	 */
	tmp = m->mrtn_root_ipv4addrport;
	for (;;) {
		if (tmp && mrtree_node_1st_is_contained_in_2nd(&key, tmp)) {
			if (tmp->mr && (!cur || tmp->len > cur->len))
				if (!fro || tmp->mr->p.forwarding_rule ==
					    MAP_FORWARDING_RULE_T)
					cur = tmp;
			tmp = mrtree_node_next_of_1st_for_2nd(tmp, &key);
			if (!tmp)
				break;
		} else
			break;
		if (tmp->len > 32) {
			if (tmp->parent)
				tmp = tmp->parent;
			break;
		}
	}
	for (i = 0; i < 17; ++i) {
		if (m->psid_offset_nums[i] == 0)
			continue;
		key.val[1] = ((__u32)ntohs(*port)) << (16 + i);
		tmp2 = tmp;
		for (;;) {
			if (tmp2 &&
			    mrtree_node_1st_is_contained_in_2nd(&key, tmp2)) {
				if (tmp2->mr && (!cur || tmp2->len > cur->len))
					if (!fro || tmp2->mr->p.forwarding_rule
						    == MAP_FORWARDING_RULE_T)
						cur = tmp2;
				tmp2 = mrtree_node_next_of_1st_for_2nd(tmp2,
								       &key);
				if (!tmp2)
					break;
			} else
				break;
		}
	}
	if (cur)
		mr = cur->mr;
	read_unlock(&m->rule_lock);

	return mr;
}

void
mrtree_node_print(struct mrtree_node *node, int indent)
{
	int i;
	char head[24], foot[8];

	if (!node)
		return;

	/* for(i = 0; i < indent; ++i) pr_notice(" ");
	 * pr_notice("X");
	 * for(i = 0; i < (20-indent); ++i) pr_notice(" ");
	 */

	for (i = 0; i < 24; ++i)
		head[i] = ' ';
	i = indent;
	if (i > 22)
		head[22] = '-';
	else
		head[i] = '*';
	head[23] = '\0';

	for (i = 0; i < 8; ++i)
		foot[i] = ' ';
	if (node->parent) {
		if (node->parent->len >= node->len)
			foot[0] = '1';
		if (node->parent->children[0] != node
		    && node->parent->children[1] != node)
			foot[1] = '2';
	}
	if (node->children[0]) {
		if (node->children[0]->len <= node->len)
			foot[2] = '3';
		if (node->children[0]->parent != node)
			foot[3] = '4';
	}
	if (node->children[1]) {
		if (node->children[1]->len <= node->len)
			foot[4] = '5';
		if (node->children[1]->parent != node)
			foot[5] = '6';
	}
	foot[7] = '\0';

	pr_notice("%s 0x%08x 0x%08x 0x%08x %08x:%08x:%08x:%08x:%03d 0x%08x 0x%08x %s\n",
		head,
		(__u32)node,
		(__u32)node->children[0],
		(__u32)node->children[1],
		node->val[0],
		node->val[1],
		node->val[2],
		node->val[3],
		node->len,
		(__u32)node->mr,
		(__u32)node->parent,
		foot
	);

	mrtree_node_print(node->children[0], indent + 1);
	mrtree_node_print(node->children[1], indent + 1);
}

void
mrtree_node_dump(struct mrtree_node *root)
{
	pr_notice("                        NODE       CHILD[0]   CHILD[1]   VAL[0]   VAL[1]   VAL[2]   VAL[3]   LEN MR	 PARENT\n");
	if (root)
		mrtree_node_print(root, 0);
}

static struct map_rule *
map_rule_find(struct map *m, struct map_rule_parm *mrp)
{
	struct map_rule *mr = NULL, *tmp;

	read_lock(&m->rule_lock);
	list_for_each_entry(tmp, &m->rule_list, list) {
		if (!strncmp((void *)&tmp->p.ipv6_prefix,
		    (void *)&mrp->ipv6_prefix, sizeof(struct in6_addr)) &&
		    tmp->p.ipv4_prefix == mrp->ipv4_prefix &&
		    tmp->p.psid_prefix == mrp->psid_prefix &&
		    tmp->p.ipv6_prefix_length == mrp->ipv6_prefix_length &&
		    tmp->p.ipv4_prefix_length == mrp->ipv4_prefix_length &&
		    tmp->p.psid_prefix_length == mrp->psid_prefix_length) {
			mr = tmp;
			break;
		}
	}
	read_unlock(&m->rule_lock);

	return mr;
}

static struct map_rule *
map_rule_find_loose(struct map *m, struct map_rule_parm *mrp)
{
	struct map_rule *mr = NULL, *tmp;

	read_lock(&m->rule_lock);
	list_for_each_entry(tmp, &m->rule_list, list) {
		if ((!strncmp((void *)&tmp->p.ipv6_prefix,
		     (void *)&mrp->ipv6_prefix, sizeof(struct in6_addr)) &&
		     tmp->p.ipv6_prefix_length == mrp->ipv6_prefix_length) ||
		    (tmp->p.ipv4_prefix == mrp->ipv4_prefix &&
		     tmp->p.ipv4_prefix_length == mrp->ipv4_prefix_length &&
		     tmp->p.psid_prefix == mrp->psid_prefix &&
		     tmp->p.psid_prefix_length == mrp->psid_prefix_length)) {
			mr = tmp;
			break;
		}
	}
	read_unlock(&m->rule_lock);

	return mr;
}

int
map_rule_free(struct map *m, struct map_rule *mr)
{
	/* XXX: */
	mrtree_node_delete(mr->mrtn_ipv4addrport, &m->mrtn_root_ipv4addrport);
	mrtree_node_delete(mr->mrtn_ipv6addr, &m->mrtn_root_ipv6addr);
	list_del(&mr->list);
	kmem_cache_free(mr_kmem, mr);
	return 0;
}

int
map_rule_add(struct map *m, struct map_rule_parm *mrp)
{
	struct map_rule *mr;
	struct mrtree_node *mrtn_ipv6addr, *mrtn_ipv4addrport;

	mr = map_rule_find_loose(m, mrp);
	if (mr)
		return -1;

	if (mrp->forwarding_mode != MAP_FORWARDING_MODE_T &&
	    mrp->forwarding_mode != MAP_FORWARDING_MODE_E)
		return -1;

	if (mrp->forwarding_rule != MAP_FORWARDING_RULE_T &&
	    mrp->forwarding_rule != MAP_FORWARDING_RULE_F)
		return -1;

	mr = kmem_cache_alloc(mr_kmem, GFP_KERNEL);
	if (!mr)
		goto mr_err;
	mr->p = *mrp;

	mrtn_ipv6addr = kmem_cache_alloc(mrtn_kmem, GFP_KERNEL);
	if (!mrtn_ipv6addr)
		goto mrtn_ipv6addr_err;
	memset(mrtn_ipv6addr, 0, sizeof(*mrtn_ipv6addr));

	mrtn_ipv4addrport = kmem_cache_alloc(mrtn_kmem, GFP_KERNEL);
	if (!mrtn_ipv4addrport)
		goto mrtn_ipv4addrport_err;
	memset(mrtn_ipv4addrport, 0, sizeof(*mrtn_ipv4addrport));

	mrtree_node_init_ipv6addr(mrtn_ipv6addr, mr);
	mrtree_node_init_ipv4addrport(mrtn_ipv4addrport, mr);

	write_lock_bh(&m->psid_offset_nums_lock);
	m->psid_offset_nums[mr->p.psid_offset]++;
	write_unlock_bh(&m->psid_offset_nums_lock);

	write_lock_bh(&m->rule_lock);
	list_add_tail(&mr->list, &m->rule_list);
	mrtree_node_add(mrtn_ipv6addr, &m->mrtn_root_ipv6addr);
	mrtree_node_add(mrtn_ipv4addrport, &m->mrtn_root_ipv4addrport);
	m->p.rule_num += 1;
	write_unlock_bh(&m->rule_lock);

	return 0;

mrtn_ipv4addrport_err:
	kmem_cache_free(mrtn_kmem, mrtn_ipv6addr);
mrtn_ipv6addr_err:
	kmem_cache_free(mr_kmem, mr);
mr_err:
	pr_notice("map_rule_add: alloc failed.\n");
	return -1;
}

int
map_rule_change(struct map *m, struct map_rule_parm *mrp)
{
	struct map_rule *mr = map_rule_find(m, mrp);

	if (!mr)
		return -1;

	mr->p.ea_length = mrp->ea_length;
	mr->p.psid_offset = mrp->psid_offset;
	mr->p.forwarding_mode = mrp->forwarding_mode;
	mr->p.forwarding_rule = mrp->forwarding_rule;

	return 0;
}

int
map_rule_delete(struct map *m, struct map_rule_parm *mrp)
{
	struct map_rule *mr = map_rule_find(m, mrp);

	if (!mr)
		return -1;

	write_lock_bh(&m->psid_offset_nums_lock);
	m->psid_offset_nums[mr->p.psid_offset]--;
	write_unlock_bh(&m->psid_offset_nums_lock);

	write_lock_bh(&m->rule_lock);
	if (m->bmr == mr)
		m->bmr = NULL;
	map_rule_free(m, mr);
	m->p.rule_num -= 1;
	write_unlock_bh(&m->rule_lock);

	return 0;
}

int
map_rule_init(void)
{
	mrtn_kmem = kmem_cache_create("mrtree_node", sizeof(struct mrtree_node),
				      0, SLAB_HWCACHE_ALIGN, NULL);
	if (!mrtn_kmem)
		return -1;
	mr_kmem = kmem_cache_create("map_rule", sizeof(struct map_rule), 0,
				    SLAB_HWCACHE_ALIGN, NULL);
	if (!mr_kmem)
		return -1;
	return 0;
}

void
map_rule_exit(void)
{
	kmem_cache_destroy(mr_kmem);
	kmem_cache_destroy(mrtn_kmem);
}
