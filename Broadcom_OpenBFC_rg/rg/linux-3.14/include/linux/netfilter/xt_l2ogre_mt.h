 /****************************************************************************
 *
 * Copyright (c) 2016 Broadcom Ltd.
 *
 * Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to
 * you under the terms of the GNU General Public License version 2 (the
 * "GPL"), available at [http://www.broadcom.com/licenses/GPLv2.php], with
 * the following added to such license:
 *
 * As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy
 * and distribute the resulting executable under terms of your choice,
 * provided that you also meet, for each linked independent module, the
 * terms and conditions of the license of that module. An independent
 * module is a module which is not derived from this software. The special
 * exception does not apply to any modifications of the software.
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a
 * license other than the GPL, without Broadcom's express prior written
 * consent.
 *
 ****************************************************************************
 * Authors:Ignatius Cheng <ignatius.cheng@broadcom.com>
 *
 * Feburary, 2016
 *
 ****************************************************************************/
#ifndef _LINUX_NETFILTER_XT_L2OGRE_MT_H
#define _LINUX_NETFILTER_XT_L2OGRE_MT_H 1
#include <linux/types.h>
#include <linux/netfilter.h>

#ifdef __KERNEL__
int xt_l2ogre_mt_init(void) __init;
void xt_l2ogre_mt_finish(void) __exit;
#endif

enum {
	/* These will help identify if this is coming from Tunnel,
	and Tunnel Interface */
	XT_L2OGRE_MT_REMOTE		= 1 << 0,
	XT_L2OGRE_MT_LOCAL		= 1 << 1,
	XT_L2OGRE_MT_USE_KEYID		= 1 << 2,
	XT_L2OGRE_MT_VLANID_CHECK	= 1 << 3,
	XT_L2OGRE_MT_VLANID_EXCLUDE	= 1 << 4,
	/* up to 32 parameters - as flag is 32 bits in size */
};

/* for matching GRE packet */
struct xt_l2ogre_mtinfo {
	__u32 flags;			/* up to 32 parameters */
	union nf_inet_addr remote;
	union nf_inet_addr local;
	__u32 key_id;
	__u16 vlan_id_check;		/*  0..4096, 12-bits vlan Id */
};
#endif /* _LINUX_NETFILTER_XT_L2OGRE_MT_H */
