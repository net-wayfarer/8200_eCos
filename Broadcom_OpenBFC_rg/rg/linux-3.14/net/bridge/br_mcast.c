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

#if (defined(CONFIG_BCM_KF_IGMP) && defined(CONFIG_BR_IGMP_SNOOP)) || (defined(CONFIG_BCM_KF_MLD) && defined(CONFIG_BR_MLD_SNOOP))

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
#include <linux/list.h>
#include <linux/rtnetlink.h>
#include "br_private.h"
#if defined(CONFIG_BCM_KF_IGMP) && defined(CONFIG_BR_IGMP_SNOOP)
#include "br_igmp.h"
#endif
#if defined(CONFIG_BCM_KF_MLD) && defined(CONFIG_BR_MLD_SNOOP)
#include "br_mld.h"
#include <linux/module.h>
#endif

#if (defined(CONFIG_BCM_KF_IGMP) && defined(CONFIG_BR_IGMP_SNOOP)) || (defined(CONFIG_BCM_KF_MLD) && defined(CONFIG_BR_MLD_SNOOP))
static t_MCAST_CFG multiConfig = { -1, /* mcastPriQueue */
                                  0   /* thereIsAnUplink */
                                 };

void br_mcast_set_pri_queue(int val)
{
   multiConfig.mcastPriQueue = val;
}

int br_mcast_get_pri_queue(void)
{
   return multiConfig.mcastPriQueue;
}

void br_mcast_set_skb_mark_queue(struct sk_buff *skb)
{
   int isMulticast = 0;
   const unsigned char *dest = eth_hdr(skb)->h_dest;

#if defined(CONFIG_BR_MLD_SNOOP)
   if((BR_MLD_MULTICAST_MAC_PREFIX == dest[0]) &&
      (BR_MLD_MULTICAST_MAC_PREFIX == dest[1])) {
      isMulticast = 1;
   }
#endif

#if defined(CONFIG_BR_IGMP_SNOOP) && defined(CONFIG_BCM_KF_IGMP)
   if (is_multicast_ether_addr(dest)) {
      isMulticast = 1;
   }
#endif
}

void br_mcast_set_uplink_exists(int uplinkExists)
{
	multiConfig.thereIsAnUplink = uplinkExists;
}

int br_mcast_get_lan2lan_snooping(t_BR_MCAST_PROTO_TYPE proto, struct net_bridge *br)
{
   if (!multiConfig.thereIsAnUplink)
      {
      return BR_MC_LAN2LAN_STATUS_ENABLE;
      }
#if defined(CONFIG_BR_MLD_SNOOP)
   if ( BR_MCAST_PROTO_MLD == proto )
      {
         return br->mld_lan2lan_mc_enable;
      }
   else
#endif
   {
         return br->igmp_lan2lan_mc_enable;
      }
   }

static void br_mcast_mc_fdb_update_bydev(t_BR_MCAST_PROTO_TYPE proto,
                                         struct net_bridge    *br,
                                         struct net_device    *dev,
                                         unsigned int          flushAll)
{
    if(!br || !dev)
        return;

    switch ( proto ) {
#if defined(CONFIG_BCM_KF_IGMP) && defined(CONFIG_BR_IGMP_SNOOP)
        case BR_MCAST_PROTO_IGMP:
            br_igmp_mc_fdb_update_bydev(br, dev, flushAll);
            break;
#endif
#if defined(CONFIG_BCM_KF_MLD) && defined(CONFIG_BR_MLD_SNOOP)
        case BR_MCAST_PROTO_MLD:
            br_mld_mc_fdb_update_bydev(br, dev, flushAll);
            break;
#endif
        default:
            break;
    }
    return;
}

void br_mcast_handle_netdevice_events(struct net_device *ndev, unsigned long event)
{
    struct net_bridge *br = NULL;
    struct net_device *brDev = NULL;

    switch (event) {
        case NETDEV_DOWN:
        case NETDEV_GOING_DOWN:
        case NETDEV_CHANGE:
            rcu_read_lock();
            for_each_netdev_rcu(&init_net, brDev) {
                br = netdev_priv(brDev);
                if(brDev->priv_flags & IFF_EBRIDGE)
                {
                    /* snooping entries could be present even if snooping is
                       disabled, update existing entries */
#if defined(CONFIG_BCM_KF_IGMP) && defined(CONFIG_BR_IGMP_SNOOP)
                    br_mcast_mc_fdb_update_bydev(BR_MCAST_PROTO_IGMP, br, ndev, 1);
#endif
#if defined(CONFIG_BCM_KF_MLD) && defined(CONFIG_BR_MLD_SNOOP)
                    br_mcast_mc_fdb_update_bydev(BR_MCAST_PROTO_MLD, br, ndev, 1);
#endif
                }
            }
            rcu_read_unlock();
            break;
    }

    return;
}
#endif

#if defined(CONFIG_BCM_KF_MLD) && defined(CONFIG_BR_MLD_SNOOP)
struct net_device *br_get_device_by_index(char *brname,char index) {
	struct net_bridge *br = NULL;
	struct net_bridge_port *br_port = NULL;
	struct net_device *dev = dev_get_by_name(&init_net,brname);
	struct net_device *prtDev = NULL;

	if(!dev)
		return NULL;

	if (0 == (dev->priv_flags & IFF_EBRIDGE))
	{
		printk("%s: invalid bridge name specified %s\n",
		         __FUNCTION__, brname);
		dev_put(dev);
		return NULL;
	}
	br = netdev_priv(dev);

	rcu_read_lock();
	br_port = br_get_port(br, index);
	if ( br_port )
	{
		prtDev = br_port->dev;
	}
	rcu_read_unlock();
	dev_put(dev);
	return prtDev;
}

static RAW_NOTIFIER_HEAD(mcast_snooping_chain);

int register_mcast_snooping_notifier(struct notifier_block *nb) {
	return raw_notifier_chain_register(&mcast_snooping_chain,nb);
}

int unregister_mcast_snooping_notifier(struct notifier_block *nb) {
	return raw_notifier_chain_unregister(&mcast_snooping_chain,nb);
}

int mcast_snooping_call_chain(unsigned long val,void *v)
{
	return raw_notifier_call_chain(&mcast_snooping_chain,val,v);
}


void br_mcast_wl_flush(struct net_bridge *br) {
	t_MCPD_MLD_SNOOP_ENTRY snoopEntry;
	struct net_bridge_port *p;

	rcu_read_lock();
	list_for_each_entry_rcu(p, &br->port_list, list) {
		if(!strncmp(p->dev->name,"wl",2)){
			snoopEntry.port_no= p->port_no;
			memcpy(snoopEntry.br_name,br->dev->name,IFNAMSIZ);
			mcast_snooping_call_chain(SNOOPING_FLUSH_ENTRY_ALL,(void *)&snoopEntry);
		}
	}
	rcu_read_unlock();
}

void br_mld_wl_del_entry(struct net_bridge *br,struct net_br_mld_mc_fdb_entry *dst) {
	if(dst && (!strncmp(dst->dst->dev->name,"wl",2))) {
		t_MCPD_MLD_SNOOP_ENTRY snoopEntry;
		snoopEntry.port_no=dst->dst->port_no;
		memcpy(snoopEntry.br_name,br->dev->name,IFNAMSIZ);
		memcpy(&snoopEntry.grp,&dst->grp,sizeof(struct in6_addr));
		mcast_snooping_call_chain(SNOOPING_FLUSH_ENTRY,(void *)&snoopEntry);
	}

}
EXPORT_SYMBOL(unregister_mcast_snooping_notifier);
EXPORT_SYMBOL(register_mcast_snooping_notifier);
EXPORT_SYMBOL(br_get_device_by_index);

#endif /* defined(CONFIG_BCM_KF_MLD) && defined(CONFIG_BR_MLD_SNOOP) */

#endif /* defined(CONFIG_BCM_KF_IGMP) || defined(CONFIG_BCM_KF_MLD) */
