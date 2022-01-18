/* ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <m-asama@ginzado.co.jp> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return Masakazu Asama
 * ----------------------------------------------------------------------------
 */

#ifndef _UAPI_LINUX_IF_MAP_H_
#define _UAPI_LINUX_IF_MAP_H_

#include <linux/types.h>
#include <asm/byteorder.h>

#define SIOCGETMAP		(SIOCDEVPRIVATE + 0)
#define SIOCADDMAP		(SIOCDEVPRIVATE + 1)
#define SIOCDELMAP		(SIOCDEVPRIVATE + 2)
#define SIOCCHGMAP		(SIOCDEVPRIVATE + 3)

#define SIOCGETMAPRULES		(SIOCDEVPRIVATE + 4)
#define SIOCADDMAPRULES		(SIOCDEVPRIVATE + 5)
#define SIOCDELMAPRULES		(SIOCDEVPRIVATE + 6)
#define SIOCCHGMAPRULES		(SIOCDEVPRIVATE + 7)

#define SIOCGETMAPCURRNUM	(SIOCDEVPRIVATE + 8)
#define SIOCGETMAPCURR		(SIOCDEVPRIVATE + 9)
#define SIOCGETMAPNAPTNUM	(SIOCDEVPRIVATE + 10)
#define SIOCGETMAPNAPT		(SIOCDEVPRIVATE + 11)

#define SIOCGETMAPPOOLS		(SIOCDEVPRIVATE + 12)
#define SIOCADDMAPPOOLS		(SIOCDEVPRIVATE + 13)
#define SIOCDELMAPPOOLS		(SIOCDEVPRIVATE + 14)
#define SIOCCHGMAPPOOLS		(SIOCDEVPRIVATE + 15)

#define MAP_ROLE_BR		(1 << 0)
#define MAP_ROLE_CE		(1 << 1)

#define MAP_FORWARDING_MODE_T	(1 << 0)
#define MAP_FORWARDING_MODE_E	(1 << 1)

#define MAP_FORWARDING_RULE_T	(1 << 0)
#define MAP_FORWARDING_RULE_F	(1 << 1)

#define MAP_NAPT_ALWAYS_T	(1 << 0)
#define MAP_NAPT_ALWAYS_F	(1 << 1)

#define MAP_NAPT_FORCE_RECYCLE_T	(1 << 0)
#define MAP_NAPT_FORCE_RECYCLE_F	(1 << 1)

#define MAP_IPV4_FRAG_INNER_T	(1 << 0)
#define MAP_IPV4_FRAG_INNER_F	(1 << 1)

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

struct map_pool_parm {
	__be32			pool_prefix;
	__u8			pool_prefix_length;
};

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

#endif /* _UAPI_LINUX_IF_MAP_H_ */
