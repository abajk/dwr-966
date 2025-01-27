
#include <linux/types.h>
#include <linux/netfilter.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/stddef.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/jhash.h>
#include <linux/err.h>
#include <linux/percpu.h>
#include <linux/moduleparam.h>
#include <linux/notifier.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/socket.h>
#include <linux/mm.h>
#include <linux/nsproxy.h>
#include <linux/rculist_nulls.h>

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_session_limit.h>
#include <net/netfilter/nf_conntrack_extend.h>
#include <net/netfilter/nf_conntrack_acct.h>
#include <net/netfilter/nf_conntrack_ecache.h>
#include <net/netfilter/nf_conntrack_zones.h>
#include <net/netfilter/nf_conntrack_timestamp.h>
#include <net/netfilter/nf_conntrack_timeout.h>
#include <net/netfilter/nf_conntrack_labels.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_core.h>
#include <net/netfilter/nf_nat_helper.h>
#include <linux/netfilter_ipv4/ip_tables.h>

int nf_conntrack_low_prio_max __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_low_prio_max);
unsigned int nf_conntrack_low_prio_thresh __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_low_prio_thresh);
int nf_conntrack_default_prio_max __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_default_prio_max);
unsigned int nf_conntrack_default_prio_thresh __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_default_prio_thresh);
unsigned int nf_conntrack_low_prio_data_rate __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_low_prio_data_rate);
unsigned int nf_conntrack_default_prio_data_rate __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_default_prio_data_rate);
unsigned int nf_conntrack_session_limit_enable __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_session_limit_enable);
unsigned int nf_conntrack_tcp_initial_offset __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_tcp_initial_offset);
unsigned int nf_conntrack_tcp_steady_offset __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_tcp_steady_offset);
    /*
     * This function derive session priority from skb->mark 
     */
__u32 skb_get_session_priority(struct sk_buff *skb)
{
#if defined(CONFIG_NETFILTER_XT_EXTMARK)
	return ((skb->extmark & CONNTRACK_SESSION_MASK) >> CONNTRACK_SESSION_START_BIT_POS);
#else
	return ((skb->mark & CONNTRACK_SESSION_MASK) >> CONNTRACK_SESSION_START_BIT_POS);
#endif
}

__u32 ct_get_session_priority(struct nf_conn *ct)
{
#if defined(CONFIG_NF_CONNTRACK_EXTMARK)
	return ((ct->extmark & CONNTRACK_SESSION_MASK) >> CONNTRACK_SESSION_START_BIT_POS);
#else
	return ((ct->mark & CONNTRACK_SESSION_MASK) >> CONNTRACK_SESSION_START_BIT_POS);
#endif
}
EXPORT_SYMBOL(skb_get_session_priority);
EXPORT_SYMBOL(ct_get_session_priority);

int nf_ct_check_destroy_conntrack(struct nf_conn *ct)
{

       struct net *net = nf_ct_net(ct);
       __u32 session_prio = 0;

       /* Conntrack Classification needs to be done before we can do the rest */
       session_prio = ct_get_session_priority(ct);
       if( session_prio == 0 ) {
		session_prio = nf_conntrack_default_prio_thresh;
       }

       /*
        * To handle torrents, we want to only be able to add higher priority
        * sessions once nf_conntrack_low_prio_max threshold has been reached
        * We need the connection  classified (in xt_connmark module) to
        * determine their priority.
        * So, we take the first opportunity to destroy the existing conntrack
        * after classification to get rid off "low priority" connections if 
        * the the threshold is breached !
        */

       if ((  nf_conntrack_default_prio_max && 
		session_prio <= nf_conntrack_default_prio_thresh && 
		atomic_read(&net->ct.count) > nf_conntrack_default_prio_max ) || 
		(nf_conntrack_low_prio_max && 
		session_prio <= nf_conntrack_low_prio_thresh && 
		atomic_read(&net->ct.count) > nf_conntrack_low_prio_max)) {
                       /*
                        * Need to destroy the conntrack
                        * We could call destroy_conntrack(), OTOH, its much
                        * simpler to follow the established order and do a
                        * NF_DROP - the stack takes care
                        */
               if (net_ratelimit())
                       printk(KERN_WARNING
                              "nf_conntrack: table near full, dropping"
                              " low/default priority connection.\n");
	       destroy_conntrack(ct);
               return NF_DROP;
       }
       return !NF_DROP;
}

EXPORT_SYMBOL(nf_ct_check_destroy_conntrack);



/*
     * This function checks if the packet (and its conntrack) is above the
     * configured low/default priority threshold if the number of conntracks exceeds
     * the configured max thresh, nf_conntrack_low/default_prio_max, and rejects
     * such conntracks. It helps to keep conntrack headroom for important
     * sessions esp in torrent infested network!
     *     --Ritesh
*/

unsigned int
nf_ct_check_reject_low_prio_conntrack(
       struct net *net,
       unsigned int hook,
       struct sk_buff *skb,
       int ret
       )
{
       __u32 session_prio = 0;
       if (ret == NF_DROP) {
               goto lbl_ret;
       }

       if (!skb->nfct) {
               //DEBUGP("(%s) No conntrack for this pkt ! ...\n",__func__);
               goto lbl_ret;
       }

       if (hook != NF_INET_PRE_ROUTING) {
               goto lbl_ret;
       }
       /*
        * Check if session is CONFIRMED, then skip
        */
       if (nf_ct_is_confirmed((struct nf_conn *)skb->nfct)) {
               goto lbl_ret;
       }
       /*
        * TODO: Exclude local sessions! Note, WAN side initiated static NAT
        * sessions will get considered as local sessions, but this is ok
        * from a torrent point of view
        */
       /* Packet Classification needs to be done before we can do the rest 
        * We treat unmarked packet as default priority packet
	*/
       session_prio = skb_get_session_priority(skb);
	/* printk("session_prio=%d\n", session_prio); */
       if( session_prio == 0 ) {
		session_prio = nf_conntrack_default_prio_thresh;
		/* printk("session_prio=0 making this to %d\n", session_prio); */
       }
       /*
        * To handle torrents, we want to only be able to add higher priority
        * sessions once nf_conntrack_low_prio_max threshold has been reached
        * Unfortunately, we can't add the check in nf_conntrack_alloc() since
        * that is called before the "classify" routines are called in the hook
        * call in PREROUTING at mangle table - we need the packets classified
        * to determine their priority, and its too early to prevent conntrack
        * creation in alloc(). So, we take the first opportunity after
        * classification to get rid off "low priority" connections if the the
        * threshold is breached !
        *
        * Currently, we ignore the local traffic by design - most CPE traffic
        * sessions would be desirable ones!
        *
        */

       if ((  nf_conntrack_default_prio_max && 
		session_prio <= nf_conntrack_default_prio_thresh && 
		atomic_read(&net->ct.count) > nf_conntrack_default_prio_max ) || 
		(nf_conntrack_low_prio_max && 
		session_prio <= nf_conntrack_low_prio_thresh && 
		atomic_read(&net->ct.count) > nf_conntrack_low_prio_max)) {
                       /*
                        * Need to destroy the conntrack
                        * We could call destroy_conntrack(), OTOH, its much
                        * simpler to follow the established order and do a
                        * NF_DROP - the stack takes care
                        */
		/*printk("nct=%d, session_prio =%d, dthr=%d, dmax=%d, lthre=%d, lmax=%d\n",
			atomic_read(&net->ct.count), session_prio, nf_conntrack_default_prio_thresh, 
			nf_conntrack_default_prio_max, nf_conntrack_low_prio_thresh, nf_conntrack_low_prio_max); */
               if (net_ratelimit())
                       printk(KERN_WARNING
                              "nf_conntrack: table near full, dropping"
                              " low/default priority packet.\n");
               return NF_DROP;
       }
       lbl_ret:

       return ret;
}
EXPORT_SYMBOL(nf_ct_check_reject_low_prio_conntrack);

unsigned int do_session_limit(unsigned int hook,struct sk_buff *skb,const struct net_device *in,const struct net_device *out){
	if (nf_conntrack_session_limit_enable == 1 ){
               int ret = 0;
               ret = ipt_do_table(skb, hook, in, out,
				dev_net(in)->ipv4.iptable_mangle);
               return (nf_ct_check_reject_low_prio_conntrack(dev_net(in),hook, skb, ret));
	} else {
		return(ipt_do_table(skb, hook, in, out,dev_net(in)->ipv4.iptable_mangle));
	}
}
EXPORT_SYMBOL(do_session_limit);
