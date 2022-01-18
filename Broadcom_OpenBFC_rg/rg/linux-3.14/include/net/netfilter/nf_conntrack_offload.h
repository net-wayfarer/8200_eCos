/*
 * nf_conntrack handling for offloading flows to acceleration engine
 *
 * Copyright (c) 2016 Broadcom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation (or any later at your option).
 *
 * Author: Jayesh Patel <jayeshp@broadcom.com>
 */

#ifndef _NF_CONNTRACK_OFFLOAD_H
#define _NF_CONNTRACK_OFFLOAD_H

#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter/nf_conntrack_tuple_common.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_extend.h>
#include <net/dsfield.h>

struct offload_info {
	struct ethhdr           eh;        /* Ethernet Header     */
	int                     flow_id;   /* Flow ID             */
	int                     flow_type; /* Flow Type           */
	int                     oif;       /* Out Interface Index */
	int                     iif;       /* In Interface Index  */
	int                     vlan_id;   /* VLAN ID */
	u32                     packets;   /* Packet Count        */
	u32                     bytes;     /* Byte Count          */
	unsigned long           tstamp;    /* Timestamp           */
	enum ip_conntrack_info  ctinfo;    /* Last Conntrack Info */
	u8                      create_err;/* Flow Create error   */
	u8                      lag;
	u8                      vlan_untag;
	u8                      dscp_old;
	u8                      dscp_new;
	u8                      force_del;
	struct nf_bridge_info   *nf_bridge;
	void                    *idb;
	void                    *odb;
};

struct nf_conn_offload {
	struct offload_info info[IP_CT_DIR_MAX];
	unsigned int events_retry_timeout;
	atomic_t slavecnt;
	void (*destructor)(struct nf_conn *ct,
			   struct nf_conn_offload *ct_offload);
	void (*update_stats)(struct nf_conn *ct);
};

#define ct_offload_orig (ct_offload->info[IP_CT_DIR_ORIGINAL])
#define ct_offload_repl (ct_offload->info[IP_CT_DIR_REPLY])

static inline
struct nf_conn_offload *nf_conn_offload_find(const struct nf_conn *ct)
{
#ifdef CONFIG_NF_CONNTRACK_OFFLOAD
	return nf_ct_ext_find(ct, NF_CT_EXT_OFFLOAD);
#else
	return NULL;
#endif
}

static inline
struct nf_conn_offload *nf_ct_offload_ext_add(struct nf_conn *ct, gfp_t gfp)
{
#ifdef CONFIG_NF_CONNTRACK_OFFLOAD
	struct net *net = nf_ct_net(ct);
	struct nf_conn_offload *ct_offload;

	if (!net->ct.sysctl_offload)
		return NULL;

	ct_offload = nf_ct_ext_add(ct, NF_CT_EXT_OFFLOAD, gfp);
	if (ct_offload) {
		memset(ct_offload, 0, sizeof(struct nf_conn_offload));
		ct_offload_orig.ctinfo = -1;
		ct_offload_repl.ctinfo = -1;
		ct_offload->events_retry_timeout =
			net->ct.sysctl_events_retry_timeout;
	}

	return ct_offload;
#else
	return NULL;
#endif
};

static inline
__u8 ip_get_dscp(struct sk_buff *skb)
{
	if (skb->protocol == htons(ETH_P_IP))
		return ipv4_get_dsfield(ip_hdr(skb)) >> 2;
	else if (skb->protocol == htons(ETH_P_IPV6))
		return ipv6_get_dsfield(ipv6_hdr(skb)) >> 2;
	return 0;
}

static inline
void nf_ct_offload_update(struct sk_buff *skb)
{
#ifdef CONFIG_NF_CONNTRACK_OFFLOAD
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct = nf_ct_get(skb, &ctinfo);
	struct nf_conn_offload *ct_offload;
	if (!ct)
		return;
	ct_offload = nf_conn_offload_find(ct);
	if (!ct_offload)
		return;
	if (CTINFO2DIR(ctinfo) == IP_CT_DIR_ORIGINAL) {
		ct_offload_orig.dscp_old = ip_get_dscp(skb);
		if (!ct_offload_orig.eh.h_proto)
			memcpy(&(ct_offload_orig.eh), eth_hdr(skb), ETH_HLEN);
	} else {
		ct_offload_repl.dscp_old = ip_get_dscp(skb);
		if (!ct_offload_repl.eh.h_proto)
			memcpy(&(ct_offload_repl.eh), eth_hdr(skb), ETH_HLEN);
	}
#else
	return;
#endif
};

#ifdef CONFIG_NF_CONNTRACK_OFFLOAD
int nf_conntrack_offload_init(void);
void nf_conntrack_offload_fini(void);
int nf_conntrack_offload_pernet_init(struct net *net);
void nf_conntrack_offload_pernet_fini(struct net *net);
#else
static inline int nf_conntrack_offload_init(void)
{
	return 0;
}

static inline void nf_conntrack_offload_fini(void)
{
	return;
}
int nf_conntrack_offload_pernet_init(struct net *net)
{
	return 0;
}
void nf_conntrack_offload_pernet_fini(struct net *net)
{
	return;
}
#endif /* CONFIG_NF_CONNTRACK_OFFLOAD */

#endif /* _NF_CONNTRACK_OFFLOAD_H */
