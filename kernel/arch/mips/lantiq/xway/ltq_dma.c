/*
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 *
 *  Copyright (C) 2009~1012 Reddy <Reddy.Mallikarjun@lantiq.com>
 *  Copyright (C) 2013 Lei Chuanhua <chuanhua.lei@lantiq.com>
 */
/*!
  \file ltq_dma.c
  \ingroup LTQ_DMA_CORE
  \brief DMA driver main source file.
*/
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/percpu.h>
#include <linux/of.h>
#include <linux/seq_file.h>

#include <lantiq.h>
#include <lantiq_soc.h>
#include <lantiq_irq.h>
#include <lantiq_dma.h>
#include "ltq_dma.h"

#define ltq_dma_r32(x)			ltq_r32(ltq_dma_membase + (x))
#define ltq_dma_w32(x, y)		ltq_w32(x, ltq_dma_membase + (y))
#define ltq_dma_w32_mask(x, y, z)	ltq_w32_mask(x, y, \
						ltq_dma_membase + (z))

static void __iomem *ltq_dma_membase;

#define DMA_CH0_INT		INT_NUM_IM2_IRL0
#define DMA_CH1_INT		(INT_NUM_IM2_IRL0 + 1)
#define DMA_CH2_INT		(INT_NUM_IM2_IRL0 + 2)
#define DMA_CH3_INT		(INT_NUM_IM2_IRL0 + 3)
#define DMA_CH4_INT		(INT_NUM_IM2_IRL0 + 4)
#define DMA_CH5_INT		(INT_NUM_IM2_IRL0 + 5)
#define DMA_CH6_INT		(INT_NUM_IM2_IRL0 + 6)
#define DMA_CH7_INT		(INT_NUM_IM2_IRL0 + 7)
#define DMA_CH8_INT		(INT_NUM_IM2_IRL0 + 8)
#define DMA_CH9_INT		(INT_NUM_IM2_IRL0 + 9)
#define DMA_CH10_INT		(INT_NUM_IM2_IRL0 + 10)
#define DMA_CH11_INT		(INT_NUM_IM2_IRL0 + 11)
#define DMA_CH12_INT		(INT_NUM_IM2_IRL0 + 25)
#define DMA_CH13_INT		(INT_NUM_IM2_IRL0 + 26)
#define DMA_CH14_INT		(INT_NUM_IM2_IRL0 + 27)
#define DMA_CH15_INT		(INT_NUM_IM2_IRL0 + 28)
#define DMA_CH16_INT		(INT_NUM_IM2_IRL0 + 29)
#define DMA_CH17_INT		(INT_NUM_IM1_IRL0 + 30)
#define DMA_CH18_INT		(INT_NUM_IM2_IRL0 + 16)
#define DMA_CH19_INT		(INT_NUM_IM2_IRL0 + 21)
#define DMA_CH20_INT		INT_NUM_IM4_IRL0
#define DMA_CH21_INT		(INT_NUM_IM4_IRL0 + 1)
#define DMA_CH22_INT		(INT_NUM_IM4_IRL0 + 2)
#define DMA_CH23_INT		(INT_NUM_IM4_IRL0 + 3)
#define DMA_CH24_INT		(INT_NUM_IM4_IRL0 + 4)
#define DMA_CH25_INT		(INT_NUM_IM4_IRL0 + 5)
#define DMA_CH26_INT		(INT_NUM_IM4_IRL0 + 6)
#define DMA_CH27_INT		(INT_NUM_IM4_IRL0 + 7)

/** Default channel budget */
#define DMA_INT_BUDGET			20

#define CN_GET(X)		(int)((X) - dma_chan)

/*! \enum ifx_dma_chan_dir_t
 \brief DMA Rx/Tx channel
*/
enum ifx_dma_chan_dir {
	IFX_DMA_RX_CH = 0,  /*!< Rx channel */
	IFX_DMA_TX_CH = 1,  /*!< Tx channel */
};

/*! \typedef _dma_chan_map
 \brief The parameter structure is used to dma channel map
*/
struct ifx_dma_chan_map {
	int  port_num;	/*!< Port number */
	char dev_name[DMA_DEV_NAME_LEN]; /*!< Device Name */
	enum ifx_dma_chan_dir	dir; /*!< Direction of the DMA channel */
	int  pri;		/*!< Class value */
	int  irq;		/*!< DMA channel irq number */
	int  rel_chan_no;	/*!< Relative channel number */
};

struct dma_sched_status {
	volatile u32 dma_int_status;
	volatile int dma_in_process;
#ifdef CONFIG_NAPI_ENABLED
	volatile u32 dma_poll_int_status;
#endif
};

static struct proc_dir_entry *g_dma_dir;
static int dma_max_ports;
static int dma_max_chans;
static int dma_desc_num;
static int dma_hw_poll;
static int dma_pkt_arb_enable;

static u64 *g_desc_list;
static char dma_device_name[MAX_DMA_DEVICE_NUM][DMA_DEV_NAME_LEN];
static struct ifx_dma_chan_map ifx_default_dma_map[MAX_DMA_CHANNEL_NUM];
struct ifx_dma_chan_map *chan_map = ifx_default_dma_map;
static u64 *g_desc_list_backup;
static _dma_device_info dma_devs[MAX_DMA_DEVICE_NUM];
static _dma_channel_info dma_chan[MAX_DMA_CHANNEL_NUM];

static DEFINE_SPINLOCK(g_dma_spinlock);
static DEFINE_PER_CPU(struct dma_sched_status, dma_sched_stats);

static void do_dma_tasklet(unsigned long);
static struct tasklet_struct dma_tasklet[NR_CPUS];

/** Set the channel number */
static void select_chan(int cn)
{
	ltq_dma_w32(cn, DMA_CS);
}

/** Select the channel ON */
static void enable_chan(int cn)
{
	ltq_dma_w32(cn, DMA_CS);
	ltq_dma_w32_mask(0, DMA_CCTRL_ON, DMA_CCTRL);
}

/** Select the channel OFF */
static void disable_chan(int cn)
{
	ltq_dma_w32(cn, DMA_CS);
	ltq_dma_w32_mask(DMA_CCTRL_ON, 0, DMA_CCTRL);
}

static unsigned char *common_buffer_alloc(int len, int *byte_offset, void **opt)
{
	unsigned char *buffer = kmalloc(len, GFP_ATOMIC);
	*byte_offset = 0;
	return buffer;
}

static int common_buffer_free(unsigned char *dataptr, void *opt)
{
	kfree(dataptr);
	return 0;
}

/** Enable the DMA channel interrupt */
static void __enable_ch_irq(int cn, int dir)
{
	unsigned int val = DMA_CIE_EOP;

	if (dir == IFX_DMA_RX_CH) {
		if (dma_hw_poll)
			val |= DMA_CIE_DESCPT;
		else
			val |= DMA_CIE_DUR;

	}
	ltq_dma_w32(cn, DMA_CS);
	ltq_dma_w32(val, DMA_CIE);
	ltq_dma_w32_mask(0, (1 << cn), DMA_IRNEN);
	/*ltq_dma_w32((1 << cn), DMA_IRNCR); */
}

static void enable_ch_irq(_dma_channel_info *pch)
{
	unsigned long flags;
	int cn = CN_GET(pch);

	spin_lock_irqsave(&g_dma_spinlock, flags);
	__enable_ch_irq(cn, pch->dir);
	spin_unlock_irqrestore(&g_dma_spinlock, flags);
}

/** Dsiable the DMA channel Interrupt */
static void __disable_ch_irq(int cn)
{
	struct dma_sched_status *stats =
		&per_cpu(dma_sched_stats, smp_processor_id());

	stats->dma_int_status &= ~(1 << cn);
	ltq_dma_w32(cn, DMA_CS);
	ltq_dma_w32(DMA_CIE_DISABLE_ALL, DMA_CIE);
	ltq_dma_w32_mask((1 << cn), 0, DMA_IRNEN);
}

static void disable_ch_irq(_dma_channel_info *pch)
{
	unsigned long flags;
	int cn = CN_GET(pch);

	spin_lock_irqsave(&g_dma_spinlock, flags);
	__disable_ch_irq(cn);
	spin_unlock_irqrestore(&g_dma_spinlock, flags);
}

/**  Set the DMA channl ON*/
static void open_chan(_dma_channel_info *pch)
{
	unsigned long flags;
	int cn = CN_GET(pch);

	spin_lock_irqsave(&g_dma_spinlock, flags);
	enable_chan(cn);
	if (pch->dir == IFX_DMA_RX_CH)
		__enable_ch_irq(cn, pch->dir);
	spin_unlock_irqrestore(&g_dma_spinlock, flags);
}

/**  Set the DMA channl OFF*/
static void close_chan(_dma_channel_info *pch)
{
	int j;
	unsigned long flags;
	int cn = CN_GET(pch);

	spin_lock_irqsave(&g_dma_spinlock, flags);
	ltq_dma_w32(cn, DMA_CS);
	ltq_dma_w32_mask(DMA_CCTRL_ON, 0, DMA_CCTRL);
	for (j = 10000; (ltq_dma_r32(DMA_CCTRL) & DMA_CCTRL_ON) && j > 0; j--)
			;
	__disable_ch_irq(cn);
	spin_unlock_irqrestore(&g_dma_spinlock, flags);
	if (j == 0)
		pr_info("cn %d: can not be turned off, DMA_CCTRL %08x\n",
			cn, ltq_dma_r32(DMA_CCTRL));
}

/** Reset the dma channel */
static void reset_chan(_dma_channel_info *pch)
{
	int i;
	unsigned long flags;
	u32 prev_cn;
	int cn = CN_GET(pch);

	struct dma_sched_status *stats =
		&per_cpu(dma_sched_stats, smp_processor_id());

	spin_lock_irqsave(&g_dma_spinlock, flags);
	stats->dma_int_status &= ~(1 << cn);
	prev_cn = ltq_dma_r32(DMA_CS);
	ltq_dma_w32(cn, DMA_CS);
	ltq_dma_w32_mask(0, DMA_CCTRL_RST, DMA_CCTRL);
	while ((ltq_dma_r32(DMA_CCTRL) & DMA_CCTRL_RST))
		;
	ltq_dma_w32_mask(DMA_CCTRL_ON, 0, DMA_CCTRL);
	ltq_dma_w32(((u32)CPHYSADDR(pch->desc_base)), DMA_CDBA);
	ltq_dma_w32(pch->desc_len, DMA_CDLEN);
	pch->curr_desc = 0;
	pch->prev_desc = 0;
	ltq_dma_w32(prev_cn, DMA_CS);
	spin_unlock_irqrestore(&g_dma_spinlock, flags);
	/* clear descriptor memory, so that OWN bit is set to CPU */
	/* memset((u32*)pch->desc_base, 0,
	 pch->desc_len * sizeof (struct tx_desc)); */
	if (pch->dir == IFX_DMA_TX_CH) {
		for (i = 0; i < pch->desc_len; i++)
			((struct tx_desc *) pch->desc_base)[i].
			status.field.OWN = CPU_OWN;
	} else {
		for (i = 0; i < pch->desc_len; i++) {
			((struct rx_desc *) pch->desc_base)[i].
				status.field.OWN = CPU_OWN;
			((struct rx_desc *) pch->desc_base)[i].
				status.field.C = 1;
		}
	}
	/* TODO: Reset DMA descriptors (free buffer, reset owner bit, allocate
	 * new buffer) */
}

/* Handle the Rx channel interrupt scheduler */
static void rx_chan_intr_handler(int cn)
{
	unsigned long flags;
	unsigned int prev_cn, cis;
	struct rx_desc *rx_desc_p;
	_dma_device_info *pdev = (_dma_device_info *) dma_chan[cn].dma_dev;
	_dma_channel_info *pch = &dma_chan[cn];
	struct dma_sched_status *stats =
		&per_cpu(dma_sched_stats, smp_processor_id());

	/* Handle command complete interrupt */
	rx_desc_p = (struct rx_desc *) pch->desc_base + pch->curr_desc;
	if (rx_desc_p->status.field.OWN == CPU_OWN
		&& rx_desc_p->status.field.C) {
		/* Everything is correct, then we inform the upper layer */
		pdev->current_rx_chan = pch->rel_chan_no;
		if (pdev->intr_handler)
			pdev->intr_handler(pdev, RCV_INT);

		pch->weight--;
		/* Clear interrupt status bits once we sendout the psuedo
		 * interrupt to client driver */
		spin_lock_irqsave(&g_dma_spinlock, flags);
		prev_cn = ltq_dma_r32(DMA_CS);
		ltq_dma_w32(cn, DMA_CS);
		ltq_dma_w32_mask(0, DMA_CIS_ALL, DMA_CIS);
		ltq_dma_w32(prev_cn, DMA_CS);
		spin_unlock_irqrestore(&g_dma_spinlock, flags);
	} else {
		/* Read channel interrupt status bit to determine if we should
		 * wait or move forward */
		spin_lock_irqsave(&g_dma_spinlock, flags);
		/* Clear the DMA Node status register bit to remove the dummy
		 * interrupts */
		ltq_dma_w32((1 << cn), DMA_IRNCR);
		prev_cn = ltq_dma_r32(DMA_CS);
		ltq_dma_w32(cn, DMA_CS);
		cis = ltq_dma_r32(DMA_CIS);
		/*
		 * In most cases, interrupt bit has been cleared in the first
		 * branch, it will hit the following. However, in some corner
		 * cases, the above branch doesn't happen, so it indicates that
		 * interrupt is received actually, but OWN bit and
		 * Complete bit not update yet, so go back to DMA tasklet and
		 * let DMA tasklet wait <polling>. This is the most efficient
		 * way. NB, only EOP and Descript Complete interrupt enabled
		 * in Channel Interrupt Enable Register.
		 */
		if ((cis & ltq_dma_r32(DMA_CIE)) == 0) {
			if (rx_desc_p->status.field.OWN != CPU_OWN)
				stats->dma_int_status &= ~(1 << cn);
			/*
			 * Enable this channel interrupt again after processing
			 * all packets available Placing this line at the last
			 * line will avoid interrupt coming again in heavy load
			 * to great extent
			 */
			ltq_dma_w32_mask(0, (1 << cn), DMA_IRNEN);
		}
		ltq_dma_w32(prev_cn, DMA_CS);
		spin_unlock_irqrestore(&g_dma_spinlock, flags);
		if (!dma_hw_poll) {
			if (cis & DMA_CIS_DUR)
				pch->dur = 1;
		}
	}

}

/* Handle the Tx channel interrupt scheduler */
static void tx_chan_intr_handler(int cn)
{
	unsigned long flags;
	unsigned int prev_cn, cis;
	_dma_device_info *pdev = (_dma_device_info *) dma_chan[cn].dma_dev;
	_dma_channel_info *pch = &dma_chan[cn];
	struct tx_desc *tx_desc_p =
		(struct tx_desc *) pch->desc_base + pch->curr_desc;
	struct dma_sched_status *stats =
		&per_cpu(dma_sched_stats, smp_processor_id());

	spin_lock_irqsave(&g_dma_spinlock, flags);
	prev_cn = ltq_dma_r32(DMA_CS);
	ltq_dma_w32(cn, DMA_CS);
	cis = ltq_dma_r32(DMA_CIS);
	ltq_dma_w32(cis | DMA_CIS_ALL, DMA_CIS);
	ltq_dma_w32(prev_cn, DMA_CS);

	pdev->current_tx_chan = pch->rel_chan_no;
	/* DMA Descriptor update by Hardware is not sync with DMA interrupt
	 * (EOP/Complete). To ensure descriptor is available before sending
	 * psudo interrupt to the client drivers. */
	if (!(tx_desc_p->status.field.OWN == DMA_OWN)) {
		if ((cis & DMA_CIS_EOP) && pdev->intr_handler) {
			spin_unlock_irqrestore(&g_dma_spinlock, flags);
			pdev->intr_handler(pdev, TRANSMIT_CPT_INT);
			spin_lock_irqsave(&g_dma_spinlock, flags);
			stats->dma_int_status &= ~(1 << cn);
		}
	}
	spin_unlock_irqrestore(&g_dma_spinlock, flags);
}

/* Handle the Rx channel interrupt handler*/
void rx_chan_handler(int cn)
{
	unsigned long flags;
	unsigned int prev_cn;
	struct rx_desc *rx_desc_p;
	_dma_device_info *pdev =
		(_dma_device_info *) dma_chan[cn].dma_dev;
	_dma_channel_info *pch = &dma_chan[cn];

	/* Handle command complete interrupt */
	rx_desc_p = (struct rx_desc *) pch->desc_base + pch->prev_desc;
	if ((rx_desc_p->status.field.OWN == CPU_OWN)
		&& rx_desc_p->status.field.C) {
		/* Everything is correct, then we inform the upper layer */
		pdev->current_rx_chan = pch->rel_chan_no;
		if (pdev->intr_handler)
			pdev->intr_handler(pdev, RCV_INT);

		/* Clear interrupt status bits once we sendout the psuedo
		* interrupt to client driver */
		spin_lock_irqsave(&g_dma_spinlock, flags);
		prev_cn = ltq_dma_r32(DMA_CS);
		ltq_dma_w32(cn, DMA_CS);
		ltq_dma_w32_mask(0, DMA_CIS_ALL, DMA_CIS);
		ltq_dma_w32(prev_cn, DMA_CS);
		/*
		 * Enable this channel interrupt again after processing all
		 * packets available Placing this line at the last line will
		 * avoid interrupt coming again in heavy load to great extent
		 */
		ltq_dma_w32_mask(0, (1 << cn), DMA_IRNEN);
		spin_unlock_irqrestore(&g_dma_spinlock, flags);
		pch->prev_desc = (pch->prev_desc + 1) % (pch->desc_len);
	} else {
		spin_lock_irqsave(&g_dma_spinlock, flags);
		/*
		 * Enable this channel interrupt again after processing all
		 * packets available Placing this line at the last line will
		 * avoid interrupt coming again in heavy load to great extent
		 */
		ltq_dma_w32_mask(0, (1 << cn), DMA_IRNEN);
		spin_unlock_irqrestore(&g_dma_spinlock, flags);
	}
}

/** Trigger when taklet schedule calls*/
static void do_dma_tasklet(unsigned long cpu)
{
	unsigned long flags;
	int i, cn = 0, budget = DMA_INT_BUDGET, weight = 0;
	struct dma_sched_status *stats = &per_cpu(dma_sched_stats, cpu);

	while (stats->dma_int_status) {
		if (budget-- < 0) {
			tasklet_schedule(&dma_tasklet[cpu]);
			return;
		}
		cn = -1;
		weight = 0;
		/* WFQ algorithm to select the channel */
		for (i = 0; i < dma_max_chans; i++) {
			if ((stats->dma_int_status & (1 << i))
				&& dma_chan[i].weight > 0) {
				if (dma_chan[i].weight > weight) {
					cn = i;
					weight = dma_chan[cn].weight;
				}
			}
		}
		if (cn >= 0) {
			if (chan_map[cn].dir == IFX_DMA_RX_CH)
				rx_chan_intr_handler(cn);
			else
				tx_chan_intr_handler(cn);

		} else {
			/* Reset the default weight vallue for all channels */
			for (i = 0; i < dma_max_chans; i++) {
				dma_chan[i].weight =
					dma_chan[i].default_weight;
			}
		}
	}
	/*
	 * Sanity check, check if there is new packet coming during this small
	 * gap However, at the same time, the following may cause interrupts is
	 * coming again on the same channel, because of rescheduling.
	 */
	spin_lock_irqsave(&g_dma_spinlock, flags);
	stats->dma_in_process = 0;
	if (stats->dma_int_status) {
		stats->dma_in_process = 1;
		tasklet_schedule(&dma_tasklet[cpu]);
	}
	spin_unlock_irqrestore(&g_dma_spinlock, flags);
}

/**
 * This function works as the following,
 * 1) Valid check
 * 2) Disable interrupt on this DMA channel, so that no further interrupts are
 *  coming
 *    when DMA tasklet is running. However, it still allows other DMA channel
 *    interrupt to come in.
 * 3) Record interrupt on this DMA channel, and dispatch it to DMA tasklet later
 * 4) If NAPI is enabled, submit to polling process instead of DMA tasklet.
 * 5) Check if DMA tasklet is running, if not, invoke it so that context switch
 *is minimized
 */
static irqreturn_t dma_interrupt(int irq, void *dev_id)
{
	int cn;
	unsigned long flags;
	_dma_channel_info *pch;
	int cpu = smp_processor_id();
	struct dma_sched_status *stats = &per_cpu(dma_sched_stats, cpu);

	pch = (_dma_channel_info *) dev_id;
	cn = CN_GET(pch);

	if (cn < 0 || cn > (dma_max_chans - 1)) {
		pr_err("%s: dma_interrupt irq=%d cn=%d\n",
			__func__, irq, cn);
		return IRQ_NONE;
	}
	/*
	 * Disable interrupt on this channel, later tasklet will enable it after
	 * processing all available packets on this channel.
	 */
	spin_lock_irqsave(&g_dma_spinlock, flags);
	ltq_dma_w32_mask((1 << cn), 0, DMA_IRNEN);
	spin_unlock_irqrestore(&g_dma_spinlock, flags);
	if (ltq_is_ase()) {
		/* SDIO channels direct call interrupt routine,
		  without tasklet schedule */
		if (cn == 6) {
			rx_chan_handler(cn);
			return IRQ_HANDLED;
		}
		if (cn == 7) {
			tx_chan_intr_handler(cn);
			return IRQ_HANDLED;
		}
	}
	/* Record this channel interrupt for tasklet */
	spin_lock_irqsave(&g_dma_spinlock, flags);
	stats->dma_int_status |= (1 << cn);
	spin_unlock_irqrestore(&g_dma_spinlock, flags);

#ifdef CONFIG_NAPI_ENABLED
	if (pch->dir == IFX_DMA_RX_CH) {
		_dma_device_info *pdev = (_dma_device_info *) pch->dma_dev;
		if (pdev->activate_poll) {
			/* Handled by polling rather than tasklet */
			spin_lock_irqsave(&g_dma_spinlock, flags);
			stats->dma_int_status &= ~(1 << cn);
			stats->dma_poll_int_status |= (1 << cn);
			spin_unlock_irqrestore(&g_dma_spinlock, flags);
			if (pdev->activate_poll)
				pdev->activate_poll(pdev);
			return IRQ_RETVAL(1);
		}
	}
#endif /* CONFIG_NAPI_ENABLED */
	/* if not in process, then invoke the tasklet */
	spin_lock_irqsave(&g_dma_spinlock, flags);
	if (!stats->dma_in_process) {
		stats->dma_in_process = 1;
		tasklet_schedule(&dma_tasklet[cpu]);
	}
	spin_unlock_irqrestore(&g_dma_spinlock, flags);
	return IRQ_HANDLED;
}

/*!
  \fn _dma_device_info* dma_device_reserve(char* dev_name)
  \ingroup  IFX_DMA_DRV_API
  \brief Reserve the dma device port

  This function should call before the dma_device_register
  It is reserved the dma device port if the port is supported and send
  the dma device info pointer
  \param    dev_name --> The supported port device name.
  \return on success send dma devcie pointer else NULL
*/
_dma_device_info *dma_device_reserve(char *dev_name)
{
	int i;

	for (i = 0; i < dma_max_ports; i++) {
		if (strcmp(dev_name, dma_devs[i].device_name) == 0) {
			if (dma_devs[i].port_reserved)
				return NULL;
			dma_devs[i].port_reserved = 1;
		#ifdef CONFIG_NAPI_ENABLED
			dma_devs[i].activate_poll = NULL;
			dma_devs[i].inactivate_poll = NULL;
		#endif /* CONFIG_NAPI_ENABLED */
			return &dma_devs[i];
		}
	}
	return NULL;
}
EXPORT_SYMBOL(dma_device_reserve);

/*!
  \fn int dma_device_release(_dma_device_info* dev)
  \ingroup  IFX_DMA_DRV_API
  \brief Unreseve the dma device port

  This function will called after the dma_device_unregister
  This fucnction unreseve the port.

  \param    dev --> pointer to DMA device structure
  \return 0
*/
int dma_device_release(_dma_device_info *dev)
{
	int i;
	for (i = 0; i < dma_max_ports; i++) {
		if (strcmp(dma_devs[i].device_name, dev->device_name) == 0) {
			if (dev->port_reserved) {
				dev->port_reserved = 0;
				return 0;
			}
		}
	}
	pr_err("%s: Device Port released failed: %s\n",
		__func__, dev->device_name);
	return -EIO;
}
EXPORT_SYMBOL(dma_device_release);

static inline int dma_class_value_set(int value)
{
	if (ltq_is_vr9())
		return (value & 0x7) << 9;
	else
		return (value & 0x3) << 9;
}

static inline int dma_class_mask_get(void)
{
	if (ltq_is_vr9())
		return 0x00000E00;
	else
		return 0x00000C00;
}

/*!
  \fn int dma_device_register(_dma_device_info* dev)
  \ingroup  IFX_DMA_DRV_API
  \brief Register with DMA device driver.

  This function registers with dma device driver to handle dma functionality.
  Should provide the configuration info during the register with dma device.
  if not provide channel/port configuration info, then take default values.
  This function should call after dma_device_reserve function.
  This function configure the Tx/Rx channel info as well as device port info

  \param    dev --> pointer to DMA device structure
  \return 0/-EINVAL
*/
int dma_device_register(_dma_device_info *dev)
{
	unsigned long flags;
	int result = 0, i, j, cn = 0, byte_offset = 0, txbl, rxbl;
	unsigned int reg_val;
	unsigned char *buffer;
	_dma_device_info *pdev;
	_dma_channel_info *pch;
	struct rx_desc *rx_desc_p;
	struct tx_desc *tx_desc_p;
	int port_no = dev->port_num;

	if (port_no < 0 || port_no > dma_max_ports)
		pr_err("%s: Wrong port number(%d)!!!\n", __func__, port_no);
	/* burst length Configration */
	switch (dev->tx_burst_len) {
	case 8:
		txbl = IFX_DMA_BURSTL_8;
		break;
	case 4:
		txbl = IFX_DMA_BURSTL_4;
		break;
	default:
		txbl = DMA_PORT_DEFAULT_TX_BURST_LEN;
	}
	switch (dev->rx_burst_len) {
	case 8:
		rxbl = IFX_DMA_BURSTL_8;
		break;
	case 4:
		rxbl = IFX_DMA_BURSTL_4;
		break;
	default:
		rxbl = DMA_PORT_DEFAULT_RX_BURST_LEN;
	}
	spin_lock_irqsave(&g_dma_spinlock, flags);
	ltq_dma_w32(port_no, DMA_PS);
	reg_val = DMA_PCTRL_TXWGT_SET_VAL(dev->port_tx_weight)
		| DMA_PCTRL_TXENDI_SET_VAL(dev->tx_endianness_mode)
		| DMA_PCTRL_RXENDI_SET_VAL(dev->rx_endianness_mode)
		| DMA_PCTRL_PDEN_SET_VAL(dev->port_packet_drop_enable)
		| DMA_PCTRL_TXBL_SET_VAL(txbl)
		| DMA_PCTRL_RXBL_SET_VAL(rxbl);
	if (dev->mem_port_control)
		reg_val |= DMA_PCTRL_GPC;
	else
		reg_val &= ~DMA_PCTRL_GPC;

	ltq_dma_w32(reg_val, DMA_PCTRL);
	spin_unlock_irqrestore(&g_dma_spinlock, flags);
	/* Tx channel register */
	for (i = 0; i < dev->max_tx_chan_num; i++) {
		pch = dev->tx_chan[i];
		if (pch->control == IFX_DMA_CH_ON) {
			cn = CN_GET(pch);
			if (pch->desc_len > dma_desc_num) {
				pr_info("%s Tx Channel %d descriptor length %d out of range <1~%d>\n",
					__func__, cn, pch->desc_len,
					dma_desc_num);
				result = -EINVAL;
				goto done;
			}
			for (j = 0; j < pch->desc_len; j++) {
				tx_desc_p =
					(struct tx_desc *) pch->desc_base + j;
				memset(tx_desc_p, 0, sizeof(struct tx_desc));
			}
			spin_lock_irqsave(&g_dma_spinlock, flags);
			ltq_dma_w32(cn, DMA_CS);

			/* check if the descriptor base is changed */
			if (ltq_dma_r32(DMA_CDBA) !=
				(u32)CPHYSADDR(pch->desc_base)) {
				ltq_dma_w32(((u32)CPHYSADDR(pch->desc_base)),
					DMA_CDBA);
			}
			/* Check if the descriptor length is changed */
			if (ltq_dma_r32(DMA_CDLEN) != pch->desc_len)
				ltq_dma_w32(pch->desc_len, DMA_CDLEN);

			ltq_dma_w32_mask(DMA_CCTRL_ON, 0, DMA_CCTRL);
			udelay(20);
			ltq_dma_w32_mask(0, DMA_CCTRL_RST, DMA_CCTRL);
			while ((ltq_dma_r32(DMA_CCTRL) & DMA_CCTRL_RST))
				;
			ltq_dma_w32((1 << cn), DMA_IRNCR);
			ltq_dma_w32_mask(0, (1 << cn), DMA_IRNEN);
			reg_val = ltq_dma_r32(DMA_CCTRL);
			reg_val &= ~DMA_CCTRL_TXWGT_MASK;
			reg_val |= DMA_CCTRL_TXWGT_VAL(pch->tx_channel_weight);
			if (pch->channel_packet_drop_enable)
				reg_val |= DMA_CCTRL_PDEN;
			else
				reg_val &= ~DMA_CCTRL_PDEN;
			if (pch->class_value) {
				reg_val &= ~dma_class_mask_get();
				reg_val |=
					dma_class_value_set(pch->class_value);
			}
			if (pch->peri_to_peri) {
				reg_val |= DMA_CCTRL_P2PCPY;
				if (pch->global_buffer_len)
					ltq_dma_w32(pch->global_buffer_len,
						DMA_CGBL);
			} else
				reg_val &= ~DMA_CCTRL_P2PCPY;

			if (pch->loopback_enable) {
				reg_val |= DMA_CCTRL_LBEN;
				if (pch->loopback_channel_number) {
					reg_val &= ~DMA_CCTRL_LBCHNR_MASK;
					reg_val |= DMA_CCTRL_LBCHNR_SET(
						pch->loopback_channel_number);
				}
			} else
				reg_val &= ~DMA_CCTRL_LBEN;
			ltq_dma_w32(reg_val, DMA_CCTRL);
			if (pch->req_irq_to_free == dma_chan[cn].irq) {
				/* free_irq(dma_chan[cn].irq,
					(void*)&dma_chan[cn]); */
			} else
				enable_irq(dma_chan[cn].irq);
			spin_unlock_irqrestore(&g_dma_spinlock, flags);
		}
	}
	/* RX channel register */
	for (i = 0; i < dev->max_rx_chan_num; i++) {
		pch = dev->rx_chan[i];
		if (pch->control == IFX_DMA_CH_ON) {
			cn = CN_GET(pch);

			if (pch->desc_len > dma_desc_num) {
				pr_info("%s Rx Channel %d descriptor length %d out of range <1~%d>\n",
					__func__, cn, pch->desc_len,
					dma_desc_num);
				result = -EINVAL;
				goto done;
			}
			for (j = 0; j < pch->desc_len; j++) {
				rx_desc_p =
					(struct rx_desc *) pch->desc_base + j;
				pdev = (_dma_device_info *) (pch->dma_dev);
				buffer = pdev->buffer_alloc(
					pch->packet_size, &byte_offset,
					(void *)&(pch->opt[j]));
				if (!buffer)
					break;
				dma_cache_inv((unsigned long) buffer,
					pch->packet_size);
				rx_desc_p->data_pointer =
					(u32) CPHYSADDR((u32) buffer);
				rx_desc_p->status.word = 0;
				rx_desc_p->status.field.byte_offset =
					byte_offset;
				rx_desc_p->status.field.OWN = DMA_OWN;
				rx_desc_p->status.field.data_length =
					pch->packet_size;
			}

			spin_lock_irqsave(&g_dma_spinlock, flags);
			ltq_dma_w32(cn, DMA_CS);
			/* Check if the descriptor base is changed */
			if (ltq_dma_r32(DMA_CDBA) !=
				(u32)CPHYSADDR(pch->desc_base)) {
				ltq_dma_w32(((u32)CPHYSADDR(pch->desc_base)),
					DMA_CDBA);
			}
			/* Check if the descriptor length is changed */
			if (ltq_dma_r32(DMA_CDLEN) != pch->desc_len)
				ltq_dma_w32(((pch->desc_len)), DMA_CDLEN);

			ltq_dma_w32_mask(DMA_CCTRL_ON, 0, DMA_CCTRL);
			udelay(20);
			ltq_dma_w32_mask(0, DMA_CCTRL_RST, DMA_CCTRL);
			while ((ltq_dma_r32(DMA_CCTRL) & DMA_CCTRL_RST))
				;

			/* XXX, should enable all interrupts? */
			ltq_dma_w32(DMA_CIE_DEFAULT, DMA_CIE);
			ltq_dma_w32((1 << cn), DMA_IRNCR);
			ltq_dma_w32_mask(0, (1 << cn), DMA_IRNEN);
			reg_val = ltq_dma_r32(DMA_CCTRL);
			/*
			reg_val &= ~DMA_CCTRL_TXWGT_MASK;
			reg_val |= DMA_CCTRL_TXWGT_VAL(pch->tx_channel_weight);
			 */
			if (pch->channel_packet_drop_enable)
				reg_val |= DMA_CCTRL_PDEN;
			else
				reg_val &= ~DMA_CCTRL_PDEN;
			if (pch->class_value) {
				reg_val &= ~dma_class_mask_get();
				reg_val |=
					dma_class_value_set(pch->class_value);
			}
			if (pch->loopback_enable) {
				reg_val |= DMA_CCTRL_LBEN;
				if (pch->loopback_channel_number) {
					reg_val &= ~DMA_CCTRL_LBCHNR_MASK;
					reg_val |= DMA_CCTRL_LBCHNR_SET(pch->
						loopback_channel_number);
				}
			} else
				reg_val &= ~DMA_CCTRL_LBEN;
			ltq_dma_w32(reg_val, DMA_CCTRL);
			if (pch->req_irq_to_free == dma_chan[cn].irq) {
				/* free_irq(dma_chan[cn].irq,
					(void*)&dma_chan[cn]); */
			} else
				enable_irq(dma_chan[cn].irq);
			spin_unlock_irqrestore(&g_dma_spinlock, flags);
		}
	}
done:
	return result;
}
EXPORT_SYMBOL(dma_device_register);

/*!
  \fn int dma_device_unregister(_dma_device_info* dev)
  \ingroup  IFX_DMA_DRV_API
  \brief Unregister with DMA core driver

  This function unregisters with dma core driver. Once it unregisters
  there is no
  DMA handling with client driver.

  \param    dev --> pointer to DMA device structure
  \return 0
*/
int dma_device_unregister(_dma_device_info *dev)
{
	unsigned long flags;
	int result = 0, i, j, cn;
	unsigned int reg_val;
	_dma_channel_info *pch;
	struct rx_desc *rx_desc_p;
	struct tx_desc *tx_desc_p;
	int port_no = dev->port_num;
	struct dma_sched_status *stats =
		&per_cpu(dma_sched_stats, smp_processor_id());

	if (port_no < 0 || port_no > dma_max_ports)
		pr_err("%s: Wrong port number(%d)!!!\n", __func__, port_no);
	for (i = 0; i < dev->max_tx_chan_num; i++) {
		pch = dev->tx_chan[i];
		if (pch->control == IFX_DMA_CH_ON) {
			cn = CN_GET(pch);
			spin_lock_irqsave(&g_dma_spinlock, flags);
			ltq_dma_w32(cn, DMA_CS);
			pch->curr_desc = 0;
			pch->prev_desc = 0;
			pch->control = IFX_DMA_CH_OFF;

			/* XXX, Should dislabe all interrupts here */
			ltq_dma_w32(0, DMA_CIE);

			/* Disable this channel interrupt */
			ltq_dma_w32_mask((1 << cn), 0, DMA_IRNEN);
			ltq_dma_w32((1 << cn), DMA_IRNCR);
			ltq_dma_w32_mask(DMA_CCTRL_ON, 0, DMA_CCTRL);
			for (j = 10000; (ltq_dma_r32(DMA_CCTRL) &
				DMA_CCTRL_ON) && j > 0; j--)
				;
			spin_unlock_irqrestore(&g_dma_spinlock, flags);
			if (j == 0)
				pr_info("Port %d TX CH %d: can not be turned off, DMA_CCTRL %08x\n",
					port_no, i, ltq_dma_r32(DMA_CCTRL));
			for (j = 0; j < pch->desc_len; j++) {
				if (pch->peri_to_peri)
					break;

				tx_desc_p =
					(struct tx_desc *) pch->desc_base + j;
				if (((tx_desc_p->status.field.OWN == CPU_OWN)
					&& tx_desc_p->status.field.C)
					|| (tx_desc_p->status.field.OWN ==
					DMA_OWN
					&& tx_desc_p->status.field.
					data_length > 0)) {
					dev->buffer_free(
					(u8 *)__va(tx_desc_p->data_pointer),
						(void *) pch->opt[j]);
				}
				tx_desc_p->status.field.OWN = CPU_OWN;
				memset(tx_desc_p, 0, sizeof(struct tx_desc));
			}
			if (pch->req_irq_to_free != dma_chan[cn].irq)
				disable_irq(dma_chan[cn].irq);
			pch->desc_base = (u32) g_desc_list +
				cn * dma_desc_num * 8;
			pch->desc_len = dma_desc_num;

			pch->channel_packet_drop_enable =
				DMA_DEF_CHAN_BASED_PKT_DROP_EN;
			pch->peri_to_peri = 0;
			pch->loopback_enable = 0;
			pch->req_irq_to_free = -1;
		}
		/* XXX, Should free buffer that is not transferred by dma */

	}
	for (i = 0; i < dev->max_rx_chan_num; i++) {
		pch = dev->rx_chan[i];
		cn = CN_GET(pch);
		spin_lock_irqsave(&g_dma_spinlock, flags);
		stats->dma_int_status &= ~(1 << cn);
		pch->curr_desc = 0;
		pch->prev_desc = 0;
		pch->control = IFX_DMA_CH_OFF;
		ltq_dma_w32(cn, DMA_CS);
		if (ltq_dma_r32(DMA_CS) != cn) {
			pr_info(":%d:%s: *DMA_CS (%d) != cn (%d)\n",
				__LINE__, __func__, ltq_dma_r32(DMA_CS), cn);
		}
		/* XXX, should disable all interrupts? */
		ltq_dma_w32(0, DMA_CIE);
		/* Disable this channel interrupt */
		ltq_dma_w32_mask((1 << cn), 0, DMA_IRNEN);
		ltq_dma_w32((1 << cn), DMA_IRNCR);
		ltq_dma_w32_mask(DMA_CCTRL_ON, 0 , DMA_CCTRL);
		for (j = 10000; (ltq_dma_r32(DMA_CCTRL) & DMA_CCTRL_ON)
			&& j > 0; j--)
			;
		spin_unlock_irqrestore(&g_dma_spinlock, flags);
		if (j == 0)
			pr_info("Port %d RX CH %d: can not be turned off, DMA_CCTRL %08x\n",
				port_no, i, ltq_dma_r32(DMA_CCTRL));
		for (j = 0; j < pch->desc_len; j++) {
			rx_desc_p = (struct rx_desc *) pch->desc_base + j;
			if (pch->loopback_enable ||
				(rx_desc_p->status.field.OWN == CPU_OWN
				&& rx_desc_p->status.field.C)
				|| (rx_desc_p->status.field.OWN == DMA_OWN
				&& rx_desc_p->status.field.data_length > 0)) {
				dev->buffer_free((u8 *)
					__va(rx_desc_p->data_pointer),
					(void *) pch->opt[j]);
			}
		}
		if (pch->req_irq_to_free != dma_chan[cn].irq)
			disable_irq(dma_chan[cn].irq);
		pch->desc_base = (u32) g_desc_list + cn * dma_desc_num * 8;
		pch->desc_len = dma_desc_num;

		pch->channel_packet_drop_enable =
			DMA_DEF_CHAN_BASED_PKT_DROP_EN;
		pch->peri_to_peri = 0;
		pch->loopback_enable = 0;
		pch->req_irq_to_free = -1;
	}
	/*  Port configuration stuff  */
	spin_lock_irqsave(&g_dma_spinlock, flags);
	ltq_dma_w32(port_no, DMA_PS);
	reg_val = DMA_PCTRL_TXWGT_SET_VAL(DMA_TX_PORT_DEFAULT_WEIGHT)
		| DMA_PCTRL_TXENDI_SET_VAL(DMA_DEFAULT_TX_ENDIANNESS)
		| DMA_PCTRL_RXENDI_SET_VAL(DMA_DEFAULT_RX_ENDIANNESS)
		| DMA_PCTRL_PDEN_SET_VAL(DMA_DEF_PORT_BASED_PKT_DROP_EN)
		| DMA_PCTRL_TXBL_SET_VAL(DMA_PORT_DEFAULT_TX_BURST_LEN)
		| DMA_PCTRL_RXBL_SET_VAL(DMA_PORT_DEFAULT_RX_BURST_LEN);
	reg_val &= ~DMA_PCTRL_GPC;
	ltq_dma_w32(reg_val, DMA_PCTRL);
	spin_unlock_irqrestore(&g_dma_spinlock, flags);
	return result;
}
EXPORT_SYMBOL(dma_device_unregister);

/*!
  \fn int dma_device_desc_setup(_dma_device_info *dma_dev, char *buf,
   size_t len)
  \ingroup  IFX_DMA_DRV_API
  \brief Setup the DMA channel descriptor.

  This function setup the descriptor of the DMA channel used by client driver.
  The client driver will handle the buffer allocation and
  do proper checking of buffer for DMA burst alignment.
  Handle with care when call this function as well as dma_device_read function
  (in both function the current descriptor is incrementing)

  \param    dma_dev --> pointer to DMA device structure
  \param    buf     --> Pointer to the Databuffer
  \param    len     --> number of bytes.
  \return 0
*/
int dma_device_desc_setup(_dma_device_info *dma_dev, char *buf,
			size_t len)
{
	int byte_offset = 0;
	struct rx_desc *rx_desc_p;

	_dma_channel_info *pch =
		dma_dev->rx_chan[dma_dev->current_rx_chan];

	rx_desc_p = (struct rx_desc *) pch->desc_base + pch->curr_desc;
	if (!(rx_desc_p->status.field.OWN == CPU_OWN)) {
		pr_err("%s: Err!!!(Already setup the descriptor)\n", __func__);
		return -EBUSY;
	}
	dma_cache_inv((unsigned long) buf, len);
	pch->opt[pch->curr_desc] = NULL;
	rx_desc_p->data_pointer = (u32) CPHYSADDR((u32) buf);
	rx_desc_p->status.word =
		(DMA_OWN << 31) | ((byte_offset) << 23) | len;
	smp_wmb();
	if (!dma_hw_poll) {
		int prev_cn;
		unsigned long flags;

		spin_lock_irqsave(&g_dma_spinlock, flags);
		prev_cn = ltq_dma_r32(DMA_CS);
		ltq_dma_w32(CN_GET(pch), DMA_CS);
		/* trigger descriptor fetching by clearing DUR interrupt */
		ltq_dma_w32(DMA_CIS_DUR, DMA_CIS);
		ltq_dma_w32(prev_cn, DMA_CS);
		spin_unlock_irqrestore(&g_dma_spinlock, flags);
	}
	/* Increase descriptor index and process wrap around
	   Handle it with care when use the dma_device_desc_setup fucntion* */
	if (!pch->desc_handle) {
		pch->curr_desc++;
		if (pch->curr_desc == pch->desc_len)
			pch->curr_desc = 0;
	}
	return 0;
}
EXPORT_SYMBOL(dma_device_desc_setup);

/*!
  \fn int dma_device_clear_desc_status_word(_dma_device_info *dma_dev,
  int dir)
  \ingroup  IFX_DMA_DRV_API
  \brief Clear the descriptor status word

  Clear the status word of the descriptor from the client driver once receive
  a pseudo interrupt(RCV_INT)
  from the DMA module to avoid duplicate interrupts from tasklet.
  This function used to exit from DMA tasklet(tasklet don't need to run again
  and again once the DMA owner bit belongs to CPU i.e. packet is available in
  descriptor ) This is used to avoid multiple pseudo interrupt (RCV_INT) per
  packet. All buffers are provided by DMA client. DMA client driver has no
  method to modify descriptor state. The only method is to change descriptor
  status.

  \param    dma_dev --> pointer to DMA device structure
  \param    dir     --> Direction of the channel
  \return 0
*/
int dma_device_clear_desc_status_word(_dma_device_info *dma_dev, int dir)
{
	struct rx_desc *rx_desc_p;
	struct tx_desc *tx_desc_p;
	_dma_channel_info *pch = NULL;

	if (dir == IFX_DMA_TX_CH) {
		pch = dma_dev->tx_chan[dma_dev->current_tx_chan];
		if (!pch->curr_desc) {
			tx_desc_p = (struct tx_desc *) pch->desc_base;
			tx_desc_p->status.word = 0x0;
		} else {
			tx_desc_p = (struct tx_desc *) pch->desc_base +
				(pch->curr_desc - 1);
			tx_desc_p->status.word = 0x0;
		}
		smp_wmb();
	} else if (dir == IFX_DMA_RX_CH) {
		pch = dma_dev->rx_chan[dma_dev->current_rx_chan];
		if (!pch->curr_desc) {
			rx_desc_p = (struct rx_desc *) pch->desc_base;
			rx_desc_p->status.word = 0x0;
		} else {
			rx_desc_p = (struct rx_desc *) pch->desc_base +
				(pch->curr_desc - 1);
			rx_desc_p->status.word = 0x0;
		}
		smp_wmb();
	} else {
		pr_err("%s: Unknow channel direction\n", __func__);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(dma_device_clear_desc_status_word);

/*!
  \fn void dma_device_setup_class_val(_dma_channel_info* pch, int cls)
  \ingroup  IFX_DMA_DRV_API
  \brief Set the DMA chanel class value

  This function used to setup the DMA class value..
  \param    pch --> pointer to DMA channel structure
  \param    cls --> chanel Class value
  \return void
*/
void dma_device_setup_class_val(_dma_channel_info *pch, int cls)
{
	unsigned long flags;
	int prev_cn, cn = CN_GET(pch);

	spin_lock_irqsave(&g_dma_spinlock, flags);
	prev_cn = ltq_dma_r32(DMA_CS);
	ltq_dma_w32(cn, DMA_CS);
	ltq_dma_w32_mask(0, dma_class_value_set(cls), DMA_CCTRL);
	ltq_dma_w32(prev_cn, DMA_CS);
	spin_unlock_irqrestore(&g_dma_spinlock, flags);
}
EXPORT_SYMBOL(dma_device_setup_class_val);

/*!
  \fn int dma_device_clear_int(_dma_device_info *dma_dev, int dir)
  \ingroup  IFX_DMA_DRV_API
  \brief Clear the interrupt status flag

  Clear the interrupt status flag from the client driver once receive
  pseudo interrupt
  from the dma driver to avoid duplicate interrupts from tasklet.
  This function used to exit from DMA tasklet(tasklet don't need to
  run again and again until the DMA owner bit is cleared)
  This is used to avoid multiple pseudo interrupt (RCV_INT) per packet.
  All buffers are provided by DMA client. DMA client driver has no method
  to modify descriptor state. The only method is to change interrupt status

  \param    dma_dev --> pointer to DMA device structure
  \param    dir     --> Direction of the channel
  \return 0
*/
int dma_device_clear_int(_dma_device_info *dma_dev, int dir)
{
	int cn;
	unsigned long flags;
	_dma_channel_info *pch = 0;
	struct dma_sched_status *stats =
		&per_cpu(dma_sched_stats, smp_processor_id());

	if (dir == IFX_DMA_TX_CH)
		pch = dma_dev->tx_chan[dma_dev->current_tx_chan];
	else if (dir == IFX_DMA_RX_CH)
		pch = dma_dev->rx_chan[dma_dev->current_rx_chan];
	else {
		pr_err("%s: Unknow channel direction\n", __func__);
		return -EINVAL;
	}
	cn = CN_GET(pch);
	spin_lock_irqsave(&g_dma_spinlock, flags);
	stats->dma_int_status &= ~(1 << cn);
	spin_unlock_irqrestore(&g_dma_spinlock, flags);
	return 0;
}
EXPORT_SYMBOL(dma_device_clear_int);

/*!
 \fn void poll_dma_ownership_bit(_dma_device_info *dma_dev)
 \ingroup  IFX_DMA_DRV_API
 \brief    poll the rx descriptor OWN and C bits
 \param    dma_dev --> pointer to DMA device structure
 \param    dir     --> Direction of the channel
 \return
*/
void poll_dma_ownership_bit(_dma_device_info *dma_dev)
{
	struct rx_desc *rx_desc_p;

	_dma_channel_info *pch =
		dma_dev->rx_chan[dma_dev->current_rx_chan];
	rx_desc_p = (struct rx_desc *) pch->desc_base + pch->curr_desc;

	while (rx_desc_p->status.field.OWN
		|| !(rx_desc_p->status.field.C))
		;
}
EXPORT_SYMBOL(poll_dma_ownership_bit);

/*!
  \fn int dma_device_read(struct dma_device_info* dma_dev,
   unsigned char** dataptr, void** opt)
  \ingroup  IFX_DMA_DRV_API
  \brief Get data packet from DMA Rx channel.

  This functions is called when the client driver gets a pseudo
  * DMA interrupt(RCV_INT). Handle with care when call this function
   as well as dma_device_desc_setup function.
  Allocated memory data pointer should align with DMA burst length.
  The follwoing action are done:
*   Get current receive descriptor
*   Allocate memory via buffer alloc callback function. If buffer
 allocation failed, then return 0 and use the existing data pointer.
*   Pass valid descriptor data pointer to the client buffer pointer.
*   Update current descriptor position.
*   Clear the descriptor status once it's called when the buffer is
*    not allocated with client driver and also set desc_handle is 1

  \param    dma_dev     --> pointer to DMA device structure
  \param    dataptr     --> pointer to received data
  \param    opt         --> Generic pointer
  \return   -1          --> Data is not available
	0           --> Data is available but buffer allocation for new
		descriptor failed.
	len	--> Valid packet data length.
*/
int dma_device_read(struct dma_device_info *dma_dev,
		unsigned char **dataptr, void **opt)
{
	unsigned long sys_flag;
	unsigned char *buf;
	int len, byte_offset = 0;
	void *p = NULL;
	struct rx_desc *rx_desc_p;
	_dma_channel_info *pch =
		dma_dev->rx_chan[dma_dev->current_rx_chan];

	spin_lock_irqsave(&pch->irq_lock, sys_flag);
	/* Get the rx data first */
	rx_desc_p = (struct rx_desc *) pch->desc_base + pch->curr_desc;
	if (!((rx_desc_p->status.field.OWN == CPU_OWN)
		&& rx_desc_p->status.field.C)) {
		spin_unlock_irqrestore(&pch->irq_lock, sys_flag);
		return -1;
	}
	buf = (u8 *) __va(rx_desc_p->data_pointer);
	*(u32 *) dataptr = (u32) buf;
	len = rx_desc_p->status.field.data_length;
	if (opt)
		*(int *) opt = (int) pch->opt[pch->curr_desc];

	/* Replace with a new allocated buffer */
	buf = dma_dev->buffer_alloc(pch->packet_size, &byte_offset, &p);
	if (buf != NULL) {
		pch->opt[pch->curr_desc] = p;
		if (ltq_is_ase())
			dma_cache_wback_inv((unsigned long) buf, 64);
		else
			dma_cache_inv((unsigned long) buf, pch->packet_size);
		rx_desc_p->data_pointer = (u32) CPHYSADDR((u32) buf);
		rx_desc_p->status.word = (DMA_OWN << 31) | ((byte_offset) << 23)
			| pch->packet_size;
		smp_wmb();
	} else {

	/** It will handle client driver using the dma_device_desc_setup
	 * function. So, handle with care. */
		if (!pch->desc_handle) {
			*(u32 *) dataptr = 0;
			if (opt)
				*(int *) opt = 0;
			len = 0;
			rx_desc_p->status.word = ((DMA_OWN << 31) |
				 ((byte_offset) << 23) | pch->packet_size);
			smp_wmb();
		} else {
			/* Better to clear used descriptor status bits
			 * (C, SOP & EOP) to avoid multiple read access as
			 * well as to keep current descriptor pointers
			 * correctly */
			rx_desc_p->status.word = 0;
		}
	}
	if (!dma_hw_poll) {
		unsigned long flags;

		spin_lock_irqsave(&g_dma_spinlock, flags);
		if (pch->dur) {
			unsigned int prev_cn = ltq_dma_r32(DMA_CS);

			ltq_dma_w32(CN_GET(pch), DMA_CS);
			ltq_dma_w32(DMA_CIS_DUR, DMA_CIS);
			ltq_dma_w32(prev_cn, DMA_CS);
			pch->dur = 0;
		}
		spin_unlock_irqrestore(&g_dma_spinlock, flags);
	}
	/* Increase descriptor index and process wrap around
	   Handle ith care when use the dma_device_desc_setup fucntion */
	pch->curr_desc++;
	if (pch->curr_desc == pch->desc_len)
		pch->curr_desc = 0;
	spin_unlock_irqrestore(&pch->irq_lock, sys_flag);
	return len;
}
EXPORT_SYMBOL(dma_device_read);

/*!
  \fn int dma_device_write(struct dma_device_info* dma_dev,
  unsigned char* dataptr, int len, void* opt)
  \ingroup  IFX_DMA_DRV_API
  \brief Write data Packet to DMA Tx channel.

  This function gets the data packet from the client driver and sends over on
  DMA channel.
  The following actions are done
*   Descriptor is not available then sends a pseudo DMA
*   interrupt(TX_BUF_FULL_INT) to client driver to stop transmission.
*   The client driver should stop the transmission and enable the Tx dma
*   channel interrupt.
*   Once descriptor is free then the DMA interrupt handler send an pseudo
*   DMA interrupt(TRANSMIT_CPT_INT)
*   to client driver to start transmission.
*   The client driver should start the transmission and disable the Tx dma
*   channel interrupt.
*   Previous descriptor already sendout, then Free memory via buffer free
*   callback function
*   Update current descriptor position
*   If the channel is not open, then it will open.
  \param   dma_dev --> pointer to DMA device structure
  \param   dataptr --> pointer to Transmit data
  \param   len     --> length of transmit data
  \param   opt     --> Generic void pointer
  \return length   --> Return the length of the packet.
*/
int dma_device_write(struct dma_device_info *dma_dev,
		unsigned char *dataptr, int len, void *opt)
{
	unsigned long sys_flag, flags;
	u32 cctrls;
	_dma_channel_info *pch;
	int cn, byte_offset, prev_ch;
	struct tx_desc *tx_desc_p;
	pch = dma_dev->tx_chan[dma_dev->current_tx_chan];
	cn = CN_GET(pch);

	spin_lock_irqsave(&pch->irq_lock, sys_flag);

	/* Set the previous descriptor pointer to verify data is sendout or not
	 * If its sendout then clear the buffer based on the client driver
	 * buffer free callaback function */
	tx_desc_p = (struct tx_desc *) pch->desc_base + pch->prev_desc;
	while (tx_desc_p->status.field.OWN == CPU_OWN
		&& tx_desc_p->status.field.C) {
		dma_dev->buffer_free((u8 *) __va(tx_desc_p->data_pointer),
			pch->opt[pch->prev_desc]);
		memset(tx_desc_p, 0, sizeof(struct tx_desc));
		pch->prev_desc = (pch->prev_desc + 1) % (pch->desc_len);
		tx_desc_p = (struct tx_desc *) pch->desc_base + pch->prev_desc;
	}
	/* Set the current descriptor pointer */
	tx_desc_p = (struct tx_desc *) pch->desc_base + pch->curr_desc;

	/* Check whether this descriptor is available CPU and DMA excute
	 * tasks in its own envionment. DMA will change ownership and
	 * complete bit. Check the descriptors are avaliable or not to
	 * process the packet. */
	if (tx_desc_p->status.field.OWN == DMA_OWN
		|| tx_desc_p->status.field.C) {
		/* This descriptor has not been released */
		dma_dev->intr_handler(dma_dev, TX_BUF_FULL_INT);
		spin_unlock_irqrestore(&pch->irq_lock, sys_flag);
		return 0;
	}

	pch->opt[pch->curr_desc] = opt;
	byte_offset = ((u32) CPHYSADDR((u32) dataptr)) %
		((dma_dev->tx_burst_len) * 4);
	if (ltq_is_ase())
		dma_cache_wback_inv((unsigned long) dataptr, len);
	else
		dma_cache_wback((unsigned long) dataptr, len);
	tx_desc_p->data_pointer = (u32)CPHYSADDR((u32)dataptr) - byte_offset;
	tx_desc_p->status.word = (DMA_OWN << 31) | DMA_DESC_SOP_SET |
		DMA_DESC_EOP_SET | ((byte_offset) << 23) | len;
	smp_wmb();
	pch->curr_desc++;
	if (pch->curr_desc == pch->desc_len)
		pch->curr_desc = 0;
	/* Check if the next descriptor is available */
	tx_desc_p = (struct tx_desc *) pch->desc_base + pch->curr_desc;
	if ((tx_desc_p->status.field.OWN == DMA_OWN)
		/* || tx_desc_p->status.field.C */)
		dma_dev->intr_handler(dma_dev, TX_BUF_FULL_INT);

	spin_lock_irqsave(&g_dma_spinlock, flags);
	prev_ch = ltq_dma_r32(DMA_CS);
	ltq_dma_w32(cn, DMA_CS);
	cctrls = ltq_dma_r32(DMA_CCTRL);
	ltq_dma_w32(prev_ch, DMA_CS);
	spin_unlock_irqrestore(&g_dma_spinlock, flags);

	/* If not open this channel, open it */
	if (!(cctrls & DMA_CCTRL_ON))
		pch->open(pch);

	if (!dma_hw_poll) {
		spin_lock_irqsave(&g_dma_spinlock, flags);
		prev_ch = ltq_dma_r32(DMA_CS);
		ltq_dma_w32(cn, DMA_CS);
		/* trigger descriptor fetching by clearing DUR interrupt */
		ltq_dma_w32(DMA_CIS_DUR, DMA_CIS);
		ltq_dma_w32(prev_ch, DMA_CS);
		spin_unlock_irqrestore(&g_dma_spinlock, flags);
	}
	spin_unlock_irqrestore(&pch->irq_lock, sys_flag);
	return len;
}
EXPORT_SYMBOL(dma_device_write);

int dma_device_write_sdio(struct dma_device_info *dma_dev,
			u32 *chunkdataptr, void *opt)
{
	unsigned long flags;
	u32 cctrls, byte_offset, prev_cn, status_word;
	_dma_channel_info *pch;
	int cn, len = 0;
	int total_length, cur_length, cur_chunk;
	struct tx_desc *tx_desc_p;
	struct tx_desc *first_tx_desc;
	u8 *dataptr;

	pch = dma_dev->tx_chan[dma_dev->current_tx_chan];
	cn = CN_GET(pch);
	tx_desc_p = (struct tx_desc *) pch->desc_base + pch->prev_desc;
	while (tx_desc_p->status.field.OWN == CPU_OWN
		&& tx_desc_p->status.field.C) {
		dma_dev->buffer_free((u8 *) __va(tx_desc_p->data_pointer),
			pch->opt[pch->prev_desc]);
		memset(tx_desc_p, 0, sizeof(struct tx_desc));
		pch->prev_desc = (pch->prev_desc + 1) % (pch->desc_len);
		tx_desc_p = (struct tx_desc *) pch->desc_base + pch->prev_desc;
	}

	total_length = chunkdataptr[0];
	cur_length = 0;
	cur_chunk = 0;

	first_tx_desc = (struct tx_desc *) pch->desc_base + pch->curr_desc;
	while (cur_length < total_length) {
		len = chunkdataptr[1 + cur_chunk];
		dataptr = (u8 *) chunkdataptr[2 + cur_chunk];
		cur_length += len;

		tx_desc_p = (struct tx_desc *) pch->desc_base + pch->curr_desc;

		/*Check whether this descriptor is available */
		if (tx_desc_p->status.field.OWN == DMA_OWN
			|| tx_desc_p->status.field.C) {
			/*if not , the tell the upper layer device */
			dma_dev->intr_handler(dma_dev, TX_BUF_FULL_INT);
			pr_info("%s : failed to write!\n", __func__);
			return 0;
		}
		pch->opt[pch->curr_desc] = opt;
		byte_offset = ((u32) CPHYSADDR((u32) dataptr)) %
			((dma_dev->tx_burst_len) * 4);
		dma_cache_wback_inv((unsigned long) dataptr, len);
		byte_offset = byte_offset & 0x1f;
		tx_desc_p->data_pointer =
			(u32)CPHYSADDR((u32) dataptr) - byte_offset;
		smp_wmb();
		/* For the first descriptor for this packet, we do not set
		 * the OWN bit to DM to prevent starting the DMA before
		 * everything has been set up */
		status_word = (byte_offset << 23) | len;
		if (cur_chunk == 0) /* First data packet for this transaction */
			status_word |= DMA_DESC_SOP_SET;
		else
			status_word |= (DMA_OWN << 31);

		if (cur_length == total_length)/* Last data packet */
			status_word |= DMA_DESC_EOP_SET;
		tx_desc_p->status.word = status_word;
		smp_wmb();

		pch->curr_desc++;
		if (pch->curr_desc == pch->desc_len)
			pch->curr_desc = 0;
		cur_chunk += 2; /* 2 words per entry (length, dataptr) */
	}
	/* Now let the DMA start working, by releasing the first descriptor
	 * for this packet. */
	/*Check whether this descriptor is available */
	tx_desc_p = (struct tx_desc *) pch->desc_base + pch->curr_desc;
	if (tx_desc_p->status.field.OWN == DMA_OWN) {
		/*if not , the tell the upper layer device */
		dma_dev->intr_handler(dma_dev, TX_BUF_FULL_INT);
	}
	spin_lock_irqsave(&g_dma_spinlock, flags);
	prev_cn = ltq_dma_r32(DMA_CS);
	ltq_dma_w32(cn, DMA_CS);
	cctrls = ltq_dma_r32(DMA_CCTRL);
	first_tx_desc->status.word |= (DMA_OWN << 31);
	ltq_dma_w32_mask(0, DMA_CIS_ALL, DMA_CIS);
	ltq_dma_w32(prev_cn, DMA_CS);
	spin_unlock_irqrestore(&g_dma_spinlock, flags);

	if (!(cctrls & DMA_CCTRL_ON))
		pch->open(pch);

	if (!dma_hw_poll) {
		spin_lock_irqsave(&g_dma_spinlock, flags);
		prev_cn = ltq_dma_r32(DMA_CS);
		ltq_dma_w32(cn, DMA_CS);
		/* trigger descriptor fetching by clearing DUR interrupt */
		ltq_dma_w32(DMA_CIS_DUR, DMA_CIS);
		ltq_dma_w32(prev_cn, DMA_CS);
		spin_unlock_irqrestore(&g_dma_spinlock, flags);
	}
	return len;
}
EXPORT_SYMBOL(dma_device_write_sdio);

/*!
  \fn int dma_device_poll(struct dma_device_info* dma_dev,
    int work_to_do, int *work_done)
  \ingroup  IFX_DMA_DRV_API
  \brief Poll the DMA device

  This function keeps polls the DMA owner bit status.
  if valid packets available then  send a pseudo interrupt to the client driver

  \param  dma_dev     --> pointer to DMA device structure
  \param  work_to_do  --> poll budget value
  \param  work_done   --> Remaining budget value
  \return 0/1.
*/
int dma_device_poll(struct dma_device_info *dma_dev, int work_to_do,
		    int *work_done)
{
#ifdef CONFIG_NAPI_ENABLED
	int ret, i, dma_int_status_mask, cn = 0;
	u32 prev_cn, cis;
	_dma_channel_info *pch;
	struct rx_desc *rx_desc_p;
	struct dma_sched_status *stats =
		&per_cpu(dma_sched_stats, smp_processor_id());
	unsigned long flags;

	dma_int_status_mask = 0;
	for (i = 0; i < dma_dev->max_rx_chan_num; i++)
		dma_int_status_mask |= (1 << CN_GET(dma_dev->rx_chan[i]));

	i = 0;
	while ((stats->dma_poll_int_status & dma_int_status_mask)) {
		cn = CN_GET(dma_dev->rx_chan[i]);
		pch = &dma_chan[cn];
		rx_desc_p = (struct rx_desc *) pch->desc_base + pch->curr_desc;

		if ((rx_desc_p->status.field.OWN == CPU_OWN)
			&& rx_desc_p->status.field.C) {
			dma_dev->current_rx_chan = pch->rel_chan_no;
			if (dma_dev->intr_handler)
				dma_dev->intr_handler(dma_dev, RCV_INT);
			spin_lock_irqsave(&g_dma_spinlock, flags);
			prev_cn = ltq_dma_r32(DMA_CS);
			ltq_dma_w32(cn, DMA_CS);
			cis = ltq_dma_r32(DMA_CIS);
			ltq_dma_w32((cis | DMA_CIS_ALL), DMA_CIS);
			ltq_dma_w32(prev_cn, DMA_CS);
			spin_unlock_irqrestore(&g_dma_spinlock, flags);
			if (!dma_hw_poll) {
				if (cis & DMA_CIS_DUR)
					pch->dur = 1;
			}

			if (++*work_done >= work_to_do)
				return 1;
		} else {
			spin_lock_irqsave(&g_dma_spinlock, flags);
			/* Clear the DMA Node status register bit to remove
			 * the dummy interrupts */
			ltq_dma_w32((1 << cn), DMA_IRNCR);
			prev_cn = ltq_dma_r32(DMA_CS);
			ltq_dma_w32(cn, DMA_CS);
			cis = ltq_dma_r32(DMA_CIS);

			/*
			 * In most cases, interrupt bit has been cleared in the
			 * first branch, it will hit the following. However,
			 * in some corner cases, the above branch doesn't
			 * happen, so it indicates that interrupt is received
			 * actually, but OWN bit and Complete bit not update
			 * yet, so go back to DMA tasklet and let DMA tasklet
			 * wait <polling>. This is the most efficient way.
			 * NB, only EOP and Descript Complete interrupt enabled
			 * in Channel Interrupt Enable Register.
			 */
			if ((cis & ltq_dma_r32(DMA_CIE)) == 0) {
				if (rx_desc_p->status.field.OWN != CPU_OWN) {
					stats->dma_poll_int_status &=
						~(1 << cn);
				}
				/*
				 * Enable this channel interrupt again after
				 * processing all packets available
				 * Placing this line at the last line will avoid
				 * interrupt coming again in heavy load to great
				 * extent
				 */
				ltq_dma_w32_mask(0, (1 << cn), DMA_IRNEN);
			}
			ltq_dma_w32(prev_cn, DMA_CS);
			spin_unlock_irqrestore(&g_dma_spinlock, flags);
		}
		if (!dma_hw_poll) {
			/* remember that we ran out of descriptors,
			 * don't trigger polling now */
			if (cis & DMA_CIS_DUR)
				pch->dur = 1;
		}
		if (++i == dma_dev->max_rx_chan_num)
			i = 0;
	}
	spin_lock_irqsave(&g_dma_spinlock, flags);
	if ((stats->dma_poll_int_status & dma_int_status_mask) == 0) {
		if (dma_dev->inactivate_poll) {
			spin_unlock_irqrestore(&g_dma_spinlock, flags);
			dma_dev->inactivate_poll(dma_dev);
			spin_lock_irqsave(&g_dma_spinlock, flags);
		}
		ret = 0;
	} else
		ret = 1;

	spin_unlock_irqrestore(&g_dma_spinlock, flags);
	return ret;
#else
	return 0;
#endif /* CONFIG_NAPI_ENABLED */
}
EXPORT_SYMBOL(dma_device_poll);

static void *dma_desc_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= dma_max_chans)
		return NULL;
	return &dma_chan[*pos];
}

static void *dma_desc_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	if (++*pos >= dma_max_chans)
		return NULL;
	return  &dma_chan[*pos];
}

static void dma_desc_seq_stop(struct seq_file *s, void *v)
{

}

static int dma_desc_seq_show(struct seq_file *s, void *v)
{
	u32 *p;
	int desc_pos;
	_dma_channel_info *pch = (_dma_channel_info *)v;
	int cn = CN_GET(pch);

	p = (u32 *) pch->desc_base;

	for (desc_pos = 0; desc_pos < pch->desc_len; desc_pos++) {
		if (desc_pos == 0) {
			if (pch->dir == IFX_DMA_RX_CH) {
				seq_printf(s,
					"channel %d %s Rx descriptor list:\n",
					cn, ((_dma_device_info *)pch->dma_dev)
					->device_name);
			} else {
				seq_printf(s,
					"channel %d %s Tx descriptor list:\n",
					cn, ((_dma_device_info *)pch->dma_dev)
					->device_name);
			}
			seq_puts(s, " no  address        data pointer command");
			seq_puts(s, " bits Own, Complete, SoP, EoP, Offset)\n");
			seq_puts(s, "----------------------------------------");
			seq_puts(s, "--------------------------------------\n");
		}
		seq_printf(s, "%3d  ", desc_pos);
		seq_printf(s, "0x%08x     ", (u32) (p + (desc_pos * 2)));
		seq_printf(s, "%08x     ", *(p + (desc_pos * 2 + 1)));
		seq_printf(s, "%08x     ", *(p + (desc_pos * 2)));

		if (*(p + (desc_pos * 2)) & 0x80000000)
			seq_puts(s, "D ");
		else
			seq_puts(s, "C ");
		if (*(p + (desc_pos * 2)) & 0x40000000)
			seq_puts(s, "C ");
		else
			seq_puts(s, "c ");
		if (*(p + (desc_pos * 2)) & 0x20000000)
			seq_puts(s, "S ");
		else
			seq_puts(s, "s ");
		if (*(p + (desc_pos * 2)) & 0x10000000)
			seq_puts(s, "E ");
		else
			seq_puts(s, "e ");
		/* byte offset is different for rx and tx descriptors */
		if (pch->dir == IFX_DMA_RX_CH) {
			seq_printf(s, "%01x ",
				(*(p + (desc_pos * 2)) & 0x01800000) >> 23);
		} else {
			seq_printf(s, "%02x ",
				(*(p + (desc_pos * 2)) & 0x0F800000) >> 23);
		}
		if (pch->curr_desc == desc_pos)
			seq_puts(s, "<- CURR");
		if (pch->prev_desc == desc_pos)
			seq_puts(s, "<- PREV");
		seq_puts(s, "\n");
	}
	return 0;
}

static const struct seq_operations dma_desc_seq_ops = {
	.start = dma_desc_seq_start,
	.next = dma_desc_seq_next,
	.stop = dma_desc_seq_stop,
	.show = dma_desc_seq_show,
};

static int dma_desc_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &dma_desc_seq_ops);
}

static const struct file_operations dma_desc_proc_fops = {
	.open = dma_desc_seq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static void *dma_weight_seq_start(struct seq_file *s, loff_t *pos)
{
	seq_puts(s, "Qos dma channel weight list\n");
	seq_puts(s, "chan_no default_weight current_weight   device   Tx/Rx\n");
	if (*pos >= dma_max_chans)
		return NULL;
	return &dma_chan[*pos];
}

static void *dma_weight_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	if (++*pos >= dma_max_chans)
		return NULL;
	return  &dma_chan[*pos];
}

static void dma_weight_seq_stop(struct seq_file *s, void *v)
{

}

static int dma_weight_seq_show(struct seq_file *s, void *v)
{
	_dma_channel_info *pch = (_dma_channel_info *)v;
	int cn = CN_GET(pch);

	if (pch->dir == IFX_DMA_RX_CH) {
		seq_printf(s, "     %2d      %08x        %08x      %10s   Rx\n",
			cn, pch->default_weight, pch->weight,
			((_dma_device_info *) (pch->dma_dev))->device_name);
	} else {
		seq_printf(s, "     %2d      %08x        %08x      %10s   Tx\n",
			cn, pch->default_weight, pch->weight,
			((_dma_device_info *) (pch->dma_dev))->device_name);
	}
	return 0;
}

static const struct seq_operations dma_weight_seq_ops = {
	.start = dma_weight_seq_start,
	.next = dma_weight_seq_next,
	.stop = dma_weight_seq_stop,
	.show = dma_weight_seq_show,
};

static int dma_weight_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &dma_weight_seq_ops);
}

static const struct file_operations dma_weight_proc_fops = {
	.open = dma_weight_seq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static void *dma_reg_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= dma_max_chans)
		return NULL;
	return &dma_chan[*pos];
}

static void *dma_reg_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	if (++*pos >= dma_max_chans)
		return NULL;
	return  &dma_chan[*pos];
}

static void dma_reg_seq_stop(struct seq_file *s, void *v)
{
}

static int dma_reg_seq_show(struct seq_file *s, void *v)
{
	_dma_channel_info *pch = (_dma_channel_info *)v;
	int cn = CN_GET(pch);

	seq_puts(s, "-----------------------------------------\n");
	if (pch->dir == IFX_DMA_RX_CH) {
		seq_printf(s, "Channel %d - Device %s Rx\n", cn,
			((_dma_device_info *) (pch->dma_dev))->device_name);
	} else {
		seq_printf(s, "Channel %d - Device %s Tx\n", cn,
			((_dma_device_info *) (pch->dma_dev))->device_name);
	}
	ltq_dma_w32(cn, DMA_CS);
	seq_printf(s, "DMA_CCTRL=  %08x\n", ltq_dma_r32(DMA_CCTRL));
	seq_printf(s, "DMA_CDBA=   %08x\n", ltq_dma_r32(DMA_CDBA));
	seq_printf(s, "DMA_CIE=    %08x\n", ltq_dma_r32(DMA_CIE));
	seq_printf(s, "DMA_CIS=    %08x\n", ltq_dma_r32(DMA_CIS));
	seq_printf(s, "DMA_CDLEN=  %08x\n", ltq_dma_r32(DMA_CDLEN));
	return 0;
}

static const struct seq_operations dma_reg_seq_ops = {
	.start = dma_reg_seq_start,
	.next = dma_reg_seq_next,
	.stop = dma_reg_seq_stop,
	.show = dma_reg_seq_show,
};

static int dma_reg_seq_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &dma_reg_seq_ops);
}

static const struct file_operations dma_reg_proc_fops = {
	.open = dma_reg_seq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int dma_gen_port_read_proc(struct seq_file *s, void *v)
{
	int i;

	seq_puts(s, "\nGeneral DMA Registers\n");
	seq_puts(s, "-----------------------------------------\n");
	seq_printf(s, "CLC=        %08x\n", ltq_dma_r32(DMA_CLC));
	seq_printf(s, "ID=         %08x\n", ltq_dma_r32(DMA_ID));
	seq_printf(s, "DMA_CPOLL=  %08x\n", ltq_dma_r32(DMA_CPOLL));
	seq_printf(s, "DMA_CS=     %08x\n", ltq_dma_r32(DMA_CS));
	seq_printf(s, "DMA_PS=     %08x\n", ltq_dma_r32(DMA_PS));
	seq_printf(s, "DMA_IRNEN=  %08x\n", ltq_dma_r32(DMA_IRNEN));
	seq_printf(s, "DMA_IRNCR=  %08x\n", ltq_dma_r32(DMA_IRNCR));
	seq_printf(s, "DMA_IRNICR= %08x\n", ltq_dma_r32(DMA_IRNICR));

	seq_puts(s, "\nDMA Port Registers\n");
	seq_puts(s, "-----------------------------------------\n");
	for (i = 0; i < dma_max_ports; i++) {
		ltq_dma_w32(i, DMA_PS);
		seq_printf(s, "Port %d DMA_PCTRL= %08x\n",
			i, ltq_dma_r32(DMA_PCTRL));
	}

	return 0;
}
static int dma_gen_port_read_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, dma_gen_port_read_proc, NULL);
}

static const struct file_operations dma_gen_port_proc_fops = {
	.open           = dma_gen_port_read_proc_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

/**  Set the  default values in dma device/channel structure */
static int map_dma_chan(struct ifx_dma_chan_map *map)
{
	int i, j, result;
	unsigned int reg_val;

	for (i = 0; i < dma_max_ports; i++)
		strcpy(dma_devs[i].device_name, dma_device_name[i]);

	for (i = 0; i < dma_max_chans; i++) {
		dma_chan[i].irq = map[i].irq;

		result = request_irq(dma_chan[i].irq, dma_interrupt,
			IRQF_DISABLED, "dma-core", (void *) &dma_chan[i]);
		if (result) {
			pr_err("%s: Request IRQ failed!!!\n", __func__);
			free_irq(dma_chan[i].irq, (void *) &dma_chan[i]);
			return -EFAULT;
		}
		disable_irq(dma_chan[i].irq);
	}
	for (i = 0; i < dma_max_ports; i++) {
		dma_devs[i].num_tx_chan = 0;
		dma_devs[i].num_rx_chan = 0;
		dma_devs[i].max_rx_chan_num = 0;
		dma_devs[i].max_tx_chan_num = 0;
		dma_devs[i].mem_port_control = 0;
		dma_devs[i].port_tx_weight = DMA_TX_PORT_DEFAULT_WEIGHT;
		dma_devs[i].tx_endianness_mode = DMA_DEFAULT_TX_ENDIANNESS;
		dma_devs[i].rx_endianness_mode = DMA_DEFAULT_RX_ENDIANNESS;
		dma_devs[i].port_packet_drop_enable =
			DMA_DEF_PORT_BASED_PKT_DROP_EN;
		dma_devs[i].port_packet_drop_counter = 0;
		dma_devs[i].buffer_alloc = &common_buffer_alloc;
		dma_devs[i].buffer_free = &common_buffer_free;
		dma_devs[i].intr_handler = NULL;
		dma_devs[i].tx_burst_len = DMA_PORT_DEFAULT_TX_BURST_LEN;
		dma_devs[i].rx_burst_len = DMA_PORT_DEFAULT_RX_BURST_LEN;
		dma_devs[i].port_num = i;

		for (j = 0; j < dma_max_chans; j++) {
			dma_chan[j].byte_offset = 0;
			dma_chan[j].open = &open_chan;
			dma_chan[j].close = &close_chan;
			dma_chan[j].reset = &reset_chan;
			dma_chan[j].enable_irq = &enable_ch_irq;
			dma_chan[j].disable_irq = &disable_ch_irq;
			dma_chan[j].rel_chan_no = map[j].rel_chan_no;
			dma_chan[j].control = IFX_DMA_CH_OFF;
			dma_chan[j].default_weight = DMA_CH_DEFAULT_WEIGHT;
			dma_chan[j].weight = dma_chan[j].default_weight;
			dma_chan[j].tx_channel_weight =
				DMA_TX_CHAN_DEFAULT_WEIGHT;
			dma_chan[j].channel_packet_drop_enable =
				DMA_DEF_CHAN_BASED_PKT_DROP_EN;
			dma_chan[j].channel_packet_drop_counter = 0;
			dma_chan[j].class_value = 0;
			dma_chan[j].peri_to_peri = 0;
			dma_chan[j].global_buffer_len = 0;
			dma_chan[j].loopback_enable = 0;
			dma_chan[j].loopback_channel_number = 0;
			dma_chan[j].curr_desc = 0;
			dma_chan[j].desc_handle = 0;
			dma_chan[j].prev_desc = 0;
			dma_chan[j].req_irq_to_free = -1;
			dma_chan[j].dur = 0;
		}

		for (j = 0; j < dma_max_chans; j++) {
			if (strcmp(dma_devs[i].device_name,
				map[j].dev_name) == 0) {
				if (map[j].dir == IFX_DMA_RX_CH) {
					dma_chan[j].dir = IFX_DMA_RX_CH;
					dma_devs[i].max_rx_chan_num++;
					dma_devs[i].rx_chan[dma_devs[i].
						max_rx_chan_num - 1] =
						&dma_chan[j];
					dma_devs[i].rx_chan[dma_devs[i].
						max_rx_chan_num - 1]->
						class_value = map[j].pri;
					dma_chan[j].dma_dev =
						(void *) &dma_devs[i];
					/*Program default class value */
					ltq_dma_w32(j, DMA_CS);
					reg_val = ltq_dma_r32(DMA_CCTRL);
					reg_val &= ~dma_class_mask_get();
					reg_val |=
						dma_class_value_set(map[j].pri);
					ltq_dma_w32(reg_val, DMA_CCTRL);
				} else if (map[j].dir == IFX_DMA_TX_CH) {
					dma_chan[j].dir = IFX_DMA_TX_CH;
					dma_devs[i].max_tx_chan_num++;
					dma_devs[i].tx_chan[dma_devs[i].
						max_tx_chan_num - 1] =
						&dma_chan[j];
					dma_devs[i].tx_chan[dma_devs[i].
						max_tx_chan_num - 1]->
						class_value = map[j].pri;
					dma_chan[j].dma_dev =
						(void *) &dma_devs[i];
					/*Program default class value */
					ltq_dma_w32(j, DMA_CS);
					reg_val = ltq_dma_r32(DMA_CCTRL);
					reg_val &= ~dma_class_mask_get();
					reg_val |=
						dma_class_value_set(map[j].pri);
					ltq_dma_w32(reg_val, DMA_CCTRL);
				} else
					pr_info("%s wrong dma channel map!\n",
						__func__);

			}
		}
	}

	return 0;
}

static int dma_chip_init(void)
{
	int i;

	/* Reset DMA */
	ltq_dma_w32_mask(0, DMA_CTRL_RST, DMA_CTRL);
	/* Disable all the interrupts first */
	ltq_dma_w32(0, DMA_IRNEN);
	ltq_dma_w32((1 << dma_max_chans) - 1, DMA_IRNCR);

	for (i = 0; i < dma_max_chans; i++) {
		ltq_dma_w32(i, DMA_CS);
		ltq_dma_w32_mask(0, DMA_CCTRL_RST, DMA_CCTRL);
		while ((ltq_dma_r32(DMA_CCTRL) & DMA_CCTRL_RST))
			;
		/* dislabe all interrupts here */
		ltq_dma_w32(0, DMA_CIE);
	}
	for (i = 0; i < dma_max_chans; i++)
		disable_chan(i);

	if (dma_hw_poll) {
		/* enable polling for all channels */
		ltq_dma_w32(DMA_CPOLL_EN |
			DMA_CPOLL_CNT_VAL(DMA_DEFAULT_POLL_VALUE), DMA_CPOLL);
	}
	if (dma_pkt_arb_enable) {
		if ((ltq_get_soc_type() == SOC_TYPE_VR9)
			&& !(ltq_r32((u32 *)0xBF401010) & 0x01)) { /* ToDo*/
			ltq_dma_w32_mask(DMA_CTRL_PKTARB, 0, DMA_CTRL);
		} else
			ltq_dma_w32_mask(0, DMA_CTRL_PKTARB, DMA_CTRL);
	}
	return 0;
}

/** create proc for debug  info, used ifx_dma_init_module */
static int dma_core_proc_init(void)
{
	struct proc_dir_entry *entry;

	g_dma_dir = proc_mkdir("driver/ltq_dma", NULL);
	if (!g_dma_dir)
		return -ENOMEM;

	entry = proc_create("register", 0,
			g_dma_dir, &dma_reg_proc_fops);
	if (!entry)
		goto err1;

	entry = proc_create("port", 0,
			g_dma_dir, &dma_gen_port_proc_fops);
	if (!entry)
		goto err2;

	entry = proc_create("chan_weight", 0,
			g_dma_dir, &dma_weight_proc_fops);
	if (!entry)
		goto err3;

	entry = proc_create("desc_list", 0,
			g_dma_dir, &dma_desc_proc_fops);
	if (!entry)
		goto err4;
	return 0;
err4:
	remove_proc_entry("chan_weight", g_dma_dir);
err3:
	remove_proc_entry("port", g_dma_dir);
err2:
	remove_proc_entry("register", g_dma_dir);
err1:
	remove_proc_entry("driver/ltq_dma", NULL);
	return -ENOMEM;
}

static void dma_build_device_chan_tbl(void)
{
	if (ltq_is_ase()) {
		const char device_name[3][DMA_DEV_NAME_LEN] = {
			{"PPE"}, {"SDIO"}, {"MCTRL0"}
		};
		const struct ifx_dma_chan_map dma_map[10] = {
		{0, "PPE",      IFX_DMA_RX_CH,  0,  INT_NUM_IM3_IRL0,    0},
		{0, "PPE",      IFX_DMA_TX_CH,  0,  INT_NUM_IM3_IRL0 + 1,    0},
		{0, "PPE",      IFX_DMA_RX_CH,  1,  INT_NUM_IM3_IRL0 + 2,    1},
		{0, "PPE",      IFX_DMA_TX_CH,  1,  INT_NUM_IM3_IRL0 + 3,    1},
		{0, "PPE",      IFX_DMA_RX_CH,  2,  INT_NUM_IM3_IRL0 + 4,    2},
		{0, "PPE",      IFX_DMA_RX_CH,  3,  INT_NUM_IM3_IRL0 + 5,    3},
		{1, "SDIO",     IFX_DMA_RX_CH,  0,  INT_NUM_IM3_IRL0 + 6,    0},
		{1, "SDIO",     IFX_DMA_TX_CH,  0,  INT_NUM_IM3_IRL0 + 7,    0},
		{2, "MCTRL0",   IFX_DMA_RX_CH,  0,  INT_NUM_IM3_IRL0 + 8,    0},
		{2, "MCTRL0",   IFX_DMA_TX_CH,  0,  INT_NUM_IM3_IRL0 + 9,    0},
		};
		memcpy(dma_device_name, device_name, sizeof(device_name));
		memcpy(ifx_default_dma_map, dma_map, sizeof(dma_map));
	} else if (ltq_is_danube() || ltq_is_ar9()) {
		const char device_name[6][DMA_DEV_NAME_LEN] = {
			{"PPE"}, {"DEU"}, {"SPI"}, {"SDIO"},
			{"MCTRL0"}, {"MCTRL1"}
		};
		const struct ifx_dma_chan_map dma_map[20] = {
		/* portnum, device name, channel direction, class value,
		 * IRQ number, relative channel number */
		{0, "PPE",      IFX_DMA_RX_CH,  0,  DMA_CH0_INT,    0},
		{0, "PPE",      IFX_DMA_TX_CH,  0,  DMA_CH1_INT,    0},
		{0, "PPE",      IFX_DMA_RX_CH,  1,  DMA_CH2_INT,    1},
		{0, "PPE",      IFX_DMA_TX_CH,  1,  DMA_CH3_INT,    1},
		{0, "PPE",      IFX_DMA_RX_CH,  2,  DMA_CH4_INT,    2},
		{0, "PPE",      IFX_DMA_TX_CH,  2,  DMA_CH5_INT,    2},
		{0, "PPE",      IFX_DMA_RX_CH,  3,  DMA_CH6_INT,    3},
		{0, "PPE",      IFX_DMA_TX_CH,  3,  DMA_CH7_INT,    3},
		{1, "DEU",      IFX_DMA_RX_CH,  0,  DMA_CH8_INT,    0},
		{1, "DEU",      IFX_DMA_TX_CH,  0,  DMA_CH9_INT,    0},
		{1, "DEU",      IFX_DMA_RX_CH,  1,  DMA_CH10_INT,   1},
		{1, "DEU",      IFX_DMA_TX_CH,  1,  DMA_CH11_INT,   1},
		{2, "SPI",      IFX_DMA_RX_CH,  0,  DMA_CH12_INT,   0},
		{2, "SPI",      IFX_DMA_TX_CH,  0,  DMA_CH13_INT,   0},
		{3, "SDIO",     IFX_DMA_RX_CH,  0,  DMA_CH14_INT,   0},
		{3, "SDIO",     IFX_DMA_TX_CH,  0,  DMA_CH15_INT,   0},
		{4, "MCTRL0",   IFX_DMA_RX_CH,  0,  DMA_CH16_INT,   0},
		{4, "MCTRL0",   IFX_DMA_TX_CH,  0,  DMA_CH17_INT,   0},
		{4, "MCTRL1",   IFX_DMA_RX_CH,  1,  DMA_CH18_INT,   1},
		{4, "MCTRL1",   IFX_DMA_TX_CH,  1,  DMA_CH19_INT,   1},
		};
		memcpy(dma_device_name, device_name, sizeof(device_name));
		memcpy(ifx_default_dma_map, dma_map, sizeof(dma_map));
	} else if (of_machine_is_compatible("lantiq,vr9")) {
		const char device_name[7][DMA_DEV_NAME_LEN] = {
			{"PPE"}, {"DEU"}, {"SPI"}, {"SDIO" },
			{"MCTRL"}, {"USIF"}, {"HSNAND"}
		};
		const struct ifx_dma_chan_map dma_map[28] = {
		/* portnum, device name, channel direction, class value,
		 * IRQ number, relative channel number */
		{0, "PPE",      IFX_DMA_RX_CH,  0,  DMA_CH0_INT,    0},
		{0, "PPE",      IFX_DMA_TX_CH,  0,  DMA_CH1_INT,    0},
		{0, "PPE",      IFX_DMA_RX_CH,  1,  DMA_CH2_INT,    1},
		{0, "PPE",      IFX_DMA_TX_CH,  1,  DMA_CH3_INT,    1},
		{0, "PPE",      IFX_DMA_RX_CH,  2,  DMA_CH4_INT,    2},
		{0, "PPE",      IFX_DMA_TX_CH,  2,  DMA_CH5_INT,    2},
		{0, "PPE",      IFX_DMA_RX_CH,  3,  DMA_CH6_INT,    3},
		{0, "PPE",      IFX_DMA_TX_CH,  3,  DMA_CH7_INT,    3},
		{1, "DEU",      IFX_DMA_RX_CH,  0,  DMA_CH8_INT,    0},
		{1, "DEU",      IFX_DMA_TX_CH,  0,  DMA_CH9_INT,    0},
		{1, "DEU",      IFX_DMA_RX_CH,  1,  DMA_CH10_INT,   1},
		{1, "DEU",      IFX_DMA_TX_CH,  1,  DMA_CH11_INT,   1},
		{2, "SPI",      IFX_DMA_RX_CH,  0,  DMA_CH12_INT,   0},
		{2, "SPI",      IFX_DMA_TX_CH,  0,  DMA_CH13_INT,   0},
		{3, "SDIO",     IFX_DMA_RX_CH,  0,  DMA_CH14_INT,   0},
		{3, "SDIO",     IFX_DMA_TX_CH,  0,  DMA_CH15_INT,   0},
		{4, "MCTRL",    IFX_DMA_RX_CH,  0,  DMA_CH16_INT,   0},
		{4, "MCTRL",    IFX_DMA_TX_CH,  0,  DMA_CH17_INT,   0},
		{4, "MCTRL",    IFX_DMA_RX_CH,  1,  DMA_CH18_INT,   1},
		{4, "MCTRL",    IFX_DMA_TX_CH,  1,  DMA_CH19_INT,   1},
		{0, "PPE",      IFX_DMA_RX_CH,  4,  DMA_CH20_INT,   4},
		{0, "PPE",      IFX_DMA_RX_CH,  5,  DMA_CH21_INT,   5},
		{0, "PPE",      IFX_DMA_RX_CH,  6,  DMA_CH22_INT,   6},
		{0, "PPE",      IFX_DMA_RX_CH,  7,  DMA_CH23_INT,   7},
		{5, "USIF",     IFX_DMA_RX_CH,  0,  DMA_CH24_INT,   0},
		{5, "USIF",     IFX_DMA_TX_CH,  0,  DMA_CH25_INT,   0},
		{6, "HSNAND",   IFX_DMA_RX_CH,  0,  DMA_CH26_INT,   0},
		{6, "HSNAND",   IFX_DMA_TX_CH,  0,  DMA_CH27_INT,   0},
		};
		memcpy(dma_device_name, device_name, sizeof(device_name));
		memcpy(ifx_default_dma_map, dma_map, sizeof(dma_map));
	} else if (of_machine_is_compatible("lantiq,ar10")
		|| of_machine_is_compatible("lantiq,grx390")) {
		const char device_name[5][DMA_DEV_NAME_LEN] = {
			{"PPE"}, {"DEU"}, {"USIF"},
			{"HSNAND"}, {"MCTRL"}
		};
		const struct ifx_dma_chan_map dma_map[24] = {
		/* portnum, device name, channel direction, class value,
		 * IRQ number, relative channel number */
		{0, "PPE",      IFX_DMA_RX_CH,  0,  DMA_CH0_INT,    0},
		{0, "PPE",      IFX_DMA_TX_CH,  0,  DMA_CH1_INT,    0},
		{0, "PPE",      IFX_DMA_RX_CH,  1,  DMA_CH2_INT,    1},
		{0, "PPE",      IFX_DMA_TX_CH,  1,  DMA_CH3_INT,    1},
		{0, "PPE",      IFX_DMA_RX_CH,  2,  DMA_CH4_INT,    2},
		{0, "PPE",      IFX_DMA_TX_CH,  2,  DMA_CH5_INT,    2},
		{0, "PPE",      IFX_DMA_RX_CH,  3,  DMA_CH6_INT,    3},
		{0, "PPE",      IFX_DMA_TX_CH,  3,  DMA_CH7_INT,    3},
		{1, "DEU",      IFX_DMA_RX_CH,  0,  DMA_CH8_INT,    0},
		{1, "DEU",      IFX_DMA_TX_CH,  0,  DMA_CH9_INT,    0},
		{1, "DEU",      IFX_DMA_RX_CH,  1,  DMA_CH10_INT,   1},
		{1, "DEU",      IFX_DMA_TX_CH,  1,  DMA_CH11_INT,   1},
		{2, "USIF",     IFX_DMA_RX_CH,  0,  DMA_CH12_INT,   0},
		{2, "USIF",     IFX_DMA_TX_CH,  0,  DMA_CH13_INT,   0},
		{3, "HSNAND",   IFX_DMA_RX_CH,  0,  DMA_CH14_INT,   0},
		{3, "HSNAND",   IFX_DMA_TX_CH,  0,  DMA_CH15_INT,   0},
		{4, "MCTRL",    IFX_DMA_RX_CH,  0,  DMA_CH16_INT,   0},
		{4, "MCTRL",    IFX_DMA_TX_CH,  0,  DMA_CH17_INT,   0},
		{4, "MCTRL",    IFX_DMA_RX_CH,  1,  DMA_CH18_INT,   1},
		{4, "MCTRL",    IFX_DMA_TX_CH,  1,  DMA_CH19_INT,   1},
		{0, "PPE",      IFX_DMA_RX_CH,  4,  DMA_CH20_INT,   4},
		{0, "PPE",      IFX_DMA_RX_CH,  5,  DMA_CH21_INT,   5},
		{0, "PPE",      IFX_DMA_RX_CH,  6,  DMA_CH22_INT,   6},
		{0, "PPE",      IFX_DMA_RX_CH,  7,  DMA_CH23_INT,   7},
		};
		memcpy(dma_device_name, device_name, sizeof(device_name));
		memcpy(ifx_default_dma_map, dma_map, sizeof(dma_map));
	} else
		pr_debug("%s: SoC not found\n", __func__);

}

static int ltq_dma_init(struct platform_device *pdev)
{
	int i;
	struct clk *clk;
	const __be32 *desc_num, *hw_poll, *pkt_arb;
	struct resource *res;
	struct device_node *node = pdev->dev.of_node;
	u32 id;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		panic("Failed to get dma resource");

	for_each_possible_cpu(i)
		tasklet_init(&dma_tasklet[i],
			do_dma_tasklet, (unsigned long) i);
	/* remap dma register range */
	ltq_dma_membase = devm_request_and_ioremap(&pdev->dev, res);
	if (!ltq_dma_membase)
		panic("Failed to remap dma resource");

	desc_num = of_get_property(node, "lantiq,desc-num", NULL);
	if (!desc_num)
		panic("Failed to get maximum DMA descriptor number");
	dma_desc_num = (*desc_num);
	if (dma_desc_num > MAX_DMA_DESC_NUM)
		dma_desc_num = MAX_DMA_DESC_NUM;

	hw_poll = of_get_property(node, "lantiq,dma-hw-poll", NULL);
	if (!hw_poll)
		panic("Failed to DMA hardware poll setting");
	dma_hw_poll = *hw_poll;

	pkt_arb = of_get_property(node, "lantiq,dma-pkt-arb", NULL);
	if (!pkt_arb)
		panic("Failed to DMA Packet Arbitration setting");
	dma_pkt_arb_enable = *pkt_arb;
	/* power up and reset the dma engine */
	clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk))
		panic("Failed to get dma clock");

	clk_enable(clk);

	id = ltq_dma_r32(DMA_ID);
	dma_max_ports = (id >> 16) & 0xf;
	dma_max_chans = id >> 20;
	/** Reset the dma channels and disable channel interrupts */
	dma_chip_init();

	dma_build_device_chan_tbl();
	/** Map the default values in dma device/channel structure */
	map_dma_chan(ifx_default_dma_map);

	g_desc_list = kmalloc(dma_desc_num * dma_max_chans * sizeof(u64),
				GFP_DMA);
	if (g_desc_list == NULL) {
		dev_err(&pdev->dev,
			"%s: No memory for DMA descriptors list\n", __func__);
		return -ENOMEM;
	}
	/** Invalidate the DMA cache memory for all the descriptors*/
	dma_cache_inv((unsigned long) g_desc_list,
		dma_desc_num * dma_max_chans * sizeof(u64));
	/** just backup when want to cleanup the module */
	g_desc_list_backup = g_desc_list;
	/** Set uncached memory for DMA descriptors */
	g_desc_list = (u64 *) ((u32) g_desc_list | KSEG1);
	/** Clear whole descriptor memory */
	memset(g_desc_list, 0, dma_desc_num * dma_max_chans * sizeof(u64));
	for (i = 0; i < dma_max_chans; i++) {
		/** init spin lock */
		spin_lock_init(&dma_chan[i].irq_lock);

		/** set the Desc base address based per channel */
		dma_chan[i].desc_base = (u32) g_desc_list +
			i * dma_desc_num * sizeof(u64);
		dma_chan[i].curr_desc = 0;
		/** Number of descriptor per channel */
		dma_chan[i].desc_len = dma_desc_num;
		/** select the channel */
		select_chan(i);
		/** set the desc base address and number of
		 * descriptord per channel */
		ltq_dma_w32(((u32)CPHYSADDR(dma_chan[i].desc_base)), DMA_CDBA);
		ltq_dma_w32(dma_chan[i].desc_len, DMA_CDLEN);
	}

	/** create proc for debug  info*/
	dma_core_proc_init();

	/* Print the driver version info */
	dev_info(&pdev->dev,
		"Init done - hw rev: %X, ports: %d, channels: %d\n",
		id & 0x1f, dma_max_ports, dma_max_chans);
	return 0;
}


static const struct of_device_id dma_match[] = {
	{ .compatible = "lantiq,dma-xway" },
	{},
};
MODULE_DEVICE_TABLE(of, dma_match);

static struct platform_driver dma_driver = {
	.probe = ltq_dma_init,
	.driver = {
		.name = "dma-xway",
		.owner = THIS_MODULE,
		.of_match_table = dma_match,
	},
};

static int __init dma_init(void)
{
	return platform_driver_register(&dma_driver);
}

postcore_initcall(dma_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Reddy.Mallikarjun@infineon.com");
MODULE_DESCRIPTION("LTQ CPE Central DMA device driver");
MODULE_SUPPORTED_DEVICE("LTQ CPE Devices(ASE, Danube, AR9, VR9, AR10/11)");
