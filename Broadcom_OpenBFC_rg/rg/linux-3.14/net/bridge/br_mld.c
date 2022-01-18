#if defined(CONFIG_BCM_KF_MLD)
/*
*    Copyright (c) 2012 Broadcom Corporation
*    All Rights Reserved
*
<:label-BRCM:2012:DUAL/GPL:standard

Unless you and Broadcom execute a separate written software license
agreement governing use of this software, this software is licensed
to you under the terms of the GNU General Public License version 2
(the "GPL"), available at http://www.broadcom.com/licenses/GPLv2.php,
with the following added to such license:

   As a special exception, the copyright holders of this software give
   you permission to link this software with independent modules, and
   to copy and distribute the resulting executable under terms of your
   choice, provided that you also meet, for each linked independent
   module, the terms and conditions of the license of that module.
   An independent module is a module which is not derived from this
   software.  The special exception does not apply to any modifications
   of the software.

Not withstanding the above, under no circumstances may you combine
this software in any way with any other Broadcom software provided
under a license other than the GPL, without Broadcom's express prior
written consent.

:>
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/times.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/jhash.h>
#include <asm/atomic.h>
#include <linux/ip.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>
#include <linux/list.h>
#include <linux/rtnetlink.h>
#include "br_private.h"
#include "br_mld.h"
#include <linux/if_vlan.h>
#include "br_mcast.h"


#include <linux/module.h>
int br_mld_snooping_enabled(struct net_device *dev) {
	struct net_bridge *br;
        struct net_bridge_port *port;

        port = br_port_get_rcu(dev);
	if (port) {
		br = port->br;
		if (br->mld_snooping==SNOOPING_DISABLED_MODE)
		return 0;
		else return 1;
	}
	return 0;
}

static struct kmem_cache *br_mld_mc_fdb_cache __read_mostly;
static struct kmem_cache *br_mld_mc_rep_cache __read_mostly;
static u32 br_mld_mc_fdb_salt __read_mostly;
static struct proc_dir_entry *br_mld_entry = NULL;

extern int mcpd_process_skb(struct net_bridge *br, struct sk_buff *skb,
                            unsigned short protocol);

static struct in6_addr all_dhcp_srvr_addr = { .in6_u.u6_addr32 = {0xFF050000,
                                                                  0x00000000,
                                                                  0x00000000,
                                                                  0x00010003 } };

static inline int br_mld_mc_fdb_hash(const struct in6_addr *grp)
{
	return jhash_1word((grp->s6_addr32[0] | grp->s6_addr32[3]),
                                   br_mld_mc_fdb_salt) & (BR_MLD_HASH_SIZE - 1);
}

static int br_mld_control_filter(const unsigned char *dest, const struct in6_addr *ipv6)
{
    /* ignore any packets that are not multicast
       ignore scope0, node and link local addresses
       ignore IPv6 all DHCP servers address */
    if(((dest) && is_broadcast_ether_addr(dest)) ||
       (!BCM_IN6_IS_ADDR_MULTICAST(ipv6)) ||
       (BCM_IN6_IS_ADDR_MC_SCOPE0(ipv6)) ||
       (BCM_IN6_IS_ADDR_MC_NODELOCAL(ipv6)) ||
       (BCM_IN6_IS_ADDR_MC_LINKLOCAL(ipv6)) ||
       (0 == memcmp(ipv6, &all_dhcp_srvr_addr, sizeof(struct in6_addr))))
        return 0;
    else
        return 1;
}

/* This function requires that br->mld_mcl_lock is already held */
void br_mld_mc_fdb_del_entry(struct net_bridge *br,
                             struct net_br_mld_mc_fdb_entry *mld_fdb,
                             struct in6_addr *rep)
{
	struct net_br_mld_mc_rep_entry *rep_entry = NULL;
	struct net_br_mld_mc_rep_entry *rep_entry_n = NULL;

	list_for_each_entry_safe(rep_entry,
	                         rep_entry_n, &mld_fdb->rep_list, list)
	{
		if((NULL == rep) ||
		   (BCM_IN6_ARE_ADDR_EQUAL(&rep_entry->rep, rep)))
		{
			list_del(&rep_entry->list);
			kmem_cache_free(br_mld_mc_rep_cache, rep_entry);
			if (rep)
{
				break;
			}
		}
	}
	if(list_empty(&mld_fdb->rep_list))
{
		hlist_del(&mld_fdb->hlist);
		br_mld_wl_del_entry(br, mld_fdb);
		kmem_cache_free(br_mld_mc_fdb_cache, mld_fdb);
}

	return;
}

void br_mld_set_timer( struct net_bridge *br )
{
	struct net_br_mld_mc_fdb_entry *mcast_group;
	int                             i;
	unsigned long                   tstamp;
	unsigned int                    found;

	if ( br->mld_snooping == 0 )
	{
		del_timer(&br->mld_timer);
		return;
	}

	/* the largest timeout is BR_MLD_MEMBERSHIP_TIMEOUT */
	tstamp = jiffies + (BR_MLD_MEMBERSHIP_TIMEOUT*HZ*2);
	found = 0;
	for (i = 0; i < BR_MLD_HASH_SIZE; i++)
	{
		hlist_for_each_entry(mcast_group, &br->mld_mc_hash[i], hlist)
		{
			struct net_br_mld_mc_rep_entry *reporter;
			list_for_each_entry(reporter, &mcast_group->rep_list, list)
			{
				if ( time_after(tstamp, reporter->tstamp) )
				{
					tstamp = reporter->tstamp;
					found  = 1;
				}
			}
		}
	}

	if ( 0 == found )
	{
		del_timer(&br->mld_timer);
	}
	else
	{
		mod_timer(&br->mld_timer, (tstamp + TIMER_CHECK_TIMEOUT));
	}
}


static void br_mld_query_timeout(unsigned long ptr)
{
	struct net_br_mld_mc_fdb_entry *mcast_group;
	struct net_bridge *br;
	int i;

	br = (struct net_bridge *) ptr;

	spin_lock_bh(&br->mld_mcl_lock);
	for (i = 0; i < BR_MLD_HASH_SIZE; i++)
	{
		struct hlist_node *n_group;
		hlist_for_each_entry_safe(mcast_group, n_group, &br->mld_mc_hash[i], hlist)
		{
			struct net_br_mld_mc_rep_entry *reporter, *n_reporter;
			list_for_each_entry_safe(reporter, n_reporter, &mcast_group->rep_list, list)
			{
				if (time_after_eq(jiffies, reporter->tstamp))
				{
					br_mld_mc_fdb_del_entry(br, mcast_group, &reporter->rep);
				}
			}
		}
	}

	br_mld_set_timer(br);
	spin_unlock_bh(&br->mld_mcl_lock);
}

static struct net_br_mld_mc_rep_entry *
                br_mld_rep_find(const struct net_br_mld_mc_fdb_entry *mc_fdb,
                                const struct in6_addr *rep)
{
	struct net_br_mld_mc_rep_entry *rep_entry;

	list_for_each_entry(rep_entry, &mc_fdb->rep_list, list)
   {
		if(BCM_IN6_ARE_ADDR_EQUAL(&rep_entry->rep, rep))
			return rep_entry;
    }

	return NULL;
}

/* In the case where a reporter has changed ports, this function
   will remove all records pointing to the old port */
void br_mld_wipe_reporter_for_port (struct net_bridge *br,
                                    struct in6_addr *rep,
                                    u16 oldPort)
{
    int hashIndex = 0;
    struct hlist_node *n = NULL;
    struct hlist_head *head = NULL;
    struct net_br_mld_mc_fdb_entry *mc_fdb;

    spin_lock_bh(&br->mld_mcl_lock);
    for ( ; hashIndex < BR_MLD_HASH_SIZE ; hashIndex++)
    {
        head = &br->mld_mc_hash[hashIndex];
        hlist_for_each_entry_safe(mc_fdb, n, head, hlist)
        {
            if ((br_mld_rep_find(mc_fdb, rep)) &&
                (mc_fdb->dst->port_no == oldPort))
            {
                /* The reporter we're looking for has been found
                   in a record pointing to its old port */
                br_mld_mc_fdb_del_entry (br, mc_fdb, rep);
            }
        }
    }
    br_mld_set_timer(br);
    spin_unlock_bh(&br->mld_mcl_lock);
}

/* this is called during addition of a snooping entry and requires that
   mld_mcl_lock is already held */
static int br_mld_mc_fdb_update(struct net_bridge *br,
                                struct net_bridge_port *prt,
                                struct in6_addr *grp,
                                struct in6_addr *rep,
                                int mode,
                                struct in6_addr *src,
                                struct net_device *from_dev)
{
	struct net_br_mld_mc_fdb_entry *dst;
	struct net_br_mld_mc_rep_entry *rep_entry = NULL;
	int ret = 0;
	int filt_mode;
	struct hlist_head *head;

	if(mode == SNOOP_IN_ADD)
		filt_mode = MCAST_INCLUDE;
	else
		filt_mode = MCAST_EXCLUDE;

	head = &br->mld_mc_hash[br_mld_mc_fdb_hash(grp)];
	hlist_for_each_entry(dst, head, hlist) {
		if (BCM_IN6_ARE_ADDR_EQUAL(&dst->grp, grp))
		{
			if((BCM_IN6_ARE_ADDR_EQUAL(src, &dst->src_entry.src)) &&
			   (filt_mode == dst->src_entry.filt_mode) &&
			   (dst->from_dev == from_dev) &&
			   (dst->dst == prt) )
			{
				/* found entry - update TS */
				struct net_br_mld_mc_rep_entry *reporter = br_mld_rep_find(dst, rep);
				if(reporter == NULL)
				{
					rep_entry = kmem_cache_alloc(br_mld_mc_rep_cache, GFP_ATOMIC);
					if(rep_entry)
					{
						BCM_IN6_ASSIGN_ADDR(&rep_entry->rep, rep);
						rep_entry->tstamp = jiffies + BR_MLD_MEMBERSHIP_TIMEOUT*HZ;
						list_add_tail(&rep_entry->list, &dst->rep_list);
						br_mld_set_timer(br);
					}
				}
				else
				{
					reporter->tstamp = jiffies + BR_MLD_MEMBERSHIP_TIMEOUT*HZ;
					br_mld_set_timer(br);
				}
				ret = 1;
			}
		}
	}
	return ret;
}

int br_mld_process_if_change(struct net_bridge *br, struct net_device *ndev)
{
	struct net_br_mld_mc_fdb_entry *dst;
	int i;

	spin_lock_bh(&br->mld_mcl_lock);
	for (i = 0; i < BR_MLD_HASH_SIZE; i++)
	{
		struct hlist_node *n;
		hlist_for_each_entry_safe(dst, n, &br->mld_mc_hash[i], hlist)
		{
			if ((NULL == ndev) ||
			    (dst->dst->dev == ndev) ||
			    (dst->from_dev == ndev))
			{
				br_mld_mc_fdb_del_entry(br, dst, NULL);
			}
		}
	}
	br_mld_set_timer(br);
	spin_unlock_bh(&br->mld_mcl_lock);

	return 0;
}

int br_mld_mc_fdb_add(struct net_device *from_dev,
                        int wan_ops,
                        struct net_bridge *br,
                        struct net_bridge_port *prt,
                        struct in6_addr *grp,
                        struct in6_addr *rep,
                        int mode,
                        int tci,
                        struct in6_addr *src)
{
	struct net_br_mld_mc_fdb_entry *mc_fdb;
	struct net_br_mld_mc_rep_entry *rep_entry = NULL;
	struct hlist_head *head = NULL;

	if(!br || !prt || !grp|| !rep || !from_dev)
		return 0;

	if(!br_mld_control_filter(NULL, grp))
		return 0;

	if((SNOOP_IN_ADD != mode) && (SNOOP_EX_ADD != mode))
		return 0;

	/* allocate before getting the lock so that GFP_KERNEL can be used */
	mc_fdb = kmem_cache_alloc(br_mld_mc_fdb_cache, GFP_KERNEL);
	if (!mc_fdb)
	{
		return -ENOMEM;
	}
	rep_entry = kmem_cache_alloc(br_mld_mc_rep_cache, GFP_KERNEL);
	if ( !rep_entry )
	{
		kmem_cache_free(br_mld_mc_fdb_cache, mc_fdb);
		return -ENOMEM;
	}

	spin_lock_bh(&br->mld_mcl_lock);
	if (br_mld_mc_fdb_update(br, prt, grp, rep, mode, src, from_dev))
	{
		kmem_cache_free(br_mld_mc_fdb_cache, mc_fdb);
		kmem_cache_free(br_mld_mc_rep_cache, rep_entry);
		spin_unlock_bh(&br->mld_mcl_lock);
		return 0;
	}

	BCM_IN6_ASSIGN_ADDR(&mc_fdb->grp, grp);
	BCM_IN6_ASSIGN_ADDR(&mc_fdb->src_entry, src);
	mc_fdb->src_entry.filt_mode =
	             (mode == SNOOP_IN_ADD) ? MCAST_INCLUDE : MCAST_EXCLUDE;
	mc_fdb->dst = prt;
	mc_fdb->lan_tci = tci;
	mc_fdb->wan_tci = 0;
	mc_fdb->num_tags = 0;
	mc_fdb->from_dev = from_dev;
	mc_fdb->type = wan_ops;
	INIT_LIST_HEAD(&mc_fdb->rep_list);
	BCM_IN6_ASSIGN_ADDR(&rep_entry->rep, rep);
	rep_entry->tstamp = jiffies + (BR_MLD_MEMBERSHIP_TIMEOUT*HZ);
	list_add_tail(&rep_entry->list, &mc_fdb->rep_list);

	head = &br->mld_mc_hash[br_mld_mc_fdb_hash(grp)];
	hlist_add_head(&mc_fdb->hlist, head);

	br_mld_set_timer(br);
	spin_unlock_bh(&br->mld_mcl_lock);

	return 1;
}
EXPORT_SYMBOL(br_mld_mc_fdb_add);

void br_mld_mc_fdb_cleanup(struct net_bridge *br)
{
	struct net_br_mld_mc_fdb_entry *dst;
	int i;

	spin_lock_bh(&br->mld_mcl_lock);
	for (i = 0; i < BR_MLD_HASH_SIZE; i++)
	{
		struct hlist_node *n;
		hlist_for_each_entry_safe(dst, n, &br->mld_mc_hash[i], hlist)
		{
			br_mld_mc_fdb_del_entry(br, dst, NULL);
		}
	}
	br_mld_set_timer(br);
	spin_unlock_bh(&br->mld_mcl_lock);
}

void br_mld_mc_fdb_remove_grp(struct net_bridge *br,
                              struct net_bridge_port *prt,
                              struct in6_addr *grp)
{
	struct net_br_mld_mc_fdb_entry *dst;
	struct hlist_head *head = NULL;
	struct hlist_node *n;

	if(!br || !prt || !grp)
		return;

	spin_lock_bh(&br->mld_mcl_lock);
	head = &br->mld_mc_hash[br_mld_mc_fdb_hash(grp)];
	hlist_for_each_entry_safe(dst, n, head, hlist) {
		if ((BCM_IN6_ARE_ADDR_EQUAL(&dst->grp, grp)) &&
		    (dst->dst == prt))
		{
			br_mld_mc_fdb_del_entry(br, dst, NULL);
		}
	}
	br_mld_set_timer(br);
	spin_unlock_bh(&br->mld_mcl_lock);
}

int br_mld_mc_fdb_remove(struct net_device *from_dev,
                         struct net_bridge *br,
                         struct net_bridge_port *prt,
                         struct in6_addr *grp,
                         struct in6_addr *rep,
                         int mode,
                         struct in6_addr *src)
{
	struct net_br_mld_mc_fdb_entry *mc_fdb;
	int filt_mode;
	struct hlist_head *head = NULL;
	struct hlist_node *n;

	if(!br || !prt || !grp|| !rep || !from_dev)
		return 0;

	if(!br_mld_control_filter(NULL, grp))
		return 0;

	if((SNOOP_IN_CLEAR != mode) && (SNOOP_EX_CLEAR != mode))
		return 0;

	if(mode == SNOOP_IN_CLEAR)
		filt_mode = MCAST_INCLUDE;
	else
		filt_mode = MCAST_EXCLUDE;

	spin_lock_bh(&br->mld_mcl_lock);
	head = &br->mld_mc_hash[br_mld_mc_fdb_hash(grp)];
	hlist_for_each_entry_safe(mc_fdb, n, head, hlist)
	{
		if ((BCM_IN6_ARE_ADDR_EQUAL(&mc_fdb->grp, grp)) &&
		    (filt_mode == mc_fdb->src_entry.filt_mode) &&
		    (BCM_IN6_ARE_ADDR_EQUAL(&mc_fdb->src_entry.src, src)) &&
		    (mc_fdb->from_dev == from_dev) &&
		    (mc_fdb->dst == prt))
		{
			br_mld_mc_fdb_del_entry(br, mc_fdb, rep);
		}
	}
	br_mld_set_timer(br);
	spin_unlock_bh(&br->mld_mcl_lock);

	return 0;
}
EXPORT_SYMBOL(br_mld_mc_fdb_remove);

int br_mld_mc_forward(struct net_bridge *br,
                      struct sk_buff *skb,
                      int forward,
                      int is_routed)
{
	struct net_br_mld_mc_fdb_entry *dst;
	int status = 0;
	struct sk_buff *skb2;
	struct net_bridge_port *p, *p_n;
    const unsigned char *dest = eth_hdr(skb)->h_dest;
    struct hlist_head *head = NULL;
	const struct ipv6hdr *pipv6 = ipv6_hdr(skb);
	struct icmp6hdr *pIcmp = NULL;
	u8 *nextHdr;

	if(vlan_eth_hdr(skb)->h_vlan_proto != htons(ETH_P_IPV6))
	{
		if ( vlan_eth_hdr(skb)->h_vlan_proto == htons(ETH_P_8021Q) )
		{
			if ( vlan_eth_hdr(skb)->h_vlan_encapsulated_proto != htons(ETH_P_IPV6) )
			{
				return status;
			}
			pipv6  = (struct ipv6hdr *)(skb_network_header(skb) + sizeof(struct vlan_hdr));
		}
		else
		{
			return status;
		}
	}

	nextHdr = (u8 *)((u8*)pipv6 + sizeof(struct ipv6hdr));
	if ( (pipv6->nexthdr == IPPROTO_HOPOPTS) &&
        (*nextHdr == IPPROTO_ICMPV6) )
   {
		/* skip past hop by hop hdr */
		pIcmp =  (struct icmp6hdr *)(nextHdr + 8);
		if((pIcmp->icmp6_type == ICMPV6_MGM_REPORT) ||
			(pIcmp->icmp6_type == ICMPV6_MGM_REDUCTION) ||
			(pIcmp->icmp6_type == ICMPV6_MLD2_REPORT))
		{
			if(skb->dev && br_port_get_rcu(skb->dev) &&
			   (br->mld_snooping || br->mld_proxy))
			{
				/* for bridged WAN service, do not pass any MLD packets
				   coming from the WAN port to mcpd */
#if defined(CONFIG_BCM_KF_WANDEV)
				if ( skb->dev->priv_flags & IFF_WANDEV )
				{
					kfree_skb(skb);
					status = 1;
				}
				else
#endif
				{
				   mcpd_process_skb(br, skb, ETH_P_IPV6);
				}
			}
			return status;
		}
	}

	/* snooping could be disabled and still have entries */

	/* drop traffic by default when snooping is enabled
	   in blocking mode */
	if ((br->mld_snooping == SNOOPING_BLOCKING_MODE) &&
	     br_mld_control_filter(dest, &pipv6->daddr))
	{
		status = 1;
	}

	spin_lock_bh(&br->mld_mcl_lock);
    head = &br->mld_mc_hash[br_mld_mc_fdb_hash(&pipv6->daddr)];
    hlist_for_each_entry(dst, head, hlist) {
		if (BCM_IN6_ARE_ADDR_EQUAL(&dst->grp, &pipv6->daddr)) {
			/* routed packet will have bridge as from dev - cannot match to mc_fdb */
			if ( is_routed ) {
				if ( dst->type != MCPD_IF_TYPE_ROUTED ) {
					continue;
				}
			}
			else {
				if ( dst->type != MCPD_IF_TYPE_BRIDGED ) {
					continue;
				}
#if defined(CONFIG_BCM_KF_WANDEV)
				if (skb->dev->priv_flags & IFF_WANDEV) {
					/* match exactly if skb device is a WAN device - otherwise continue */
					if (dst->from_dev != skb->dev)
						continue;
				}
				else
#endif
				{
					/* if this is not an L2L mc_fdb entry continue */
					if (dst->from_dev != br->dev)
						continue;
				}
			}
			if((dst->src_entry.filt_mode == MCAST_INCLUDE) &&
			   (BCM_IN6_ARE_ADDR_EQUAL(&pipv6->saddr, &dst->src_entry.src))) {
				if (!dst->dst->dirty) {
					if((skb2 = skb_clone(skb, GFP_ATOMIC)) == NULL)
					{
						spin_unlock_bh(&br->mld_mcl_lock);
						return 0;
					}
					if(forward)
						br_forward(dst->dst, skb2, NULL);
					else
						br_deliver(dst->dst, skb2);
				}
				dst->dst->dirty = 1;
				status = 1;
			}
			else if(dst->src_entry.filt_mode == MCAST_EXCLUDE) {
				if((0 == dst->src_entry.src.s6_addr[0]) ||
                   (!BCM_IN6_ARE_ADDR_EQUAL(&pipv6->saddr, &dst->src_entry.src))) {
					if (!dst->dst->dirty) {
						if((skb2 = skb_clone(skb, GFP_ATOMIC)) == NULL)
						{
							spin_unlock_bh(&br->mld_mcl_lock);
							return 0;
						}
						if(forward)
							br_forward(dst->dst, skb2, NULL);
						else
							br_deliver(dst->dst, skb2);
					}
					dst->dst->dirty = 1;
					status = 1;
				}
				    else if( BCM_IN6_ARE_ADDR_EQUAL(&pipv6->saddr, &dst->src_entry.src)) {
					     status = 1;
                }
			}
		}
	}
	spin_unlock_bh(&br->mld_mcl_lock);

	if (status) {
		list_for_each_entry_safe(p, p_n, &br->port_list, list) {
			p->dirty = 0;
		}
	}

	if(status)
		kfree_skb(skb);

	return status;
}

int br_mld_mc_fdb_update_bydev( struct net_bridge *br,
                                struct net_device *dev,
                                unsigned int       flushAll)
{
	struct net_br_mld_mc_fdb_entry *mc_fdb;
	int i;

	if(!br || !dev)
		return 0;

	spin_lock_bh(&br->mld_mcl_lock);
	for (i = 0; i < BR_MLD_HASH_SIZE; i++)
	{
		struct hlist_node *n;
		hlist_for_each_entry_safe(mc_fdb, n, &br->mld_mc_hash[i], hlist)
		{
			if ((mc_fdb->dst->dev == dev) ||
			    (mc_fdb->from_dev == dev))
			{
				/* do not remove the root entry */
				if ((0 == mc_fdb->root) || (1 == flushAll))
				{
					br_mld_mc_fdb_del_entry(br, mc_fdb, NULL);
				}
			}
		}
	}

	if ( 0 == flushAll )
	{
		for (i = 0; i < BR_MLD_HASH_SIZE; i++)
		{
			struct hlist_node *n;
			hlist_for_each_entry_safe(mc_fdb, n, &br->mld_mc_hash[i], hlist)
			{
				if ( (1 == mc_fdb->root) &&
				     ((mc_fdb->dst->dev == dev) ||
				      (mc_fdb->from_dev == dev)) )
				{
					mc_fdb->wan_tci  = 0;
					mc_fdb->num_tags = 0;
					{
						br_mld_mc_fdb_del_entry(br, mc_fdb, NULL);
					}
				}
			}
		}
	}
	br_mld_set_timer(br);
	spin_unlock_bh(&br->mld_mcl_lock);

	return 0;
}

static void *snoop_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct net_device *dev;
	loff_t offs = 0;

	rcu_read_lock();
	for_each_netdev_rcu(&init_net, dev) {
		if ((dev->priv_flags & IFF_EBRIDGE) && (*pos == offs)) {
			return dev;
		}
	}
	++offs;
	return NULL;
}

static void *snoop_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct net_device *dev = v;

	++*pos;
	for(dev = next_net_device_rcu(dev); dev; dev = next_net_device_rcu(dev)) {
		if(dev->priv_flags & IFF_EBRIDGE) {
			return dev;
		}
	}
	return NULL;
}

static int snoop_seq_show(struct seq_file *seq, void *v)
{
	struct net_device *dev = v;
	struct net_br_mld_mc_fdb_entry *dst;
	struct net_bridge *br = netdev_priv(dev);
	struct net_br_mld_mc_rep_entry *rep_entry;
	int first;
	int i;
	int tstamp;

	seq_printf(seq, "mld snooping %d  proxy %d  lan2lan-snooping %d/%d, priority %d\n",
              br->mld_snooping,
              br->mld_proxy,
	      br->mld_lan2lan_mc_enable,
	      br_mcast_get_lan2lan_snooping(BR_MCAST_PROTO_MLD, br),
              br_mcast_get_pri_queue());
	seq_printf(seq, "bridge device src-dev #tags lan-tci    wan-tci");
	seq_printf(seq, "    group                               mode source");
	seq_printf(seq, "                              timeout reporter\n");

	for (i = 0; i < BR_MLD_HASH_SIZE; i++)
	{
		struct hlist_node *h;
		hlist_for_each(h, &br->mld_mc_hash[i])
		{
			dst = hlist_entry(h, struct net_br_mld_mc_fdb_entry, hlist);
			if( dst )
	{
		seq_printf(seq, "%-6s %-6s %-7s %02d    0x%08x 0x%08x",
		           br->dev->name,
		           dst->dst->dev->name,
		           dst->from_dev->name,
		           dst->num_tags,
		           ntohs(dst->lan_tci),
		           ntohl(dst->wan_tci));

		seq_printf(seq, " %08x:%08x:%08x:%08x",
		           htonl(dst->grp.s6_addr32[0]),
		           htonl(dst->grp.s6_addr32[1]),
		           htonl(dst->grp.s6_addr32[2]),
		           htonl(dst->grp.s6_addr32[3]));

		seq_printf(seq, " %-4s %08x:%08x:%08x:%08x",
		           (dst->src_entry.filt_mode == MCAST_EXCLUDE) ?
		            "EX" : "IN",
		           htonl(dst->src_entry.src.s6_addr32[0]),
		           htonl(dst->src_entry.src.s6_addr32[1]),
		           htonl(dst->src_entry.src.s6_addr32[2]),
		           htonl(dst->src_entry.src.s6_addr32[3]));

		      first = 1;
				list_for_each_entry(rep_entry, &dst->rep_list, list)
				{

					if ( 0 == br->mld_snooping )
					{
						tstamp = 0;
					}
					else
					{
						tstamp = (int)(rep_entry->tstamp - jiffies) / HZ;
					}

					if(first)
					{
		seq_printf(seq, " %-7d %08x:%08x:%08x:%08x\n",
						           tstamp,
						           htonl(rep_entry->rep.s6_addr32[0]),
						           htonl(rep_entry->rep.s6_addr32[1]),
						           htonl(rep_entry->rep.s6_addr32[2]),
						           htonl(rep_entry->rep.s6_addr32[3]));
						first = 0;
					}
					else
					{
						seq_printf(seq, "%126s %-7d %08x:%08x:%08x:%08x\n", " ",
						           tstamp,
						           htonl(rep_entry->rep.s6_addr32[0]),
						           htonl(rep_entry->rep.s6_addr32[1]),
						           htonl(rep_entry->rep.s6_addr32[2]),
						           htonl(rep_entry->rep.s6_addr32[3]));
					}
				}
			}
		}
	}

	return 0;
}

static void snoop_seq_stop(struct seq_file *seq, void *v)
{
	rcu_read_unlock();
}

static struct seq_operations snoop_seq_ops = {
	.start = snoop_seq_start,
	.next  = snoop_seq_next,
	.stop  = snoop_seq_stop,
	.show  = snoop_seq_show,
};

static int snoop_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &snoop_seq_ops);
}

static struct file_operations br_mld_snoop_proc_fops = {
	.owner = THIS_MODULE,
	.open  = snoop_seq_open,
	.read  = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

void br_mld_snooping_br_init( struct net_bridge *br )
{
	spin_lock_init(&br->mld_mcl_lock);
	br->mld_lan2lan_mc_enable = BR_MC_LAN2LAN_STATUS_DEFAULT;
	setup_timer(&br->mld_timer, br_mld_query_timeout, (unsigned long)br);
}

void br_mld_snooping_br_fini( struct net_bridge *br )
{
	del_timer_sync(&br->mld_timer);
}

int __init br_mld_snooping_init(void)
{
	br_mld_entry = proc_create("mld_snooping", 0, init_net.proc_net,
			   &br_mld_snoop_proc_fops);

	if(!br_mld_entry) {
		printk("error while creating mld_snooping proc\n");
        return -ENOMEM;
	}

	br_mld_mc_fdb_cache = kmem_cache_create("bridge_mld_mc_fdb_cache",
                            sizeof(struct net_br_mld_mc_fdb_entry),
                            0,
                            SLAB_HWCACHE_ALIGN, NULL);
    if (!br_mld_mc_fdb_cache)
		return -ENOMEM;

    br_mld_mc_rep_cache = kmem_cache_create("br_mld_mc_rep_cache",
                                             sizeof(struct net_br_mld_mc_rep_entry),
                                             0,
                                             SLAB_HWCACHE_ALIGN, NULL);
    if (!br_mld_mc_rep_cache)
    {
       kmem_cache_destroy(br_mld_mc_fdb_cache);
		 return -ENOMEM;
    }

	 get_random_bytes(&br_mld_mc_fdb_salt, sizeof(br_mld_mc_fdb_salt));

    return 0;
}

void br_mld_snooping_fini(void)
{
	kmem_cache_destroy(br_mld_mc_fdb_cache);
	kmem_cache_destroy(br_mld_mc_rep_cache);

    return;
}

EXPORT_SYMBOL(br_mld_control_filter);
EXPORT_SYMBOL(br_mld_snooping_enabled);
#endif
