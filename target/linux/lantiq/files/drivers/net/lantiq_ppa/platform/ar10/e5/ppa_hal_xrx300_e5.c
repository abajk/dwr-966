/******************************************************************************
**
** FILE NAME    : ppa_hal_ar10_e5.c
** PROJECT      : UEIP
** MODULES      : PTM + MII0/1 Acceleration Package (AR9 PPA E5)
**
** DATE         : 20 DEC 2010
** AUTHOR       : Xu Liang
** DESCRIPTION  : PTM + MII0/1 Driver with Acceleration Firmware (E5)
** COPYRIGHT    :              Copyright (c) 2009
**                          Lantiq Deutschland GmbH
**                   Am Campeon 3; 85579 Neubiberg, Germany
**
**   For licensing information, see the file 'LICENSE' in the root folder of
**   this software module.
**
** HISTORY
** $Date        $Author         $Comment
** 20 DEC 2010  Xu Liang        Initiate Version
*******************************************************************************/



/*!
  \defgroup AMAZON_S_PPA_PPE_E5_HAL Amazon-S (AR9) PPA PPE E5 HAL layer driver module
  \brief Amazon-S (AR9) PPA PPE E5 HAL layer driver module
 */

/*!
  \defgroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS Compile Parametere
  \ingroup AMAZON_S_PPA_PPE_E5_HAL
  \brief compile options to turn on/off some feature
 */

/*!
  \defgroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS Exported Functions
  \ingroup AMAZON_S_PPA_PPE_E5_HAL
  \brief exported functions for other driver use
 */

/*!
  \file ifxmips_ppa_hal_ar9_e5.c
  \ingroup AMAZON_S_PPA_PPE_E5_HAL
  \brief Amazon-S (AR9) PPA PPE E5 HAL layer driver file
 */

/*
 * ####################################
 *              Head File
 * ####################################
 */

/*
 *  Common Head File
 */
//#include <linux/autoconf.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
#include <linux/autoconf.h>
#else
#include <generated/autoconf.h>
#endif

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <net/checksum.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 13)
#include <net/ipip.h>
#else
#include <lantiq.h>
#include <lantiq_soc.h>
#include <linux/clk.h>
#include <net/ip_tunnels.h>
#endif

#include <linux/if_arp.h>
#include <linux/in.h>
#include <asm/uaccess.h>
#include <net/ip6_tunnel.h>
#include <net/ipv6.h>

/*
 *  Chip Specific Head File
 */
#include <net/ppa_api.h>
#include <net/ppa_ppe_hal.h>
#include "../../ppa_datapath.h"
#include "ppa_hal_xrx300_e5.h"

/*
 * ####################################
 *              Definition
 * ####################################
 */

/*
 * ####################################
 *              Version No.
 * ####################################
 */
#define VER_FAMILY      0x80    //  bit 0: res
                                //      1: Danube
                                //      2: Twinpass
                                //      3: Amazon-SE
                                //      4: res
                                //      5: AR9
                                //      6: VR9
                                //      7: AR10
#define VER_DRTYPE      0x04    //  bit 0: Normal Data Path driver
                                //      1: Indirect-Fast Path driver
                                //      2: HAL driver
                                //      3: Hook driver
                                //      4: Stack/System Adaption Layer driver
                                //      5: PPA API driver
#define VER_INTERFACE   0x0B    //  bit 0: MII 0
                                //      1: MII 1
                                //      2: ATM WAN
                                //      3: PTM WAN
#define VER_ACCMODE     0x03    //  bit 0: Routing
                                //      1: Bridging
#define VER_MAJOR       0
#define VER_MID         0
#define VER_MINOR       1
#define SESSION_MIB_MULTIPLEXER  32

/*
 *  Compilation Switch
 */

#define ENABLE_NEW_HASH_ALG                     1

/*!
  \addtogroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
 */
/*@{*/
/*!
  \brief Turn on/off debugging message and disable inline optimization.
 */
#define ENABLE_DEBUG                            1

/*!
  \brief Turn on/off ASSERT feature, print message while condition is not fulfilled.
 */
#define ENABLE_ASSERT                           1
/*@}*/

#if defined(ENABLE_DEBUG) && ENABLE_DEBUG
  #define ENABLE_DEBUG_PRINT                    1
  #define DISABLE_INLINE                        1
#endif

#if defined(DISABLE_INLINE) && DISABLE_INLINE
  #define INLINE
#else
  #define INLINE                                inline
#endif

#if defined(ENABLE_DEBUG_PRINT) && ENABLE_DEBUG_PRINT
  #undef  dbg
  static unsigned int ppa_hal_dbg_enable = 0;
  #define dbg(format, arg...)                   do { if ( ppa_hal_dbg_enable  ) printk(KERN_WARNING ":%d:%s: " format "\n", __LINE__, __FUNCTION__, ##arg); } while ( 0 )
#else
  #if !defined(dbg)
    #define dbg(format, arg...)
  #endif
#endif

#if defined(ENABLE_ASSERT) && ENABLE_ASSERT
  #define ASSERT(cond, format, arg...)          do { if ( !(cond) ) printk(KERN_ERR __FILE__ ":%d:%s: " format "\n", __LINE__, __FUNCTION__, ##arg); } while ( 0 )
#else
  #define ASSERT(cond, format, arg...)
#endif



/*
 * ####################################
 *             Declaration
 * ####################################
 */
static void add_tunnel_6rd_header(struct net_device *dev, volatile struct iphdr *iph);

#ifdef CONFIG_PPA_QOS
    extern u32 cgu_get_pp32_clock(void);
    static int32_t get_basic_time_tick(void);
    static uint32_t qos_queue_portid = 1;  //QOS note: At present A5 Ethernet WAN mode and D5's portid is 1, and E5's port id is 11
    #define PPE_MAX_ETH1_QOS_QUEUE 8
   #ifdef CONFIG_PPA_QOS_WFQ
        #define PPA_DRV_QOS_WFQ_WLEVEL_2_W  ( 200 )
        static uint32_t wfq_multiple = PPA_DRV_QOS_WFQ_WLEVEL_2_W;
        static uint32_t wfq_strict_pri_weight = 0x7FFFFF;
        int32_t get_qos_wfq( uint32_t portid, uint32_t queueid, uint32_t *weight_level, uint32_t flag);
    #endif

    #ifdef CONFIG_PPA_QOS_RATE_SHAPING
        uint32_t  default_qos_rateshaping_burst = 6000;
        int32_t get_qos_rate( uint32_t portid, uint32_t queueid, uint32_t *rate, uint32_t *burst, uint32_t flag);
    #endif
    static uint32_t set_qos_port_id(void)
    {
        uint32_t res = PPA_SUCCESS;

        uint32_t wan_itf = *CFG_WAN_PORTMAP;
        uint32_t mixed_mode = *CFG_MIXED_PORTMAP;

        if( mixed_mode ) //not support QOS in mixed mode
        {
            qos_queue_portid = -1; //invalid
            res = PPA_FAILURE;
        }

        if ( wan_itf & PPA_DEST_LIST_ETH0 ) qos_queue_portid = PPA_WAN_QOS_ETH0;
        else if ( wan_itf & PPA_DEST_LIST_ETH1 ) qos_queue_portid = PPA_WAN_QOS_ETH1;
        else if ( wan_itf & PPA_DEST_LIST_PTM ) qos_queue_portid = PPA_WAN_QOS_PTM0;
        else
        {
            qos_queue_portid = -1; //invalid
            res = PPA_FAILURE;
        }

        return res;
    }

#endif



/*
 * ####################################
 *           Global Variable
 * ####################################
 */

//const static int dest_list_map[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x00, 0x00, 0x80};

static uint16_t g_ipv6_ip_counter[MAX_IPV6_IP_ENTRIES] = {0};
static PPE_LOCK g_ipv6_ip_lock;

static uint16_t g_lan_routing_entry_occupation[(MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK + 64 + BITSIZEOF_UINT16 - 1) / BITSIZEOF_UINT16] = {0};
static uint32_t g_lan_collision_routing_entries = 0;
static PPE_LOCK g_lan_routing_lock;
static uint16_t g_wan_routing_entry_occupation[(MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK + 64 + BITSIZEOF_UINT16 - 1) / BITSIZEOF_UINT16] = {0};
static uint32_t g_wan_collision_routing_entries = 0;
static PPE_LOCK g_wan_routing_lock;

static uint32_t g_wan_mc_entry_occupation[(MAX_WAN_MC_ENTRIES + BITSIZEOF_UINT32 - 1) / BITSIZEOF_UINT32] = {0};
static uint32_t g_wan_mc_entries = 0;
static PPE_LOCK g_wan_mc_lock;

#if defined(CAP_WAP_CONFIG) && CAP_WAP_CONFIG
static uint8_t g_capwap_entry_occupation[(MAX_CAPWAP_ENTRIES + BITSIZEOF_UINT8 - 1) / BITSIZEOF_UINT8] = {0};
static uint8_t g_capwap_entries = 5;
static PPE_LOCK g_capwap_lock;
#endif

static uint32_t g_pppoe_entry_counter[MAX_PPPOE_ENTRIES] = {0};
static PPE_LOCK g_pppoe_lock;

static uint32_t g_mtu_entry_counter[MAX_MTU_ENTRIES] = {0};
static PPE_LOCK g_mtu_lock;

static uint32_t g_mac_entry_counter[MAX_MAC_ENTRIES] = {0};
static PPE_LOCK g_mac_lock;

static uint32_t g_outer_vlan_entry_counter[MAX_OUTER_VLAN_ENTRIES] = {0};
static PPE_LOCK g_outer_vlan_lock;

static uint32_t g_classification_entry_counter[MAX_CLASSIFICATION_ENTRIES] = {0};
static PPE_LOCK g_classification_lock;

static uint32_t g_6rd_tunnel_counter[MAX_6RD_TUNNEL_ENTRIES] = {0};
static PPE_LOCK g_6rd_tunnel_lock;
static uint32_t g_dslite_tunnel_counter[MAX_DSLITE_TUNNEL_ENTRIES] = {0};
static PPE_LOCK g_dslite_tunnel_lock;

static PPE_LOCK             g_mac_tbl_lock;
static struct mac_tbl_item *g_mac_tbl_hash[BRIDGING_SESSION_LIST_HASH_TABLE_SIZE] = {0};
static PPA_MEM_CACHE       *g_mac_tbl_item_cache = NULL;

static int g_ipv6_acc_en = 0;
static unsigned int g_ipv4_lan_collision_rout_fwdc_table_base;
static unsigned int g_ipv4_wan_collision_rout_fwda_table_base;

/*
 * ####################################
 *           Extern Functions
 * ####################################
 */

extern int32_t (*ppa_drv_datapath_mac_entry_setting)(uint8_t  *mac, uint32_t fid, uint32_t portid, uint32_t agetime, uint32_t st_entry , uint32_t action) ;
static int32_t mac_entry_setting(uint8_t  *mac, uint32_t fid, uint32_t portid, uint32_t agetime, uint32_t st_entry , uint32_t action)
{
    if( !ppa_drv_datapath_mac_entry_setting ) return PPA_FAILURE;

    return ppa_drv_datapath_mac_entry_setting(mac, fid, portid, agetime, st_entry, action );
}



/*
 * ####################################
 *           Extern Variable
 * ####################################
 */



/*
 * ####################################
 *            Local Function
 * ####################################
 */

#ifdef CONFIG_PPA_QOS_PROC
static int proc_read_qos(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    int len_max = off + count;
    char *pstr;
    char str[200];
    int llen;
    int i;
    struct wtx_eg_q_shaping_cfg qos_cfg;
    struct wtx_qos_q_mib_table qos_queue_mib;
    volatile struct tx_qos_cfg tx_qos_cfg = *TX_QOS_CFG;
    volatile struct wtx_qos_q_desc_cfg qos_q_desc_cfg[PPE_MAX_ETH1_QOS_QUEUE];

    pstr = *start = page;

    __sync();

    if ( set_qos_port_id() != PPA_SUCCESS )
    {
        llen = sprintf(pstr, "\n  Note: PPA QOS is disabled for wan_itf=%x  mixed_itf=%x\n", *CFG_WAN_PORTMAP, *CFG_MIXED_PORTMAP);
        pstr += llen;
        len += llen;

        *eof = 1;
        return len - off;
    }

    llen = sprintf(pstr, "\n  qos          : %s\n  wfq          : %s\n  Rate shaping : %s\n\n",
                    tx_qos_cfg.eth1_qss ?"enabled":"disabled",
                    tx_qos_cfg.wfq_en?"enabled":"disabled",
                    tx_qos_cfg.shape_en ?"enabled":"disabled");
    pstr += llen;
    len += llen;

    llen = sprintf(pstr, "  Ticks  =%u,    overhd    =%u,       qnum=%u  @%p\n", (u32)tx_qos_cfg.time_tick, (u32)tx_qos_cfg.overhd_bytes, (u32)tx_qos_cfg.eth1_eg_qnum, TX_QOS_CFG );
    pstr += llen;
    len += llen;

    llen = sprintf(pstr, "  PPE clk=%u MHz, basic tick=%u\n",(u32)cgu_get_pp32_clock()/1000000, (u32)get_basic_time_tick());
    pstr += llen;
    len += llen;
#ifdef CONFIG_PPA_QOS_WFQ
    llen = sprintf(pstr, "\n  wfq_multiple : %08u @0x%p", wfq_multiple, &wfq_multiple );
    pstr += llen;
    len += llen;

    llen = sprintf(pstr, "\n  strict_weight: %08u @0x%p\n", wfq_strict_pri_weight, &wfq_strict_pri_weight );
    pstr += llen;
    len += llen;

#endif

    if ( tx_qos_cfg.eth1_eg_qnum )
    {
        uint32_t bit_rate_kbps=0;
        uint32_t weight_level=0;

        llen = sprintf(pstr, "\n  Cfg :    T     R     S -->  Bit-rate(kbps)      Weight --> Level       Address       d/w      tick_cnt   b/S\n");
        pstr += llen;
        len += llen;
        for ( i = 0; i < PPE_MAX_ETH1_QOS_QUEUE /*tx_qos_cfg.eth1_eg_qnum*/; i++ )
        {
            qos_cfg = *WTX_EG_Q_SHAPING_CFG(i);
#ifdef CONFIG_PPA_QOS_RATE_SHAPING
            get_qos_rate( qos_queue_portid, i, &bit_rate_kbps, NULL,0);
#endif
#ifdef CONFIG_PPA_QOS_WFQ
            get_qos_wfq( qos_queue_portid, i, &weight_level, 0);
#endif

            llen = sprintf(str, "\n      %2u:  %03u  %05u  %05u   %07u            %08u   %03u        @0x%p   %08u    %03u     %05u\n", i, qos_cfg.t, qos_cfg.r, qos_cfg.s, bit_rate_kbps, qos_cfg.w, weight_level, WTX_EG_Q_SHAPING_CFG(i), qos_cfg.d, qos_cfg.tick_cnt, qos_cfg.b);
            if ( len <= off && len + llen > off )
            {
                ppa_memcpy(pstr, str + off - len, len + llen - off);
                pstr += len + llen - off;
            }
            else if ( len > off )
            {
                ppa_memcpy(pstr, str, llen);
                pstr += llen;
            }
            len += llen;
            if ( len >= len_max )
                goto PROC_READ_MAC_OVERRUN_END;
        }

        //QOS Note: For ethernat wan mode only. For E5 ptm mode, it is not necessary since there is no port based rate shaping
        if( 1 )
        {
            qos_cfg = *WTX_EG_Q_PORT_SHAPING_CFG(0);
#ifdef CONFIG_PPA_QOS_RATE_SHAPING
            get_qos_rate( qos_queue_portid, i, &bit_rate_kbps, NULL,0);
#endif

            llen = sprintf(str, "\n    port:  %03u  %05u  %05u   %07u                                  @0x%p   %08u    %03u     %05u\n", qos_cfg.t, qos_cfg.r, qos_cfg.s, bit_rate_kbps, WTX_EG_Q_PORT_SHAPING_CFG(0), qos_cfg.d, qos_cfg.tick_cnt, qos_cfg.b);
            if ( len <= off && len + llen > off )
            {
                ppa_memcpy(pstr, str + off - len, len + llen - off);
                pstr += len + llen - off;
            }
            else if ( len > off )
            {
                ppa_memcpy(pstr, str, llen);
                pstr += llen;
            }
            len += llen;
            if ( len >= len_max )
                goto PROC_READ_MAC_OVERRUN_END;
        }
        //QOS Note: For ethernat wan mode only. For E5 ptm mode, it is not necessary since there is no port based rate shaping --End

        llen = sprintf(pstr, "\n  MIB : rx_pkt/rx_bytes         tx_pkt/tx_bytes       cpu_small_drop/cpu_drop  fast_small_drop/fast_drop_cnt\n");
        pstr += llen;
        len += llen;
        for ( i = 0; i < PPE_MAX_ETH1_QOS_QUEUE /*tx_qos_cfg.eth1_eg_qnum*/; i++ )
        {
            qos_queue_mib = *WTX_QOS_Q_MIB_TABLE(i);

            llen = sprintf(str, "    %2u: %010u/%010u  %010u/%010u  %010u/%010u  %010u/%010u  @0x%p\n", i,
                qos_queue_mib.wrx_total_pdu, qos_queue_mib.wrx_total_bytes,
                qos_queue_mib.wtx_total_pdu, qos_queue_mib.wtx_total_bytes,
                qos_queue_mib.wtx_cpu_drop_small_pdu, qos_queue_mib.wtx_cpu_drop_pdu,
                qos_queue_mib.wtx_fast_drop_small_pdu, qos_queue_mib.wtx_fast_drop_pdu,
                WTX_QOS_Q_MIB_TABLE(i));

            if ( len <= off && len + llen > off )
            {
                ppa_memcpy(pstr, str + off - len, len + llen - off);
                pstr += len + llen - off;
            }
            else if ( len > off )
            {
                ppa_memcpy(pstr, str, llen);
                pstr += llen;
            }
            len += llen;
            if ( len >= len_max )
                goto PROC_READ_MAC_OVERRUN_END;
        }


        //QOS queue descriptor
        llen = sprintf(pstr, "\n  Desc: threshold  num    base_addr  rd_ptr   wr_ptr\n");
        pstr += llen;
        len += llen;
        for(i=0; i<PPE_MAX_ETH1_QOS_QUEUE /*tx_qos_cfg.eth1_eg_qnum*/; i++)
        {
            qos_q_desc_cfg[i] = *WTX_QOS_Q_DESC_CFG(i);

            llen = sprintf(pstr, "    %2u: 0x%02x       0x%02x   0x%04x     0x%04x   0x%04x  @0x%p\n", i,
                qos_q_desc_cfg[i].threshold,
                qos_q_desc_cfg[i].length,
                qos_q_desc_cfg[i].addr,
                qos_q_desc_cfg[i].rd_ptr,
                qos_q_desc_cfg[i].wr_ptr,
                WTX_QOS_Q_DESC_CFG(i) );

            pstr += llen;
            len += llen;
        }

        llen = sprintf(pstr, "\n  wan_itf=%x  mixed_itf=%x\n", *CFG_WAN_PORTMAP, *CFG_MIXED_PORTMAP);
        pstr += llen;
        len += llen;
    }


    *eof = 1;

    return len - off;

PROC_READ_MAC_OVERRUN_END:
    return len - llen - off;
}
#endif

/*
 * ####################################
 *           Global Function
 * ####################################
 */

void get_ppe_hal_id(uint32_t *p_family,
                    uint32_t *p_type,
                    uint32_t *p_if,
                    uint32_t *p_mode,
                    uint32_t *p_major,
                    uint32_t *p_mid,
                    uint32_t *p_minor)
{
    if ( p_family )
        *p_family = VER_FAMILY;

    if ( p_type )
        *p_type = VER_DRTYPE;

    if ( p_if )
        *p_if = VER_INTERFACE;

    if ( p_mode )
        *p_mode = VER_ACCMODE;

    if ( p_major )
        *p_major = VER_MAJOR;

    if ( p_mid )
        *p_mid = VER_MID;

    if ( p_minor )
        *p_minor = VER_MINOR;
}

/*!
  \fn uint32_t get_firmware_id(uint32_t *p_family,
                     uint32_t *p_type,
                     uint32_t *p_major,
                     uint32_t *p_mid,
                     uint32_t *p_minor)
  \ingroup AMAZON_S_PPA_PPE_D5_HAL_GLOBAL_FUNCTIONS
  \brief read firmware ID
  \param p_family   get family code
  \param p_type     get firmware type
  \param p_major    get major number
  \param p_mid       get middle number
  \param p_minor    get minor number
  \return no return value
 */
uint32_t get_firmware_id(uint32_t pp32_index,
                     uint32_t *p_family,
                     uint32_t *p_type,
                     uint32_t *p_major,
                     uint32_t *p_mid,
                     uint32_t *p_minor)
{
    register struct fw_ver_id id;

    if( pp32_index != 0 ) return PPA_FAILURE;

    id = *FW_VER_ID;

    if ( p_family )
        *p_family = id.family;

    if ( p_type )
        *p_type = id.package;

    if ( p_major )
        *p_major = id.major;

    if ( p_mid )
        *p_mid = id.middle;

    if ( p_minor )
        *p_minor = id.minor;
    return PPA_SUCCESS;
}


uint32_t get_number_of_phys_port(void)
{
    return 8;
}

void get_phys_port_info(uint32_t port,
                        uint32_t *p_flags,
                        PPA_IFNAME ifname[PPA_IF_NAME_SIZE])
{
    char *str_ifname[] = {
        "eth0",
        "eth1",
        "",
        "",
        "",
        "",
        "",
        "ptm0"
    };
/*
    uint32_t flags[] = {
        0,  //GEN_MODE_CFG1->sys_cfg == 2 ? PPA_PHYS_PORT_FLAGS_MODE_ETH_MIX_VALID : PPA_PHYS_PORT_FLAGS_MODE_ETH_LAN_VALID,
        0,  //  real flags for port 1 could be found below
        PPA_PHYS_PORT_FLAGS_MODE_CPU_VALID,
        PPA_PHYS_PORT_FLAGS_MODE_EXT_LAN_VALID | PPA_PHYS_PORT_FLAGS_EXT_CPU0,
        PPA_PHYS_PORT_FLAGS_MODE_EXT_LAN_VALID | PPA_PHYS_PORT_FLAGS_EXT_CPU0,
        PPA_PHYS_PORT_FLAGS_MODE_EXT_LAN_VALID | PPA_PHYS_PORT_FLAGS_EXT_CPU0 | PPA_PHYS_PORT_FLAGS_EXT_CPU1,
        PPA_PHYS_PORT_FLAGS_MODE_EXT_LAN_VALID | PPA_PHYS_PORT_FLAGS_EXT_CPU0,
        GEN_MODE_CFG1->sys_cfg == 0 ? PPA_PHYS_PORT_FLAGS_MODE_ETH_WAN_VALID : 0
    };
 */

    if ( port >= sizeof(str_ifname) / sizeof(*str_ifname) )
    {
        if ( p_flags )
            *p_flags = 0;
        if ( ifname )
            *ifname = 0;
        return;
    }
/*
    if ( p_flags )
    {
        *p_flags = flags[port];
        if ( port == 1 )
            switch ( GEN_MODE_CFG1->sys_cfg )
            {
            case 0: *p_flags = PPA_PHYS_PORT_FLAGS_MODE_ETH_LAN_VALID; break;
            case 3: *p_flags = PPA_PHYS_PORT_FLAGS_MODE_ETH_WAN_VALID; break;
            }
        if ( (GEN_MODE_CFG2->itf_outer_vlan_vld & (1 << port))
            && (flags[port] & PPA_PHYS_PORT_FLAGS_TYPE_MASK) != PPA_PHYS_PORT_FLAGS_TYPE_ATM )
            *p_flags |= PPA_PHYS_PORT_FLAGS_OUTER_VLAN;
    }
 */
    if(p_flags)
    {
        *p_flags = 0;
        switch(port)
        {
            case 0: /*eth0*/
            case 1: /*eth1*/
                if(*CFG_WAN_PORTMAP & (1 << port)){//ethx wan
                    if(*CFG_MIXED_PORTMAP & (1 << port)){//mix mode
                        *p_flags = PPA_PHYS_PORT_FLAGS_MODE_ETH_MIX_VALID;
                    }else{
                        *p_flags = PPA_PHYS_PORT_FLAGS_MODE_ETH_WAN_VALID;
                    }
                }else{
                    *p_flags = PPA_PHYS_PORT_FLAGS_MODE_ETH_LAN_VALID;
                }
                break;

            case 2: /*CPU port*/
                *p_flags = PPA_PHYS_PORT_FLAGS_MODE_CPU_VALID;
                break;

            case 3: /*EXT port*/
            case 4:
            case 5:
            //case 6:
                   /*
                                   * Note:
                                   *        Disable port6 for directpath @E5 because we only alloc 3 unit in directpath array 
                                   *        Allow the 4th directpath will potentially cause A5/E5 switch error 
                                  */
                if(*CFG_WAN_PORTMAP & ( 1 << port)){
                    *p_flags = PPA_PHYS_PORT_FLAGS_MODE_EXT_WAN_VALID | PPA_PHYS_PORT_FLAGS_EXT_CPU0;
                }else{
                    *p_flags = PPA_PHYS_PORT_FLAGS_MODE_EXT_LAN_VALID | PPA_PHYS_PORT_FLAGS_EXT_CPU0;
                }
                if(port == 5){
                    *p_flags |= PPA_PHYS_PORT_FLAGS_EXT_CPU1;
                }
                break;

            case 7:
                if(*CFG_WAN_PORTMAP & (1 << port)){
                    *p_flags = PPA_PHYS_PORT_FLAGS_MODE_ETH_WAN_VALID;
                }
                break;

            default:
                *p_flags = 0;
                break;
        }

        if ( (GEN_MODE_CFG2->itf_outer_vlan_vld & (1 << port))
            && (*p_flags & PPA_PHYS_PORT_FLAGS_TYPE_MASK) != PPA_PHYS_PORT_FLAGS_TYPE_ATM )
            *p_flags |= PPA_PHYS_PORT_FLAGS_OUTER_VLAN;
    }

    if ( ifname )
        strcpy(ifname, str_ifname[port]);
}

/*!
  \fn void get_max_route_entries(uint32_t *p_entry,
                           uint32_t *p_mc_entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief get maximum number of routing entries
  \param p_entry    get maximum number of uni-cast routing entries. In Amazon-S (AR9) A5, either LAN side or WAN side has 8 hash entries, as well as 64 collision routing entries. In each hash entry, there are 16 routing entries.
  \param p_mc_entry get maximum number of multicast routing entries. In Amazon-S (AR9) A5, there are 64 entries.
  \return no return value
 */
void get_max_route_entries(uint32_t *p_entry,
                           uint32_t *p_mc_entry)
{
    if ( p_entry )
        *p_entry = MAX_ROUTING_ENTRIES;

    if ( p_mc_entry )
        *p_mc_entry = MAX_WAN_MC_ENTRIES;
}

void get_max_bridging_entries(uint32_t *p_entry)
{
    if ( p_entry )
        *p_entry = MAX_BRIDGING_ENTRIES;    //  tricky to help bridging over ATM
}


/*!
  \fn void set_wan_vlan_id(uint32_t vlan_id)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief set VLAN ID range for WAN side
  \param vlan_id    least significant 16 bits is lower bound of WAN side VLAN ID (12-bit),
                    most significant 16 bits is upper bound of WAN side VLAN ID (12-bit)
  \return no return value
 */
void set_wan_vlan_id(uint32_t vlan_id)
{
    MIXED_WAN_VLAN_CFG->wan_vlanid_lo = vlan_id & ((1 << 12) - 1);
    MIXED_WAN_VLAN_CFG->wan_vlanid_hi = (vlan_id >> 16) & ((1 << 12) - 1);
}

/*!
  \fn uint32_t get_wan_vlan_id(void)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief get VLAN ID range for WAN side
  \return least significant 16 bits is lower bound of WAN side VLAN ID,
          most significant 16 bits is upper bound of WAN side VLAN ID
 */
uint32_t get_wan_vlan_id(void)
{
    return (MIXED_WAN_VLAN_CFG->wan_vlanid_hi << 16) | MIXED_WAN_VLAN_CFG->wan_vlanid_lo;
}

/*!
  \fn void set_if_type(uint32_t if_type,
                 uint32_t if_no)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief set interface type (LAN, WAN, or Mix mode)
  \param if_type    1: LAN, 2: WAN, 3: Mix
  \param if_no      interface no. - 0: eth0, 1: eth1
  \return no return value
 */
//  if_type:
//    bit 0: LAN
//    bit 1: WAN
void set_if_type(uint32_t if_type,
                 uint32_t if_no)
{
    uint8_t if_type_template[4] = {2, 0, 1, 2}; //  0: LAN, 1: WAN, 2: MIX (new spec)

    if ( if_no == 0 )
        MIXED_WAN_VLAN_CFG->eth0_type = if_type_template[if_type];
    else if ( if_no == 1 )
        MIXED_WAN_VLAN_CFG->eth1_type = if_type_template[if_type];
}

/*!
  \fn uint32_t get_if_type(uint32_t if_no)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief get interface type (LAN, WAN, or Mix mode)
  \param if_no      interface no. - 0: eth0
  \return 0: reserved, 1: LAN, 2: WAN, 3: Mix
 */
uint32_t get_if_type(uint32_t if_no)
{
    uint32_t if_type_template[4] = {PPA_IF_TYPE_LAN, PPA_IF_TYPE_WAN, PPA_IF_TYPE_MIX, PPA_IF_NOT_FOUND};

    if ( if_no == 0 )
        return if_type_template[MIXED_WAN_VLAN_CFG->eth0_type];
    else if ( if_no == 1 )
        return if_type_template[MIXED_WAN_VLAN_CFG->eth1_type];
    else
        return PPA_IF_NOT_FOUND;
}

/*!
  \fn void set_route_cfg(uint32_t f_is_lan,
                   uint32_t entry_num,
                   uint32_t mc_entry_num,
                   uint32_t f_ip_verify,
                   uint32_t f_tcpudp_verify,
                   uint32_t f_iptcpudp_err_drop,
                   uint32_t f_drop_on_no_hit,
                   uint32_t f_mc_drop_on_no_hit,
                   uint32_t flags)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief setup routing table configuration
  \param f_is_lan               setup LAN side routing table configuration
  \param entry_num              maximum number of LAN/WAN side uni-cast routing entries (min 512 max 512 + 64)
  \param mc_entry_num           maximum number of WAN side multicast routing entries (max 64)
  \param f_ip_verify            turn on/off IP checksum verification
  \param f_tcpudp_verify        turn on/off TCP/UDP checksum verification
  \param f_iptcpudp_err_drop    drop/not drop if IP/TCP/UDP checksum is wrong
  \param f_drop_on_no_hit       drop/not drop if uni-cast packet does not match any entry
  \param f_mc_drop_on_no_hit    drop/not drop if multicast packet does not match any entry
  \param flags                  bit 0: entry_num is valid,
                                bit 1: mc_entry_num is valid,
                                bit 2: f_ip_verify is valid,
                                bit 3: f_tcpudp_verify is valid,
                                bit 4: f_tcpudp_err_drop is valid,
                                bit 5: f_drop_on_no_hit is valid,
                                bit 6: f_mc_drop_on_no_hit is valid
  \return no return value
 */
//  flags
//    bit 0: entry_num is valid
//    bit 1: mc_entry_num is valid
//    bit 2: f_ip_verify is valid
//    bit 3: f_tcpudp_verify is valid
//    bit 4: f_tcpudp_err_drop is valid
//    bit 5: f_drop_on_no_hit is valid
//    bit 6: f_mc_drop_on_no_hit is valid
void set_route_cfg(uint32_t f_is_lan,
                   uint32_t entry_num,      //  routing entries, include both hash entries and collision entries, min 128 max 128 + 64
                   uint32_t mc_entry_num,   //  max 64, reserved in LAN route table config
                   uint32_t f_ip_verify,
                   uint32_t f_tcpudp_verify,
                   uint32_t f_iptcpudp_err_drop,
                   uint32_t f_drop_on_no_hit,
                   uint32_t f_mc_drop_on_no_hit,    //  reserved in LAN route table config
                   uint32_t flags)
{
    //  LAN route config is only a little different
    struct rout_tbl_cfg cfg;
    uint16_t *p_occupation;

    if ( entry_num <= MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK )
        entry_num = MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK + 1;
    else if ( entry_num > MAX_WAN_ROUTING_ENTRIES )
        entry_num = MAX_WAN_ROUTING_ENTRIES;

    if ( mc_entry_num < 1 )
        mc_entry_num = 1;
    else if ( mc_entry_num > MAX_WAN_MC_ENTRIES )
        mc_entry_num = MAX_WAN_MC_ENTRIES;

    cfg = f_is_lan ? *LAN_ROUT_TBL_CFG : *WAN_ROUT_TBL_CFG;

    if ( (flags & PPA_SET_ROUTE_CFG_ENTRY_NUM) )
    {
        cfg.rout_num = entry_num - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK;

        if ( f_is_lan )
        {
            g_lan_collision_routing_entries = cfg.rout_num;
            p_occupation = g_lan_routing_entry_occupation;
        }
        else
        {
            g_wan_collision_routing_entries = cfg.rout_num;
            p_occupation = g_wan_routing_entry_occupation;
        }

        ppa_memset(p_occupation, 0, (entry_num + BITSIZEOF_UINT16 - 1)  / BITSIZEOF_UINT16 * sizeof(uint16_t));
        if ( entry_num % BITSIZEOF_UINT16 )
        {
            p_occupation += entry_num / BITSIZEOF_UINT16;
            *p_occupation = ~0 ^ ((1 << (entry_num % BITSIZEOF_UINT16)) - 1);
        }
    }

    if ( !f_is_lan && (flags & PPA_SET_ROUTE_CFG_MC_ENTRY_NUM) )
    {
        g_wan_mc_entries = mc_entry_num;
        cfg.wan_rout_mc_num = mc_entry_num;

        if ( mc_entry_num % BITSIZEOF_UINT32 )
            g_wan_mc_entry_occupation[mc_entry_num / BITSIZEOF_UINT32] = ~0 ^ ((1 << (mc_entry_num % BITSIZEOF_UINT32)) - 1);
    }

    if ( (flags & PPA_SET_ROUTE_CFG_IP_VERIFY) )
        cfg.ip_ver_en = f_ip_verify ? 1 : 0;

    if ( (flags & PPA_SET_ROUTE_CFG_TCPUDP_VERIFY) )
        cfg.tcpdup_ver_en = f_tcpudp_verify ? 1 : 0;

    if ( (flags & PPA_SET_ROUTE_CFG_TCPUDP_ERR_DROP) )
        cfg.iptcpudperr_drop = f_iptcpudp_err_drop ? 1 : 0;

    if ( (flags & PPA_SET_ROUTE_CFG_DROP_ON_NOT_HIT) )
        cfg.rout_drop = f_drop_on_no_hit ? 1 : 0;

    if ( !f_is_lan && (flags & PPA_SET_ROUTE_CFG_MC_DROP_ON_NOT_HIT) )
        cfg.wan_rout_mc_drop = f_mc_drop_on_no_hit ? 1 : 0;

    if ( f_is_lan )
        *LAN_ROUT_TBL_CFG = cfg;
    else
        *WAN_ROUT_TBL_CFG = cfg;
}

void set_bridging_cfg(uint32_t entry_num,
                      uint32_t br_to_src_port_mask, uint32_t br_to_src_port_en,
                      uint32_t f_dest_vlan_en,
                      uint32_t f_src_vlan_en,
                      uint32_t f_mac_change_drop,
                      uint32_t flags)
{
}

/*!
  \fn void set_fast_mode(uint32_t mode,
                   uint32_t flags)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief set fastpath mode for interfaces
  \param mode   bit 0 - app2 (1 direct, 0 indirect), bit 1 - eth1 (1 direct, 0 indirect)
  \param flags  bit 0 - app2 field in mode is valid, bit 1 - eth1 field in mode is valid
  \return no return value
 */
void set_fast_mode(uint32_t mode,
                   uint32_t flags)
{
    if ( (flags & PPA_SET_FAST_MODE_APP2) )
        GEN_MODE_CFG->app2_indirect = (mode & PPA_SET_FAST_MODE_APP2_DIRECT) ? 0 : 1;

    if ( (flags & PPA_SET_FAST_MODE_ETH1) )
        GEN_MODE_CFG->us_indirect = (mode & PPA_SET_FAST_MODE_ETH1_DIRECT) ? 0 : 1;

//  TODO
//    reconfig_dma_channel(GEN_MODE_CFG->cpu1_fast_mode && GEN_MODE_CFG->wan_fast_mode);
}

/*!
  \fn void set_if_wfq(uint32_t if_wfq,
                uint32_t if_no)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief obsolete in A5
  \return no return value
 */
void set_if_wfq(uint32_t if_wfq,
                uint32_t if_no)
{
}

/*!
  \fn void set_dplus_wfq(uint32_t wfq)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief obsolete in A5
  \return no return value
 */
void set_dplus_wfq(uint32_t wfq)
{
}

/*!
  \fn void set_fastpath_wfq(uint32_t wfq)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief obsolete in A5
  \return no return value
 */
void set_fastpath_wfq(uint32_t wfq)
{
}

/*!
  \fn void get_acc_mode(uint32_t f_is_lan,
                  uint32_t *p_acc_mode)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief get acceleration mode for interfaces (LAN/WAN)
  \param f_is_lan       0: WAN interface, 1: LAN interface
  \param p_acc_mode     a u32 data pointer to get acceleration mode (PPA_ACC_MODE_ROUTING / PPA_ACC_MODE_NONE)
  \return no return value
 */
void get_acc_mode(uint32_t f_is_lan,
                  uint32_t *p_acc_mode)
{
    if ( p_acc_mode )
        *p_acc_mode = (f_is_lan ? GEN_MODE_CFG->lan_acc_en : GEN_MODE_CFG->wan_acc_en) ? PPA_ACC_MODE_ROUTING: PPA_ACC_MODE_NONE;
}

/*!
  \fn void set_acc_mode(uint32_t f_is_lan,
                  uint32_t acc_mode)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief set acceleration mode for interfaces (LAN/WAN)
  \param f_is_lan       0: WAN interface, 1: LAN interface
  \param p_acc_mode     acceleration mode (PPA_ACC_MODE_ROUTING / PPA_ACC_MODE_NONE)
  \return no return value
 */
//  acc_mode:
//    0: no acceleration
//    2: routing acceleration
void set_acc_mode(uint32_t f_is_lan,
                  uint32_t acc_mode)
{
    if ( f_is_lan )
        GEN_MODE_CFG->lan_acc_en = (acc_mode & PPA_ACC_MODE_ROUTING) ? 1 : 0;
    else
        GEN_MODE_CFG->wan_acc_en = (acc_mode & PPA_ACC_MODE_ROUTING) ? 1 : 0;
}

#if defined(MIB_MODE_ENABLE) && MIB_MODE_ENABLE
/*!
  \fn void set_mib_mode(uint8_t mib_mode)
  \ingroup AMAZON_S_PPA_PPE_D5_HAL_GLOBAL_FUNCTIONS
  \brief set Unicast/multicast session mib mode 
  \param mib_mob        0: Session Mib in terms of Byte, 1: Session Mib interms of Packet
  \return no return value
 */
void set_mib_mode(uint8_t mib_mode)
{
        PS_MC_GENCFG3->session_mib_unit = mib_mode ? 1 : 0;
}

/*!
  \fn void get_mib_mode(uint8_t mib_mode)
  \ingroup AMAZON_S_PPA_PPE_D5_HAL_GLOBAL_FUNCTIONS
  \brief get Unicast/multicast session mib mode 
  \param mib_mob        0: Session Mib in terms of Byte, 1: Session Mib interms of Packet
  \return no return value
 */
void get_mib_mode(uint8_t *mib_mode)
{
        *mib_mode = PS_MC_GENCFG3->session_mib_unit;
}


#endif

/*!
  \fn void set_default_dest_list(uint32_t uc_dest_list,
                           uint32_t mc_dest_list,
                           uint32_t if_no)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief obsolete in A5
  \return no return value
 */
void set_default_dest_list(uint32_t uc_dest_list,
                           uint32_t mc_dest_list,
                           uint32_t if_no)
{
}

void set_bridge_if_vlan_config(uint32_t if_no,
                               uint32_t f_eg_vlan_insert,
                               uint32_t f_eg_vlan_remove,
                               uint32_t f_ig_vlan_aware,
                               uint32_t f_ig_src_ip_based,
                               uint32_t f_ig_eth_type_based,
                               uint32_t f_ig_vlanid_based,
                               uint32_t f_ig_port_based,
                               uint32_t f_eg_out_vlan_insert,
                               uint32_t f_eg_out_vlan_remove,
                               uint32_t f_ig_out_vlan_aware)
{
}

void get_bridge_if_vlan_config(uint32_t if_no,
                               uint32_t *f_eg_vlan_insert,
                               uint32_t *f_eg_vlan_remove,
                               uint32_t *f_ig_vlan_aware,
                               uint32_t *f_ig_src_ip_based,
                               uint32_t *f_ig_eth_type_based,
                               uint32_t *f_ig_vlanid_based,
                               uint32_t *f_ig_port_based,
                               uint32_t *f_eg_out_vlan_insert,
                               uint32_t *f_eg_out_vlan_remove,
                               uint32_t *f_ig_out_vlan_aware)
{
}

int32_t add_vlan_map(uint32_t ig_criteria_type,
                     uint32_t ig_criteria,
                     uint32_t new_vci,
                     uint32_t dest_qos,
                     uint32_t outer_vlan_ix,
                     uint32_t in_out_etag_ctrl,
                     uint32_t vlan_port_map)
{
    return PPA_EPERM;
}

void del_vlan_map(uint32_t ig_criteria_type,
                  uint32_t ig_criteria)
{
}

int32_t get_vlan_map(uint32_t ig_criteria_type,
                     uint32_t entry,
                     uint32_t *ig_criteria,
                     uint32_t *new_vci,
                     uint32_t *dest_qos,
                     uint32_t *outer_vlan_ix,
                     uint32_t *in_out_etag_ctrl,
                     uint32_t *vlan_port_map)
{
    return PPA_ENOTAVAIL;
}

void del_all_vlan_map(void)
{
}

int32_t is_ipv6_enabled(void)
{
    return GEN_MODE_CFG1->ipv6_acc_en ? 1 : 0;
}

int32_t add_ipv6_ip_entry(uint32_t ipv6_ip[4],
                          uint32_t *p_entry)
{
    int32_t ret = 0;
    int32_t entry = -1, empty_entry = -1;
    int x, i;

    if ( GEN_MODE_CFG1->ipv6_acc_en == 0 )
        return -1;

    ASSERT(p_entry != NULL, "p_entry == NULL");

    ppe_lock_get(&g_ipv6_ip_lock);

    for ( i = 0; entry < 0 && i < MAX_IPV6_IP_ENTRIES_PER_BLOCK; i++ )
        for ( x = 0; entry < 0 && x < MAX_IPV6_IP_ENTRIES_BLOCK; x++ )
        {
            if ( g_ipv6_ip_counter[x * MAX_IPV6_IP_ENTRIES_PER_BLOCK + i] )
            {
                if ( ppa_memcmp((void *)IPv6_IP_IDX_TBL(x, i), ipv6_ip, 16) == 0 )
                    entry = x * MAX_IPV6_IP_ENTRIES_PER_BLOCK + i;
            }
            else if ( empty_entry < 0 )
                empty_entry = x * MAX_IPV6_IP_ENTRIES_PER_BLOCK + i;
        }

    if ( entry >= 0 )
    {
        g_ipv6_ip_counter[entry]++;
        *p_entry = entry;
    }
    else if ( empty_entry >= 0 )
    {
        ppa_memcpy((void *)IPv6_IP_IDX_TBL(empty_entry / MAX_IPV6_IP_ENTRIES_PER_BLOCK, empty_entry % MAX_IPV6_IP_ENTRIES_PER_BLOCK), ipv6_ip, 16);
        g_ipv6_ip_counter[empty_entry]++;
        *p_entry = empty_entry;

        i = empty_entry % MAX_IPV6_IP_ENTRIES_PER_BLOCK;
        if ( i >= GEN_MODE_CFG1->ipv6_rout_num )
            GEN_MODE_CFG1->ipv6_rout_num = i + 1;
    }
    else
        ret = -1;

    ppe_lock_release(&g_ipv6_ip_lock);

    return ret;
}

void del_ipv6_ip_entry(uint32_t entry)
{
    int x = entry / MAX_IPV6_IP_ENTRIES_PER_BLOCK;
    int i = entry % MAX_IPV6_IP_ENTRIES_PER_BLOCK;

    if ( GEN_MODE_CFG1->ipv6_acc_en != 0 && x < MAX_IPV6_IP_ENTRIES_BLOCK )
    {
        ppe_lock_get(&g_ipv6_ip_lock);
        if ( g_ipv6_ip_counter[entry] && !--g_ipv6_ip_counter[entry] )
            ppa_memset((void *)IPv6_IP_IDX_TBL(x, i), 0, 16);

        for ( i = GEN_MODE_CFG1->ipv6_rout_num - 1; i >= 0; i-- )
            for ( x = 0; x < MAX_IPV6_IP_ENTRIES_BLOCK; x++ )
            {
                if ( IPv6_IP_IDX_TBL(x, i)[0] == 0 && IPv6_IP_IDX_TBL(x, i)[1] == 0 && IPv6_IP_IDX_TBL(x, i)[2] == 0 && IPv6_IP_IDX_TBL(x, i)[3] == 0 )
                    continue;

                GEN_MODE_CFG1->ipv6_rout_num = i > 1 ? i + 1 : 2;
                goto DEL_IPV6_IP_ENTRY_QUIT;
            }

DEL_IPV6_IP_ENTRY_QUIT:
        ppe_lock_release(&g_ipv6_ip_lock);
    }
}

int32_t get_ipv6_ip_entry(uint32_t entry,
                          uint32_t ipv6_ip[4])
{
    int x = entry / MAX_IPV6_IP_ENTRIES_PER_BLOCK;
    int i = entry % MAX_IPV6_IP_ENTRIES_PER_BLOCK;

    if ( GEN_MODE_CFG1->ipv6_acc_en == 0 || x >= MAX_IPV6_IP_ENTRIES_BLOCK )
        return -1;

    ppe_lock_get(&g_ipv6_ip_lock);

    if ( !g_ipv6_ip_counter[entry] )
    {
        ppe_lock_release(&g_ipv6_ip_lock);
        return -1;
    }

    ASSERT(ipv6_ip != NULL, "ipv6_ip == NULL");
    ppa_memcpy(ipv6_ip, (void *)IPv6_IP_IDX_TBL(x, i), 16);

    ppe_lock_release(&g_ipv6_ip_lock);

    return 0;
}

static inline int32_t get_hash(PPE_SESSION_HASH *hash_info)
{
    uint32_t hash;
        
#if defined(ENABLE_NEW_HASH_ALG) && ENABLE_NEW_HASH_ALG
    hash = ((unsigned int)hash_info->src_port << 16) | (hash_info->dst_port & 0xFFFF);
    hash = hash_info->src_ip ^ hash_info->dst_ip ^ hash;
    hash = (((hash >> 30) & 0x03) ^ (hash >> 27) ^ (hash >> 24) ^ (hash >> 21)
            ^ (hash >> 18) ^ (hash >> 15) ^ (hash >> 12) ^ (hash >> 9)
            ^ (hash >> 6) ^ (hash >> 3) ^ hash) & 0x07;
#endif

    if ( hash_info->f_is_lan )
    {       
#if !defined(ENABLE_NEW_HASH_ALG) || !ENABLE_NEW_HASH_ALG
        //  A5
        hash = ((hash_info->src_ip >> 2) & 0x07)
             ^ (((hash_info->src_ip & 0x03) << 1) | ((hash_info->src_port >> 15) & 0x01))
             ^ ((hash_info->src_port >> 12) & 0x07)
             ^ ((hash_info->src_port >> 9) & 0x07)
             ^ ((hash_info->src_port >> 6) & 0x07)
             ^ ((hash_info->src_port >> 3) & 0x07)
             ^ (hash_info->src_port & 0x07);

        //  D5/E5
        //hash = (((src_ip & 0x0F) << 1) | ((src_port >> 15) & 0x01))
        //     ^ ((src_port >> 10) & 0x1F)
        //     ^ ((src_port >> 5) & 0x1F)
        //     ^ (src_port & 0x1F);
#endif
             hash_info->hash_table_id=0;
    }
    else
    {       
        if ( GEN_MODE_CFG->wan_hash_alg )
        {
#if !defined(ENABLE_NEW_HASH_ALG) || !ENABLE_NEW_HASH_ALG
            //  A5
            hash = ((hash_info->dst_ip >> 2) & 0x07)
                 ^ (((hash_info->dst_ip & 0x03) << 1) | ((hash_info->dst_port >> 15) & 0x01))
                 ^ ((hash_info->dst_port >> 12) & 0x07)
                 ^ ((hash_info->dst_port >> 9) & 0x07)
                 ^ ((hash_info->dst_port >> 6) & 0x07)
                 ^ ((hash_info->dst_port >> 3) & 0x07)
                 ^ (hash_info->dst_port & 0x07);

            //  D5/E5
            //hash = (((hash_info->dst_ip & 0x0F) << 1) | ((hash_info->dst_port >> 15) & 0x01))
            //     ^ ((hash_info->dst_port >> 10) & 0x1F)
            //     ^ ((hash_info->dst_port >> 5) & 0x1F)
            //     ^ (hash_info->dst_port & 0x1F);
#endif
        }
        else
            hash = hash_info->dst_port & 0x07;
            hash_info->hash_table_id=1;
    }
    hash_info->hash_index = hash;
    
    return PPA_SUCCESS;
}

/*!
  \fn int32_t add_routing_entry(uint32_t f_is_lan,
                          uint32_t src_ip,
                          uint32_t src_port,
                          uint32_t dst_ip,
                          uint32_t dst_port,
                          uint32_t f_is_tcp,
                          uint32_t route_type,
                          uint32_t new_ip,
                          uint32_t new_port,
                          uint8_t  new_mac[PPA_ETH_ALEN],
                          uint32_t new_src_mac_ix,
                          uint32_t mtu_ix,
                          uint32_t f_new_dscp_enable,
                          uint32_t new_dscp,
                          uint32_t f_vlan_ins_enable,
                          uint32_t new_vci,
                          uint32_t f_vlan_rm_enable,
                          uint32_t pppoe_mode,
                          uint32_t pppoe_ix,
                          uint32_t f_out_vlan_ins_enable,
                          uint32_t out_vlan_ix,
                          uint32_t f_out_vlan_rm_enable,
                          uint32_t dslwan_qid,
                          uint32_t dest_list,
                          uint32_t *p_entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief add one routing entry
  \param src_ip                 source IP address
  \param src_port               source PORT number
  \param dst_ip                 destination IP address
  \param dst_port               destination PORT number
  \param f_is_tcp               0: UDP, 1: TCP
  \param route_type             0: no action, 1: IPv4, 2: NAT, 3:NAPT
  \param new_ip                 new IP address (valid in NAT and NAPT)
  \param new_port               new PORT number (valid in NAPT)
  \param new_mac                new destination MAC address
  \param new_src_mac_ix         index of new source MAC address
  \param mtu_ix                 index of Max Transmission Unit
  \param f_new_dscp_enable      replace DSCP value
  \param new_dscp               new DSCP value
  \param f_vlan_ins_enable      insert inner VLAN tag
  \param new_vci                new inner VLAN tag
  \param f_vlan_rm_enable       remove inner VLAN tag (if there is VLAN tag in incoming packet)
  \param pppoe_mode             PPPoE termination, LAN side add PPPoE header, WAN side remove PPPoE header
  \param pppoe_ix               index of PPPoE header, valid only in LAN side
  \param f_out_vlan_ins_enable  insert outer VLAN tag
  \param out_vlan_ix            index of new outer VLAN tag
  \param f_out_vlan_rm_enable   remove outer VLAN tag, this tag is used for PVCs differentiation of WAN side traffic
  \param dslwan_qid             destination WAN QID (PVC) or LAN QID (Switch Queue)
  \param dest_list              destination ports, bit 0: eth0, bit 1: eth1, bit 7: ATM
  \param p_entry                a data pointer to get entry number
  \return 0: OK, otherwise: fail
 */
int32_t add_routing_entry(uint32_t f_is_lan,
                          uint32_t src_ip,
                          uint32_t src_port,
                          uint32_t dst_ip,
                          uint32_t dst_port,
                          uint32_t f_is_tcp,    //  1: TCP, 0: UDP
                          uint32_t route_type,
                          uint32_t new_ip,
                          uint32_t new_port,
                          uint8_t  new_mac[PPA_ETH_ALEN],
                          uint32_t new_src_mac_ix,
                          uint32_t mtu_ix,
                          uint32_t f_new_dscp_enable,
                          uint32_t new_dscp,
                          uint32_t f_vlan_ins_enable,
                          uint32_t new_vci,
                          uint32_t f_vlan_rm_enable,
                          uint32_t pppoe_mode,
                          uint32_t pppoe_ix,
                          uint32_t f_out_vlan_ins_enable,
                          uint32_t out_vlan_ix,
                          uint32_t f_out_vlan_rm_enable,
                          uint32_t dslwan_qid,
                          uint32_t dest_list,
                          uint32_t tunnel_idx,
                          uint32_t *p_entry,
                          uint8_t *collion_flag)
{
    PPE_LOCK *p_lock;
    uint32_t entry;
    struct rout_forward_action_tbl action = {0};
    struct rout_forward_compare_tbl compare;
    volatile struct rout_forward_action_tbl *paction;
    volatile struct rout_forward_compare_tbl *pcompare;
    volatile u32 *phit;
    u32 hitbit;
    uint32_t hash;
    uint16_t occupation;
    uint32_t f_collision;
    uint16_t *p_occupation;
    uint32_t entries;
    uint32_t i;
    uint16_t bit;
    PPE_SESSION_HASH hash_info;

#if 0
    printk("add_routing_entry: \n");
    printk("  f_is_lan              = %d\n", f_is_lan);
    printk("  src_ip                = %d.%d.%d.%d\n", src_ip >> 24, (src_ip >> 16) & 0xFF, (src_ip >> 8) & 0xFF, src_ip & 0xFF);
    printk("  src_port              = %d\n", src_port);
    printk("  dst_ip                = %d.%d.%d.%d\n", dst_ip >> 24, (dst_ip >> 16) & 0xFF, (dst_ip >> 8) & 0xFF, dst_ip & 0xFF);
    printk("  dst_port              = %d\n", dst_port);
    printk("  f_is_tcp              = %d\n", f_is_tcp);
    printk("  route_type            = %d\n", route_type);
    printk("  new_ip                = %d.%d.%d.%d\n", new_ip >> 24, (new_ip >> 16) & 0xFF, (new_ip >> 8) & 0xFF, new_ip & 0xFF);
    printk("  new_port              = %d\n", new_port);
    printk("  new_mac               = %02x:%02x:%02x:%02x:%02x:%02x\n", (uint32_t)new_mac[0], (uint32_t)new_mac[1], (uint32_t)new_mac[2], (uint32_t)new_mac[3], (uint32_t)new_mac[4], (uint32_t)new_mac[5]);
    printk("  new_src_mac_ix        = %d (%02x:%02x:%02x:%02x:%02x:%02x)\n", new_src_mac_ix, ROUT_MAC_CFG_TBL(new_src_mac_ix)[0] >> 24, (ROUT_MAC_CFG_TBL(new_src_mac_ix)[0] >> 16) & 0xFF, (ROUT_MAC_CFG_TBL(new_src_mac_ix)[0] >> 8) & 0xFF, ROUT_MAC_CFG_TBL(new_src_mac_ix)[0] & 0xFF, (ROUT_MAC_CFG_TBL(new_src_mac_ix)[1] >> 24) & 0xFF, (ROUT_MAC_CFG_TBL(new_src_mac_ix)[1] >> 16) & 0xFF);
    printk("  mtu_ix                = %d (%d)\n", mtu_ix, *MTU_CFG_TBL(mtu_ix));
    printk("  f_new_dscp_enable     = %d\n", f_new_dscp_enable);
    printk("  new_dscp              = %d\n", new_dscp);
    printk("  f_vlan_ins_enable     = %d\n", f_vlan_ins_enable);
    printk("  new_vci               = %04x\n", new_vci);
    printk("  f_vlan_rm_enable      = %d\n", f_vlan_rm_enable);
    printk("  pppoe_mode            = %d\n", pppoe_mode);
    if ( f_is_lan )
        printk("  pppoe_ix              = %d (%d)\n", pppoe_ix, *PPPOE_CFG_TBL(pppoe_ix));
    else
        printk("  pppoe_ix              = %d\n", pppoe_ix);
    printk("  f_out_vlan_ins_enable = %d\n", f_out_vlan_ins_enable);
    printk("  out_vlan_ix           = %04x\n", out_vlan_ix);
    printk("  f_out_vlan_rm_enable  = %d\n", f_out_vlan_rm_enable);
    if ( f_is_lan )
        printk("  dslwan_qid            = %d (conn %d)\n", dslwan_qid & 0xFF, (dslwan_qid >> 8) & 0xFF);
    else
        printk("  dest_qid (dslwan_qid) = %d\n", dslwan_qid);
    printk("  dest_list             = %02X\n", dest_list);
    printk("  p_entry               = %08X\n", (uint32_t)p_entry);
#endif

    ////  dest_list remap
    //if ( !(dest_list & PPA_DEST_LIST_NO_REMAP) )
    //{
    //    uint32_t org_dest_list = dest_list;
    //
    //    dest_list = 0;
    //    for ( i = 0; org_dest_list && i < sizeof(dest_list_map) / sizeof(*dest_list_map); i++, org_dest_list >>= 1 )
    //    {
    //        if ( (org_dest_list & 0x01) )
    //            dest_list |= dest_list_map[i];
    //    }
    //}
    //else
    //    dest_list &= ~PPA_DEST_LIST_NO_REMAP;

    hash_info.f_is_lan = f_is_lan;
    hash_info.src_ip = src_ip;
    hash_info.src_port = src_port;
    hash_info.dst_ip = dst_ip;
    hash_info.dst_port = dst_port;
    get_hash( &hash_info);
    hash = hash_info.hash_index;

    if ( f_is_lan )
    {
        p_lock = &g_lan_routing_lock;
        p_occupation = g_lan_routing_entry_occupation;
        entries = g_lan_collision_routing_entries;
    }
    else
    {
        p_lock = &g_wan_routing_lock;
        p_occupation = g_wan_routing_entry_occupation;
        entries = g_wan_collision_routing_entries;        
    }

    action.new_port           = new_port;
    action.new_dest_mac54     = (((uint32_t)new_mac[0] & 0xFF) << 8) | ((uint32_t)new_mac[1] & 0xFF);
    action.new_dest_mac30     = (((uint32_t)new_mac[2] & 0xFF) << 24) | (((uint32_t)new_mac[3] & 0xFF) << 16) | (((uint32_t)new_mac[4] & 0xFF) << 8) | ((uint32_t)new_mac[5] & 0xFF);
    action.new_ip             = new_ip;
    action.rout_type          = route_type;
    action.new_dscp           = new_dscp;
    action.mtu_ix             = mtu_ix < MAX_MTU_ENTRIES ? mtu_ix : 0;
    action.in_vlan_ins        = f_vlan_ins_enable ? 1 : 0;
    action.in_vlan_rm         = f_vlan_rm_enable ? 1 : 0;
    action.new_dscp_en        = f_new_dscp_enable ? 1 : 0;
    action.protocol           = f_is_tcp ? 1 : 0;
    action.dest_list          = dest_list; 
    action.pppoe_mode         = pppoe_mode ? 1 : 0;
    if ( f_is_lan && pppoe_mode )
        action.pppoe_ix       = pppoe_ix < MAX_PPPOE_ENTRIES ? pppoe_ix : 0;
    action.new_src_mac_ix     = new_src_mac_ix < MAX_MAC_ENTRIES ? new_src_mac_ix : 0;
    action.new_in_vci         = f_vlan_ins_enable ? new_vci : 0;
    action.out_vlan_ix        = f_out_vlan_ins_enable ? out_vlan_ix : 0;
    action.out_vlan_ins       = f_out_vlan_ins_enable ? 1 : 0;
    action.out_vlan_rm        = f_out_vlan_rm_enable ? 1 : 0;
    if(tunnel_idx & 0x01){
        if(f_is_lan){
             action.encap_tunnel = 1;
             action.tnnl_hdr_idx = (tunnel_idx >> 1) & 0x03;
        }else{
             action.encap_tunnel = 1; //do decap actually, so don't need tunnel idx
        }
    }
    action.dest_qid           = dslwan_qid;

    compare.src_ip            = src_ip;
    compare.dest_ip           = dst_ip;
    compare.src_port          = src_port;
    compare.dest_port         = dst_port;

    ppe_lock_get(p_lock);
    occupation = p_occupation[hash];
    if ( occupation == (uint16_t)~0 )
    {
#if defined(CONFIG_LTQ_PPA_HANDLE_CONNTRACK_SESSIONS)
	if ( collion_flag && *collion_flag == 2 ) {
		*collion_flag = 0;
        	ppe_lock_release(p_lock);
	        return PPA_EAGAIN;
	}
	*collion_flag  = 0;	
#endif
        //  collision
        for ( i = 0; i < (entries + BITSIZEOF_UINT16 - 1) / BITSIZEOF_UINT16; i++ )
            if ( p_occupation[MAX_HASH_BLOCK + i] != (uint16_t)~0 )
                goto ADD_ROUTING_ENTRY_GOON;
        //  no empty entry
        ppe_lock_release(p_lock);
        return PPA_EAGAIN;

ADD_ROUTING_ENTRY_GOON:
        entry = MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK + i * BITSIZEOF_UINT16;
        bit = 1;
        while ( (p_occupation[MAX_HASH_BLOCK + i] & bit) )
        {
            bit <<= 1;
            entry++;
        }
        p_occupation[MAX_HASH_BLOCK + i] |= bit;
        f_collision = 1;
    }
    else
    {
        entry = hash * MAX_ROUTING_ENTRIES_PER_HASH_BLOCK;
        bit = 1;
        while ( (occupation & bit) )
        {
            bit <<= 1;
            entry++;
        }
        p_occupation[hash] |= bit;
        f_collision = 0;
    }

    if ( f_is_lan)
    {
        if ( f_collision )
        {
            pcompare = ROUT_LAN_COLL_CMP_TBL(entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK);
            paction  = ROUT_LAN_COLL_ACT_TBL(entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK);
            phit     = ROUT_LAN_COLL_HIT_STAT_TBL((entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK) >> 5);
            hitbit   = ~(1 << (BITSIZEOF_UINT32 - 1 - ((entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK) & 0x1F)));
        }
        else
        {
            pcompare = ROUT_LAN_HASH_CMP_TBL(entry);
            paction  = ROUT_LAN_HASH_ACT_TBL(entry);
            phit     = ROUT_LAN_HASH_HIT_STAT_TBL(entry >> 5);
            hitbit   = ~(1 << (BITSIZEOF_UINT32 - 1 - (entry & 0x1F)));
        }
    }
    else
    {
        if ( f_collision )
        {
            pcompare = ROUT_WAN_COLL_CMP_TBL(entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK);
            paction  = ROUT_WAN_COLL_ACT_TBL(entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK);
            phit     = ROUT_WAN_COLL_HIT_STAT_TBL((entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK) >> 5);
            hitbit   = ~(1 << (BITSIZEOF_UINT32 - 1 - ((entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK) & 0x1F)));
        }
        else
        {
            pcompare = ROUT_WAN_HASH_CMP_TBL(entry);
            paction  = ROUT_WAN_HASH_ACT_TBL(entry);
            phit     = ROUT_WAN_HASH_HIT_STAT_TBL(entry >> 5);
            hitbit   = ~(1 << (BITSIZEOF_UINT32 - 1 - (entry & 0x1F)));
        }
    }

    //  before enable this entry, clear hit status
    *pcompare = compare;
    *paction  = action;
    *phit &= hitbit;
    wmb();
    paction->entry_vld = 1;         //  enable this entry finally

    ppe_lock_release(p_lock);

    if ( f_is_lan )
        entry |= 0x80000000;    //  bit 31: 0 - WAN, 1 - LAN

    ASSERT(p_entry != NULL, "p_entry == NULL");
    *p_entry = entry;
    if( collion_flag ) *collion_flag = f_collision;

    return PPA_SUCCESS;
}

int32_t add_ipv6_routing_entry(uint32_t f_is_lan,
                               uint32_t src_ip[4],
                               uint32_t src_port,
                               uint32_t dst_ip[4],
                               uint32_t dst_port,
                               uint32_t f_is_tcp,    //  1: TCP, 0: UDP
                               uint32_t route_type,
                               uint32_t new_ip,
                               uint32_t new_port,
                               uint8_t  new_mac[PPA_ETH_ALEN],
                               uint32_t new_src_mac_ix,
                               uint32_t mtu_ix,
                               uint32_t f_new_dscp_enable,
                               uint32_t new_dscp,
                               uint32_t f_vlan_ins_enable,
                               uint32_t new_vci,
                               uint32_t f_vlan_rm_enable,
                               uint32_t pppoe_mode,
                               uint32_t pppoe_ix,
                               uint32_t f_out_vlan_ins_enable,
                               uint32_t out_vlan_ix,
                               uint32_t f_out_vlan_rm_enable,
                               uint32_t dslwan_qid,
                               uint32_t dest_list,
                               uint32_t tunnel_idx,
                               uint32_t *p_entry,
                               uint8_t *collion_flag)
{
    uint32_t src_ip_entry, dst_ip_entry, entry;
    int32_t ret = PPA_FAILURE;
    struct gen_mode_cfg1 gen_mode_cfg1=*GEN_MODE_CFG1;

    if( !gen_mode_cfg1.ipv6_acc_en  ) return ret;
    if ( add_ipv6_ip_entry(src_ip, &src_ip_entry) != 0 )
        goto ADD_SRC_IP_FAIL;

    if ( add_ipv6_ip_entry(dst_ip, &dst_ip_entry) != 0 )
        goto ADD_DST_IP_FAIL;

    if ( (ret = add_routing_entry(f_is_lan,
                           src_ip_entry | *PSEUDO_IPv4_BASE_ADDR,
                           src_port,
                           dst_ip_entry | *PSEUDO_IPv4_BASE_ADDR,
                           dst_port,
                           f_is_tcp,
                           route_type,
                           new_ip,
                           new_port,
                           new_mac,
                           new_src_mac_ix,
                           mtu_ix,
                           f_new_dscp_enable,
                           new_dscp,
                           f_vlan_ins_enable,
                           new_vci,
                           f_vlan_rm_enable,
                           pppoe_mode,
                           pppoe_ix,
                           f_out_vlan_ins_enable,
                           out_vlan_ix,
                           f_out_vlan_rm_enable,
                           dslwan_qid,
                           dest_list,
                           tunnel_idx,
                           &entry,
                           collion_flag)) != PPA_SUCCESS )
        goto ADD_ROUTING_ENTRY_FAIL;

    ASSERT(p_entry != NULL, "p_entry == NULL");
    *p_entry = entry;

    return ret;

ADD_ROUTING_ENTRY_FAIL:
    del_ipv6_ip_entry(dst_ip_entry);
ADD_DST_IP_FAIL:
    del_ipv6_ip_entry(src_ip_entry);
ADD_SRC_IP_FAIL:
    return ret;
}

/*!
  \fn void del_routing_entry(uint32_t entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief delete one routing entry
  \param entry  entry number got from function call "add_routing_entry"
  \return no return value
 */
void del_routing_entry(uint32_t entry)
{
    PPE_LOCK *p_lock;
    volatile struct rout_forward_action_tbl *paction;
    volatile struct rout_forward_compare_tbl *pcompare;
    uint16_t *p_occupation;
    uint32_t src_ip_entry = ~0, dst_ip_entry = ~0;

    if ( (entry & 0x80000000) )
    {
        p_lock = &g_lan_routing_lock;
        entry &= 0x7FFFFFFF;
        p_occupation = g_lan_routing_entry_occupation;
        if ( entry >= MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK )
        {
            pcompare = ROUT_LAN_COLL_CMP_TBL(entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK);
            paction  = ROUT_LAN_COLL_ACT_TBL(entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK);
        }
        else
        {
            pcompare = ROUT_LAN_HASH_CMP_TBL(entry);
            paction  = ROUT_LAN_HASH_ACT_TBL(entry);
        }
    }
    else
    {
        p_lock = &g_wan_routing_lock;
        p_occupation = g_wan_routing_entry_occupation;
        if ( entry >= MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK )
        {
            pcompare = ROUT_WAN_COLL_CMP_TBL(entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK);
            paction  = ROUT_WAN_COLL_ACT_TBL(entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK);
        }
        else
        {
            pcompare = ROUT_WAN_HASH_CMP_TBL(entry);
            paction  = ROUT_WAN_HASH_ACT_TBL(entry);
        }
    }

    ppe_lock_get(p_lock);
    if ( g_ipv6_acc_en != 0 )
    {
        src_ip_entry = pcompare->src_ip & 0xFF;
        dst_ip_entry = pcompare->dest_ip & 0xFF;
    }
    paction->entry_vld = 0;
    paction->dest_list = PPA_DEST_LIST_CPU0;
    ppa_memset((void *)pcompare, 0, sizeof(*pcompare));
    ppa_memset((void *)paction, 0, sizeof(*paction));
    p_occupation[entry >> 4] &= ~(1 << (entry & 0x0F));
    ppe_lock_release(p_lock);

    if ( g_ipv6_acc_en != 0 )
    {
        del_ipv6_ip_entry(src_ip_entry);
        del_ipv6_ip_entry(dst_ip_entry);
    }
}

/*!
  \fn int32_t update_routing_entry(uint32_t entry,
                             uint32_t route_type,
                             uint32_t new_ip,
                             uint32_t new_port,
                             uint8_t  new_mac[PPA_ETH_ALEN],
                             uint32_t new_src_mac_ix,
                             uint32_t mtu_ix,
                             uint32_t f_new_dscp_enable,
                             uint32_t new_dscp,
                             uint32_t f_vlan_ins_enable,
                             uint32_t new_vci,
                             uint32_t f_vlan_rm_enable,
                             uint32_t pppoe_mode,
                             uint32_t pppoe_ix,
                             uint32_t f_out_vlan_ins_enable,
                             uint32_t out_vlan_ix,
                             uint32_t f_out_vlan_rm_enable,
                             uint32_t dslwan_qid,
                             uint32_t dest_list,
                             uint32_t flags)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief update one routing entry
  \param entry                  entry number got from function call "add_routing_entry"
  \param route_type             0: no action, 1: IPv4, 2: NAT, 3:NAPT
  \param new_ip                 new IP address (valid in NAT and NAPT)
  \param new_port               new PORT number (valid in NAPT)
  \param new_mac                new destination MAC address
  \param new_src_mac_ix         index of new source MAC address
  \param mtu_ix                 index of Max Transmission Unit
  \param f_new_dscp_enable      replace DSCP value
  \param new_dscp               new DSCP value
  \param f_vlan_ins_enable      insert inner VLAN tag
  \param new_vci                new inner VLAN tag
  \param f_vlan_rm_enable       remove inner VLAN tag (if there is VLAN tag in incoming packet)
  \param pppoe_mode             PPPoE termination, LAN side add PPPoE header, WAN side remove PPPoE header
  \param pppoe_ix               index of PPPoE header, valid only in LAN side
  \param f_out_vlan_ins_enable  insert outer VLAN tag
  \param out_vlan_ix            index of new outer VLAN tag
  \param f_out_vlan_rm_enable   remove outer VLAN tag
  \param dslwan_qid             destination WAN QID (PVC) or LAN QID (Switch Queue)
  \param dest_list              destination ports, bit 0: eth0, bit 1: eth1, bit 7: ATM
  \param flags                  mask of the fields to be updated
  \return 0: OK, otherwise: fail
 */
int32_t update_routing_entry(uint32_t entry,
                             uint32_t route_type,
                             uint32_t new_ip,
                             uint32_t new_port,
                             uint8_t  new_mac[PPA_ETH_ALEN],
                             uint32_t new_src_mac_ix,
                             uint32_t mtu_ix,
                             uint32_t f_new_dscp_enable,
                             uint32_t new_dscp,
                             uint32_t f_vlan_ins_enable,
                             uint32_t new_vci,
                             uint32_t f_vlan_rm_enable,
                             uint32_t pppoe_mode,
                             uint32_t pppoe_ix,
                             uint32_t f_out_vlan_ins_enable,
                             uint32_t out_vlan_ix,
                             uint32_t f_out_vlan_rm_enable,
                             uint32_t dslwan_qid,
                             uint32_t dest_list,
                             uint32_t flags)
{
    PPE_LOCK *p_lock;
    uint16_t *p_occupation;
    volatile struct rout_forward_action_tbl *paction;
    struct rout_forward_action_tbl action;

    if ( (entry & 0x80000000) )
    {
        //  LAN
        p_lock = &g_lan_routing_lock;
        entry &= 0x7FFFFFFF;
        p_occupation = g_lan_routing_entry_occupation;
        if ( entry >= MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK )
            paction  = ROUT_LAN_COLL_ACT_TBL(entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK);
        else
            paction  = ROUT_LAN_HASH_ACT_TBL(entry);
    }
    else
    {
        //  WAN
        p_lock = &g_wan_routing_lock;
        p_occupation = g_wan_routing_entry_occupation;
        if ( entry >= MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK )
            paction  = ROUT_WAN_COLL_ACT_TBL(entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK);
        else
            paction  = ROUT_WAN_HASH_ACT_TBL(entry);
    }

    ppe_lock_get(p_lock);

    if ( !(p_occupation[entry >> 4] & (1 << (entry & 0x0F))) )
    {
        ppe_lock_release(p_lock);
        return PPA_EINVAL;
    }

    action = *paction;

    //  disable this entry
    paction->entry_vld = 0;
    paction->dest_list = PPA_DEST_LIST_CPU0;

    //  if dest_chid is not update, keep it
    if ( !(flags & PPA_UPDATE_ROUTING_ENTRY_DEST_LIST) )
        dest_list = action.dest_list;
    //else
    //{
    //    if ( !(dest_list & PPA_DEST_LIST_NO_REMAP) )
    //    {
    //        uint32_t org_dest_list = dest_list;
    //        int i;
    //
    //        dest_list = 0;
    //        for ( i = 0; org_dest_list && i < sizeof(dest_list_map) / sizeof(*dest_list_map); i++, org_dest_list >>= 1 )
    //            if ( (org_dest_list & 0x01) )
    //                dest_list |= dest_list_map[i];
    //    }
    //    else
    //        dest_list &= ~PPA_DEST_LIST_NO_REMAP;
    //}
    action.dest_list = PPA_DEST_LIST_CPU0;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_NEW_PORT) )
        action.new_port           = new_port;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_NEW_MAC) )
    {
        action.new_dest_mac54     = (((uint32_t)new_mac[0] & 0xFF) << 8) | ((uint32_t)new_mac[1] & 0xFF);
        action.new_dest_mac30     = (((uint32_t)new_mac[2] & 0xFF) << 24) | (((uint32_t)new_mac[3] & 0xFF) << 16) | (((uint32_t)new_mac[4] & 0xFF) << 8) | ((uint32_t)new_mac[5] & 0xFF);
    }

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_NEW_IP) )
        action.new_ip             = new_ip;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_ROUTE_TYPE) )
        action.rout_type          = route_type;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_NEW_DSCP) )
        action.new_dscp           = new_dscp;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_MTU_IX) )
        action.mtu_ix             = mtu_ix < MAX_MTU_ENTRIES ? mtu_ix : 0;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_VLAN_INS_EN) )
        action.in_vlan_ins        = f_vlan_ins_enable ? 1 : 0;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_VLAN_RM_EN) )
        action.in_vlan_rm         = f_vlan_rm_enable ? 1 : 0;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_NEW_DSCP_EN) )
        action.new_dscp_en        = f_new_dscp_enable ? 1 : 0;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_PPPOE_MODE) )
        action.pppoe_mode         = pppoe_mode ? 1 : 0;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_OUT_VLAN_INS_EN) )
        action.out_vlan_ins       = f_out_vlan_ins_enable ? 1 : 0;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_OUT_VLAN_RM_EN) )
        action.out_vlan_rm        = f_out_vlan_rm_enable ? 1 : 0;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_PPPOE_IX) )
        action.pppoe_ix           = action.pppoe_mode && pppoe_ix < MAX_PPPOE_ENTRIES ? pppoe_ix : 0;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_NEW_SRC_MAC_IX) )
        action.new_src_mac_ix     = new_src_mac_ix < MAX_MAC_ENTRIES ? new_src_mac_ix : 0;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_NEW_VCI) )
        action.new_in_vci         = action.in_vlan_ins ? new_vci : 0;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_OUT_VLAN_IX) )
        action.out_vlan_ix        = action.out_vlan_ins ? out_vlan_ix : 0;

    if ( (flags & PPA_UPDATE_ROUTING_ENTRY_DEST_QID) )
        action.dest_qid         = dslwan_qid;

    action.bytes = paction->bytes;
    *paction = action;

    paction->dest_list = dest_list;
    paction->entry_vld = 1;

    ppe_lock_release(p_lock);

    return PPA_SUCCESS;
}

#if defined(CAP_WAP_CONFIG) && CAP_WAP_CONFIG
/*!
  \fn int32_t add_capwap_entry(uint8_t directpath_portid, 
                        uint8_t f_ipv6,
                        uint8_t qid, 
                        uint16_t dest_ifid,
                        uint8_t *dst_mac, 
                        uint8_t *src_mac, 
                        uint8_t tos, 
                        uint8_t ttl, 
                        uint32_t source_ip, 
                        uint32_t dest_ip, 
                        uint16_t ip_chksum, 
                        uint16_t source_port,
                        uint16_t dest_port, 
                        uint16_t udp_chksum, 
                        uint8_t rid, 
                        uint8_t wbid,
                        uint8_t t_flag,
                        uint32_t max_frg_size,
                        uint32_t *p_entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_GLOBAL_FUNCTIONS
  \brief add one capwap entry
  \param f_ipv6                 Is IPv6 or IPv4 header ? 
  \param qid                    destination Queue ID (Switch Queue)
  \param dest_ifid              destination ports, bit 0: eth0, bit 1: eth1
  \param dst_mac                destination mac 
  \param src_mac                source mac 
  \param tos                    IP ToS 
  \param ttl                    TTL 
  \param source_ip              source IP address
  \param dest_ip                destination IP address
  \param ip_chksum              IP checksum 
  \param source_port            Source port 
  \param dest_port              Destination port 
  \param udp_chksum             UDP checksum 
  \param rid                    CAPWAP rid 
  \param wbid                   CAPWAP wbid 
  \param t_flag                 CAPWAP T Flag 
  \param max_frg_size           Maximum fragment size 
  \param p_entry                a data pointer to get entry number
  \return 0: OK, otherwise: fail
 */
int32_t add_capwap_entry(uint8_t directpath_portid, 
                        uint8_t f_ipv6,
                        uint8_t qid, 
                        uint16_t dest_ifid,
                        uint8_t *dst_mac, 
                        uint8_t *src_mac, 
                        uint8_t tos, 
                        uint8_t ttl, 
                        uint32_t source_ip, 
                        uint32_t dest_ip, 
                        uint16_t ip_chksum, 
                        uint16_t source_port,
                        uint16_t dest_port, 
                        uint16_t udp_chksum, 
                        uint8_t rid, 
                        uint8_t wbid,
                        uint8_t t_flag,
                        uint32_t max_frg_size,
                        uint32_t *p_entry)
{
    uint32_t entry;
    uint32_t bit;


    /*
     *  find empty entry
     */


    ppe_lock_get(&g_capwap_lock);
   
    entry = directpath_portid - 3;
    bit = 1 << entry;

    *CAPWAP_CONFIG = ( *CAPWAP_CONFIG | (bit << 3) ); 
    /*  0h  */
//    unsigned int    us_max_frag_size    :16;
    CAPWAP_CONFIG_TBL(entry)->us_max_frag_size = max_frg_size;
//    unsigned int    us_dest_list        :8;
    CAPWAP_CONFIG_TBL(entry)->us_dest_list = dest_ifid;
//    unsigned int    qid                 :4;
    CAPWAP_CONFIG_TBL(entry)->qid = qid;
//    unsigned int    is_ipv4header       :1;
    //CAPWAP_CONFIG_TBL(entry)->is_ipv4header= 0x1;
    CAPWAP_CONFIG_TBL(entry)->is_ipv4header= f_ipv6 ? 0 : 1;
//    unsigned int    acc_en              :1; 
    CAPWAP_CONFIG_TBL(entry)->acc_en = 0x1;


    /*  5h  */
    //unsigned int    da_mac_hi;           
    CAPWAP_CONFIG_TBL(entry)->da_mac_hi = ( (*(dst_mac + 0) << 24) | (*(dst_mac + 1) << 16) | (*(dst_mac + 2) << 8) | *(dst_mac + 3) );
    
    /*  6h  */
    //unsigned int    da_mac_lo           :16;
    CAPWAP_CONFIG_TBL(entry)->da_mac_lo =( (*(dst_mac + 4) << 8) | *(dst_mac + 5) );
    //unsigned int    sa_mac_hi           :16; 
    CAPWAP_CONFIG_TBL(entry)->sa_mac_hi =( (*(src_mac + 0) << 8) | *(src_mac + 1) );
    
    /*  7h  */
    //unsigned int    sa_mac_lo           ; 
    CAPWAP_CONFIG_TBL(entry)->sa_mac_lo =( (*(src_mac + 2) << 24) | (*(src_mac + 3) << 16) | (*(src_mac + 4) << 8) | *(src_mac + 5) );
;

    /*  8h  */
    //unsigned int    eth_type            :16; 
    CAPWAP_CONFIG_TBL(entry)->eth_type = ETHER_TYPE;
    //unsigned int    ver                 :4;
    CAPWAP_CONFIG_TBL(entry)->ver = IP_VERSION;
    //unsigned int    header_len          :4;
    CAPWAP_CONFIG_TBL(entry)->header_len = IP_HEADER_LEN;
    //unsigned int    tos                 :8;
    CAPWAP_CONFIG_TBL(entry)->tos = tos;
    
    /*  9h  */
    //unsigned int    total_len           :16 
    CAPWAP_CONFIG_TBL(entry)->total_len = IP_TOTAL_LEN;
    //unsigned int    identifier          :16
    CAPWAP_CONFIG_TBL(entry)->identifier = IP_IDENTIFIER;
   
    /*  10h  */
    //unsigned int    ip_flags            :3 
    CAPWAP_CONFIG_TBL(entry)->ip_flags = IP_FLAG;
    //unsigned int    ip_frag_off         :13
    CAPWAP_CONFIG_TBL(entry)->ip_frag_off = IP_FRAG_OFF;
    //unsigned int    ttl                 :8
    CAPWAP_CONFIG_TBL(entry)->ttl = ttl; 
    //unsigned int    protocol            :8
    CAPWAP_CONFIG_TBL(entry)->protocol = IP_PROTO_UDP;

    /*  11h  */
    //unsigned int    ip_checksum         :16 
    CAPWAP_CONFIG_TBL(entry)->ip_checksum =ip_chksum;
    //unsigned int    src_ip_hi           :16 
    CAPWAP_CONFIG_TBL(entry)->src_ip_hi = (source_ip >> 16) & 0xFFFF;
    
    /*  12h  */
    //unsigned int    src_ip_lo           :16 
    CAPWAP_CONFIG_TBL(entry)->src_ip_lo = source_ip & 0xFFFF;
    //unsigned int    dst_ip_hi           :16 
    CAPWAP_CONFIG_TBL(entry)->dst_ip_hi = (dest_ip >> 16) & 0xFFFF;
   
    /*  13h  */
    //unsigned int    dst_ip_lo           :16 
    CAPWAP_CONFIG_TBL(entry)->dst_ip_lo = dest_ip & 0xFFFF;
    //unsigned int    src_port            :16 
    CAPWAP_CONFIG_TBL(entry)->src_port = source_port;
   
    /*  14h  */
    //unsigned int    dst_port            :16 
    CAPWAP_CONFIG_TBL(entry)->dst_port = dest_port;
    //unsigned int    udp_ttl_len         :16 
    CAPWAP_CONFIG_TBL(entry)->udp_ttl_len = UDP_TOTAL_LENGTH;
   
    /*  15h  */
    //unsigned int    udp_checksum        :16 
    CAPWAP_CONFIG_TBL(entry)->udp_checksum = udp_chksum;
    //unsigned int    preamble            :8 
    CAPWAP_CONFIG_TBL(entry)->preamble = CAPWAP_PREAMBLE;
    //unsigned int    hlen                :5 
    CAPWAP_CONFIG_TBL(entry)->hlen = CAPWAP_HDLEN;
    //unsigned int    rid_hi              :3 
    CAPWAP_CONFIG_TBL(entry)->rid_hi = (rid >> 2) & 0x7;
   
    /*  16h  */
    //unsigned int    rid_lo              :2 
    CAPWAP_CONFIG_TBL(entry)->rid_lo = rid & 0x3;
    //unsigned int    wbid                :5 
    CAPWAP_CONFIG_TBL(entry)->wbid = wbid;
    //unsigned int    t_flag              :1 
    CAPWAP_CONFIG_TBL(entry)->t_flag = t_flag;
    //unsigned int    f_flag              :1 
    CAPWAP_CONFIG_TBL(entry)->f_flag = CAPWAP_F_FLAG;
    //unsigned int    l_flag              :1 
    CAPWAP_CONFIG_TBL(entry)->l_flag = CAPWAP_L_FLAG;
    //unsigned int    w_flag              :1 
    CAPWAP_CONFIG_TBL(entry)->w_flag = CAPWAP_W_FLAG;
    //unsigned int    m_flag              :1 
    CAPWAP_CONFIG_TBL(entry)->m_flag = CAPWAP_M_FLAG;
    //unsigned int    k_flag              :1 
    CAPWAP_CONFIG_TBL(entry)->k_flag = CAPWAP_K_FLAG;
    //unsigned int    flags               :3 
    CAPWAP_CONFIG_TBL(entry)->flags = CAPWAP_FLAGS;
    //unsigned int    frag_id             :16 
    CAPWAP_CONFIG_TBL(entry)->frag_id = CAPWAP_FRAG_ID;
   
    /*  17h  */
    //unsigned int    frag_off            :13 
    CAPWAP_CONFIG_TBL(entry)->frag_off = CAPWAP_FRAG_OFF;
    //unsigned int    capwap_rsw          :3 
    //unsigned int    payload             :16 
  
    wmb();

    ppe_lock_release(&g_capwap_lock);

    ASSERT(p_entry != NULL, "p_entry == NULL");

    *p_entry = entry;
    
    return PPA_SUCCESS;
}

/*!
  \fn void del_capwap_entry(uint32_t entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_GLOBAL_FUNCTIONS
  \brief delete one capwap entry
  \param entry  entry number got from function call "add_capwap_entry"
  \return no return value
 */
void del_capwap_entry(uint32_t entry)
{
    if ( entry < g_capwap_entries )
    {
        volatile uint32_t *p;

        ppe_lock_get(&g_capwap_lock);

        p= (volatile uint32_t *)CAPWAP_CONFIG_TBL(entry);
        p[17] = 0;
        p[16] = 0;
        p[15] = 0;
        p[14] = 0;
        p[13] = 0;
        p[12] = 0;
        p[11] = 0;
        p[10] = 0;
        p[9] = 0;
        p[8] = 0;
        p[7] = 0;
        p[6] = 0;
        p[5] = 0;
        p[4] = 0;
        p[3] = 0;
        p[2] = 0;
        p[1] = 0;
        p[0] = 0;

        g_capwap_entry_occupation[entry >> 3] &= ~(1 << (entry & 0x7));

        entry = (1 << (entry & 0x7));
        *CAPWAP_CONFIG = ( *CAPWAP_CONFIG & ~(entry  << 3) ); 

        ppe_lock_release(&g_capwap_lock);
    }
}


/*!
  \fn void get_capwap_mib(uint32_t entry, uint32_t *ds_mib, uint32_t *us_mib)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_GLOBAL_FUNCTIONS
  \brief Get CAPWAP DS and US MIB 
  \param entry  entry number got from function call "add_wan_mc_entry"
  \param ds_mib  CAPWAP DS MIB"
  \param us_mib  CAPWAP US MIB"
  \return no return value
 */
void get_capwap_mib(uint32_t entry, uint32_t *ds_mib, uint32_t *us_mib)
{
    if ( entry < g_capwap_entries )
    {
        volatile uint32_t *p;

        ppe_lock_get(&g_capwap_lock);

        p= (volatile uint32_t *)CAPWAP_CONFIG_TBL(entry);

        *ds_mib = CAPWAP_CONFIG_TBL(entry)->ds_mib;
        *us_mib = CAPWAP_CONFIG_TBL(entry)->us_mib;

        ppe_lock_release(&g_capwap_lock);
    }
}

#endif


/*!
  \fn int32_t add_wan_mc_entry(uint32_t dest_ip_compare,
                         uint32_t f_vlan_ins_enable,
                         uint32_t new_vci,
                         uint32_t f_vlan_rm_enable,
                         uint32_t f_src_mac_enable,
                         uint32_t src_mac_ix,
                         uint32_t pppoe_mode,
                         uint32_t f_out_vlan_ins_enable,
                         uint32_t out_vlan_ix,
                         uint32_t f_out_vlan_rm_enable,
                         uint32_t f_new_dscp_en,
                         uint32_t new_dscp,
                         uint32_t dest_qid,
                         uint32_t dest_list,
                         uint32_t route_type,
                         uint32_t *p_entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief add one multicast routing entry
  \param dest_ip_compare        destination IP address
  \param f_vlan_ins_enable      insert inner VLAN tag
  \param new_vci                new inner VLAN tag
  \param f_vlan_rm_enable       remove inner VLAN tag (if there is VLAN tag in incoming packet)
  \param src_mac_ix             index of new source MAC address
  \param pppoe_mode             PPPoE termination, LAN side add PPPoE header, WAN side remove PPPoE header
  \param f_out_vlan_ins_enable  insert outer VLAN tag
  \param out_vlan_ix            index of new outer VLAN tag
  \param f_out_vlan_rm_enable   remove outer VLAN tag
  \param f_new_dscp_en          replace DSCP value
  \param new_dscp               new DSCP value
  \param dest_qid               destination Queue ID (Switch Queue)
  \param dest_list              destination ports, bit 0: eth0, bit 1: eth1
  \param route_type      bridge or route
  \param src_ip_compare   source IP address
  \param f_tunnel_rm_enable  remove tunnel(6RD/DSLITE) header
  \param p_entry                a data pointer to get entry number
  \return 0: OK, otherwise: fail
 */
int32_t add_wan_mc_entry(uint32_t dest_ip_compare,
                         uint32_t f_vlan_ins_enable,
                         uint32_t new_vci,
                         uint32_t f_vlan_rm_enable,
                         uint32_t f_src_mac_enable,
                         uint32_t src_mac_ix,
                         uint32_t pppoe_mode,
                         uint32_t f_out_vlan_ins_enable,
                         uint32_t out_vlan_ix,
                         uint32_t f_out_vlan_rm_enable,
                         uint32_t f_new_dscp_en,
                         uint32_t new_dscp,
                         uint32_t dest_qid,
                         uint32_t dest_list,
                         uint32_t route_type,
                         uint32_t src_ip_compare,
                         uint32_t f_tunnel_rm_enable,
#if defined(RTP_SAMPLING_ENABLE) && RTP_SAMPLING_ENABLE
                         uint8_t sample_en,
#endif
                         uint32_t *p_entry)
{
    uint32_t entry;
    struct wan_rout_multicast_cmp_tbl compare = {0};
    struct wan_rout_multicast_act_tbl action = {0};
    uint32_t entries;
    uint32_t i;
    uint32_t bit;

    ////  dest_list remap
    //if ( !(dest_list & PPA_DEST_LIST_NO_REMAP) )
    //{
    //    uint32_t org_dest_list = dest_list;
    //
    //    dest_list = 0;
    //    for ( i = 0; org_dest_list && i < sizeof(dest_list_map) / sizeof(*dest_list_map); i++, org_dest_list >>= 1 )
    //        if ( (org_dest_list & 0x01) )
    //            dest_list |= dest_list_map[i];
    //}
    //else
    //    dest_list &= PPA_DEST_LIST_NO_REMAP;

    compare.wan_dest_ip   = dest_ip_compare;
    compare.wan_src_ip    = src_ip_compare;

    action.rout_type      = route_type;  //sgh add to support multicast bridging/routing feature
    action.new_dscp       = f_new_dscp_en ? new_dscp : 0;
    action.in_vlan_ins    = f_vlan_ins_enable ? 1 : 0;
    action.in_vlan_rm     = f_vlan_rm_enable ? 1 : 0;
    action.new_dscp_en    = f_new_dscp_en ? 1 : 0;
    action.new_src_mac_en = f_src_mac_enable ? 1 : 0;
    action.dest_list      = dest_list;
    action.pppoe_mode     = pppoe_mode ? 1 : 0;
#if defined(RTP_SAMPLING_ENABLE) && RTP_SAMPLING_ENABLE
    action.sample_en      = sample_en ? 1 : 0;
#endif
    action.new_src_mac_ix = f_src_mac_enable ? src_mac_ix : 0;
    action.new_in_vci     = f_vlan_ins_enable ? new_vci : 0;
    action.out_vlan_ix    = f_out_vlan_ins_enable ? out_vlan_ix : 0;
    action.out_vlan_ins   = f_out_vlan_ins_enable ? 1 : 0;
    action.out_vlan_rm    = f_out_vlan_rm_enable? 1 : 0;
    action.tunnel_rm      = f_tunnel_rm_enable ? 1 : 0;
    action.dest_qid       = dest_qid;

    /*
     *  find empty entry
     */

    entries = (g_wan_mc_entries + BITSIZEOF_UINT32 - 1) / BITSIZEOF_UINT32;

    ppe_lock_get(&g_wan_mc_lock);

    for ( i = 0; i < entries; i++ )
        if ( g_wan_mc_entry_occupation[i] != ~0 )
            goto ADD_WAN_MC_ENTRY_GOON;
    //  no empty entry
    ppe_lock_release(&g_wan_mc_lock);
    return PPA_EAGAIN;

ADD_WAN_MC_ENTRY_GOON:
    entry = i * BITSIZEOF_UINT32;
    bit = 1;
    while ( (g_wan_mc_entry_occupation[i] & bit) )
    {
        bit <<= 1;
        entry++;
    }
    g_wan_mc_entry_occupation[i] |= bit;

    *ROUT_WAN_MC_CMP_TBL(entry) = compare;
    *ROUT_WAN_MC_ACT_TBL(entry) = action;
    *ROUT_WAN_MC_HIT_STAT_TBL(entry >> 5) &= ~(1 << (BITSIZEOF_UINT32 - 1 - (entry & 0x1F)));
    wmb();
    ROUT_WAN_MC_ACT_TBL(entry)->entry_vld = 1;    

    ppe_lock_release(&g_wan_mc_lock);

    ASSERT(p_entry != NULL, "p_entry == NULL");
    *p_entry = entry;

    return PPA_SUCCESS;
}

/*!
  \fn void del_wan_mc_entry(uint32_t entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief delete one multicast routing entry
  \param entry  entry number got from function call "add_wan_mc_entry"
  \return no return value
 */
void del_wan_mc_entry(uint32_t entry)
{
    if ( entry < g_wan_mc_entries )
    {
        volatile uint32_t *p;

        ppe_lock_get(&g_wan_mc_lock);

        //  disable entry
        ROUT_WAN_MC_ACT_TBL(entry)->entry_vld = 0;
        ROUT_WAN_MC_ACT_TBL(entry)->dest_list = PPA_DEST_LIST_CPU0;

        ROUT_WAN_MC_CMP_TBL(entry)->wan_dest_ip = 0;
        ROUT_WAN_MC_CMP_TBL(entry)->wan_src_ip  = 0;

        //Clear Mib counter
        *ROUT_WAN_MC_CNT(entry) = 0;

        p = (volatile uint32_t *)ROUT_WAN_MC_ACT_TBL(entry);
        p[1] = 0;
        p[0] = 0;

#if defined(RTP_SAMPLING_ENABLE) && RTP_SAMPLING_ENABLE
        //Clear RTP MIB
        p = (volatile uint32_t *)MULTICAST_RTP_MIB_TBL(entry);
        p[0] = 0;
#endif    


        g_wan_mc_entry_occupation[entry >> 5] &= ~(1 << (entry & 0x1F));

        ppe_lock_release(&g_wan_mc_lock);
    }
}

/*!
  \fn int32_t update_wan_mc_entry(uint32_t entry,
                            uint32_t f_vlan_ins_enable,
                            uint32_t new_vci,
                            uint32_t f_vlan_rm_enable,
                            uint32_t f_src_mac_enable,
                            uint32_t src_mac_ix,
                            uint32_t pppoe_mode,
                            uint32_t f_out_vlan_ins_enable,
                            uint32_t out_vlan_ix,
                            uint32_t f_out_vlan_rm_enable,
                            uint32_t f_new_dscp_en,
                            uint32_t new_dscp,
                            uint32_t dest_qid,
                            uint32_t dest_list,
                            uint32_t flags)
  \ingroup AMAZON_S_PPA_PPE_D5_HAL_GLOBAL_FUNCTIONS
  \brief update one multicast routing entry
  \param entry                  entry number got from function call "add_wan_mc_entry"
  \param f_vlan_ins_enable      insert inner VLAN tag
  \param new_vci                new inner VLAN tag
  \param f_vlan_rm_enable       remove inner VLAN tag (if there is VLAN tag in incoming packet)
  \param src_mac_ix             index of new source MAC address
  \param pppoe_mode             PPPoE termination, LAN side add PPPoE header, WAN side remove PPPoE header
  \param f_out_vlan_ins_enable  insert outer VLAN tag
  \param out_vlan_ix            index of new outer VLAN tag
  \param f_out_vlan_rm_enable   remove outer VLAN tag
  \param f_new_dscp_en          replace DSCP value
  \param new_dscp               new DSCP value
  \param dest_qid               destination Queue ID (Switch Queue)
  \param dest_list              destination ports, bit 0: eth0, bit 1: eth1
  \param flags                  mask of the fields to be updated
  \return 0: OK, otherwise: fail
 */
int32_t update_wan_mc_entry(uint32_t entry,
                            uint32_t f_vlan_ins_enable,
                            uint32_t new_vci,
                            uint32_t f_vlan_rm_enable,
                            uint32_t f_src_mac_enable,
                            uint32_t src_mac_ix,
                            uint32_t pppoe_mode,
                            uint32_t f_out_vlan_ins_enable,
                            uint32_t out_vlan_ix,
                            uint32_t f_out_vlan_rm_enable,
                            uint32_t f_new_dscp_en,
                            uint32_t new_dscp,
                            uint32_t dest_qid,
                            uint32_t dest_list,
                            uint32_t flags)
{
    struct wan_rout_multicast_act_tbl action;

    if ( entry >= g_wan_mc_entries )
        return PPA_EINVAL;

    ppe_lock_get(&g_wan_mc_lock);

    if ( !(g_wan_mc_entry_occupation[entry >> 5] & (1 << (entry & 0x1F))) )
    {
        ppe_lock_release(&g_wan_mc_lock);
        return PPA_EINVAL;
    }

    action = *ROUT_WAN_MC_ACT_TBL(entry);
    action.entry_vld = 0;

    //  if not update, keep it
    if ( !(flags & PPA_UPDATE_WAN_MC_ENTRY_DEST_LIST) )
        dest_list = action.dest_list;
    //else
    //{
    //    if ( !(dest_list & PPA_DEST_LIST_NO_REMAP) )
    //    {
    //        uint32_t org_dest_list = dest_list;
    //        int i;
    //
    //        dest_list = 0;
    //        for ( i = 0; org_dest_list && i < sizeof(dest_list_map) / sizeof(*dest_list_map); i++, org_dest_list >>= 1 )
    //            if ( (org_dest_list & 0x01) )
    //                dest_list |= dest_list_map[i];
    //    }
    //    else
    //        dest_list &= ~PPA_DEST_LIST_NO_REMAP;
    //}
    action.dest_list = PPA_DEST_LIST_CPU0;

    if ( (flags & PPA_UPDATE_WAN_MC_ENTRY_VLAN_INS_EN) )
        action.in_vlan_ins    = f_vlan_ins_enable ? 1 : 0;

    if ( (flags & PPA_UPDATE_WAN_MC_ENTRY_NEW_VCI) )
        action.new_in_vci     = action.in_vlan_ins ? new_vci : 0;

    if ( (flags & PPA_UPDATE_WAN_MC_ENTRY_VLAN_RM_EN) )
        action.in_vlan_rm     = f_vlan_rm_enable ? 1 : 0;

    if ( (flags & PPA_UPDATE_WAN_MC_ENTRY_SRC_MAC_EN) )
        action.new_src_mac_en = f_src_mac_enable ? 1 : 0;

    if ( (flags & PPA_UPDATE_WAN_MC_ENTRY_SRC_MAC_IX) )
        action.new_src_mac_ix = action.new_src_mac_en ? src_mac_ix : 0;

    if ( (flags & PPA_UPDATE_WAN_MC_ENTRY_PPPOE_MODE) )
        action.pppoe_mode     = pppoe_mode ? 1 : 0;

    if ( (flags & PPA_UPDATE_WAN_MC_ENTRY_OUT_VLAN_INS_EN) )
        action.out_vlan_ins   = f_out_vlan_ins_enable ? 1 : 0;

    if ( (flags & PPA_UPDATE_WAN_MC_ENTRY_OUT_VLAN_IX) )
        action.out_vlan_ix    = action.out_vlan_ins ? out_vlan_ix : 0;

    if ( (flags & PPA_UPDATE_WAN_MC_ENTRY_OUT_VLAN_RM_EN) )
        action.out_vlan_rm    = f_out_vlan_rm_enable ? 1 : 0;

    if ( (flags & PPA_UPDATE_WAN_MC_ENTRY_NEW_DSCP_EN) )
        action.new_dscp_en    = f_new_dscp_en ? 1 : 0;

    if ( (flags & PPA_UPDATE_WAN_MC_ENTRY_NEW_DSCP) )
        action.new_dscp       = action.new_dscp_en ? new_dscp : 0;

    if ( (flags & PPA_UPDATE_WAN_MC_ENTRY_DEST_QID) )
        action.dest_qid       = dest_qid;

    ROUT_WAN_MC_ACT_TBL(entry)->entry_vld = 0;
    *ROUT_WAN_MC_ACT_TBL(entry) = action;
    ROUT_WAN_MC_ACT_TBL(entry)->dest_list = dest_list;
    ROUT_WAN_MC_ACT_TBL(entry)->entry_vld = 1;

    ppe_lock_release(&g_wan_mc_lock);

    return PPA_SUCCESS;
}

#if defined(RTP_SAMPLING_ENABLE) && RTP_SAMPLING_ENABLE
/*!
  \fn int32_t set_wan_mc_rtp(uint32_t entry,
                             uint8_t  sample_en)
  \ingroup AMAZON_S_PPA_PPE_D5_HAL_GLOBAL_FUNCTIONS
  \brief update one multicast routing entry
  \param entry                  entry number got from function call "set_wan_mc_rtp"
  \param sample_en              RTP sampling enable/disable flag 
  \return 0: OK, otherwise: fail
 */

int32_t set_wan_mc_rtp(uint32_t entry,
                       uint8_t  sample_en)
{

    if ( entry >= g_wan_mc_entries )
        return PPA_EINVAL;

    ppe_lock_get(&g_wan_mc_lock);

    if ( !(g_wan_mc_entry_occupation[entry >> 5] & (1 << (entry & 0x1F))) )
    {
        ppe_lock_release(&g_wan_mc_lock);
        return PPA_EINVAL;
    }

    ROUT_WAN_MC_ACT_TBL(entry)->entry_vld = 0;

    ROUT_WAN_MC_ACT_TBL(entry)->sample_en = sample_en ? 1 : 0;
  
    ROUT_WAN_MC_ACT_TBL(entry)->entry_vld = 1;

    ppe_lock_release(&g_wan_mc_lock);

    return PPA_SUCCESS;
}


/*!
  \fn int32_t get_wan_mc_rtp_sampling_cnt(uint32_t entry,
                                    uint32_t *rtp_pkt_cnt,
                                    uint32_t *rtp_seq_num)
  \ingroup AMAZON_S_PPA_PPE_D5_HAL_GLOBAL_FUNCTIONS
  \brief Read RTP mib [packet count, sequence number] per multicast routing entry
  \param entry                  Index to multicast rtp mib table
  \param rtp_pkt_cnt            RTP packet count 
  \param rtp_seq_num            RTP sequence number 
  \return 0: OK, otherwise: fail
 */

int32_t get_wan_mc_rtp_sampling_cnt(uint32_t entry,
                                    uint32_t *rtp_pkt_cnt,
                                    uint32_t *rtp_seq_num)
{
    struct rtp_sampling_cnt rtp_mib;   

    if ( entry >= g_wan_mc_entries )
        return PPA_EINVAL;

    ppe_lock_get(&g_wan_mc_lock);
    if ( !(g_wan_mc_entry_occupation[entry >> 5] & (1 << (entry & 0x1F))) )
    {
        ppe_lock_release(&g_wan_mc_lock);
        return PPA_EINVAL;
    }
    rtp_mib= *MULTICAST_RTP_MIB_TBL(entry);
    *rtp_pkt_cnt = rtp_mib.pkt_cnt;
    *rtp_seq_num = rtp_mib.seq_no;

    ppe_lock_release(&g_wan_mc_lock);

    return PPA_SUCCESS;
}

#endif

/*!
  \fn int32_t get_dest_ip_from_wan_mc_entry(uint32_t entry,
                                      uint32_t *p_ip)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief update one multicast routing entry
  \param entry  entry number got from function call "add_wan_mc_entry"
  \param p_ip   a data pointer to get multicast IP
  \return 0: OK, otherwise: fail
 */
int32_t get_dest_ip_from_wan_mc_entry(uint32_t entry,
                                      uint32_t *p_ip)
{
    if ( entry >= g_wan_mc_entries )
        return PPA_EINVAL;

    ASSERT(p_ip != NULL, "p_ip == NULL");
    *p_ip = ROUT_WAN_MC_CMP_TBL(entry)->wan_dest_ip;

    return PPA_SUCCESS;
}

int32_t add_bridging_entry(uint32_t port,
                           uint8_t  mac[PPA_ETH_ALEN],
                           uint32_t f_src_mac_drop,
                           uint32_t dslwan_qid,
                           uint32_t dest_list,
                           uint32_t *p_entry)
{
    struct mac_tbl_item *p_item;
    struct mac_tbl_item *p_search_item;
    int idx;
    uint32_t ret = 0;
    const uint32_t fid = 1;
    const uint32_t portid = 6; //cpu port
    const uint32_t st_entry = 1; //static entry
    const uint32_t agetime = 3;

    *p_entry = ~0;
    if(port <= get_number_of_phys_port()){//switch will deal with it
        return PPA_EINVAL;
    }

    if ( (mac[0] & 0x01) )
        return PPA_EINVAL;

    p_item = ppa_mem_cache_alloc(g_mac_tbl_item_cache);
    if ( !p_item )
        return PPA_ENOMEM;

    p_item->next = NULL;
    p_item->ref  = 1;
    ppa_memcpy(p_item->mac,mac,sizeof(p_item->mac));
    p_item->mac0 = ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16) | ((uint32_t)mac[4] << 8) | (uint32_t)mac[5];
    p_item->mac1 = ((uint32_t)mac[0] << 8) | (uint32_t)mac[1];
    p_item->age  = 0;
    p_item->timestamp = ppa_get_time_in_10msec();

    idx = BRIDGING_SESSION_LIST_HASH_VALUE(p_item->mac0);

    ppe_lock_get(&g_mac_tbl_lock);

    for ( p_search_item = g_mac_tbl_hash[idx]; p_search_item; p_search_item = p_search_item->next )
        if ( p_search_item->mac0 == p_item->mac0 && p_search_item->mac1 == p_item->mac1 )
        {
            p_search_item->ref++;
            ppe_lock_release(&g_mac_tbl_lock);
            *p_entry = (uint32_t)p_search_item;
            ppa_mem_cache_free(p_item, g_mac_tbl_item_cache);
            return PPA_SUCCESS;
        }

    ret = mac_entry_setting(p_item->mac, fid, portid, agetime, st_entry, MAC_TABLE_ENTRY_ADD);

    if ( ret != PPA_SUCCESS)
    {
        ppe_lock_release(&g_mac_tbl_lock);
        ppa_mem_cache_free(p_item, g_mac_tbl_item_cache);
        return PPA_FAILURE;
    }
    else
    {
        p_item->next = g_mac_tbl_hash[idx];
        g_mac_tbl_hash[idx] = p_item;
        ppe_lock_release(&g_mac_tbl_lock);
        p_item->timestamp = ppa_get_time_in_10msec();
        *p_entry = (uint32_t)p_item;
        return PPA_SUCCESS;
    }
}

void del_bridging_entry(uint32_t entry)
{
    struct mac_tbl_item *p_item;
    struct mac_tbl_item *p_search_item, *p_prev;
    const uint32_t fid = 1;
    int idx;

    if ( entry < KSEG0 || entry >= KSEG1 )
        return;

    p_item = (struct mac_tbl_item *)entry;
    idx = BRIDGING_SESSION_LIST_HASH_VALUE(p_item->mac0);
    p_prev = NULL;

    ppe_lock_get(&g_mac_tbl_lock);
    for ( p_search_item = g_mac_tbl_hash[idx]; p_search_item; p_search_item = p_search_item->next )
    {
        if ( p_search_item == p_item )
        {
            if ( --(p_item->ref) <= 0 )
            {
                if ( p_prev == NULL )
                    g_mac_tbl_hash[idx] = p_item->next;
                else
                    p_prev->next = p_item->next;
                ppe_lock_release(&g_mac_tbl_lock);

                mac_entry_setting(p_item->mac, fid, 0, 0, 0, MAC_TABLE_ENTRY_REMOVE);
                ppa_mem_cache_free(p_item, g_mac_tbl_item_cache);
                return;
            }
            break;
        }
        p_prev = p_search_item;
    }
    ppe_lock_release(&g_mac_tbl_lock);
}

/*!
  \fn add_6rd_tunnel_entry(struct net_device *dev,
                                        uint32_t *tunnel_idx)
  \ingroup AMAZON_S_PPA_PPE_D5_HAL_GLOBAL_FUNCTIONS
  \brief add tunnel ID
  \param dev     tunnel device
  \param tunnel_idx          a data pointer to get tunnel id
  \return 0: OK, otherwise: fail
 */

uint32_t add_6rd_tunnel_entry(struct net_device *dev, uint32_t *tunnel_idx)
{
    uint32_t entry;
    uint32_t empty_entry = MAX_6RD_TUNNEL_ENTRIES;
    volatile struct iphdr *iph;
    uint32_t ret;
    struct ip_tunnel *t;

    ASSERT(dev->type == ARPHRD_SIT, "device is not a sit device");
    t = (struct ip_tunnel *)netdev_priv(dev);

    ppe_lock_get(&g_6rd_tunnel_lock);

    for(entry = 0; entry < MAX_6RD_TUNNEL_ENTRIES; entry ++){
        if(!g_6rd_tunnel_counter[entry] && empty_entry >= MAX_6RD_TUNNEL_ENTRIES){
            empty_entry = entry;
        }else{
            iph = (struct iphdr *)TUNNEL_6RD_TBL(entry);
            if(iph->saddr == t->parms.iph.saddr && iph->daddr == t->parms.iph.daddr){
                goto ADD_TUNNEL_ENTRY_GOON;
            }
        }
    }

    entry = empty_entry;
    if(entry >= MAX_6RD_TUNNEL_ENTRIES){
        ret = PPA_EAGAIN;
        goto ADD_TUNNEL_ENTRY_FAILURE;
    }else{
        ppa_memset((void *)TUNNEL_6RD_TBL(entry), 0, sizeof(struct iphdr));
        add_tunnel_6rd_header(dev, (struct iphdr *)TUNNEL_6RD_TBL(entry));
    }

ADD_TUNNEL_ENTRY_GOON:
    g_6rd_tunnel_counter[entry] ++;
    ASSERT(tunnel_idx != NULL, "tunnel_idx == NULL");
    *tunnel_idx = entry;
    ppe_lock_release(&g_6rd_tunnel_lock);

    return PPA_SUCCESS;

ADD_TUNNEL_ENTRY_FAILURE:
    ppe_lock_release(&g_6rd_tunnel_lock);
    return ret;
}

/*!
  \fn void del_6rd_tunnel_entry(uint32_t tunnel_type, uint32_t entry)
  \ingroup AMAZON_S_PPA_PPE_D5_HAL_GLOBAL_FUNCTIONS
  \brief delete tunnel ID
  \param entry  entry number got from function call "add_tunnel_entry"
  \return no return value
 */
void del_6rd_tunnel_entry(uint32_t entry)
{
    if ( entry >= MAX_6RD_TUNNEL_ENTRIES )
        return;

    ppe_lock_get(&g_6rd_tunnel_lock);

    if ( g_6rd_tunnel_counter[entry] && !--g_6rd_tunnel_counter[entry] ){
        ppa_memset((void *)TUNNEL_6RD_TBL(entry), 0, sizeof(struct iphdr));
    }

    ppe_lock_release(&g_6rd_tunnel_lock);
    return;

}


/*!
  \fn add_dslite_tunnel_entry(struct net_device *dev,
                                        uint32_t *tunnel_idx)
  \ingroup AMAZON_S_PPA_PPE_D5_HAL_GLOBAL_FUNCTIONS
  \brief add tunnel ID
  \param dev     tunnel device
  \param tunnel_idx          a data pointer to get tunnel id
  \return 0: OK, otherwise: fail
 */

uint32_t add_dslite_tunnel_entry(struct net_device *dev, uint32_t *tunnel_idx)
{
    uint32_t entry;
    uint32_t empty_entry = MAX_DSLITE_TUNNEL_ENTRIES;
    volatile struct ipv6hdr *p_ip6hdr;
    struct ipv6hdr ip6_hdr={0};
    uint32_t ret;
    struct ip6_tnl *t;

    ASSERT(dev->type == ARPHRD_TUNNEL6, "device is not a ip4ip6 device");
    t = (struct ip6_tnl *)netdev_priv(dev);
    if(t->parms.proto != IPPROTO_IPIP && t->parms.proto != 0){
        dbg("ipv6 tunnel, but not dslite");
        return PPA_FAILURE;
    }

    ppe_lock_get(&g_dslite_tunnel_lock);

    for(entry = 0; entry < MAX_DSLITE_TUNNEL_ENTRIES; entry ++){
        if(!g_dslite_tunnel_counter[entry] && empty_entry >= MAX_DSLITE_TUNNEL_ENTRIES){
            empty_entry = entry;
        }else{
            p_ip6hdr = (struct ipv6hdr *)TUNNEL_DSLITE_TBL(entry);
            if(ipv6_addr_cmp((struct in6_addr *) &p_ip6hdr->saddr, &t->parms.laddr) == 0 && ipv6_addr_cmp((struct in6_addr *) &p_ip6hdr->daddr, &t->parms.raddr)== 0){
                goto ADD_TUNNEL_ENTRY_GOON;
            }
        }
    }

    entry = empty_entry;
    if(entry >= MAX_DSLITE_TUNNEL_ENTRIES){
        ret = PPA_EAGAIN;
        goto ADD_TUNNEL_ENTRY_FAILURE;
    }else{
        ppa_memset((void *)TUNNEL_DSLITE_TBL(entry), 0, sizeof(struct ipv6hdr));
        p_ip6hdr = (struct ipv6hdr *)TUNNEL_DSLITE_TBL(entry);
        ip6_hdr.version = 6;
        ip6_hdr.hop_limit = t->parms.hop_limit;
        ip6_hdr.nexthdr = IPPROTO_IPIP;
        ipv6_addr_copy(&ip6_hdr.saddr, &t->parms.laddr);
        ipv6_addr_copy(&ip6_hdr.daddr, &t->parms.raddr);
        *p_ip6hdr = ip6_hdr;
    }

ADD_TUNNEL_ENTRY_GOON:
    g_dslite_tunnel_counter[entry] ++;
    ASSERT(tunnel_idx != NULL, "tunnel_idx == NULL");
    *tunnel_idx = entry;
    ppe_lock_release(&g_dslite_tunnel_lock);

    return PPA_SUCCESS;

ADD_TUNNEL_ENTRY_FAILURE:
    ppe_lock_release(&g_dslite_tunnel_lock);
    return ret;
}

/*!
  \fn void del_dslite_tunnel_entry(uint32_t tunnel_type, uint32_t entry)
  \ingroup AMAZON_S_PPA_PPE_D5_HAL_GLOBAL_FUNCTIONS
  \brief delete tunnel ID
  \param entry  entry number got from function call "add_tunnel_entry"
  \return no return value
 */
void del_dslite_tunnel_entry(uint32_t entry)
{
    if ( entry >= MAX_DSLITE_TUNNEL_ENTRIES )
        return;

    ppe_lock_get(&g_dslite_tunnel_lock);

    if ( g_dslite_tunnel_counter[entry] && !--g_dslite_tunnel_counter[entry] ){
        ppa_memset((void *)TUNNEL_DSLITE_TBL(entry), 0, sizeof(struct ipv6hdr));
    }

    ppe_lock_release(&g_dslite_tunnel_lock);
    return;

}


/*!
  \fn add_tunnel_6rd_header (struct net_device *dev, struct iphdr *iph)
  \ingroup AMAZON_S_PPA_PPE_D5_HAL_GLOBAL_FUNCTIONS
  \brief add tunnel 6RD(IPv4) header
  \param dev                    tunnel device
  \param iph                     a data pointer to fill the iph header including calculate the CRC
  \param idx                     idx for tunnel header array
  \return void
 */

void add_tunnel_6rd_header(struct net_device *dev, volatile struct iphdr *iph)
{
    struct ip_tunnel  *t = (struct ip_tunnel *)netdev_priv(dev);

    ASSERT(iph != NULL, "iph == NULL");

    iph->version        = 4;
    iph->protocol       = IPPROTO_IPV6;
    iph->ihl        = 5;
    iph->ttl        = 64;
    iph->saddr      = t->parms.iph.saddr;
    iph->daddr      = t->parms.iph.daddr;

    iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
    ASSERT(iph->saddr != 0 || iph->daddr != 0, "iphdr src/dst address is zero");

    return;
}

/*!
  \fn int32_t add_pppoe_entry(uint32_t session_id,
                        uint32_t *p_entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief add PPPoE session ID
  \param session_id     PPPoE session ID
  \param p_entry        a data pointer to get entry number
  \return 0: OK, otherwise: fail
 */
int32_t add_pppoe_entry(uint32_t session_id,
                        uint32_t *p_entry)
{
    uint32_t entry, empty_entry = MAX_PPPOE_ENTRIES;

    session_id &= 0x0003FFFF;

    ppe_lock_get(&g_pppoe_lock);

    for ( entry = 0; entry < MAX_PPPOE_ENTRIES; entry++ )
        if ( !g_pppoe_entry_counter[entry] )
            empty_entry = entry;
        else if ( *PPPOE_CFG_TBL(entry) == session_id )
            goto ADD_PPPOE_ENTRY_GOON;

    //  no empty entry
    if ( empty_entry >= MAX_PPPOE_ENTRIES )
    {
        ppe_lock_release(&g_pppoe_lock);
        return PPA_EAGAIN;
    }

    entry = empty_entry;

    *PPPOE_CFG_TBL(entry) = session_id;

ADD_PPPOE_ENTRY_GOON:
    g_pppoe_entry_counter[entry]++;

    ppe_lock_release(&g_pppoe_lock);

    ASSERT(p_entry != NULL, "p_entry == NULL");
    *p_entry = entry;

    return PPA_SUCCESS;
}

/*!
  \fn void del_pppoe_entry(uint32_t entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief delete PPPoE session ID
  \param entry  entry number got from function call "add_pppoe_entry"
  \return no return value
 */
void del_pppoe_entry(uint32_t entry)
{
    if ( entry < MAX_PPPOE_ENTRIES )
    {
        ppe_lock_get(&g_pppoe_lock);
        if ( g_pppoe_entry_counter[entry] && !--g_pppoe_entry_counter[entry] )
            *PPPOE_CFG_TBL(entry) = 0;
        ppe_lock_release(&g_pppoe_lock);
    }
}

/*!
  \fn int32_t get_pppoe_entry(uint32_t entry,
                        uint32_t *p_session_id)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief get PPPoE session ID
  \param entry          entry number got from function call "add_pppoe_entry"
  \param p_session_id   a data pointer to get PPPoE session ID
  \return 0: OK, otherwise: fail
 */
int32_t get_pppoe_entry(uint32_t entry,
                        uint32_t *p_session_id)
{
    if ( entry >= MAX_PPPOE_ENTRIES )
        return PPA_EINVAL;

    ppe_lock_get(&g_pppoe_lock);

    if ( !g_pppoe_entry_counter[entry] )
    {
        ppe_lock_release(&g_pppoe_lock);
        return PPA_EINVAL;
    }

    ASSERT(p_session_id != NULL, "p_session_id == NULL");
    *p_session_id = *PPPOE_CFG_TBL(entry);

    ppe_lock_release(&g_pppoe_lock);

    return PPA_SUCCESS;
}

/*!
  \fn int32_t add_mtu_entry(uint32_t mtu_size,
                      uint32_t *p_entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief add MTU (Max Transmission Unit)
  \param mtu_size       Max Transmission Unit size
  \param p_entry        a data pointer to get entry number
  \return 0: OK, otherwise: fail
 */
int32_t add_mtu_entry(uint32_t mtu_size,
                      uint32_t *p_entry)
{
    uint32_t entry, empty_entry = MAX_MTU_ENTRIES;

    mtu_size = (mtu_size & 0x0003FFFF) + 4;

    ppe_lock_get(&g_mtu_lock);

    //  find existing entry and empty entry
    for ( entry = 0; entry < MAX_MTU_ENTRIES; entry++ )
        if ( !g_mtu_entry_counter[entry] )
            empty_entry = entry;
        else if ( *MTU_CFG_TBL(entry) == mtu_size )
            goto ADD_MTU_ENTRY_GOON;

    //  no empty entry
    if ( empty_entry >= MAX_MTU_ENTRIES )
    {
        ppe_lock_release(&g_mtu_lock);
        return PPA_EAGAIN;
    }

    entry = empty_entry;

    *MTU_CFG_TBL(entry) = mtu_size;

ADD_MTU_ENTRY_GOON:
    g_mtu_entry_counter[entry]++;

    ppe_lock_release(&g_mtu_lock);

    ASSERT(p_entry != NULL, "p_entry == NULL");
    *p_entry = entry;

    return PPA_SUCCESS;
}

/*!
  \fn void del_pppoe_entry(uint32_t entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief delete MTU (Max Transmission Unit)
  \param entry  entry number got from function call "add_mtu_entry"
  \return no return value
 */
void del_mtu_entry(uint32_t entry)
{
    if ( entry < MAX_MTU_ENTRIES )
    {
        ppe_lock_get(&g_mtu_lock);
        if ( g_mtu_entry_counter[entry] && !--g_mtu_entry_counter[entry] )
            *MTU_CFG_TBL(entry) = 0;
        ppe_lock_release(&g_mtu_lock);
    }
}

/*!
  \fn int32_t get_mtu_entry(uint32_t entry,
                      uint32_t *p_mtu_size)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief get MTU (Max Transmission Unit)
  \param entry          entry number got from function call "add_mtu_entry"
  \param p_mtu_size     a data pointer to get Max Transmission Unit size
  \return 0: OK, otherwise: fail
 */
int32_t get_mtu_entry(uint32_t entry,
                      uint32_t *p_mtu_size)
{
    if ( entry >= MAX_MTU_ENTRIES )
        return PPA_EINVAL;

    ppe_lock_get(&g_mtu_lock);

    if ( !g_mtu_entry_counter[entry] )
    {
        ppe_lock_release(&g_mtu_lock);
        return PPA_EINVAL;
    }

    ASSERT(p_mtu_size != NULL, "p_mtu_size == NULL");
    *p_mtu_size = *MTU_CFG_TBL(entry) - 4;

    ppe_lock_release(&g_mtu_lock);

    return PPA_SUCCESS;
}

/*!
  \fn void uint32_t get_routing_entry_bytes(uint32_t entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief get one routing entry's byte counter
  \param entry  entry number got from function call "add_routing_entry"
  \return byte counter value
 */
uint32_t get_routing_entry_bytes(uint32_t entry, uint32_t reset_flag)
{
    PPE_LOCK *p_lock;
    volatile struct rout_forward_action_tbl *paction;
    uint16_t *p_occupation;
    uint32_t byte_cnt = 0;

    if ( (entry & 0x80000000) )
    {
        p_lock = &g_lan_routing_lock;
        entry &= 0x7FFFFFFF;
        p_occupation = g_lan_routing_entry_occupation;
        if ( entry >= MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK )
        {
            paction  = ROUT_LAN_COLL_ACT_TBL(entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK);
        }
        else
        {
            paction  = ROUT_LAN_HASH_ACT_TBL(entry);
        }
    }
    else
    {
        p_lock = &g_wan_routing_lock;
        p_occupation = g_wan_routing_entry_occupation;
        if ( entry >= MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK )
        {
            paction  = ROUT_WAN_COLL_ACT_TBL(entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK);
        }
        else
        {
            paction  = ROUT_WAN_HASH_ACT_TBL(entry);
        }
    }

    ppe_lock_get(p_lock);
    byte_cnt = paction->bytes;
    if(reset_flag) paction->bytes=0;
    ppe_lock_release(p_lock);

    return byte_cnt;
}

/*!
  \fn void uint32_t get_mc_entry_bytes(uint32_t entry)
  \ingroup AMAZON_S_PPA_PPE_D5_HAL_GLOBAL_FUNCTIONS
  \brief get one multicast entry's byte counter
  \param entry  entry number got from function call "add_wan_mc_entry"
  \return byte counter value
 */
uint32_t get_mc_entry_bytes(uint32_t entry, uint32_t reset_flag)
{
    uint32_t count=*ROUT_WAN_MC_CNT(entry);

    if( reset_flag) *ROUT_WAN_MC_CNT(entry) = 0;
    
    return (uint32_t) count;
}

/*!
  \fn int32_t add_mac_entry(uint8_t mac[PPA_ETH_ALEN],
                      uint32_t *p_entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief add local MAC address
  \param mac            MAC address of one local interface
  \param p_entry        a data pointer to get entry number
  \return 0: OK, otherwise: fail
 */
int32_t add_mac_entry(uint8_t mac[PPA_ETH_ALEN],
                      uint32_t *p_entry)
{
    int32_t entry, empty_entry = -1;
    uint32_t mac52 = (((uint32_t)mac[0] & 0xFF) << 24) | (((uint32_t)mac[1] & 0xFF) << 16) | (((uint32_t)mac[2] & 0xFF) << 8) | ((uint32_t)mac[3] & 0xFF);
    uint32_t mac10 = (((uint32_t)mac[4] & 0xFF) << 24) | (((uint32_t)mac[5] & 0xFF) << 16);

    ppe_lock_get(&g_mac_lock);

    //  find existing entry and empty entry
    for ( entry = MAX_MAC_ENTRIES - 1; entry >= 0; entry-- )
        if ( !g_mac_entry_counter[entry] )
            empty_entry = entry;
        else if ( ROUT_MAC_CFG_TBL(entry)[0] == mac52 && ROUT_MAC_CFG_TBL(entry)[1] == mac10 )
            goto ADD_MAC_ENTRY_GOON;

    //  no empty entry
    if ( empty_entry < 0 )
    {
        ppe_lock_release(&g_mac_lock);
        return PPA_EAGAIN;
    }

    entry = empty_entry;

    ROUT_MAC_CFG_TBL(entry)[0] = mac52;
    ROUT_MAC_CFG_TBL(entry)[1] = mac10;

ADD_MAC_ENTRY_GOON:
    g_mac_entry_counter[entry]++;

    ppe_lock_release(&g_mac_lock);

    ASSERT(p_entry != NULL, "p_entry == NULL");
    *p_entry = entry;

    return PPA_SUCCESS;
}

/*!
  \fn void del_mac_entry(uint32_t entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief delete local MAC address
  \param entry  entry number got from function call "add_mac_entry"
  \return no return value
 */
void del_mac_entry(uint32_t entry)
{
    if ( entry < MAX_MAC_ENTRIES )
    {
        ppe_lock_get(&g_mac_lock);
        if ( g_mac_entry_counter[entry] && !--g_mac_entry_counter[entry] )
        {
            ROUT_MAC_CFG_TBL(entry)[0] = 0;
            ROUT_MAC_CFG_TBL(entry)[1] = 0;
        }
        ppe_lock_release(&g_mac_lock);
    }
}

/*!
  \fn int32_t get_mac_entry(uint32_t entry,
                      uint8_t mac[PPA_ETH_ALEN])
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief get local MAC address
  \param entry      entry number got from function call "add_mac_entry"
  \param mac        a data pointer to get local MAC address
  \return 0: OK, otherwise: fail
 */
int32_t get_mac_entry(uint32_t entry,
                      uint8_t mac[PPA_ETH_ALEN])
{
    if ( entry >= MAX_MAC_ENTRIES )
        return PPA_EINVAL;

    ppe_lock_get(&g_mac_lock);

    if ( !g_mac_entry_counter[entry] )
    {
        ppe_lock_release(&g_mac_lock);
        return PPA_EINVAL;
    }

    ASSERT(mac != NULL, "mac == NULL");
    ppa_memcpy(mac, (void *)ROUT_MAC_CFG_TBL(entry), PPA_ETH_ALEN);

    ppe_lock_release(&g_mac_lock);

    return PPA_SUCCESS;
}

/*!
  \fn int32_t add_outer_vlan_entry(uint32_t new_tag,
                             uint32_t *p_entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief add outer VLAN tag
  \param new_tag        ounter VLAN tag
  \param p_entry        a data pointer to get entry number
  \return 0: OK, otherwise: fail
 */
int32_t add_outer_vlan_entry(uint32_t new_tag,
                             uint32_t *p_entry)
{
    uint32_t entry, empty_entry = MAX_OUTER_VLAN_ENTRIES;

    ppe_lock_get(&g_outer_vlan_lock);

    for ( entry = 0; entry < MAX_OUTER_VLAN_ENTRIES; entry++ )
        if ( !g_outer_vlan_entry_counter[entry] )
            empty_entry = entry;
        else if ( *OUTER_VLAN_TBL(entry) == new_tag )
            goto ADD_OUTER_VLAN_ENTRY_GOON;

    //  no empty entry
    if ( empty_entry >= MAX_OUTER_VLAN_ENTRIES )
    {
        ppe_lock_release(&g_outer_vlan_lock);
        return PPA_EAGAIN;
    }

    entry = empty_entry;

    *OUTER_VLAN_TBL(entry) = new_tag;

ADD_OUTER_VLAN_ENTRY_GOON:
    g_outer_vlan_entry_counter[entry]++;

    ppe_lock_release(&g_outer_vlan_lock);

    ASSERT(p_entry != NULL, "p_entry == NULL");
    *p_entry = entry;

    return PPA_SUCCESS;
}

/*!
  \fn void del_outer_vlan_entry(uint32_t entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief delete outer VLAN tag
  \param entry  entry number got from function call "add_outer_vlan_entry"
  \return no return value
 */
void del_outer_vlan_entry(uint32_t entry)
{
    if ( entry < MAX_OUTER_VLAN_ENTRIES )
    {
        ppe_lock_get(&g_outer_vlan_lock);
        if ( g_outer_vlan_entry_counter[entry] && !--g_outer_vlan_entry_counter[entry] )
            *OUTER_VLAN_TBL(entry) = 0;
        ppe_lock_release(&g_outer_vlan_lock);
    }
}

/*!
  \fn int32_t get_outer_vlan_entry(uint32_t entry,
                             uint32_t *p_outer_vlan_tag)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief get outer VLAN tag
  \param entry              entry number got from function call "add_outer_vlan_entry"
  \param p_outer_vlan_tag   a data pointer to get outer VLAN tag
  \return 0: OK, otherwise: fail
 */
int32_t get_outer_vlan_entry(uint32_t entry,
                             uint32_t *p_outer_vlan_tag)
{
    if ( entry >= MAX_OUTER_VLAN_ENTRIES )
        return PPA_EINVAL;

    ppe_lock_get(&g_outer_vlan_lock);

    if ( !g_outer_vlan_entry_counter[entry] )
    {
        ppe_lock_release(&g_outer_vlan_lock);
        return PPA_EINVAL;
    }

    ASSERT(p_outer_vlan_tag != NULL, "p_outer_vlan_tag == NULL");
    *p_outer_vlan_tag = *OUTER_VLAN_TBL(entry);

    ppe_lock_release(&g_outer_vlan_lock);

    return PPA_SUCCESS;
}
int32_t ifx_ppe_drv_del_multifield_entry_via_index(int32_t index, uint32_t flag);

/*!
    \brief This is to enable/disable multiple field feature function
    \param[in] enable,  1--enable/0--disable multiple field
    \param[in] flag reserved for future
    \return uint8_t, The return value can be any one of the following:  \n
               PPA_SUCCESS on sucess \n
               PPA_FAILURE on error
*/
uint8_t ifx_ppe_drv_multifield_control(uint8_t enable, uint32_t flag)
{
    GEN_MODE_CFG1->brg_class_en = enable ? 1 : 0;
    GEN_MODE_CFG1->classification_num = MAX_CLASSIFICATION_ENTRIES;
    return PPA_SUCCESS;
}

/*!
    \brief This is get multiple field function status: enabled/disabled
    \param[out] enable,  1--enable/0--disable multiple field
    \param[in] flag reserved for future
    \return uint8_t, The return value can be any one of the following:  \n
               PPA_SUCCESS on sucess \n
               PPA_FAILURE on error
*/
uint8_t ifx_ppe_drv_get_multifield_status(uint32_t *enable, uint32_t flag)
{
    ASSERT(enable != NULL, "enable == NULL");

    *enable = GEN_MODE_CFG1->brg_class_en;
    return PPA_SUCCESS;
}

/*!
    \brief This is to get the maximum multiple field entry number
    \param[in] flag reserved for future
    \return int32_t, return the maximum multiple field entry number
*/
int32_t ifx_ppe_drv_get_multifield_max_entry(uint32_t flag)
{
    if( flag == SESSION_ADDED_IN_HW ) //get accelerated mf flow entries
    {
        int entry;
        int num = 0;

        for ( entry = MAX_CLASSIFICATION_ENTRIES - 1; entry >= 0; entry-- )
        {
            if ( g_classification_entry_counter[entry] ) num ++;
        }
        printk(KERN_WARNING "get mf flow for SESSION_ADDED_IN_HW only\n");
        return num;
    }
    else // get the maximum supported mf session
        return MAX_CLASSIFICATION_ENTRIES;

}

static INLINE int mfe_param2word(PPA_MULTIFIELD_FLOW_INFO *p_multifield_info, uint32_t compare[], uint32_t mask[], struct classification_act_tbl *act)
{
    int ret = PPA_SUCCESS;
    PPA_MULTIFIELD_VLAN_INFO_MANUAL *p_vlan = &(p_multifield_info->cfg0.vlan_info.vlan_info_manual);
    unsigned int out_vci;
    int i;

    compare[0]  |= ((unsigned int)p_multifield_info->cfg0.ether_type & 0xFFFF) << 16;
    mask[0]     |= ((unsigned int)p_multifield_info->cfg0.ether_type_mask & 0xFFFF) << 16;

    compare[0]  |= ((unsigned int)p_multifield_info->cfg0.dscp & 0xFF) << 8;
    mask[0]     |= ((unsigned int)p_multifield_info->cfg0.dscp_mask & 0xFF) << 8;

    compare[0]  |= (unsigned int)p_multifield_info->cfg0.pkt_length & 0xFF;
    mask[0]     |= (unsigned int)p_multifield_info->cfg0.pkt_length_mask & 0xFF;

    compare[1]  = p_multifield_info->cfg0.s_ip;
    mask[1]     = p_multifield_info->cfg0.s_ip_mask;

    compare[2]  |= ((unsigned int)p_vlan->out_vlan_pri & 0x07) << 29;
    mask[2]     |= ((unsigned int)p_vlan->out_vlan_pri_mask & 0x07) << 29;

    compare[2]  |= p_vlan->out_vlan_cfi ? (1 << 28) : 0;
    mask[2]     |= p_vlan->out_vlan_cfi_mask ? (1 << 28) : 0;

    compare[2]  |= ((unsigned int)p_vlan->out_vlan_vid & 0x0FFF) << 16;
    mask[2]     |= ((unsigned int)p_vlan->out_vlan_vid_mask & 0x0FFF) << 16;

    compare[2]  |= ((unsigned int)p_multifield_info->cfg0.l3_off0 & 0xFF) << 8;
    mask[2]     |= ((unsigned int)p_multifield_info->cfg0.l3_off0_mask & 0xFF) << 8;

    compare[2]  |= (unsigned int)p_multifield_info->cfg0.l3_off1 & 0xFF;
    mask[2]     |= (unsigned int)p_multifield_info->cfg0.l3_off1_mask & 0xFF;

    compare[3]  |= ((unsigned int)p_vlan->in_vlan_pri & 0x07) << 29;
    mask[3]     |= ((unsigned int)p_vlan->in_vlan_pri_mask & 0x07) << 29;

    compare[3]  |= p_vlan->in_vlan_cfi ? (1 << 28) : 0;
    mask[3]     |= p_vlan->in_vlan_cfi_mask ? (1 << 28) : 0;

    compare[3]  |= ((unsigned int)p_vlan->in_vlan_vid & 0x0FFF) << 16;
    mask[3]     |= ((unsigned int)p_vlan->in_vlan_vid_mask & 0x0FFF) << 16;

    compare[3]  |= 1 << (p_vlan->tx_if_id + 8);
//    mask[3]     |= 0xFF << 8;

    compare[3]  |= (unsigned int)p_vlan->rx_if_id << 5;
//    mask[3]     |= 0x07 << 5;

    compare[3]  |= p_multifield_info->cfg0.ipv4 ? (1 << 4) : 0;
    mask[3]     |= p_multifield_info->cfg0.ipv4_mask ? (1 << 4) : 0;

    compare[3]  |= p_multifield_info->cfg0.ipv6 ? (1 << 3) : 0;
    mask[3]     |= p_multifield_info->cfg0.ipv6_mask ? (1 << 3) : 0;

    compare[3]  |= p_multifield_info->cfg0.pppoe_session ? (1 << 2) : 0;
    mask[3]     |= p_multifield_info->cfg0.pppoe_session_mask ? (1 << 2) : 0;

    compare[3]  |= (unsigned int)p_vlan->is_vlan & 0x03;
    mask[3]     |= (unsigned int)p_vlan->is_vlan_mask & 0x03;

    for ( i = 0; i < 4; i++ )
        compare[i] &= ~mask[i];

    if ( p_vlan->action_in_vlan_insert )
    {
        act->in_vlan_ins = 1;
        act->new_in_vci = ((unsigned int)p_vlan->new_in_vlan_pri << 13) | ((unsigned int)p_vlan->new_in_vlan_cfi << 12) | p_vlan->new_in_vlan_vid;
    }
    act->in_vlan_rm = p_vlan->action_in_vlan_remove ? 1 : 0;

    if ( p_vlan->action_out_vlan_insert )
    {
        unsigned int out_vlan_ix =-1;
        act->out_vlan_ins = 1;
        out_vci = ((unsigned int)p_vlan->new_out_vlan_tpid << 16) | ((unsigned int)p_vlan->new_out_vlan_pri << 13) | ((unsigned int)p_vlan->new_out_vlan_cfi << 12) | p_vlan->new_out_vlan_vid;

        if ( add_outer_vlan_entry(out_vci, &out_vlan_ix) != PPA_SUCCESS )
        {
            dbg("add_outer_vlan_entry fail: for %x\n", out_vci );
            return PPA_FAILURE;
        }
        else
        {
            act->out_vlan_ix = out_vlan_ix;
        }
    }
    act->out_vlan_rm = p_vlan->action_out_vlan_remove ? 1 : 0;

    act->dest_qid = p_multifield_info->cfg0.queue_id;

    dbg("mfe_param2word ok\n");
    return ret;
}

static INLINE int mfe_word2param(PPA_MULTIFIELD_FLOW_INFO *p_multifield_info, uint32_t compare[], uint32_t mask[], struct classification_act_tbl *act)
{
    PPA_MULTIFIELD_VLAN_INFO_MANUAL *p_vlan = &(p_multifield_info->cfg0.vlan_info.vlan_info_manual);
    unsigned int out_vci=0;
    unsigned int tmp_id=0;
    int i=0;

    ppa_memset( p_vlan, 0, sizeof(*p_vlan) );
    p_multifield_info->cfg0.vlan_info.bfauto = 0;

    p_multifield_info->cfg0.ether_type = compare[0] >> 16;
    p_multifield_info->cfg0.ether_type_mask = mask[0] >> 16;

    p_multifield_info->cfg0.dscp = ( compare[0] >> 8 ) & 0xFF;
    p_multifield_info->cfg0.dscp_mask = ( mask[0] >> 8 ) & 0xFF;

    p_multifield_info->cfg0.pkt_length = compare[0] && 0xFF;
    p_multifield_info->cfg0.pkt_length_mask = mask[0] && 0xFF;

    p_multifield_info->cfg0.s_ip = compare[1];
    p_multifield_info->cfg0.s_ip_mask = mask[1];

    p_vlan->out_vlan_pri = ( compare[2] >> 29 ) & 0x07;
    p_vlan->out_vlan_pri_mask = ( mask[2] >> 29 ) & 0x07;

    p_vlan->out_vlan_cfi = ( compare[2] >> 28 ) & 0x01;
    p_vlan->out_vlan_cfi_mask = ( mask[2] >> 28 ) & 0x01;

    p_vlan->out_vlan_vid = ( compare[2] >> 16 ) & 0x0FFF;
    p_vlan->out_vlan_vid_mask = ( mask[2] >> 16 ) & 0x0FFF;

    p_multifield_info->cfg0.l3_off0 = ( compare[2] >> 8 ) & 0xFF;
    p_multifield_info->cfg0.l3_off0_mask = ( mask[2] >> 8 ) & 0xFF;

    p_multifield_info->cfg0.l3_off1 = compare[2] & 0xFF;
    p_multifield_info->cfg0.l3_off1_mask = mask[2] & 0xFF;

    p_vlan->in_vlan_pri = ( compare[3] >> 29 ) & 0x07;
    p_vlan->in_vlan_pri_mask = ( mask[3] >> 29 ) & 0x07;

    p_vlan->in_vlan_cfi = ( compare[3] >> 28 ) & 0x01;
    p_vlan->in_vlan_cfi_mask = (  mask[3] >> 28 ) & 0x01;

    p_vlan->in_vlan_vid = ( compare[3] >> 16 ) & 0x0FFF;
    p_vlan->in_vlan_vid_mask = ( mask[3] >> 16 ) & 0x0FFF;

    tmp_id = ( compare[3] >> 8 ) & 0xFF;
    for(i=0; i<8; i++ )
    {
        if( tmp_id & (1 << i) )
            p_vlan->tx_if_id = i;
    }
    p_vlan->rx_if_id = ( compare[3] >> 5 ) & 0x7;

    p_multifield_info->cfg0.ipv4 = ( compare[3] & (1 << 4) ) ? 1:0;
    p_multifield_info->cfg0.ipv4_mask = ( mask[3] & (1 << 4) ) ? 1:0;

    p_multifield_info->cfg0.ipv6 = ( compare[3] & (1 << 3) ) ? 1:0;
    p_multifield_info->cfg0.ipv6_mask = ( mask[3] & (1 << 3) ) ? 1:0;

    p_multifield_info->cfg0.pppoe_session = ( compare[3] & (1 << 2) ) ? 1:0;
    p_multifield_info->cfg0.pppoe_session_mask = ( mask[3] & (1 << 2) ) ? 1:0;

    p_vlan->is_vlan = compare[3] & 0x03;
    p_vlan->is_vlan_mask = mask[3] & 0x03;

    if( act->in_vlan_ins )
    {
        p_vlan->action_in_vlan_insert = 1;
        p_vlan->new_in_vlan_pri = act->new_in_vci >> 13;
        p_vlan->new_in_vlan_cfi = ( act->new_in_vci >> 12 ) & 1;
        p_vlan->new_in_vlan_vid = act->new_in_vci & 0xFFF;
    }
    p_vlan->action_in_vlan_remove = act->in_vlan_rm;

    if( act->out_vlan_ins )
    {
        p_vlan->action_out_vlan_insert = 1;
        if( get_outer_vlan_entry(act->out_vlan_ix, &out_vci) != PPA_SUCCESS )
        {
            dbg("get_outer_vlan_entry fail for for out_vlan_ix %x\n", act->out_vlan_ix );
            return PPA_FAILURE;
        }

        p_vlan->new_out_vlan_tpid = ( out_vci >> 16 ) & 0xFFFF;
        p_vlan->new_out_vlan_pri = ( out_vci >> 13 ) & 0x3;
        p_vlan->new_out_vlan_cfi = ( out_vci >> 12 ) & 0x1;
        p_vlan->new_out_vlan_vid = out_vci & 0xFFF;
    }
    p_vlan->action_out_vlan_remove = act->out_vlan_rm;

    p_multifield_info->cfg0.queue_id = act->dest_qid;

    return PPA_SUCCESS;
}


/*!
    \brief This is to add one multiple field entry/flow
    \param[in] p_multifield_info the detail multiple field entry classification and its action
    \param[out] index return the index of the newly added entry in the compare table, it's valid only if return PPA_SUCCESS
    \param[in] flag reserved for future
    \return int32_t, The return value can be any one of the following:  \n
               PPA_SUCCESS on sucess \n
               PPA_FAILURE on error \n
    \note, as for p_multifield_info, only sub member
*/
int32_t ifx_ppe_drv_add_multifield_entry(PPA_MULTIFIELD_FLOW_INFO *p_multifield_info, int32_t *index, uint32_t flag)
{
    int ret=PPA_FAILURE;
    int32_t entry, empty_entry = -1;
    uint32_t compare[4] = {0};
    uint32_t mask[4] = {0};
    struct classification_act_tbl act = {0};
    int i;

    ASSERT(p_multifield_info != NULL, "p_multifield_info == NULL");

    act.fw_cpu = 1;
    if( mfe_param2word(p_multifield_info, compare, mask, &act) != PPA_SUCCESS )
    {
        dbg("ifx_ppe_drv_add_multifield_entry mfe_param2word fail\n");
        return PPA_EAGAIN;
    }

    ppe_lock_get(&g_classification_lock);

    //  find existing entry and empty entry
    for ( entry = MAX_CLASSIFICATION_ENTRIES - 1; entry >= 0; entry-- )
    {
        if ( !g_classification_entry_counter[entry] )
            empty_entry = entry;
        else
        {
           if ( ppa_memcmp( (char *)CLASSIFICATION_CMP_TBL(entry), (char *)compare, sizeof(compare)) != 0 ||
                 ppa_memcmp( (char *)CLASSIFICATION_MSK_TBL(entry), (char *)mask, sizeof(mask)) != 0 )
            continue; //not equal. so continue search

            dbg("Found matched one with mfe[%d]. So replace old one\n", entry );
            if( (CLASSIFICATION_ACT_TBL(entry)->out_vlan_ins ) &&
                (CLASSIFICATION_ACT_TBL(entry)->out_vlan_ix != act.out_vlan_ix) )
            { //remove old out vlan
                del_outer_vlan_entry(CLASSIFICATION_ACT_TBL(entry)->out_vlan_ix);
            }
            //update new action
            *CLASSIFICATION_ACT_TBL(entry) = act;

            CLASSIFICATION_ACT_TBL(entry)->fw_cpu = 0;

            goto ADD_MFE_ENTRY_GOON;
        }
    }

    //  no empty entry
    if ( empty_entry < 0 )
    {
        ppe_lock_release(&g_classification_lock);
        ret = PPA_EAGAIN;
        goto ADD_MFE_ENTRY_FAIL;
    }

    entry = empty_entry;
    dbg("Found empty one with index: %d\n", entry );
    for ( i = 0; i < 4; i++ )
    {
        CLASSIFICATION_CMP_TBL(entry)[i] = compare[i];
        CLASSIFICATION_MSK_TBL(entry)[i] = mask[i];
    }
    *CLASSIFICATION_ACT_TBL(entry) = act;
    CLASSIFICATION_ACT_TBL(entry)->fw_cpu = 0;
    g_classification_entry_counter[entry]++;

ADD_MFE_ENTRY_GOON:

    ppe_lock_release(&g_classification_lock);

    ASSERT(index != NULL, "index == NULL");
    *index = entry;
    dbg("ifx_ppe_drv_add_multifield_entry ok with index=%x\n", *index);
    return PPA_SUCCESS;

ADD_MFE_ENTRY_FAIL:
   if ( p_multifield_info->cfg0.vlan_info.vlan_info_manual.action_out_vlan_insert )
        del_outer_vlan_entry(act.out_vlan_ix);
    return ret;
}

/*!
    \brief This is to get one multiple field entry as specified via index
    \param[in] index, the index of compare table to get
    \param[in] multifield_info, the pointer to store the multiple field information
    \return int32_t, return the bytes in the compare table. It can be any one of the following now: \n
               PPA_SUCCESS on sucess \n
               PPA_FAILURE on error
*/
int32_t ifx_ppe_drv_get_multifield_entry(int32_t index, PPA_MULTIFIELD_FLOW_INFO *p_multifield_info, uint32_t flag)
{
    int ret=PPA_SUCCESS;
    uint32_t compare[4] = {0};
    uint32_t mask[4] = {0};
    struct classification_act_tbl act = {0};
    int i;

    ASSERT(p_multifield_info != NULL, "p_multifield_info == NULL");
    dbg("Try to get mfe[%d]\n", index );

    if( index < 0 || index >= MAX_CLASSIFICATION_ENTRIES )
        return PPA_EAGAIN;

    ppe_lock_get(&g_classification_lock);

    if ( g_classification_entry_counter[index] )
    {
        for ( i = 0; i < 4; i++ )
        {
            compare[i] = CLASSIFICATION_CMP_TBL(index)[i];
            mask[i] = CLASSIFICATION_MSK_TBL(index)[i];
        }
        act = *CLASSIFICATION_ACT_TBL(index);

        ret = mfe_word2param(p_multifield_info, compare, mask, &act);
    }
    else
        ret = PPA_FAILURE;
    ppe_lock_release(&g_classification_lock);

    return ret;
}

/*!
    \brief This is to delete multiple field entry if compare/mask/key completely match
    \param[in] multifield_info, the pointer to the compare/action table
    \param[in] flag reserved for future
    \return int32_t, The return value can be any one of the following:  \n
               PPA_SUCCESS on sucess \n
               PPA_FAILURE on error, like entry full already
*/
int32_t ifx_ppe_drv_del_multifield_entry(PPA_MULTIFIELD_FLOW_INFO *p_multifield_info, uint32_t flag)
{
    int ret=PPA_FAILURE;
    int32_t entry;
    uint32_t compare[4] = {0};
    uint32_t mask[4] = {0};
    struct classification_act_tbl act = {0};

    ASSERT(p_multifield_info != NULL, "p_multifield_info == NULL");

    if( mfe_param2word(p_multifield_info, compare, mask, &act) != PPA_SUCCESS )
    {
        dbg("ifx_ppe_drv_del_multifield_entry mfe_param2word fail\n");
        return PPA_EAGAIN;
    }

    ppe_lock_get(&g_classification_lock);

    //  find existing entry and empty entry
    for ( entry = MAX_CLASSIFICATION_ENTRIES - 1; entry >= 0; entry-- )
    {
        if (g_classification_entry_counter[entry] )
        {
           if ( ppa_memcmp( (void *)CLASSIFICATION_CMP_TBL(entry), (void *)compare, sizeof(compare)) != 0 ||
                 ppa_memcmp( (void *)CLASSIFICATION_MSK_TBL(entry), (void *)mask, sizeof(mask)) != 0 )
            continue; //not equal. so continue search

            dbg("ifx_ppe_drv_del_multifield_entry found matched one with mfe[%d]. So delete it\n", entry );

            ppe_lock_release(&g_classification_lock);
            if ( p_multifield_info->cfg0.vlan_info.vlan_info_manual.action_out_vlan_insert )
                del_outer_vlan_entry(act.out_vlan_ix);

            ifx_ppe_drv_del_multifield_entry_via_index(entry, flag );

            return  PPA_SUCCESS;
        }
    }
   dbg("ifx_ppe_drv_del_multifield_entry failed to find a matched one\n" );
   //not found
   ppe_lock_release(&g_classification_lock);
   if ( p_multifield_info->cfg0.vlan_info.vlan_info_manual.action_out_vlan_insert )
        del_outer_vlan_entry(act.out_vlan_ix);
   return ret;
}

/*!
    \brief This is to delete multiple field entry as specified via index
    \param[out] index, the index of compare table to delete
    \param[in] flag reserved for future
    \return int32_t, return the bytes in the compare table. It can be any one of the following now:  \n
              \return int32_t, The return value can be any one of the following:  \n
               PPA_SUCCESS on sucess \n
               PPA_FAILURE on error
    \note if index is -1, it will delete all multiple field entries
*/
int32_t ifx_ppe_drv_del_multifield_entry_via_index(int32_t index, uint32_t flag)
{
    if ( index >= 0 && index < MAX_CLASSIFICATION_ENTRIES )
    {
        ppe_lock_get(&g_classification_lock);
        if ( g_classification_entry_counter[index] && !--g_classification_entry_counter[index] )
        {
            unsigned int out_vci_entry = ~0;
            struct classification_act_tbl act = {0};

            if ( CLASSIFICATION_ACT_TBL(index)->out_vlan_ins )
                out_vci_entry = CLASSIFICATION_ACT_TBL(index)->out_vlan_ix;

            CLASSIFICATION_ACT_TBL(index)->fw_cpu = 1;
            CLASSIFICATION_MSK_TBL(index)[0] = 0;
            CLASSIFICATION_MSK_TBL(index)[1] = 0;
            CLASSIFICATION_MSK_TBL(index)[2] = 0;
            CLASSIFICATION_MSK_TBL(index)[3] = 0;
            CLASSIFICATION_CMP_TBL(index)[0] = 0;
            CLASSIFICATION_CMP_TBL(index)[1] = 0;
            CLASSIFICATION_CMP_TBL(index)[2] = 0;
            CLASSIFICATION_CMP_TBL(index)[3] = 0;

            if ( out_vci_entry != ~0 )
            {
                del_outer_vlan_entry(out_vci_entry);
                dbg("out vlan_ix deleted: %d\n", out_vci_entry );
            }

            *CLASSIFICATION_ACT_TBL(index) = act;

        }
        ppe_lock_release(&g_classification_lock);
    }

    return PPA_SUCCESS;
}

void get_itf_mib(uint32_t itf, struct ppe_itf_mib *p)
{
    if ( p != NULL && itf < 8 )
        ppa_memcpy(p, (void *)ITF_MIB_TBL(itf), sizeof(*p));
}

/*!
  \fn uint32_t test_and_clear_hit_stat(uint32_t entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief check hit status and clean it
  \param entry              entry number got from function call "add_routing_entry"
  \return 0: OK, otherwise: fail
 */
uint32_t test_and_clear_hit_stat(uint32_t entry)
{
    if ( (entry & 0x7FFFFFFF) < MAX_ROUTING_ENTRIES )
    {
        uint32_t ret;
        volatile u32 *phit;
        u32 hitbit;

        if ( (entry & 0x80000000) )
        {
            //  LAN
            entry &= 0x7FFFFFFF;
            if ( entry >= MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK )
            {
                phit = ROUT_LAN_COLL_HIT_STAT_TBL((entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK) >> 5);
                hitbit = 1 << (BITSIZEOF_UINT32 - 1 - ((entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK) & 0x1F));
            }
            else
            {
                phit = ROUT_LAN_HASH_HIT_STAT_TBL(entry >> 5);
                hitbit = 1 << (BITSIZEOF_UINT32 - 1 - (entry & 0x1F));
            }
        }
        else
        {
            //  WAN
            if ( entry >= MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK )
            {
                phit = ROUT_WAN_COLL_HIT_STAT_TBL((entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK) >> 5);
                hitbit = 1 << (BITSIZEOF_UINT32 - 1 - ((entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK) & 0x1F));
            }
            else
            {
                phit = ROUT_WAN_HASH_HIT_STAT_TBL(entry >> 5);
                hitbit = 1 << (BITSIZEOF_UINT32 - 1 - (entry & 0x1F));
            }
        }

        ret = *phit;
        *phit = ret & ~hitbit;
        ret &= hitbit;

        return ret;
    }
    else
        return 0;
}

/*!
  \fn uint32_t test_and_clear_hit_stat_batch(uint32_t entry)
  \ingroup AMAZON_S_PPA_PPE_E5_HAL_COMPILE_PARAMS
  \brief check hit status of consecutive 32 entries and clean them
  \param entry              entry number got from function call "add_routing_entry"
  \return 0: OK, otherwise: fail
 */
uint32_t test_and_clear_hit_stat_batch(uint32_t entry)
{
    if ( (entry & 0x7FFFFFFF) < MAX_ROUTING_ENTRIES )
    {
        uint32_t ret;
        volatile u32 *phit;
        u32 hitbit;

        if ( (entry & 0x80000000) )
        {
            //  LAN
            entry &= 0x7FFFFFFF;
            if ( entry >= MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK )
            {
                phit = ROUT_LAN_COLL_HIT_STAT_TBL((entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK) >> 5);
                hitbit = 1 << (BITSIZEOF_UINT32 - 1 - ((entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK) & 0x1F));
            }
            else
            {
                phit = ROUT_LAN_HASH_HIT_STAT_TBL(entry >> 5);
                hitbit = 1 << (BITSIZEOF_UINT32 - 1 - (entry & 0x1F));
            }
        }
        else
        {
            //  WAN
            if ( entry >= MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK )
            {
                phit = ROUT_WAN_COLL_HIT_STAT_TBL((entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK) >> 5);
                hitbit = 1 << (BITSIZEOF_UINT32 - 1 - ((entry - MAX_ROUTING_ENTRIES_PER_HASH_BLOCK * MAX_HASH_BLOCK) & 0x1F));
            }
            else
            {
                phit = ROUT_WAN_HASH_HIT_STAT_TBL(entry >> 5);
                hitbit = 1 << (BITSIZEOF_UINT32 - 1 - (entry & 0x1F));
            }
        }

        ret = *phit;
        *phit = 0;

        return ret;
    }
    else
        return 0;
}

uint32_t test_and_clear_mc_hit_stat(uint32_t entry)
{
    if ( entry < MAX_WAN_MC_ENTRIES )
    {
        uint32_t ret;
        uint32_t bit;

        bit = 1 << (BITSIZEOF_UINT32 - 1 - (entry & 0x1F));
        ret = *ROUT_WAN_MC_HIT_STAT_TBL(entry >> 5);
        *ROUT_WAN_MC_HIT_STAT_TBL(entry >> 5) = ret & ~bit;
        ret &= bit;

        return ret;
    }
    else
        return 0;
}

uint32_t test_and_clear_mc_hit_stat_batch(uint32_t entry)
{
    if ( entry < MAX_WAN_MC_ENTRIES )
    {
        uint32_t block = entry >> 5;
        uint32_t ret = *ROUT_WAN_MC_HIT_STAT_TBL(block);

        *ROUT_WAN_MC_HIT_STAT_TBL(block) = 0;

        return ret;
    }
    else
        return 0;
}

uint32_t test_and_clear_bridging_hit_stat(uint32_t entry)
{
    return 0;
}

uint32_t test_and_clear_bridging_hit_stat_batch(uint32_t entry)
{
    return 0;
}

int32_t get_max_vfilter_entries(uint32_t vfilter_type)
{
    return 0;
}

#ifdef CONFIG_PPA_QOS
#define PPA_DRV_QOS_RATESHAPE_bitrate_2_R( bitrate_kbps, T, basic_tick) ( (bitrate_kbps) * (T) * (basic_tick) /8/1000 )
#define PPA_DRV_QOS_RATESHAPE_R_2_bitrate( R, T, basic_tick) ( (R) * 8 * 1000 / (T) / (basic_tick) )

struct bitrate_2_t_kbps
{
    uint32_t bitrate_kbps;
    uint32_t T; //Time ticks
};

struct bitrate_2_t_kbps bitrate_2_t_kbps_table[]={
    {  100000,   1},
    {   10000,  10},
    {    1000, 100},
    {       0, 250}
};


static int32_t get_basic_time_tick(void)
{
    return TX_QOS_CFG->time_tick / (cgu_get_pp32_clock() /1000000);
}

/*!
    \brief This is to get the maximum queue number supported on specified port
    \param[in] portid the physical port id which support qos queue
    \param[in] flag reserved for future
    \return returns the queue number supported on this port.
*/
int32_t get_qos_qnum( uint32_t portid, uint32_t flag)
{
    if( set_qos_port_id() != PPA_SUCCESS ) return 0;

    if ( qos_queue_portid != portid )
        return 0;

    return TX_QOS_CFG->eth1_eg_qnum;
}

int32_t get_qos_status( PPA_QOS_STATUS *stat)
{
    struct wtx_eg_q_shaping_cfg qos_cfg;
    struct wtx_qos_q_mib_table qos_queue_mib;
    volatile struct tx_qos_cfg tx_qos_cfg = *TX_QOS_CFG;
    volatile struct wtx_qos_q_desc_cfg qos_q_desc_cfg[PPE_MAX_ETH1_QOS_QUEUE];

    if( !stat ) return PPA_FAILURE;

    stat->qos_queue_portid = qos_queue_portid;

    if( set_qos_port_id() != PPA_SUCCESS )
    {
        return PPA_SUCCESS;
    }

    stat->wan_port_map = *CFG_WAN_PORTMAP;
    stat->wan_mix_map = *CFG_MIXED_PORTMAP;
    stat->eth1_qss = tx_qos_cfg.eth1_qss;
    stat->wfq_en = tx_qos_cfg.wfq_en;
    stat->shape_en = tx_qos_cfg.shape_en;
    stat->time_tick= tx_qos_cfg.time_tick;
    stat->overhd_bytes = tx_qos_cfg.overhd_bytes;
    stat->eth1_eg_qnum = tx_qos_cfg.eth1_eg_qnum;
    stat->tx_qos_cfg_addr = (uint32_t )TX_QOS_CFG;

    stat->pp32_clk = cgu_get_pp32_clock();
    stat->basic_time_tick = get_basic_time_tick();



#ifdef CONFIG_PPA_QOS_WFQ
    stat->wfq_multiple = wfq_multiple;
    stat->wfq_strict_pri_weight_addr = (uint32_t )&wfq_multiple;
#endif

    if ( tx_qos_cfg.eth1_eg_qnum )
    {
        uint32_t bit_rate_kbps=0;
        uint32_t weight_level=0;
        int i;

        for ( i = 0; i < PPE_MAX_ETH1_QOS_QUEUE && i<=stat->max_buffer_size; i++ )
        {
            qos_cfg = *WTX_EG_Q_SHAPING_CFG(i);
#ifdef CONFIG_PPA_QOS_RATE_SHAPING
            get_qos_rate( qos_queue_portid, i, &bit_rate_kbps, NULL,0);
            stat->queue_internal[i].bit_rate_kbps = bit_rate_kbps;
#endif
#ifdef CONFIG_PPA_QOS_WFQ
            get_qos_wfq( qos_queue_portid, i, &weight_level, 0);
            stat->queue_internal[i].weight_level = weight_level;
#endif

            stat->queue_internal[i].t = qos_cfg.t;
            stat->queue_internal[i].r = qos_cfg.r;
            stat->queue_internal[i].s = qos_cfg.s;

            stat->queue_internal[i].w = qos_cfg.w;
            stat->queue_internal[i].d = qos_cfg.d;
            stat->queue_internal[i].tick_cnt = qos_cfg.tick_cnt;
            stat->queue_internal[i].b = qos_cfg.b;

            stat->queue_internal[i].reg_addr = (uint32_t )WTX_EG_Q_SHAPING_CFG(i);
        }

        //QOS Note: For ethernat wan mode only. For E5 ptm mode, there are two bare channel 0 & 1 port rateshaping
        {
            qos_cfg = *WTX_EG_Q_PORT_SHAPING_CFG(0);

#ifdef CONFIG_PPA_QOS_RATE_SHAPING
            get_qos_rate( qos_queue_portid, i, &bit_rate_kbps, NULL,0);
            stat->qos_port_rate_internal.bit_rate_kbps = bit_rate_kbps;
#endif

            stat->qos_port_rate_internal.t = qos_cfg.t;
            stat->qos_port_rate_internal.r = qos_cfg.r;
            stat->qos_port_rate_internal.s = qos_cfg.s;

            stat->qos_port_rate_internal.w = qos_cfg.w;
            stat->qos_port_rate_internal.d = qos_cfg.d;
            stat->qos_port_rate_internal.tick_cnt = qos_cfg.tick_cnt;
            stat->qos_port_rate_internal.b = qos_cfg.b;

            stat->qos_port_rate_internal.reg_addr = (uint32_t)WTX_EG_Q_PORT_SHAPING_CFG(0);
        }

        if (qos_queue_portid == PPA_WAN_QOS_PTM0){
            for( i = 0; i < MAX_PTM_QOS_PORT_NUM; i ++){
                qos_cfg = *WTX_PTM_EG_Q_PORT_SHAPING_CFG(i);
#ifdef CONFIG_PPA_QOS_RATE_SHAPING
                get_qos_rate( qos_queue_portid, ~i, &bit_rate_kbps, NULL,0);
                stat->ptm_qos_port_rate_shaping[i].bit_rate_kbps = bit_rate_kbps;
#endif
                stat->ptm_qos_port_rate_shaping[i].t = qos_cfg.t;
                stat->ptm_qos_port_rate_shaping[i].r = qos_cfg.r;
                stat->ptm_qos_port_rate_shaping[i].s = qos_cfg.s;

                stat->ptm_qos_port_rate_shaping[i].w = qos_cfg.w;
                stat->ptm_qos_port_rate_shaping[i].d = qos_cfg.d;
                stat->ptm_qos_port_rate_shaping[i].tick_cnt = qos_cfg.tick_cnt;
                stat->ptm_qos_port_rate_shaping[i].b = qos_cfg.b;

                stat->ptm_qos_port_rate_shaping[i].reg_addr = (uint32_t)WTX_PTM_EG_Q_PORT_SHAPING_CFG(0);
            }
        }

        for ( i = 0; i < PPE_MAX_ETH1_QOS_QUEUE && i<=stat->max_buffer_size; i++ )
        {
            qos_queue_mib = *WTX_QOS_Q_MIB_TABLE(i);
            stat->mib[i].mib.total_rx_pkt= qos_queue_mib.wrx_total_pdu;
            stat->mib[i].mib.total_rx_bytes  = qos_queue_mib.wrx_total_bytes;
            stat->mib[i].mib.total_tx_pkt  = qos_queue_mib.wtx_total_pdu;
            stat->mib[i].mib.total_tx_bytes  = qos_queue_mib.wtx_total_bytes;
            stat->mib[i].mib.cpu_path_small_pkt_drop_cnt  = qos_queue_mib.wtx_cpu_drop_small_pdu;
            stat->mib[i].mib.cpu_path_total_pkt_drop_cnt  = qos_queue_mib.wtx_cpu_drop_pdu;
            stat->mib[i].mib.fast_path_small_pkt_drop_cnt  = qos_queue_mib.wtx_fast_drop_small_pdu;
            stat->mib[i].mib.fast_path_total_pkt_drop_cnt  = qos_queue_mib.wtx_fast_drop_pdu;

            stat->mib[i].reg_addr = (uint32_t)WTX_QOS_Q_MIB_TABLE(i);
        }


        //QOS queue descriptor
        for(i=0;  i < PPE_MAX_ETH1_QOS_QUEUE && i<=stat->max_buffer_size; i++)
        {
            qos_q_desc_cfg[i] = *WTX_QOS_Q_DESC_CFG(i);

            stat->desc_cfg_interanl[i].threshold = qos_q_desc_cfg[i].threshold;
            stat->desc_cfg_interanl[i].length = qos_q_desc_cfg[i].length;
            stat->desc_cfg_interanl[i].addr = qos_q_desc_cfg[i].addr;
            stat->desc_cfg_interanl[i].rd_ptr = qos_q_desc_cfg[i].rd_ptr;
            stat->desc_cfg_interanl[i].wr_ptr = qos_q_desc_cfg[i].wr_ptr;

            stat->desc_cfg_interanl[i].reg_addr = (uint32_t)WTX_QOS_Q_DESC_CFG(i);
        }
    }

    return PPA_SUCCESS;
}

/*!
    \brief This is to get the mib couter for specified port and queue
    \param[in] portid the physical port id
    \param[in] queueid the queueid for the mib counter to get
    \param[out] mib the buffer for mib counter
    \param[in] flag reserved for future
    \return returns the queue number supported on this port.
*/
int32_t get_qos_mib(uint32_t portid, uint32_t queueid, PPA_QOS_MIB *mib, uint32_t flag)
{
    if( set_qos_port_id() != PPA_SUCCESS ) return PPA_FAILURE; //QOS Note: Different condition with different mode/platfrom

    if ( !mib )
        return PPA_FAILURE;
    if ( qos_queue_portid != portid )
        return PPA_FAILURE;

    if ( queueid >= TX_QOS_CFG->eth1_eg_qnum )
        return PPA_FAILURE;

  #if 1
    {
        struct wtx_qos_q_mib_table qos_queue_mib;

        qos_queue_mib = *WTX_QOS_Q_MIB_TABLE(queueid);

        mib->total_rx_pkt   = qos_queue_mib.wrx_total_pdu;
        mib->total_rx_bytes = qos_queue_mib.wrx_total_bytes;
        mib->total_tx_pkt   = qos_queue_mib.wtx_total_pdu;
        mib->total_tx_bytes = qos_queue_mib.wtx_total_bytes;

        mib->cpu_path_small_pkt_drop_cnt    = qos_queue_mib.wtx_cpu_drop_small_pdu;
        mib->cpu_path_total_pkt_drop_cnt    = qos_queue_mib.wtx_cpu_drop_pdu;
        mib->fast_path_small_pkt_drop_cnt   = qos_queue_mib.wtx_fast_drop_small_pdu;
        mib->fast_path_total_pkt_drop_cnt   = qos_queue_mib.wtx_fast_drop_pdu;
    }
  #else
    {
        struct wtx_mib_e1 qos_queue_mib;

        qos_queue_mib = *(struct wtx_mib_e1 *)WTX_MIB_TBL(queueid);

        mib->total_rx_pkt   = 0;
        mib->total_rx_bytes = 0;
        mib->total_tx_pkt   = qos_queue_mib.total_pdu;
        mib->total_tx_bytes = qos_queue_mib.total_bytes;

        mib->cpu_path_small_pkt_drop_cnt    = qos_queue_mib.wtx_cpu_drop_small_pdu;
        mib->cpu_path_total_pkt_drop_cnt    = qos_queue_mib.wtx_cpu_drop_pdu;
        mib->fast_path_small_pkt_drop_cnt   = qos_queue_mib.wtx_fast_drop_small_pdu;
        mib->fast_path_total_pkt_drop_cnt   = qos_queue_mib.wtx_fast_drop_pdu;
    }
  #endif

    return PPA_SUCCESS;
}

#ifdef CONFIG_PPA_QOS_RATE_SHAPING
/*!
    \brief This is to eanble/disable Rate Shaping feature
    \param[in] portid the phisical port id which support qos queue
    \param[in] enabled 1:enable 0: disable
    \param[in] flag reserved for future
    \return The return value can be any one of the following:  \n
               - PPA_SUCCESS on success \n
               - PPA_FAILURE on error \n
*/
int32_t set_ctrl_qos_rate( uint32_t portid, uint32_t enable, uint32_t flag)
{
    struct tx_qos_cfg tx_qos_cfg = {0};

    if( set_qos_port_id() != PPA_SUCCESS )  return PPA_FAILURE; //QOS Note: Different condition with different mode/platfrom

    if ( qos_queue_portid != portid )
        return PPA_FAILURE;
    tx_qos_cfg = *TX_QOS_CFG;
    if ( !tx_qos_cfg.eth1_qss )
        return PPA_FAILURE;

    if ( enable )
    {
        tx_qos_cfg.shape_en = 1;
        tx_qos_cfg.overhd_bytes = OVERSIZE_HEADER_LEN; //add 24 bytes preamble and inter-frame gape
    }
    else
    {
        tx_qos_cfg.shape_en = 0;

        if ( !tx_qos_cfg.shape_en && !tx_qos_cfg.wfq_en )
        {
            //tx_qos_cfg.eth1_qss = 1;
            //tx_qos_cfg.eth1_eg_qnum = 0;
        }
    }

    *TX_QOS_CFG = tx_qos_cfg;

    return PPA_SUCCESS;
}

/*!
    \brief This is to get Rate Shaping feature status: enabled or disabled
    \param[in] portid the phisical port id which support qos queue
    \param[out] enabled 1:enable 0: disable
    \param[in] flag reserved for future
    \return The return value can be any one of the following:  \n
               - PPA_SUCCESS on success \n
               - PPA_FAILURE on error \n
*/
int32_t get_ctrl_qos_rate( uint32_t portid, uint32_t *enable, uint32_t flag)
{
    if( set_qos_port_id() != PPA_SUCCESS )  return PPA_FAILURE; //QOS Note: Different condition with different mode/platfrom

    if ( qos_queue_portid != portid )
        return PPA_FAILURE;

    if ( enable )
    {
        if ( TX_QOS_CFG->shape_en )
            *enable = 1;
        else
            *enable =0;
    }

    return PPA_SUCCESS;
}

/*!
    \brief This is to set Rate Shaping for one specified port and queue
    \param[in] portid the phisical port id which support qos queue
    \param[in] queueid the queue id need to set rate shaping. \n
              If queue id bigger than muximum queue id, it will be regarded as port based rate shaping.
    \param[in] rate the maximum rate limit in kbps
    \param[in] burst the maximun burst in bytes
    \param[in] flag reserved for future
    \return The return value can be any one of the following:  \n
               - PPA_SUCCESS on success \n
               - PPA_FAILURE on error \n
*/
int32_t set_qos_rate( uint32_t portid, uint32_t queueid, uint32_t rate, uint32_t burst, uint32_t flag)
{
    int i;
    struct wtx_eg_q_shaping_cfg qos_cfg = {0};
    volatile struct wtx_eg_q_shaping_cfg *p_qos_cfg = NULL;
    uint32_t qos_portid;//port rateshaping port id

    if( set_qos_port_id() != PPA_SUCCESS ) return PPA_FAILURE; //QOS Note: Different condition with different mode/platfrom

    if ( qos_queue_portid!= portid )
        return PPA_FAILURE;

    if ( queueid >= TX_QOS_CFG->eth1_eg_qnum )  //regard it as port based rate shaping
    {
        if( qos_queue_portid == PPA_WAN_QOS_PTM0){//ptm_wan_port;
            qos_portid = ~ queueid;
            if(qos_portid >= MAX_PTM_QOS_PORT_NUM){
                return PPA_FAILURE;
            }
            qos_cfg = *WTX_PTM_EG_Q_PORT_SHAPING_CFG(qos_portid);
            p_qos_cfg = WTX_PTM_EG_Q_PORT_SHAPING_CFG(qos_portid);
        }else{//eth_wan_port
            qos_portid = 0;
            qos_cfg = *WTX_EG_Q_PORT_SHAPING_CFG(qos_portid);
            p_qos_cfg = WTX_EG_Q_PORT_SHAPING_CFG(qos_portid);
        }
    }
    else
    {
        qos_cfg = *WTX_EG_Q_SHAPING_CFG(queueid);
        p_qos_cfg = WTX_EG_Q_SHAPING_CFG(queueid);
    }

    if ( rate >= bitrate_2_t_kbps_table[0].bitrate_kbps )
    {
        qos_cfg.t = bitrate_2_t_kbps_table[0].T;
    }
    else
    {
        for( i = 0; i < sizeof(bitrate_2_t_kbps_table) / sizeof(bitrate_2_t_kbps_table[0]) - 1; i++ )
        {
            if ( rate < bitrate_2_t_kbps_table[i].bitrate_kbps && rate >= bitrate_2_t_kbps_table[i+1].bitrate_kbps )
            {
                qos_cfg.t = bitrate_2_t_kbps_table[i+1].T;
                break;
            }
        }
    }
    if ( qos_cfg.t == 0 )
    {
        return PPA_FAILURE;
    }
    if( burst == 0 )
    {
        burst = default_qos_rateshaping_burst;
    }

    qos_cfg.r = PPA_DRV_QOS_RATESHAPE_bitrate_2_R( rate, qos_cfg.t, get_basic_time_tick() );
    qos_cfg.s = burst;

    *p_qos_cfg = qos_cfg;

    return PPA_SUCCESS;
}

/*!
    \brief This is to get Rate Shaping settings for one specified port and queue
    \param[in] portid the phisical port id which support qos queue
    \param[in] queueid the queue id need to set rate shaping \n
              If queue id bigger than muximum queue id, it will be regarded as port based rate shaping.
    \param[out] rate the maximum rate limit in kbps
    \param[out] burst the maximun burst in bytes
    \param[in] flag reserved for future
    \return The return value can be any one of the following:  \n
               - PPA_SUCCESS on success \n
               - PPA_FAILURE on error \n
*/
int32_t get_qos_rate( uint32_t portid, uint32_t queueid, uint32_t *rate, uint32_t *burst, uint32_t flag)
{
    struct wtx_eg_q_shaping_cfg qos_cfg = {0};

    if( set_qos_port_id() != PPA_SUCCESS ) return PPA_FAILURE; //A5 Ethernet WAN mode only QOS Note: Different condition with different mode/platfrom

    if ( qos_queue_portid != portid )
        return PPA_FAILURE;

    if ( queueid >= TX_QOS_CFG->eth1_eg_qnum )
    {
        if( qos_queue_portid == PPA_WAN_QOS_PTM0) {
            queueid = ~queueid;
            qos_cfg = *WTX_PTM_EG_Q_PORT_SHAPING_CFG(queueid);
        }else{
            //otherwise it is E5 ethernet wan mode, so regard it as port based rate shaping, not queue based rate shaping
            qos_cfg = *WTX_EG_Q_PORT_SHAPING_CFG(0);
        }
    }
    else
        qos_cfg = *WTX_EG_Q_SHAPING_CFG(queueid); //queue based rate shaping

    if ( qos_cfg.t != 0 )   //not set yet
    {
        if ( rate )
            *rate = PPA_DRV_QOS_RATESHAPE_R_2_bitrate(qos_cfg.r, qos_cfg.t, get_basic_time_tick());
    }
    else
    {
        if ( rate )
            *rate = 0;
    }
    if ( burst )
        *burst = qos_cfg.s;

    return PPA_SUCCESS;
}

/*!
    \brief This is to reset Rate Shaping for one specified port and queue (
    \param[in] portid the phisical port id which support qos queue
    \param[in] queueid the queue id need to set rate shaping
    \param[in] flag reserved for future
    \return The return value can be any one of the following:  \n
               - PPA_SUCCESS on success \n
               - PPA_FAILURE on error \n
*/
int32_t reset_qos_rate( uint32_t portid, uint32_t queueid, uint32_t flag )
{
    if( set_qos_port_id() != PPA_SUCCESS ) return PPA_FAILURE;
    if ( qos_queue_portid != portid )
        return PPA_FAILURE;
    set_qos_rate(portid, queueid, 1000000, default_qos_rateshaping_burst, flag);
    return PPA_SUCCESS;
}

int32_t init_qos_rate(void)
{
    int i;

    if( set_qos_port_id() != PPA_SUCCESS ) return PPA_FAILURE; //QOS Note: Different condition with different mode/platfrom

    for ( i = 0; i <= get_qos_qnum(qos_queue_portid, 0); i++ )  //here we purposely use <= to set port based rate sahping also
        reset_qos_rate(qos_queue_portid, i, 0);

    if(qos_queue_portid == PPA_WAN_QOS_PTM0){
        for( i = 0; i < MAX_PTM_QOS_PORT_NUM; i ++){
            reset_qos_rate(qos_queue_portid, ~i, 0); //set ~i to indicate it is not a queueid
        }
    }

    return PPA_SUCCESS;
}

 #endif /*end of CONFIG_PPA_QOS_RATE_SHAPING*/

 #ifdef CONFIG_PPA_QOS_WFQ

/*!
    \brief This is to eanble/disable WFQ feature
    \param[in] portid the phisical port id which support qos queue
    \param[in] enabled 1:enable 0: disable
    \param[in] flag reserved for future
    \return The return value can be any one of the following:  \n
               - PPA_SUCCESS on success \n
               - PPA_FAILURE on error \n
*/
int32_t set_ctrl_qos_wfq( uint32_t portid, uint32_t enable, uint32_t flag)
{

    struct tx_qos_cfg tx_qos_cfg = {0};

    if( set_qos_port_id() != PPA_SUCCESS ) return PPA_FAILURE; //QOS Note: Different condition with different mode/platfrom

    if ( qos_queue_portid != portid )
        return PPA_FAILURE;
    tx_qos_cfg = *TX_QOS_CFG;

    if ( enable )
    {
        tx_qos_cfg.wfq_en= 1;
        tx_qos_cfg.overhd_bytes = OVERSIZE_HEADER_LEN; //add 24 bytes preamble and inter-frame gape
        //tx_qos_cfg.eth1_qss = 1;

        if( flag != 0 )
            wfq_multiple= flag;
        else
            wfq_multiple= PPA_DRV_QOS_WFQ_WLEVEL_2_W;
    }
    else
    {
        tx_qos_cfg.wfq_en = 0;

        if ( !tx_qos_cfg.shape_en && !tx_qos_cfg.wfq_en )
        {
            //tx_qos_cfg.eth1_qss = 1;
            //tx_qos_cfg.eth1_eg_qnum = 0;
        }
    }

    *TX_QOS_CFG = tx_qos_cfg;

    return PPA_SUCCESS;
}

/*!
    \brief This is to get WFQ feature status: enabled or disabled
    \param[in] portid the phisical port id which support qos queue
    \param[out] enabled 1:enable 0: disable
    \param[in] flag reserved for future
    \return The return value can be any one of the following:  \n
               - PPA_SUCCESS on success \n
               - PPA_FAILURE on error \n
*/
int32_t get_ctrl_qos_wfq( uint32_t portid, uint32_t *enable, uint32_t flag)
{
    if( set_qos_port_id() != PPA_SUCCESS ) return PPA_FAILURE; //QOS Note: Different condition with different mode/platfrom

    if ( qos_queue_portid != portid )
        return PPA_FAILURE;
    if ( enable )
    {
        if ( TX_QOS_CFG->wfq_en )
            *enable = 1;
        else
            *enable =0;
    }

    return PPA_SUCCESS;
}

/*!
    \brief This is to set WFQ weight level for one specified port and queue
    \param[in] portid the phisical port id which support qos queue
    \param[in] queueid the queue id need to set WFQ
    \param[in] weight_level the value should be 0 ~ 100. It will be mapped to internal PPE FW WFQ real weight
    \param[in] flag reserved for future
    \return The return value can be any one of the following:  \n
               - PPA_SUCCESS on success \n
               - PPA_FAILURE on error \n
*/
int32_t set_qos_wfq( uint32_t portid, uint32_t queueid, uint32_t weight_level, uint32_t flag)
{
    struct wtx_eg_q_shaping_cfg qos_cfg = {0};

    if( set_qos_port_id() != PPA_SUCCESS ) return PPA_FAILURE; //QOS Note: Different condition with different mode/platfrom

    if ( qos_queue_portid != portid )
        return PPA_FAILURE;
    if ( queueid >= TX_QOS_CFG->eth1_eg_qnum )
        return PPA_FAILURE;

    qos_cfg = *WTX_EG_Q_SHAPING_CFG(queueid);

    if ( weight_level == 100 )
    {
        if( wfq_multiple != 1 )
            qos_cfg.w = wfq_strict_pri_weight;
        else
            qos_cfg.w = weight_level * wfq_multiple;
    }
    else
    {
        qos_cfg.w = weight_level * wfq_multiple;
    }

    *WTX_EG_Q_SHAPING_CFG(queueid) = qos_cfg;
    *TX_QOS_WFQ_RELOAD_MAP |= 1 << queueid;
    return PPA_SUCCESS;
}

/*!
    \brief This is to get WFQ settings for one specified port and queue ( default value should be 0xFFFF)
    \param[in] portid the phisical port id which support qos queue
    \param[in] queueid the queue id need to set WFQ
    \param[out] weight_level the value should be 0 ~ 100.
    \param[in] flag reserved for future
    \return The return value can be any one of the following:  \n
               - PPA_SUCCESS on success \n
               - PPA_FAILURE on error \n
*/
int32_t get_qos_wfq( uint32_t portid, uint32_t queueid, uint32_t *weight_level, uint32_t flag)
{
    struct wtx_eg_q_shaping_cfg qos_cfg = {0};

    if( set_qos_port_id() != PPA_SUCCESS ) return PPA_FAILURE; //QOS Note: Different condition with different mode/platfrom

    if ( qos_queue_portid != portid )
        return PPA_FAILURE;
    if ( queueid >= TX_QOS_CFG->eth1_eg_qnum )
        return PPA_FAILURE;
    if ( !weight_level )
        return PPA_FAILURE;

    qos_cfg = *WTX_EG_Q_SHAPING_CFG(queueid);

    if ( qos_cfg.w == wfq_strict_pri_weight )
    {
        if( wfq_multiple != 1 )
            *weight_level = 100;
        else
            *weight_level = qos_cfg.w / wfq_multiple;

    }
    else
    {
        *weight_level = qos_cfg.w / wfq_multiple;
    }

    return PPA_SUCCESS;
}

/*!
    \brief This is to reset WFQ for one specified port and queue ( default value should be 0xFFFF)
    \param[in] portid the phisical port id which support qos queue
    \param[in] queueid the queue id need to set WFQ
    \param[in] flag reserved for future
    \return The return value can be any one of the following:  \n
               - PPA_SUCCESS on success \n
               - PPA_FAILURE on error \n
*/
int32_t reset_qos_wfq( uint32_t portid, uint32_t queueid, uint32_t flag )
{
    return set_qos_wfq(portid, queueid, 100, flag);
}

int32_t init_qos_wfq(void)
{
    int i;

    if( set_qos_port_id() != PPA_SUCCESS ) return PPA_FAILURE; //QOS Note: Different condition with different mode/platfrom

    for ( i = 0; i < get_qos_qnum(qos_queue_portid, 0); i++ )
        reset_qos_wfq(qos_queue_portid, i, 0);

    return PPA_SUCCESS;
}

 #endif /*end of CONFIG_PPA_QOS_WFQ*/
#endif  /*end of CONFIG_PPA_QOS*/

int32_t ppa_drv_get_mib(PPA_DSL_QUEUE_MIB *buffer, uint32_t flag)
{
    return -1;
}

int32_t ppa_drv_clear_mib(PPA_DSL_QUEUE_MIB *buffer, uint32_t flag)
{
    return -1;
}

static INLINE uint32_t ppa_drv_get_phys_port_num(void)
{
        return 8;
}

static INLINE uint32_t set_wanitf(PPE_WANITF_CFG *wanitf_cfg, uint32_t flag)
{
    if( ! wanitf_cfg ) return PPA_FAILURE;

    if( wanitf_cfg->physical_port > ppa_drv_get_phys_port_num() ) return PPA_FAILURE;


    wanitf_cfg->old_lan_flag = ( (*CFG_WAN_PORTMAP) & ( 1<<wanitf_cfg->physical_port ) ) ? 0:1;
    dbg("old wanitf=%x with old_lan_flag=%d\n", *CFG_WAN_PORTMAP,  wanitf_cfg->old_lan_flag);
    if( wanitf_cfg->lan_flag )  //unset the bit for lan flag is enabled
        *CFG_WAN_PORTMAP &= ~( 1<<wanitf_cfg->physical_port );
    else // set the bit
        *CFG_WAN_PORTMAP |= ( 1<<wanitf_cfg->physical_port );


    dbg("new wanitf=%x\n for port %d with flag %s\n", *CFG_WAN_PORTMAP,  wanitf_cfg->physical_port, wanitf_cfg->lan_flag?"LAN":"WAN");
    return PPA_SUCCESS;
}

static int32_t set_variable_value(PPA_CMD_VARIABLE_VALUE_INFO *cfg, int32_t flag )
{
    int32_t res = PPA_SUCCESS;

    switch (cfg->id ) {
        case PPA_VARIABLE_EARY_DROP_FLAG:
            if( cfg->value ) 
                GEN_MODE_CFG1->us_early_discard_en = 1;
            else
                GEN_MODE_CFG1->us_early_discard_en = 0;
            break;

         default:
            res = PPA_FAILURE;
    }

    return res;
}


static int32_t get_variable_value(PPA_CMD_VARIABLE_VALUE_INFO *cfg, int32_t flag )
{
    int32_t res = PPA_SUCCESS;

    switch (cfg->id ) {
        case PPA_VARIABLE_EARY_DROP_FLAG:
            cfg->value = GEN_MODE_CFG1->us_early_discard_en;
            break;

         default:
            res = PPA_FAILURE;
    }

    return res;
}

static int32_t ppa_hal_generic_hook(PPA_GENERIC_HOOK_CMD cmd, void *buffer, uint32_t flag)
{
    switch (cmd)  {
    case PPA_GENERIC_HAL_GET_DSL_MIB:
        return ppa_drv_get_mib( (PPA_DSL_QUEUE_MIB *)buffer, flag);

    case PPA_GENERIC_HAL_CLEAR_DSL_MIB:
        return ppa_drv_clear_mib( (PPA_DSL_QUEUE_MIB *)buffer, flag);
    case PPA_GENERIC_WAN_INFO:
        {
            PPA_WAN_INFO *wan_info = (PPA_WAN_INFO *)buffer;
            wan_info->mixed = *CFG_MIXED_PORTMAP;
            wan_info->wan_port_map = *CFG_WAN_PORTMAP;
        }
        return PPA_SUCCESS;

    case PPA_GENERIC_HAL_GET_PORT_MIB:
        {
            int i=0;
            int num;
            PPA_PORT_MIB *mib = (PPA_PORT_MIB*) buffer;
            num = NUM_ENTITY(mib->mib_info) > ppa_drv_get_phys_port_num() ? ppa_drv_get_phys_port_num():NUM_ENTITY(mib->mib_info) ;
            for(i=0; i<num; i++)
            {
                mib->mib_info[i].ig_fast_brg_pkts = ITF_MIB_TBL(i)->ig_fast_brg_pkts;
                mib->mib_info[i].ig_fast_brg_bytes = ITF_MIB_TBL(i)->ig_fast_brg_bytes;
                mib->mib_info[i].ig_fast_rt_ipv4_udp_pkts = ITF_MIB_TBL(i)->ig_fast_rt_ipv4_udp_pkts;
                mib->mib_info[i].ig_fast_rt_ipv4_tcp_pkts = ITF_MIB_TBL(i)->ig_fast_rt_ipv4_tcp_pkts;
                mib->mib_info[i].ig_fast_rt_ipv4_mc_pkts = ITF_MIB_TBL(i)->ig_fast_rt_ipv4_mc_pkts;
                mib->mib_info[i].ig_fast_rt_ipv4_bytes = ITF_MIB_TBL(i)->ig_fast_rt_ipv4_bytes;
                mib->mib_info[i].ig_fast_rt_ipv6_udp_pkts = ITF_MIB_TBL(i)->ig_fast_rt_ipv6_udp_pkts;
                mib->mib_info[i].ig_fast_rt_ipv6_tcp_pkts = ITF_MIB_TBL(i)->ig_fast_rt_ipv6_tcp_pkts;
                mib->mib_info[i].ig_fast_rt_ipv6_mc_pkts = ITF_MIB_TBL(i)->ig_fast_rt_ipv6_mc_pkts;
                mib->mib_info[i].ig_fast_rt_ipv6_bytes = ITF_MIB_TBL(i)->ig_fast_rt_ipv6_bytes;
                mib->mib_info[i].ig_cpu_pkts = ITF_MIB_TBL(i)->ig_cpu_pkts;
                mib->mib_info[i].ig_cpu_bytes = ITF_MIB_TBL(i)->ig_cpu_bytes;
                mib->mib_info[i].ig_drop_pkts = ITF_MIB_TBL(i)->ig_drop_pkts;
                mib->mib_info[i].ig_drop_bytes = ITF_MIB_TBL(i)->ig_drop_bytes;
                mib->mib_info[i].eg_fast_pkts = ITF_MIB_TBL(i)->eg_fast_pkts;

                if( i== 0 || i == 1 )
                    mib->mib_info[i].port_flag = PPA_PORT_MODE_ETH;
                else if( i== 7 )
                    mib->mib_info[i].port_flag = PPA_PORT_MODE_DSL;
                else if( i == 2)  // 2 is CPU port
                    mib->mib_info[i].port_flag = PPA_PORT_MODE_CPU;
                else
                    mib->mib_info[i].port_flag = PPA_PORT_MODE_EXT;

            }
            mib->port_num = num;
            dbg("port_num=%d\n", mib->port_num);
            return PPA_SUCCESS;
        }

    case PPA_GENERIC_HAL_SET_DEBUG:
        {
            ppa_hal_dbg_enable = *(unsigned int *) buffer;
            dbg("Set ppa_hal_dbg_enable to 0x%x\n", ppa_hal_dbg_enable );
        }
        return PPA_SUCCESS;

    case PPA_GENERIC_HAL_GET_FEATURE_LIST:
        {
            PPA_FEATURE_INFO *feature = (PPA_FEATURE_INFO *) buffer;

            feature->ipv6_en = GEN_MODE_CFG1->ipv6_acc_en ? 1 : 0 ;
            feature->qos_en= TX_QOS_CFG->eth1_eg_qnum;
            return PPA_SUCCESS;
        }

   case PPA_GENERIC_HAL_GET_MAX_ENTRIES:
        {
            PPA_MAX_ENTRY_INFO *entry=(PPA_MAX_ENTRY_INFO *)buffer;

            entry->max_lan_entries = MAX_ROUTING_ENTRIES/2;
            entry->max_wan_entries = MAX_ROUTING_ENTRIES/2;
            entry->max_mc_entries = MAX_WAN_MC_ENTRIES;
            entry->max_bridging_entries = MAX_BRIDGING_ENTRIES;
            entry->max_6rd_entries = MAX_6RD_TUNNEL_ENTRIES;
            entry->max_dslite_entries = MAX_DSLITE_TUNNEL_ENTRIES;
            entry->max_lan_collision_entries = MAX_COLLISION_ROUTING_ENTRIES;
            entry->max_wan_collision_entries = MAX_COLLISION_ROUTING_ENTRIES;
            entry->session_hash_table_num = 2; //LAN/WAN
            entry->max_lan_hash_index_num = MAX_HASH_BLOCK;
            entry->max_wan_hash_index_num = MAX_HASH_BLOCK;
            entry->max_lan_hash_bucket_num = MAX_ROUTING_ENTRIES_PER_HASH_BLOCK;
            entry->max_wan_hash_bucket_num = MAX_ROUTING_ENTRIES_PER_HASH_BLOCK;
            if( GEN_MODE_CFG1->ipv6_acc_en  )
                entry->max_ipv6_addr_entries =  MAX_IPV6_IP_ENTRIES_PER_BLOCK* MAX_IPV6_IP_ENTRIES_BLOCK;
            else
                entry->max_ipv6_addr_entries = 0;
            entry->max_fw_queue= TX_QOS_CFG->eth1_eg_qnum;

            return PPA_SUCCESS;
        }
//Fix warning message when exports API from different PPE FW Driver--begin
   case PPA_GENERIC_HAL_GET_HAL_VERSION:
         {
            PPA_VERSION *v=(PPA_VERSION *)buffer;
            get_ppe_hal_id( &v->family, &v->type,&v->itf, &v->mode, &v->major, &v->mid, &v->minor );
            return PPA_SUCCESS;
         }

   case PPA_GENERIC_HAL_GET_PPE_FW_VERSION:
         {
            PPA_VERSION *v=(PPA_VERSION *)buffer;
            return get_firmware_id(v->index, &v->family, &v->type,&v->major, &v->mid, &v->minor );
         }


   case PPA_GENERIC_HAL_GET_PHYS_PORT_NUM:
         {
            PPE_COUNT_CFG *count=(PPE_COUNT_CFG *)buffer;
            count->num = get_number_of_phys_port();
            return PPA_SUCCESS;
         }

   case PPA_GENERIC_HAL_GET_PHYS_PORT_INFO:
    {
        PPE_IFINFO *info = (PPE_IFINFO *) buffer;
        get_phys_port_info(info->port, &info->if_flags, info->ifname);
        return PPA_SUCCESS;

    }

   case PPA_GENERIC_HAL_SET_MIX_WAN_VLAN_ID:
       {
           PPE_WAN_VID_RANGE *range_vlan=(PPE_WAN_VID_RANGE *)buffer;
           set_wan_vlan_id(range_vlan->vid);
           return PPA_SUCCESS;
       }

   case PPA_GENERIC_HAL_GET_MIX_WAN_VLAN_ID:
       {
           PPE_WAN_VID_RANGE *range_vlan=(PPE_WAN_VID_RANGE *)buffer;
           range_vlan->vid = get_wan_vlan_id();
           return PPA_SUCCESS;
       }

   case PPA_GENERIC_HAL_SET_ROUT_CFG:
       {
           PPE_ROUTING_CFG *cfg=(PPE_ROUTING_CFG *)buffer;

           set_route_cfg(cfg->f_is_lan, cfg->entry_num, cfg->mc_entry_num, cfg->f_ip_verify, cfg->f_tcpudp_verify,
                                    cfg->f_tcpudp_err_drop, cfg->f_drop_on_no_hit, cfg->f_mc_drop_on_no_hit, cfg->flags);
           return PPA_SUCCESS;
       }

   case PPA_GENERIC_HAL_SET_BRDG_CFG:
       {
           PPE_BRDG_CFG *cfg=(PPE_BRDG_CFG *)buffer;

           set_bridging_cfg( cfg->entry_num,
                       cfg->br_to_src_port_mask,  cfg->br_to_src_port_en,
                       cfg->f_dest_vlan_en,
                       cfg->f_src_vlan_en,
                       cfg->f_mac_change_drop,
                       cfg->flags);

           return PPA_SUCCESS;
       }

    case PPA_GENERIC_HAL_SET_FAST_MODE_CFG:
       {
           PPE_FAST_MODE_CFG *cfg=(PPE_FAST_MODE_CFG *)buffer;

            set_fast_mode( cfg->mode, cfg->flags);

           return PPA_SUCCESS;
       }

    case PPA_GENERIC_HAL_SET_DEST_LIST:
    {
             PPE_DEST_LIST *dst =(PPE_DEST_LIST *)buffer;
            set_default_dest_list(dst->uc_dest_list, dst->mc_dest_list, dst->if_no );
            return PPA_SUCCESS;
     }

    case PPA_GENERIC_HAL_SET_ACC_ENABLE:
       {
           PPE_ACC_ENABLE *cfg=(PPE_ACC_ENABLE *)buffer;

            set_acc_mode( cfg->f_is_lan, cfg->f_enable);

            return PPA_SUCCESS;
        }

#if defined(MIB_MODE_ENABLE) && MIB_MODE_ENABLE

    case PPA_GENERIC_HAL_SET_MIB_MODE_ENABLE:
        {
            PPE_MIB_MODE_ENABLE *cfg=(PPE_MIB_MODE_ENABLE *)buffer;

            set_mib_mode( cfg->session_mib_unit);

            return PPA_SUCCESS;
        }

    case PPA_GENERIC_HAL_GET_MIB_MODE_ENABLE:
        {
            PPE_MIB_MODE_ENABLE *cfg=(PPE_MIB_MODE_ENABLE *)buffer;

            get_mib_mode( &cfg->session_mib_unit);

            return PPA_SUCCESS;
        }

#endif

    case PPA_GENERIC_HAL_GET_ACC_ENABLE:
       {
           PPE_ACC_ENABLE *cfg=(PPE_ACC_ENABLE *)buffer;

            get_acc_mode( cfg->f_is_lan, &cfg->f_enable);

           return PPA_SUCCESS;
       }


    case PPA_GENERIC_HAL_GET_BRDG_VLAN_CFG:
       {
           PPE_BRDG_VLAN_CFG *cfg=(PPE_BRDG_VLAN_CFG *)buffer;

            get_bridge_if_vlan_config( cfg->if_no,
                                                        &cfg->f_eg_vlan_insert,
                                                        &cfg->f_eg_vlan_remove,
                                                        &cfg->f_ig_vlan_aware,
                                                        &cfg->f_ig_src_ip_based,
                                                        &cfg->f_ig_eth_type_based,
                                                        &cfg->f_ig_vlanid_based,
                                                        &cfg->f_ig_port_based,
                                                        &cfg->f_eg_out_vlan_insert,
                                                        &cfg->f_eg_out_vlan_remove,
                                                        &cfg->f_ig_out_vlan_aware );

           return PPA_SUCCESS;
       }

    case PPA_GENERIC_HAL_SET_BRDG_VLAN_CFG:
       {
            PPE_BRDG_VLAN_CFG *cfg=(PPE_BRDG_VLAN_CFG *)buffer;

            set_bridge_if_vlan_config( cfg->if_no,
                                                        cfg->f_eg_vlan_insert,
                                                        cfg->f_eg_vlan_remove,
                                                        cfg->f_ig_vlan_aware,
                                                        cfg->f_ig_src_ip_based,
                                                        cfg->f_ig_eth_type_based,
                                                        cfg->f_ig_vlanid_based,
                                                        cfg->f_ig_port_based,
                                                        cfg->f_eg_out_vlan_insert,
                                                        cfg->f_eg_out_vlan_remove,
                                                        cfg->f_ig_out_vlan_aware );
           return PPA_SUCCESS;
       }

    case PPA_GENERIC_HAL_ADD_BRDG_VLAN_FITLER:
       {
           PPE_BRDG_VLAN_FILTER_MAP *filter=(PPE_BRDG_VLAN_FILTER_MAP *)buffer;

            return add_vlan_map( filter->ig_criteria_type,
                                     filter->ig_criteria,
                                     filter->new_vci,
                                     filter->dest_qos,
                                     filter->out_vlan_info.vlan_entry,
                                     filter->in_out_etag_ctrl,
                                     filter->vlan_port_map);
       }


    case PPA_GENERIC_HAL_DEL_BRDG_VLAN_FITLER:
       {
           PPE_BRDG_VLAN_FILTER_MAP *filter=(PPE_BRDG_VLAN_FILTER_MAP *)buffer;

            del_vlan_map( filter->ig_criteria_type, filter->ig_criteria);
            return PPA_SUCCESS;
       }

    case PPA_GENERIC_HAL_GET_BRDG_VLAN_FITLER:
       {
           PPE_BRDG_VLAN_FILTER_MAP *filter=(PPE_BRDG_VLAN_FILTER_MAP *)buffer;

            return get_vlan_map( filter->ig_criteria_type,
                                     filter->entry,
                                     &filter->ig_criteria,
                                     &filter->new_vci,
                                     &filter->dest_qos,
                                     &filter->out_vlan_info.vlan_entry,
                                     &filter->in_out_etag_ctrl,
                                     &filter->vlan_port_map);
       }

     case PPA_GENERIC_HAL_DEL_BRDG_VLAN_ALL_FITLER_MAP:
       {
            del_all_vlan_map( );
            return PPA_SUCCESS;
       }
     case PPA_GENERIC_HAL_GET_MAX_VFILTER_ENTRY_NUM: //get the maxumum entry for vlan filter
        {
            PPE_VFILTER_COUNT_CFG *vlan_count=(PPE_VFILTER_COUNT_CFG *)buffer;
            vlan_count->num = get_max_vfilter_entries( vlan_count->vfitler_type );
            return PPA_SUCCESS;
        }
     case PPA_GENERIC_HAL_GET_IPV6_FLAG:
        {
            return is_ipv6_enabled();
        }
     case PPA_GENERIC_HAL_ADD_ROUTE_ENTRY:
       {
           PPE_ROUTING_INFO *route=(PPE_ROUTING_INFO *)buffer;

            if( !route->src_ip.f_ipv6 && !route->dst_ip.f_ipv6 && !route->new_ip.f_ipv6) //all are IPV4
            {  //need to check dslite later ????
                return add_routing_entry(route->f_is_lan,  route->src_ip.ip.ip, route->src_port, route->dst_ip.ip.ip, route->dst_port,
                          route->f_is_tcp, route->route_type, route->new_ip.ip.ip, route->new_port,
                          route->new_dst_mac, route->src_mac.mac_ix, route->mtu_info.mtu_ix, route->f_new_dscp_enable, route->new_dscp,
                          route->f_vlan_ins_enable, route->new_vci, route->f_vlan_rm_enable, route->pppoe_mode, route->pppoe_info.pppoe_ix,
                          route->f_out_vlan_ins_enable, route->out_vlan_info.vlan_entry, route->f_out_vlan_rm_enable,
                          route->dslwan_qid,  route->dest_list, route->tnnl_info.tunnel_idx, &route->entry, &route->collision_flag);
            }
            else if( route->src_ip.f_ipv6 && route->dst_ip.f_ipv6 ) //both are IPV6
            { //late need to check 6rd ???
                return add_ipv6_routing_entry(route->f_is_lan,  route->src_ip.ip.ip6, route->src_port, route->dst_ip.ip.ip6, route->dst_port,
                          route->f_is_tcp, route->route_type, route->new_ip.ip.ip, route->new_port,
                          route->new_dst_mac, route->src_mac.mac_ix, route->mtu_info.mtu_ix, route->f_new_dscp_enable, route->new_dscp,
                          route->f_vlan_ins_enable, route->new_vci, route->f_vlan_rm_enable, route->pppoe_mode, route->pppoe_info.pppoe_ix,
                          route->f_out_vlan_ins_enable, route->out_vlan_info.vlan_entry, route->f_out_vlan_rm_enable,
                          route->dslwan_qid,  route->dest_list, route->tnnl_info.tunnel_idx,&route->entry, &route->collision_flag);
            }
            else
            {
                dbg("ip type not match at all:%d/%d/%d(src_ip/dst_ip/new_ip))\n", route->src_ip.f_ipv6, route->dst_ip.f_ipv6, route->new_ip.f_ipv6);
                return PPA_FAILURE;
            }
        }

    case PPA_GENERIC_HAL_DEL_ROUTE_ENTRY:
       {
           PPE_ROUTING_INFO *route=(PPE_ROUTING_INFO *)buffer;
           del_routing_entry( route->entry );
           return PPA_SUCCESS;
        }
    case PPA_GENERIC_HAL_UPDATE_ROUTE_ENTRY:
       {
           PPE_ROUTING_INFO *route=(PPE_ROUTING_INFO *)buffer;
           return update_routing_entry( route->entry,  route->route_type, route->new_ip.ip.ip, route->new_port,
                             route->new_dst_mac, route->src_mac.mac_ix, route->mtu_info.mtu_ix,  route->f_new_dscp_enable, route->new_dscp,
                             route->f_vlan_ins_enable, route->new_vci, route->f_vlan_rm_enable, route->pppoe_mode, route->pppoe_info.pppoe_ix,
                             route->f_out_vlan_ins_enable, route->out_vlan_info.vlan_entry, route->f_out_vlan_rm_enable, route->dslwan_qid,
                             route->dest_list, route->update_flags);
        }

#if defined(CAP_WAP_CONFIG) && CAP_WAP_CONFIG
    case PPA_GENERIC_HAL_ADD_CAPWAP_ENTRY:
        {
            PPA_CMD_CAPWAP_INFO *capwap = (PPA_CMD_CAPWAP_INFO *)buffer;

            return add_capwap_entry(capwap->directpath_portid,
                                    capwap->source_ip.f_ipv6,
                                    capwap->qid, capwap->dest_ifid,
                                    capwap->dst_mac, 
                                    capwap->src_mac, capwap->tos, 
                                    capwap->ttl, 
                                    capwap->source_ip.ip.ip, capwap->dest_ip.ip.ip, 
                                    capwap->ip_chksum, capwap->source_port,
                                    capwap->dest_port, capwap->udp_chksum, 
                                    capwap->rid, capwap->wbid, 
                                    capwap->t_flag,
                                    capwap->max_frg_size,
                                    &capwap->p_entry);
                   
        }

   case PPA_GENERIC_HAL_DEL_CAPWAP_ENTRY:
        {

            
            PPA_CMD_CAPWAP_INFO *capwap = (PPA_CMD_CAPWAP_INFO *)buffer;
            del_capwap_entry(capwap->p_entry);
            return PPA_SUCCESS;
        }

   case PPA_GENERIC_HAL_GET_CAPWAP_MIB:
        {

            PPA_CMD_CAPWAP_INFO *capwap = (PPA_CMD_CAPWAP_INFO *)buffer;
            get_capwap_mib(capwap->p_entry,&capwap->ds_mib,&capwap->us_mib);
            return PPA_SUCCESS;
        }

#endif



   case PPA_GENERIC_HAL_ADD_MC_ENTRY:
       {
           PPE_MC_INFO *mc = (PPE_MC_INFO *)buffer;

           return add_wan_mc_entry(mc->dest_ip_compare, mc->f_vlan_ins_enable, mc->new_vci, mc->f_vlan_rm_enable, mc->f_src_mac_enable,
                                                        mc->src_mac_ix, mc->pppoe_mode, mc->f_out_vlan_ins_enable, mc->out_vlan_info.vlan_entry,  mc->f_out_vlan_rm_enable,
                                                        mc->f_new_dscp_enable, mc->new_dscp, mc->dest_qid, mc->dest_list, mc->route_type,mc->src_ip_compare,mc->f_tunnel_rm_enable,
#if defined(RTP_SAMPLING_ENABLE) && RTP_SAMPLING_ENABLE
                             mc->sample_en,
#endif
                                                        &mc->p_entry);
        }
   case PPA_GENERIC_HAL_DEL_MC_ENTRY:
       {
           PPE_MC_INFO *mc = (PPE_MC_INFO *)buffer;

           del_wan_mc_entry(mc->p_entry);
           return PPA_SUCCESS;
        }
   case PPA_GENERIC_HAL_UPDATE_MC_ENTRY:
       {
           PPE_MC_INFO *mc = (PPE_MC_INFO *)buffer;

           return update_wan_mc_entry(mc->p_entry, mc->f_vlan_ins_enable, mc->new_vci, mc->f_vlan_rm_enable, mc->f_src_mac_enable,
                                       mc->src_mac_ix, mc->pppoe_mode, mc->f_out_vlan_ins_enable, mc->out_vlan_info.vlan_entry, mc->f_out_vlan_rm_enable,
                                       mc->f_new_dscp_enable, mc->new_dscp, mc->dest_qid, mc->dest_list, mc->update_flags);
        }

#if defined(RTP_SAMPLING_ENABLE) && RTP_SAMPLING_ENABLE
    case PPA_GENERIC_HAL_SET_MC_RTP:
        {
            PPE_MC_INFO *mc = (PPE_MC_INFO *)buffer;
            return set_wan_mc_rtp(mc->p_entry, mc->sample_en);
        }

    case PPA_GENERIC_HAL_GET_MC_RTP_SAMPLING_CNT:
        {
            PPE_MC_INFO *mc = (PPE_MC_INFO *)buffer;
            return get_wan_mc_rtp_sampling_cnt(mc->p_entry,&mc->rtp_pkt_cnt,&mc->rtp_seq_num);
        }

#endif


   case PPA_GENERIC_HAL_ADD_BR_MAC_BRIDGING_ENTRY:
       {
           PPE_BR_MAC_INFO *br_mac = (PPE_BR_MAC_INFO *)buffer;

           return add_bridging_entry(br_mac->port, br_mac->mac, br_mac->f_src_mac_drop, br_mac->dslwan_qid, br_mac->dest_list, &br_mac->p_entry);
        }
   case PPA_GENERIC_HAL_DEL_BR_MAC_BRIDGING_ENTRY:
       {
           PPE_BR_MAC_INFO *br_mac = (PPE_BR_MAC_INFO *)buffer;

           del_bridging_entry(br_mac->p_entry);
           return PPA_SUCCESS;
        }
    case PPA_GENERIC_HAL_ADD_6RD_TUNNEL_ENTRY:
        {
            PPE_TUNNEL_INFO *tnnl_info = (PPE_TUNNEL_INFO *)buffer;
            return add_6rd_tunnel_entry(tnnl_info->tx_dev,&tnnl_info->tunnel_idx);
        }
    case PPA_GENERIC_HAL_DEL_6RD_TUNNEL_ENTRY:
        {
            PPE_TUNNEL_INFO *tnnl_info = (PPE_TUNNEL_INFO *)buffer;
            del_6rd_tunnel_entry(tnnl_info->tunnel_idx);
            return PPA_SUCCESS;
        }
    case PPA_GENERIC_HAL_ADD_DSLITE_TUNNEL_ENTRY:
        {
            PPE_TUNNEL_INFO *tnnl_info = (PPE_TUNNEL_INFO *)buffer;
            return add_dslite_tunnel_entry(tnnl_info->tx_dev,&tnnl_info->tunnel_idx);
        }
    case PPA_GENERIC_HAL_DEL_DSLITE_TUNNEL_ENTRY:
        {
            PPE_TUNNEL_INFO *tnnl_info = (PPE_TUNNEL_INFO *)buffer;
            del_dslite_tunnel_entry(tnnl_info->tunnel_idx);
            return PPA_SUCCESS;
        }
    case PPA_GENERIC_HAL_ADD_PPPOE_ENTRY:
       {
           PPE_PPPOE_INFO *pppoe_info=(PPE_PPPOE_INFO *)buffer;
           return add_pppoe_entry( pppoe_info->pppoe_session_id, &pppoe_info->pppoe_ix);
        }
    case PPA_GENERIC_HAL_DEL_PPPOE_ENTRY:
       {
           PPE_PPPOE_INFO *pppoe_info=(PPE_PPPOE_INFO *)buffer;
           del_pppoe_entry(  pppoe_info->pppoe_ix);
           return PPA_SUCCESS;
        }
    case PPA_GENERIC_HAL_GET_PPPOE_ENTRY:
       {
           PPE_PPPOE_INFO *pppoe_info=(PPE_PPPOE_INFO *)buffer;
           return get_pppoe_entry(  pppoe_info->pppoe_ix, &pppoe_info->pppoe_session_id);
        }

  case PPA_GENERIC_HAL_ADD_MTU_ENTRY:
       {
           PPE_MTU_INFO *mtu_info=(PPE_MTU_INFO *)buffer;
           return add_mtu_entry( mtu_info->mtu, &mtu_info->mtu_ix);
        }
    case PPA_GENERIC_HAL_DEL_MTU_ENTRY:
       {
           PPE_MTU_INFO *mtu_info=(PPE_MTU_INFO *)buffer;
           del_mtu_entry( mtu_info->mtu_ix);
           return PPA_SUCCESS;
        }
    case PPA_GENERIC_HAL_GET_MTU_ENTRY:
       {
           PPE_MTU_INFO *mtu_info=(PPE_MTU_INFO *)buffer;
           return get_mtu_entry( mtu_info->mtu_ix, &mtu_info->mtu);
        }
    case PPA_GENERIC_HAL_GET_ROUTE_ACC_BYTES:
        {
           PPE_ROUTING_INFO *route=(PPE_ROUTING_INFO *)buffer;

#if defined(MIB_MODE_ENABLE) && MIB_MODE_ENABLE
           if(PS_MC_GENCFG3->session_mib_unit == 1)
              //route->bytes = 0x0;
              route->bytes = get_routing_entry_bytes( route->entry, (flag & PPA_CMD_CLEAR_PORT_MIB)?1:0);
           else
              route->bytes = get_routing_entry_bytes( route->entry, (flag & PPA_CMD_CLEAR_PORT_MIB)?1:0) * (uint64_t)SESSION_MIB_MULTIPLEXER;

#else
           route->bytes = get_routing_entry_bytes( route->entry, (flag & PPA_CMD_CLEAR_PORT_MIB)?1:0) * (uint64_t)SESSION_MIB_MULTIPLEXER;
#endif           
           return PPA_SUCCESS;
        }

    case PPA_GENERIC_HAL_GET_MC_ACC_BYTES:
        {
            PPE_MC_INFO *mc =(PPE_MC_INFO *)buffer;
            mc->bytes = get_mc_entry_bytes( mc->p_entry, (flag & PPA_CMD_CLEAR_PORT_MIB)?1:0);
            return PPA_SUCCESS;
        }
        
    case PPA_GENERIC_HAL_ADD_MAC_ENTRY:
       {
           PPE_ROUTE_MAC_INFO *mac_info=(PPE_ROUTE_MAC_INFO *)buffer;
           return add_mac_entry( mac_info->mac, &mac_info->mac_ix);
        }
    case PPA_GENERIC_HAL_DEL_MAC_ENTRY:
       {
           PPE_ROUTE_MAC_INFO *mac_info=(PPE_ROUTE_MAC_INFO *)buffer;
           del_mac_entry( mac_info->mac_ix);
           return PPA_SUCCESS;
        }
    case PPA_GENERIC_HAL_GET_MAC_ENTRY:
       {
           PPE_ROUTE_MAC_INFO *mac_info=(PPE_ROUTE_MAC_INFO *)buffer;
           return get_mac_entry( mac_info->mac_ix, mac_info->mac);
        }
    case PPA_GENERIC_HAL_ADD_OUT_VLAN_ENTRY:
       {
           PPE_OUT_VLAN_INFO *vlan=(PPE_OUT_VLAN_INFO *)buffer;
           return add_outer_vlan_entry( vlan->vlan_id, &vlan->vlan_entry);
        }
    case PPA_GENERIC_HAL_DEL_OUT_VLAN_ENTRY:
       {
           PPE_OUT_VLAN_INFO *vlan=(PPE_OUT_VLAN_INFO *)buffer;
           del_outer_vlan_entry( vlan->vlan_entry);
           return PPA_SUCCESS;
        }
    case PPA_GENERIC_HAL_GET_OUT_VLAN_ENTRY:
       {
           PPE_OUT_VLAN_INFO *vlan=(PPE_OUT_VLAN_INFO *)buffer;
           return get_outer_vlan_entry( vlan->vlan_entry, &vlan->vlan_id);
        }
    case PPA_GENERIC_HAL_ADD_IPV6_ENTRY:
        {
            uint32_t ret;
            PPE_IPV6_INFO *ip6_addr = (PPE_IPV6_INFO *)buffer;
            ret = add_ipv6_ip_entry(ip6_addr->ip.ip6, &ip6_addr->ipv6_entry);
            if(ret == PPA_SUCCESS){
                ip6_addr->psuedo_ip = *PSEUDO_IPv4_BASE_ADDR | ip6_addr->ipv6_entry;
            }

            return ret;
        }

    case PPA_GENERIC_HAL_DEL_IPV6_ENTRY:
        {
            PPE_IPV6_INFO *ip6_addr = (PPE_IPV6_INFO *)buffer;
            del_ipv6_ip_entry(ip6_addr->ipv6_entry);
            return PPA_SUCCESS;
        }
    case PPA_GENERIC_HAL_GET_ITF_MIB:
       {
           PPE_ITF_MIB_INFO *mib=(PPE_ITF_MIB_INFO *)buffer;
           get_itf_mib( mib->itf, &mib->mib);
           return PPA_SUCCESS;
        }
    case PPA_GENERIC_HAL_MFE_CONTROL:   //enable/disable multiple field vlan editing feature
        {
           PPE_ENABLE_CFG *cfg=(PPE_ENABLE_CFG *)buffer;
           return ifx_ppe_drv_multifield_control( cfg->f_enable, cfg->flags);
        }
    case PPA_GENERIC_HAL_MFE_STATUS:  // get a multiple field vlan editing feature status: enabled/disabled
        {
           PPE_ENABLE_CFG *cfg=(PPE_ENABLE_CFG *)buffer;
           return ifx_ppe_drv_get_multifield_status( &cfg->f_enable, cfg->flags);
        }
    case PPA_GENERIC_HAL_MFE_GET_FLOW_MAX_ENTRY: // get muaximum entry number for multiple field vlan editing
        {
           PPE_COUNT_CFG *count=(PPE_COUNT_CFG *)buffer;
           count->num = ifx_ppe_drv_get_multifield_max_entry( count->flags);
           return PPA_SUCCESS;
        }
    case PPA_GENERIC_HAL_MFE_ADD_FLOW:   //add a multiple field vlan editing entry
        {
            PPE_MULTIFILED_FLOW *flow=(PPE_MULTIFILED_FLOW *)buffer;
            return ifx_ppe_drv_add_multifield_entry( &flow->multifield_info, &flow->entry, flow->flag);
        }
    case PPA_GENERIC_HAL_MFE_DEL_FLOW:   //del a multiple field vlan editing entry
        {
            PPE_MULTIFILED_FLOW *flow=(PPE_MULTIFILED_FLOW *)buffer;
            return ifx_ppe_drv_del_multifield_entry( &flow->multifield_info, flow->flag);
        }

    case PPA_GENERIC_HAL_MFE_DEL_FLOW_VIA_ENTRY:  //del a multiple field vlan editing entry
        {
            PPE_MULTIFILED_FLOW *flow=(PPE_MULTIFILED_FLOW *)buffer;
            return ifx_ppe_drv_del_multifield_entry_via_index( flow->entry, flow->flag);
        }

    case PPA_GENERIC_HAL_MFE_GET_FLOW:  //get a multiple field vlan editing entr
        {
            PPE_MULTIFILED_FLOW *flow=(PPE_MULTIFILED_FLOW *)buffer;
            return ifx_ppe_drv_get_multifield_entry( flow->entry, &flow->multifield_info, flow->flag);
        }
     case PPA_GENERIC_HAL_TEST_CLEAR_ROUTE_HIT_STAT:  //check whether a routing entry is hit or not
        {
            PPE_ROUTING_INFO *route=(PPE_ROUTING_INFO *)buffer;
            route->f_hit = test_and_clear_hit_stat( route->entry);
            return PPA_SUCCESS;
        }
     case PPA_GENERIC_HAL_TEST_CLEAR_BR_HIT_STAT:  //check whether a bridge mac entry is hit or not
        {
            PPE_BR_MAC_INFO *br_mac=(PPE_BR_MAC_INFO *)buffer;
            br_mac->f_hit = test_and_clear_bridging_hit_stat( br_mac->p_entry);
            return PPA_SUCCESS;
        }

     case PPA_GENERIC_HAL_TEST_CLEAR_MC_HIT_STAT:  //check whether a multicast entry is hit or not
        {
            PPE_MC_INFO *mc =(PPE_MC_INFO *)buffer;
            mc->f_hit = test_and_clear_mc_hit_stat( mc->p_entry );
            return PPA_SUCCESS;
        }
     
     case PPA_GENERIC_HAL_INIT: //init HAL
        {
            return PPA_SUCCESS;
        }
     case PPA_GENERIC_HAL_EXIT: //EXIT HAL
        {
            return PPA_SUCCESS;
        }
#ifdef CONFIG_PPA_QOS
     case PPA_GENERIC_HAL_GET_QOS_STATUS:  //get QOS status
        {
            return get_qos_status((PPA_QOS_STATUS *)buffer);
        }
    case PPA_GENERIC_HAL_GET_QOS_QUEUE_NUM:  //get maximum QOS queue number
        {
            PPE_QOS_COUNT_CFG *count=(PPE_QOS_COUNT_CFG *)buffer;
            count->num = get_qos_qnum( count->portid, count->flags );
            return PPA_SUCCESS;
        }
      case PPA_GENERIC_HAL_GET_QOS_MIB:  //get maximum QOS queue number
        {
            PPE_QOS_MIB_INFO *mib_info=(PPE_QOS_MIB_INFO *)buffer;
            return get_qos_mib(mib_info->portid, mib_info->queueid, &mib_info->mib, mib_info->flag );
        }
#ifdef CONFIG_PPA_QOS_WFQ
        case PPA_GENERIC_HAL_SET_QOS_WFQ_CTRL:  //enable/disable WFQ
        {
            PPE_QOS_ENABLE_CFG *enable_cfg=(PPE_QOS_ENABLE_CFG *)buffer;
            return set_ctrl_qos_wfq( enable_cfg->portid, enable_cfg->f_enable, enable_cfg->flag );
        }
        case PPA_GENERIC_HAL_GET_QOS_WFQ_CTRL:  //get  WFQ status: enabeld/disabled
        {
            PPE_QOS_ENABLE_CFG *enable_cfg=(PPE_QOS_ENABLE_CFG *)buffer;
            return get_ctrl_qos_wfq( enable_cfg->portid, &enable_cfg->f_enable, enable_cfg->flag );
        }
        case PPA_GENERIC_HAL_SET_QOS_WFQ_CFG:  //set WFQ cfg
        {
            PPE_QOS_WFQ_CFG *cfg=(PPE_QOS_WFQ_CFG *)buffer;
            return set_qos_wfq( cfg->portid, cfg->queueid, cfg->weight_level, cfg->flag );
        }
        case PPA_GENERIC_HAL_RESET_QOS_WFQ_CFG:  //reset WFQ cfg
        {
            PPE_QOS_WFQ_CFG *cfg=(PPE_QOS_WFQ_CFG *)buffer;
            return reset_qos_wfq( cfg->portid, cfg->queueid, cfg->flag );
        }
        case PPA_GENERIC_HAL_GET_QOS_WFQ_CFG:  //get WFQ cfg
        {
            PPE_QOS_WFQ_CFG *cfg=(PPE_QOS_WFQ_CFG *)buffer;
            return get_qos_wfq( cfg->portid, cfg->queueid, &cfg->weight_level, cfg->flag );
        }
        case PPA_GENERIC_HAL_INIT_QOS_WFQ: // init QOS Rateshapping
        {
            return init_qos_wfq();
        }
#endif //end of CONFIG_PPA_QOS_WFQ
 #ifdef CONFIG_PPA_QOS_RATE_SHAPING
      case PPA_GENERIC_HAL_SET_QOS_RATE_SHAPING_CTRL:  //enable/disable Rate shaping
      {
            PPE_QOS_ENABLE_CFG *enable_cfg=(PPE_QOS_ENABLE_CFG *)buffer;
            return set_ctrl_qos_rate( enable_cfg->portid, enable_cfg->f_enable, enable_cfg->flag );
        }
      case PPA_GENERIC_HAL_GET_QOS_RATE_SHAPING_CTRL:  //get  Rateshaping status: enabeld/disabled
        {
            PPE_QOS_ENABLE_CFG *enable_cfg=(PPE_QOS_ENABLE_CFG *)buffer;
            return get_ctrl_qos_rate( enable_cfg->portid, &enable_cfg->f_enable, enable_cfg->flag );
        }
      case PPA_GENERIC_HAL_SET_QOS_RATE_SHAPING_CFG:  //set rate shaping
      {
            PPE_QOS_RATE_SHAPING_CFG*cfg=(PPE_QOS_RATE_SHAPING_CFG *)buffer;
            return set_qos_rate( cfg->portid, cfg->queueid, cfg->rate_in_kbps, cfg->burst, cfg->flag );
       }
      case PPA_GENERIC_HAL_GET_QOS_RATE_SHAPING_CFG:  //get rate shaping cfg
      {
            PPE_QOS_RATE_SHAPING_CFG*cfg=(PPE_QOS_RATE_SHAPING_CFG *)buffer;
            return get_qos_rate( cfg->portid, cfg->queueid, &cfg->rate_in_kbps, &cfg->burst, cfg->flag );
       }
      case PPA_GENERIC_HAL_RESET_QOS_RATE_SHAPING_CFG:  //reset rate shaping cfg
      {
            PPE_QOS_RATE_SHAPING_CFG*cfg=(PPE_QOS_RATE_SHAPING_CFG *)buffer;
            return reset_qos_rate( cfg->portid, cfg->queueid, cfg->flag );
       }
      case PPA_GENERIC_HAL_INIT_QOS_RATE_SHAPING: // init QOS Rateshapping
        {
            return init_qos_rate();
        }
#endif //end of CONFIG_PPA_QOS_RATE_SHAPING
#endif //end of CONFIG_PPA_QOS

    case PPA_GENERIC_HAL_SET_WANITF: // set wanitf
        {
            return set_wanitf((PPE_WANITF_CFG *) buffer, flag);
        }
    case PPA_GENERIC_HAL_GET_SESSION_HASH: // get hash index
        {
            PPE_SESSION_HASH*cfg=(PPE_SESSION_HASH *)buffer;
            get_hash(cfg);
            return PPA_SUCCESS;
        }
    case PPA_GENERIC_HAL_SET_VALUE: // set value
        {
            PPA_CMD_VARIABLE_VALUE_INFO *cfg=(PPA_CMD_VARIABLE_VALUE_INFO *)buffer;
            return set_variable_value(cfg, flag);
        }
    case PPA_GENERIC_HAL_GET_VALUE: // get value
        {
            PPA_CMD_VARIABLE_VALUE_INFO *cfg=(PPA_CMD_VARIABLE_VALUE_INFO *)buffer;
            return get_variable_value(cfg, flag);
        }

//Fix warning message when exports API from different PPE FW Driver--End

    default:
        dbg("ppa_hal_generic_hook not support cmd 0x%x\n", cmd );
        return PPA_FAILURE;
    }

    return PPA_FAILURE;

}



/*
 * ####################################
 *           Init/Cleanup API
 * ####################################
 */

static inline void hal_init(void)
{
    int i;

    //  init table with default value
    for ( i = 0; i < MAX_CLASSIFICATION_ENTRIES; i++ )
        CLASSIFICATION_ACT_TBL(i)->fw_cpu = 1;
    *KEY_SEL_n(0) = 0x00000000;
    *KEY_SEL_n(1) = 0x00000000;
    *KEY_SEL_n(2) = 0x4E4F8081;
    *KEY_SEL_n(3) = 0x52430000;
    GEN_MODE_CFG1->classification_num = MAX_CLASSIFICATION_ENTRIES;

    g_lan_collision_routing_entries = LAN_ROUT_TBL_CFG->rout_num;
    g_wan_collision_routing_entries = WAN_ROUT_TBL_CFG->rout_num;
    g_wan_mc_entries = WAN_ROUT_TBL_CFG->wan_rout_mc_num;
    g_ipv6_acc_en = GEN_MODE_CFG1->ipv6_acc_en ? 1 : 0;
    g_ipv4_lan_collision_rout_fwdc_table_base = __IPV4_LAN_COLLISION_ROUT_FWDC_TABLE_BASE;
    g_ipv4_wan_collision_rout_fwda_table_base = __IPV4_WAN_COLLISION_ROUT_FWDA_TABLE_BASE;

    ppa_mem_cache_create("mac_tbl_item", sizeof(struct mac_tbl_item), &g_mac_tbl_item_cache);

    ppe_lock_init(&g_ipv6_ip_lock);
    ppe_lock_init(&g_lan_routing_lock);
    ppe_lock_init(&g_wan_routing_lock);
    ppe_lock_init(&g_wan_mc_lock);
#if defined(CAP_WAP_CONFIG) && CAP_WAP_CONFIG
    ppe_lock_init(&g_capwap_lock);
#endif
    ppe_lock_init(&g_pppoe_lock);
    ppe_lock_init(&g_mtu_lock);
    ppe_lock_init(&g_mac_lock);
    ppe_lock_init(&g_outer_vlan_lock);
    ppe_lock_init(&g_classification_lock);
    ppe_lock_init(&g_6rd_tunnel_lock);
    ppe_lock_init(&g_dslite_tunnel_lock);
    ppe_lock_init(&g_mac_tbl_lock);
}

static inline void hal_exit(void)
{
    if ( g_mac_tbl_item_cache )
    {
        ppa_mem_cache_destroy(g_mac_tbl_item_cache);
        g_mac_tbl_item_cache = NULL;
    }
}

static int __init ppe_hal_init(void)
{
    hal_init();
    ppa_drv_hal_generic_hook = ppa_hal_generic_hook;
    return 0;
}

static void __exit ppe_hal_exit(void)
{
    ppa_drv_hal_generic_hook = NULL;
    hal_exit();
}
module_init(ppe_hal_init);
module_exit(ppe_hal_exit);

MODULE_LICENSE("GPL");
