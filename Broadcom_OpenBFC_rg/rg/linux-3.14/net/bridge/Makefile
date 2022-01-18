#
# Makefile for the IEEE 802.1d ethernet bridging layer.
#

obj-$(CONFIG_BRIDGE) += bridge.o

bridge-y	:= br.o br_device.o br_fdb.o br_forward.o br_if.o br_input.o \
			br_ioctl.o br_notify.o br_stp.o br_stp_bpdu.o \
			br_stp_if.o br_stp_timer.o br_netlink.o

bridge-$(CONFIG_SYSFS) += br_sysfs_if.o br_sysfs_br.o

# CWS FIX ifdef CONFIG_BCM_KF_IGMP
bridge-$(CONFIG_BR_IGMP_SNOOP) += br_igmp.o
bridge-$(CONFIG_BR_MLD_SNOOP) += br_mld.o
# CWS FIX endif

bridge-$(CONFIG_BRIDGE_NETFILTER) += br_netfilter.o

bridge-$(CONFIG_BRIDGE_IGMP_SNOOPING) += br_multicast.o br_mdb.o

bridge-$(CONFIG_BRIDGE_VLAN_FILTERING) += br_vlan.o

obj-$(CONFIG_BRIDGE_NF_EBTABLES) += netfilter/

# CWS FIX ifdef CONFIG_BCM_KF_IGMP
ifeq ($(strip $(CONFIG_BR_IGMP_SNOOP)),y)
bridge-$(CONFIG_BR_IGMP_SNOOP) += br_netlink_mcpd.o br_mcast.o
else
ifeq ($(strip $(CONFIG_BR_MLD_SNOOP)),y)
bridge-$(CONFIG_BR_MLD_SNOOP) += br_netlink_mcpd.o br_mcast.o
endif
endif
# CWS FIX endif
