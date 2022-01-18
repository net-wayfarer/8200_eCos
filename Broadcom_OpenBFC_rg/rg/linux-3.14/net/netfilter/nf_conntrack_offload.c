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

#include <linux/netfilter.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_acct.h>
#include <net/netfilter/nf_conntrack_zones.h>
#include <net/netfilter/nf_conntrack_timestamp.h>
#include <linux/rculist_nulls.h>
#include <net/netfilter/nf_conntrack_offload.h>

static bool nf_ct_offload __read_mostly = 1;

module_param_named(offload, nf_ct_offload, bool, 0644);
MODULE_PARM_DESC(offload, "Enable connection tracking flow offload.");

void nf_conntrack_offload_destroy(struct nf_conn *ct)
{
	struct nf_conn_offload *ct_offload = nf_conn_offload_find(ct);
	if (!ct_offload)
		return;
	if (ct_offload->destructor)
		ct_offload->destructor(ct, ct_offload);
	if (ct_offload_orig.nf_bridge)
		kfree(ct_offload_orig.nf_bridge);
	if (ct_offload_repl.nf_bridge)
		kfree(ct_offload_repl.nf_bridge);
}

static struct nf_ct_ext_type offload_extend __read_mostly = {
	.destroy = &nf_conntrack_offload_destroy,
	.len	 = sizeof(struct nf_conn_offload),
	.align	 = __alignof__(struct nf_conn_offload),
	.id	 = NF_CT_EXT_OFFLOAD,
};

#ifdef CONFIG_SYSCTL
static struct ctl_table offload_sysctl_table[] = {
	{
		.procname	= "nf_conntrack_offload",
		.data		= &init_net.ct.sysctl_offload,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{}
};

static int nf_conntrack_offload_init_sysctl(struct net *net)
{
	struct ctl_table *table;

	table = kmemdup(offload_sysctl_table, sizeof(offload_sysctl_table),
			GFP_KERNEL);
	if (!table)
		goto out;

	table[0].data = &net->ct.sysctl_offload;

	/* Don't export sysctls to unprivileged users */
	if (net->user_ns != &init_user_ns)
		table[0].procname = NULL;

	net->ct.offload_sysctl_header =
		register_net_sysctl(net, "net/netfilter", table);
	if (!net->ct.offload_sysctl_header) {
		pr_err("nf_conntrack_offload: can't register to sysctl.\n");
		goto out_register;
	}
	return 0;

out_register:
	kfree(table);
out:
	return -ENOMEM;
}

static void nf_conntrack_offload_fini_sysctl(struct net *net)
{
	struct ctl_table *table;

	table = net->ct.offload_sysctl_header->ctl_table_arg;
	unregister_net_sysctl_table(net->ct.offload_sysctl_header);
	kfree(table);
}
#else
static int nf_conntrack_offload_init_sysctl(struct net *net)
{
	return 0;
}

static void nf_conntrack_offload_fini_sysctl(struct net *net)
{
}
#endif

struct ct_iter_state {
	struct seq_net_private p;
	unsigned int bucket;
	u_int64_t time_now;
};

static struct hlist_nulls_node *ct_get_first(struct seq_file *seq)
{
	struct net *net = seq_file_net(seq);
	struct ct_iter_state *st = seq->private;
	struct hlist_nulls_node *n;

	for (st->bucket = 0;
	     st->bucket < net->ct.htable_size;
	     st->bucket++) {
		n = rcu_dereference(
		   hlist_nulls_first_rcu(&net->ct.hash[st->bucket]));
		if (!is_a_nulls(n))
			return n;
	}
	return NULL;
}

static struct hlist_nulls_node *ct_get_next(struct seq_file *seq,
				      struct hlist_nulls_node *head)
{
	struct net *net = seq_file_net(seq);
	struct ct_iter_state *st = seq->private;

	head = rcu_dereference(hlist_nulls_next_rcu(head));
	while (is_a_nulls(head)) {
		if (likely(get_nulls_value(head) == st->bucket)) {
			if (++st->bucket >= net->ct.htable_size)
				return NULL;
		}
		head = rcu_dereference(
				hlist_nulls_first_rcu(
					&net->ct.hash[st->bucket]));
	}
	return head;
}

static struct hlist_nulls_node *ct_get_idx(struct seq_file *seq, loff_t pos)
{
	struct hlist_nulls_node *head = ct_get_first(seq);

	if (head)
		while (pos && (head = ct_get_next(seq, head)))
			pos--;
	return pos ? NULL : head;
}

static void *ct_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(RCU)
{
	struct ct_iter_state *st = seq->private;

	st->time_now = ktime_to_ns(ktime_get_real());
	rcu_read_lock();
	return ct_get_idx(seq, *pos);
}

static void *ct_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos)++;
	return ct_get_next(s, v);
}

static void ct_seq_stop(struct seq_file *s, void *v)
	__releases(RCU)
{
	rcu_read_unlock();
}

/* return 0 on success, 1 in case of error */
static int ct_seq_show(struct seq_file *s, void *v)
{
	struct nf_conntrack_tuple_hash *hash = v;
	struct nf_conn *ct = nf_ct_tuplehash_to_ctrack(hash);
	const struct nf_conntrack_l3proto *l3proto;
	const struct nf_conntrack_l4proto *l4proto;
	struct nf_conn_offload *ct_offload = nf_conn_offload_find(ct);
	struct net_device *in = NULL;
	struct net_device *out = NULL;
	struct timeval tv;
	int ret = 0;

	NF_CT_ASSERT(ct);
	if (unlikely(!atomic_inc_not_zero(&ct->ct_general.use)))
		return 0;

	/* we only want to print DIR_ORIGINAL and flow is offloaded */
	if (NF_CT_DIRECTION(hash) ||
	    (ct_offload && !ct_offload_orig.flow_id && !ct_offload_repl.flow_id))
		goto release;

	if (ct_offload->update_stats)
		ct_offload->update_stats(ct);

	l3proto = __nf_ct_l3proto_find(nf_ct_l3num(ct));
	NF_CT_ASSERT(l3proto);
	l4proto = __nf_ct_l4proto_find(nf_ct_l3num(ct), nf_ct_protonum(ct));
	NF_CT_ASSERT(l4proto);

	if (seq_printf(s, "%-8s %u %-8s %u %ld ",
		       l3proto->name, nf_ct_l3num(ct),
		       l4proto->name, nf_ct_protonum(ct),
		       timer_pending(&ct->timeout)
		       ? (long)(ct->timeout.expires - jiffies)/HZ : 0) != 0)
		goto release;

	if (!ct_offload_orig.flow_id)
		goto skip_orig;

	in = __dev_get_by_index(&init_net, ct_offload_orig.iif);
	out = __dev_get_by_index(&init_net, ct_offload_orig.oif);
	if (seq_printf(s,
		       "flow=%u type=%u|%u lag=%u src=%pM dst=%pM in=%s out=%s ",
		       ct_offload_orig.flow_id,
		       ct_offload_orig.flow_type>>16,
		       ct_offload_orig.flow_type&0xFFFF,
		       ct_offload_orig.lag,
		       ct_offload_orig.eh.h_source,
		       ct_offload_orig.eh.h_dest,
		       in ? in->name:"null",
		       out ? out->name:"null") != 0)
		goto release;

	if (ct_offload_orig.dscp_old != ct_offload_orig.dscp_new)
		if (seq_printf(s,
			       "dscp=%d->%d ",
			       ct_offload_orig.dscp_old,
			       ct_offload_orig.dscp_new));

skip_orig:
	if (l4proto->print_conntrack && l4proto->print_conntrack(s, ct))
		goto release;

	if (print_tuple(s, &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple,
			l3proto, l4proto))
		goto release;

	if (seq_print_acct(s, ct, IP_CT_DIR_ORIGINAL))
		goto release;

	if (!(test_bit(IPS_SEEN_REPLY_BIT, &ct->status)))
		if (seq_printf(s, "[UNREPLIED] "))
			goto release;

	if (!ct_offload_repl.flow_id)
		goto skip_reply;

	in = __dev_get_by_index(&init_net, ct_offload_repl.iif);
	out = __dev_get_by_index(&init_net, ct_offload_repl.oif);
	if (seq_printf(s,
		       "flow=%u type=%u|%u lag=%u src=%pM dst=%pM in=%s out=%s ",
		       ct_offload_repl.flow_id,
		       ct_offload_repl.flow_type>>16,
		       ct_offload_repl.flow_type&0xFFFF,
		       ct_offload_repl.lag,
		       ct_offload_repl.eh.h_source,
		       ct_offload_repl.eh.h_dest,
		       in ? in->name:"null",
		       out ? out->name:"null") != 0)
		goto release;

	if (ct_offload_repl.dscp_old != ct_offload_repl.dscp_new)
		if (seq_printf(s,
			       "dscp=%d->%d ",
			       ct_offload_repl.dscp_old,
			       ct_offload_repl.dscp_new));
skip_reply:
	if (print_tuple(s, &ct->tuplehash[IP_CT_DIR_REPLY].tuple,
			l3proto, l4proto))
		goto release;

	if (seq_print_acct(s, ct, IP_CT_DIR_REPLY))
		goto release;

	if (test_bit(IPS_ASSURED_BIT, &ct->status))
		if (seq_printf(s, "[ASSURED] "))
			goto release;

#if defined(CONFIG_NF_CONNTRACK_MARK)
	if (seq_printf(s, "mark=%u|%u ",
		       ct->mark>>16,
		       ct->mark&0xFFFF))
		goto release;
#endif

#ifdef CONFIG_NF_CONNTRACK_ZONES
	if (seq_printf(s, "zone=%u ", nf_ct_zone(ct)))
		goto release;
#endif

	jiffies_to_timeval(jiffies - ct_offload_orig.tstamp, &tv);
	if (seq_printf(s, "dur=%ld ", tv.tv_sec))
		goto release;
	if (seq_printf(s, "use=%u\n", atomic_read(&ct->ct_general.use)))
		goto release;

release:
	nf_ct_put(ct);
	return ret;
}

static const struct seq_operations ct_seq_ops = {
	.start = ct_seq_start,
	.next  = ct_seq_next,
	.stop  = ct_seq_stop,
	.show  = ct_seq_show
};

static int ct_open(struct inode *inode, struct file *file)
{
	return seq_open_net(inode, file, &ct_seq_ops,
			sizeof(struct ct_iter_state));
}

static const struct file_operations ct_file_ops = {
	.owner   = THIS_MODULE,
	.open    = ct_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release_net,
};

static int nf_conntrack_offload_init_proc(struct net *net)
{
	struct proc_dir_entry *pde;

	pde = proc_create("nf_conntrack_offload", 0440,
			  net->proc_net, &ct_file_ops);
	if (!pde)
		return -ENOMEM;
	return 0;
}

static void nf_conntrack_offload_fini_proc(struct net *net)
{
	remove_proc_entry("nf_conntrack_offload", net->proc_net);
}

int nf_conntrack_offload_pernet_init(struct net *net)
{
	int ret = 0;
	net->ct.sysctl_offload = nf_ct_offload;
	ret = nf_conntrack_offload_init_sysctl(net);
	if (ret < 0)
		return ret;

	ret = nf_conntrack_offload_init_proc(net);
	if (ret < 0)
		nf_conntrack_offload_fini_sysctl(net);

	return ret;
}

void nf_conntrack_offload_pernet_fini(struct net *net)
{
	nf_conntrack_offload_fini_proc(net);
	nf_conntrack_offload_fini_sysctl(net);
}

int nf_conntrack_offload_init(void)
{
	int ret = 0;

	ret = nf_ct_extend_register(&offload_extend);
	if (ret < 0) {
		pr_err(KERN_ERR "nf_ct_offload: Unable to register nf extension\n");
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL(nf_conntrack_offload_init);

void nf_conntrack_offload_fini(void)
{
	nf_ct_extend_unregister(&offload_extend);
}
EXPORT_SYMBOL(nf_conntrack_offload_fini);
