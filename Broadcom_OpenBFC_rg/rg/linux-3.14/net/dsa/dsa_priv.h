/*
 * net/dsa/dsa_priv.h - Hardware switch handling
 * Copyright (c) 2008-2009 Marvell Semiconductor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __DSA_PRIV_H
#define __DSA_PRIV_H

#include <linux/phy.h>
#include <net/dsa.h>
#include <bcmnethooks.h>
#include <linux/netpoll.h>

struct dsa_slave_priv {
	/*
	 * The linux network interface corresponding to this
	 * switch port.
	 */
	struct net_device	*dev;
	struct bcm_nethooks	nethooks;	/* must be 2nd field */

	/*
	 * Which switch this port is a part of, and the port index
	 * for this port.
	 */
	struct dsa_switch	*parent;
	u8			port;

	/*
	 * The phylib phy_device pointer for the PHY connected
	 * to this port.
	 */
	struct phy_device	*phy;
	phy_interface_t		phy_interface;
	int			old_link;
	int			old_pause;
	int			old_duplex;

	struct net_bridge	*bridge_dev;
#ifdef CONFIG_NET_POLL_CONTROLLER
	struct netpoll		*netpoll;
#endif
};

static inline netdev_tx_t dsa_netpoll_send_skb(struct dsa_slave_priv *p,
					       struct sk_buff *skb)
{
#ifdef CONFIG_NET_POLL_CONTROLLER
	if (p->netpoll)
		netpoll_send_skb(p->netpoll, skb);
#else
	BUG();
#endif
	return NETDEV_TX_OK;
}

/* dsa.c */
extern char dsa_driver_version[];

/* slave.c */
void dsa_slave_mii_bus_init(struct dsa_switch *ds);
struct net_device *dsa_slave_create(struct dsa_switch *ds,
				    struct device *parent,
				    int port, char *name);
int dsa_slave_suspend(struct net_device *slave_dev);
int dsa_slave_resume(struct net_device *slave_dev);

/* tag_dsa.c */
netdev_tx_t dsa_xmit(struct sk_buff *skb, struct net_device *dev);
extern struct packet_type dsa_packet_type;

/* tag_edsa.c */
netdev_tx_t edsa_xmit(struct sk_buff *skb, struct net_device *dev);
extern struct packet_type edsa_packet_type;

/* tag_trailer.c */
netdev_tx_t trailer_xmit(struct sk_buff *skb, struct net_device *dev);
extern struct packet_type trailer_packet_type;

/* tag_brcm.c */
netdev_tx_t brcm_tag_xmit(struct sk_buff *skb, struct net_device *dev);
extern struct packet_type brcm_tag_packet_type;


#endif
