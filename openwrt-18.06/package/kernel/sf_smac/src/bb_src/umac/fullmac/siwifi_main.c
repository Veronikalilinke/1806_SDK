/**
 *
 ******************************************************************************
 *
 * @file siwifi_main.c
 *
 * @brief Entry point of the SIWIFI driver
 *
 * Copyright (C) Siflower 2018-2025
 *
 ******************************************************************************
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/inetdevice.h>
#include <net/cfg80211.h>
#include <net/ip.h>
#include <linux/etherdevice.h>
#include <linux/of_net.h>
#include <linux/of.h>
#include <linux/of_device.h>
#ifdef CONFIG_SFAX8_FACTORY_READ
#include <sfax8_factory_read.h>
#endif
#include "siwifi_defs.h"
#include "siwifi_mpw0.h"
#include "siwifi_msg_tx.h"
#include "siwifi_tx.h"
#include "reg_access.h"
#include "hal_desc.h"
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
#include "siwifi_debugfs.h"
#endif
#include "siwifi_cfgfile.h"
#include "siwifi_irqs.h"
#include "siwifi_radar.h"
#include "siwifi_version.h"
#ifdef CONFIG_SIWIFI_BFMER
#include "siwifi_bfmer.h"
#endif //(CONFIG_SIWIFI_BFMER)
#include "siwifi_tdls.h"
#include "siwifi_events.h"
#include "siwifi_compat.h"
#ifdef CONFIG_SF16A18_WIFI_ATE_TOOLS
#include "siwifi_ioctl.h"
#endif
#ifdef CONFIG_HEART_BEAT
#include <linux/timer.h>
#include "linux/sched.h"
#define MSECS(t)        msecs_to_jiffies(t)
#define SIWIFI_HEART_BEAT_TIME_MS  (15*1000)
#endif
#if defined(CONFIG_SF19A28_FULLMASK) && IS_ENABLED(CONFIG_SFAX8_HNAT_DRIVER) && IS_ENABLED(CONFIG_NF_FLOW_TABLE)
#include <linux/netfilter.h>
#include <net/netfilter/nf_flow_table.h>
#include "sf_hnat.h"
#endif

#if defined(CONFIG_SIWIFI_AMSDUS_TX) || defined(CONFIG_BRIDGE_ACCELERATE)
#include "siwifi_traffic.h"
#endif
#ifdef CONFIG_VDR_HW
#include "hw_interface.h"
#endif
#ifdef CONFIG_SF19A28_WIFI_LED
#include <linux/of_gpio.h>
#include "siwifi_led.h"
#endif
#ifdef CONFIG_SIWIFI_IGMP
#include "siwifi_igmp.h"
#endif
#if defined (CONFIG_SIWIFI_ACS) || defined (CONFIG_SIWIFI_ACS_INTERNAL)
#include "siwifi_acs.h"
#endif

#include "siwifi_mem.h"
#include "ipc_host.h"

#ifdef CONFIG_SIWIFI_REPEATER
#include "siwifi_repeater.h"
#endif

#define SF_DRV_DESCRIPTION  "Siflower 11nac driver for Linux cfg80211"
#define SF_DRV_COPYRIGHT    "Copyright(c) 2018-2025 Siflower"
#define SF_DRV_AUTHOR       "Siflower Wireless Team"

#define SIWIFI_PRINT_CFM_ERR(req) \
        printk(KERN_CRIT "%s: Status Error(%d)\n", #req, (&req##_cfm)->status)

#define SIWIFI_HT_CAPABILITIES                                    \
{                                                               \
    .ht_supported   = true,                                     \
    .cap            = 0,                                        \
    .ampdu_factor   = IEEE80211_HT_MAX_AMPDU_64K,               \
    .ampdu_density  = IEEE80211_HT_MPDU_DENSITY_16,             \
    .mcs        = {                                             \
        .rx_mask = { 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, },        \
        .rx_highest = cpu_to_le16(65),                          \
        .tx_params = IEEE80211_HT_MCS_TX_DEFINED,               \
    },                                                          \
}

#define SIWIFI_VHT_CAPABILITIES                                   \
{                                                               \
    .vht_supported = false,                                     \
    .cap       =                                                \
      (7 << IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_SHIFT),\
    .vht_mcs       = {                                          \
        .rx_mcs_map = cpu_to_le16(                              \
                      IEEE80211_VHT_MCS_SUPPORT_0_9    << 0  |  \
                      IEEE80211_VHT_MCS_NOT_SUPPORTED  << 2  |  \
                      IEEE80211_VHT_MCS_NOT_SUPPORTED  << 4  |  \
                      IEEE80211_VHT_MCS_NOT_SUPPORTED  << 6  |  \
                      IEEE80211_VHT_MCS_NOT_SUPPORTED  << 8  |  \
                      IEEE80211_VHT_MCS_NOT_SUPPORTED  << 10 |  \
                      IEEE80211_VHT_MCS_NOT_SUPPORTED  << 12 |  \
                      IEEE80211_VHT_MCS_NOT_SUPPORTED  << 14),  \
        .tx_mcs_map = cpu_to_le16(                              \
                      IEEE80211_VHT_MCS_SUPPORT_0_9    << 0  |  \
                      IEEE80211_VHT_MCS_NOT_SUPPORTED  << 2  |  \
                      IEEE80211_VHT_MCS_NOT_SUPPORTED  << 4  |  \
                      IEEE80211_VHT_MCS_NOT_SUPPORTED  << 6  |  \
                      IEEE80211_VHT_MCS_NOT_SUPPORTED  << 8  |  \
                      IEEE80211_VHT_MCS_NOT_SUPPORTED  << 10 |  \
                      IEEE80211_VHT_MCS_NOT_SUPPORTED  << 12 |  \
                      IEEE80211_VHT_MCS_NOT_SUPPORTED  << 14),  \
    }                                                           \
}

#if MY_LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
#define SIWIFI_HE_CAPABILITIES                                    \
{                                                               \
    .has_he = false,                                            \
    .he_cap_elem = {                                            \
        .mac_cap_info[0] = 0,                                   \
        .mac_cap_info[1] = 0,                                   \
        .mac_cap_info[2] = 0,                                   \
        .mac_cap_info[3] = 0,                                   \
        .mac_cap_info[4] = 0,                                   \
        .phy_cap_info[0] = 0,                                   \
        .phy_cap_info[1] = 0,                                   \
        .phy_cap_info[2] = 0,                                   \
        .phy_cap_info[3] = 0,                                   \
        .phy_cap_info[4] = 0,                                   \
        .phy_cap_info[5] = 0,                                   \
        .phy_cap_info[6] = 0,                                   \
        .phy_cap_info[7] = 0,                                   \
        .phy_cap_info[8] = 0,                                   \
    },                                                          \
    .he_mcs_nss_supp = {                                        \
        .rx_mcs_80 = cpu_to_le16(0xfffa),                       \
        .tx_mcs_80 = cpu_to_le16(0xfffa),                       \
        .rx_mcs_160 = cpu_to_le16(0xffff),                      \
        .tx_mcs_160 = cpu_to_le16(0xffff),                      \
        .rx_mcs_80p80 = cpu_to_le16(0xffff),                    \
        .tx_mcs_80p80 = cpu_to_le16(0xffff),                    \
    },                                                          \
    .ppe_thres = {0x61, 0x1c, 0xc7, 0x71},                      \
}
#endif

#define RATE(_bitrate, _hw_rate, _flags) {      \
    .bitrate    = (_bitrate),                   \
    .flags      = (_flags),                     \
    .hw_value   = (_hw_rate),                   \
}

#define CHAN(_freq) {                           \
    .center_freq    = (_freq),                  \
    .max_power  = 31, /* FIXME */               \
}


static struct ieee80211_rate siwifi_ratetable[] = {
    RATE(10,  0x00, 0),
    RATE(20,  0x01, IEEE80211_RATE_SHORT_PREAMBLE),
    RATE(55,  0x02, IEEE80211_RATE_SHORT_PREAMBLE),
    RATE(110, 0x03, IEEE80211_RATE_SHORT_PREAMBLE),
    RATE(60,  0x04, 0),
    RATE(90,  0x05, 0),
    RATE(120, 0x06, 0),
    RATE(180, 0x07, 0),
    RATE(240, 0x08, 0),
    RATE(360, 0x09, 0),
    RATE(480, 0x0A, 0),
    RATE(540, 0x0B, 0),
};

/* The channels indexes here are not used anymore */
static struct ieee80211_channel siwifi_2ghz_channels[] = {
    CHAN(2412),
    CHAN(2417),
    CHAN(2422),
    CHAN(2427),
    CHAN(2432),
    CHAN(2437),
    CHAN(2442),
    CHAN(2447),
    CHAN(2452),
    CHAN(2457),
    CHAN(2462),
    CHAN(2467),
    CHAN(2472),
    CHAN(2484),
};

static struct ieee80211_channel siwifi_5ghz_channels[] = {
    CHAN(5180),             // 36 -   20MHz
    CHAN(5200),             // 40 -   20MHz
    CHAN(5220),             // 44 -   20MHz
    CHAN(5240),             // 48 -   20MHz
    CHAN(5260),             // 52 -   20MHz
    CHAN(5280),             // 56 -   20MHz
    CHAN(5300),             // 60 -   20MHz
    CHAN(5320),             // 64 -   20MHz
    CHAN(5500),             // 100 -  20MHz
    CHAN(5520),             // 104 -  20MHz
    CHAN(5540),             // 108 -  20MHz
    CHAN(5560),             // 112 -  20MHz
    CHAN(5580),             // 116 -  20MHz
    CHAN(5600),             // 120 -  20MHz
    CHAN(5620),             // 124 -  20MHz
    CHAN(5640),             // 128 -  20MHz
    CHAN(5660),             // 132 -  20MHz
    CHAN(5680),             // 136 -  20MHz
    CHAN(5700),             // 140 -  20MHz
    CHAN(5720),             // 144 -  20MHz
    CHAN(5745),             // 149 -  20MHz
    CHAN(5765),             // 153 -  20MHz
    CHAN(5785),             // 157 -  20MHz
    CHAN(5805),             // 161 -  20MHz
    CHAN(5825),             // 165 -  20MHz
};

#if MY_LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
static struct ieee80211_sband_iftype_data siwifi_he_capa = {
    .types_mask = BIT(NL80211_IFTYPE_STATION),
    .he_cap = SIWIFI_HE_CAPABILITIES,
};
#endif

static struct ieee80211_supported_band siwifi_band_2GHz = {
    .channels   = siwifi_2ghz_channels,
    .n_channels = ARRAY_SIZE(siwifi_2ghz_channels),
    .bitrates   = siwifi_ratetable,
    .n_bitrates = ARRAY_SIZE(siwifi_ratetable),
    .ht_cap     = SIWIFI_HT_CAPABILITIES,
    .vht_cap    = SIWIFI_VHT_CAPABILITIES,
#if MY_LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
    .iftype_data = &siwifi_he_capa,
    .n_iftype_data = 1,
#endif
};

static struct ieee80211_supported_band siwifi_band_5GHz = {
    .channels   = siwifi_5ghz_channels,
    .n_channels = ARRAY_SIZE(siwifi_5ghz_channels),
    .bitrates   = &siwifi_ratetable[4],
    .n_bitrates = ARRAY_SIZE(siwifi_ratetable) - 4,
    .ht_cap     = SIWIFI_HT_CAPABILITIES,
    .vht_cap    = SIWIFI_VHT_CAPABILITIES,
#if MY_LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
    .iftype_data = &siwifi_he_capa,
    .n_iftype_data = 1,
#endif
};

static struct ieee80211_iface_limit siwifi_limits[] = {
    { .max = NX_VIRT_DEV_MAX, .types = BIT(NL80211_IFTYPE_AP) |
                                       BIT(NL80211_IFTYPE_STATION)}
};

static struct ieee80211_iface_limit siwifi_limits_dfs[] = {
    { .max = NX_VIRT_DEV_MAX, .types = BIT(NL80211_IFTYPE_AP)}
};

static const struct ieee80211_iface_combination siwifi_combinations[] = {
    {
        .limits                 = siwifi_limits,
        .n_limits               = ARRAY_SIZE(siwifi_limits),
        .num_different_channels = NX_CHAN_CTXT_CNT,
        .max_interfaces         = NX_VIRT_DEV_MAX,
    },
    /* Keep this combination as the last one */
    {
        .limits                 = siwifi_limits_dfs,
        .n_limits               = ARRAY_SIZE(siwifi_limits_dfs),
        .num_different_channels = 1,
        .max_interfaces         = NX_VIRT_DEV_MAX,
        .radar_detect_widths = (BIT(NL80211_CHAN_WIDTH_20_NOHT) |
                                BIT(NL80211_CHAN_WIDTH_20) |
                                BIT(NL80211_CHAN_WIDTH_40) |
                                BIT(NL80211_CHAN_WIDTH_80)),
    }
};

/* There isn't a lot of sense in it, but you can transmit anything you like */
static struct ieee80211_txrx_stypes
siwifi_default_mgmt_stypes[NUM_NL80211_IFTYPES] = {
    [NL80211_IFTYPE_STATION] = {
        .tx = 0xffff,
        .rx = (BIT(IEEE80211_STYPE_ACTION >> 4) |
               BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
               BIT(IEEE80211_STYPE_AUTH >> 4)),
    },
    [NL80211_IFTYPE_AP] = {
        .tx = 0xffff,
        .rx = (BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
               BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
               BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
               BIT(IEEE80211_STYPE_DISASSOC >> 4) |
               BIT(IEEE80211_STYPE_AUTH >> 4) |
               BIT(IEEE80211_STYPE_DEAUTH >> 4) |
               BIT(IEEE80211_STYPE_ACTION >> 4)),
    },
    [NL80211_IFTYPE_AP_VLAN] = {
        /* copy AP */
        .tx = 0xffff,
        .rx = (BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
               BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
               BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
               BIT(IEEE80211_STYPE_DISASSOC >> 4) |
               BIT(IEEE80211_STYPE_AUTH >> 4) |
               BIT(IEEE80211_STYPE_DEAUTH >> 4) |
               BIT(IEEE80211_STYPE_ACTION >> 4)),
    },
    [NL80211_IFTYPE_P2P_CLIENT] = {
        .tx = 0xffff,
        .rx = (BIT(IEEE80211_STYPE_ACTION >> 4) |
               BIT(IEEE80211_STYPE_PROBE_REQ >> 4)),
    },
    [NL80211_IFTYPE_P2P_GO] = {
        .tx = 0xffff,
        .rx = (BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
               BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
               BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
               BIT(IEEE80211_STYPE_DISASSOC >> 4) |
               BIT(IEEE80211_STYPE_AUTH >> 4) |
               BIT(IEEE80211_STYPE_DEAUTH >> 4) |
               BIT(IEEE80211_STYPE_ACTION >> 4)),
    },
    [NL80211_IFTYPE_P2P_DEVICE] = {
        .tx = 0xffff,
        .rx = (BIT(IEEE80211_STYPE_ACTION >> 4) |
               BIT(IEEE80211_STYPE_PROBE_REQ >> 4)),
    },
    [NL80211_IFTYPE_MESH_POINT] = {
        .tx = 0xffff,
        .rx = (BIT(IEEE80211_STYPE_ACTION >> 4) |
               BIT(IEEE80211_STYPE_AUTH >> 4) |
               BIT(IEEE80211_STYPE_DEAUTH >> 4)),
    },
};


static u32 cipher_suites[] = {
    WLAN_CIPHER_SUITE_WEP40,
    WLAN_CIPHER_SUITE_WEP104,
    WLAN_CIPHER_SUITE_TKIP,
    WLAN_CIPHER_SUITE_CCMP,
    0, // reserved entries to enable AES-CMAC and/or SMS4
    0,
};
#define NB_RESERVED_CIPHER 2;

static const int siwifi_ac2hwq[1][NL80211_NUM_ACS] = {
    {
        [NL80211_TXQ_Q_VO] = SIWIFI_HWQ_VO,
        [NL80211_TXQ_Q_VI] = SIWIFI_HWQ_VI,
        [NL80211_TXQ_Q_BE] = SIWIFI_HWQ_BE,
        [NL80211_TXQ_Q_BK] = SIWIFI_HWQ_BK
    }
};

const int siwifi_tid2hwq[IEEE80211_NUM_TIDS] = {
    SIWIFI_HWQ_BE,
    SIWIFI_HWQ_BK,
    SIWIFI_HWQ_BK,
    SIWIFI_HWQ_BE,
    SIWIFI_HWQ_VI,
    SIWIFI_HWQ_VI,
    SIWIFI_HWQ_VO,
    SIWIFI_HWQ_VO,
    /* TID_8 is used for management frames */
    SIWIFI_HWQ_VO,
    /* At the moment, all others TID are mapped to BE */
    SIWIFI_HWQ_BE,
    SIWIFI_HWQ_BE,
    SIWIFI_HWQ_BE,
    SIWIFI_HWQ_BE,
    SIWIFI_HWQ_BE,
    SIWIFI_HWQ_BE,
    SIWIFI_HWQ_BE,
};

static const int siwifi_hwq2uapsd[NL80211_NUM_ACS] = {
    [SIWIFI_HWQ_VO] = IEEE80211_WMM_IE_STA_QOSINFO_AC_VO,
    [SIWIFI_HWQ_VI] = IEEE80211_WMM_IE_STA_QOSINFO_AC_VI,
    [SIWIFI_HWQ_BE] = IEEE80211_WMM_IE_STA_QOSINFO_AC_BE,
    [SIWIFI_HWQ_BK] = IEEE80211_WMM_IE_STA_QOSINFO_AC_BK,
};

static int32_t siwifi_wiphy_addmask[9]  = {
    0,
    1,
    1,
    3,
    3,
    7,
    7,
    7,
    7
};

/* For calculating legacy rate */
extern const uint8_t legrates_lut[];
static const uint32_t legrates_tbl[] = {10, 20, 55, 110,
    60, 90, 120, 180, 240, 360, 480, 540};

#ifdef CONFIG_SIWIFI_CACHE_ALLOC
struct kmem_cache *sw_txhdr_cache __read_mostly;
#endif

/**
 * siwifi_calculate_legrate - Calculate legacy bitrate
 * reference to RW-WLAN-nX-MAC-PHY-IF-FS
 *
 * @legrate: legacy Rate of the PPDU, legRate[3:0]
 */
static uint32_t siwifi_calculate_legrate(uint8_t legrate, bool is_tx)
{
    int rate_idx;
    uint32_t rate_kbps = 0;
    if (legrate > 15) {
        printk("%s invalid legrate: %u\n", __func__, legrate);
        return rate_kbps;
    }

    if (is_tx)
        rate_idx = legrate;
    else
        rate_idx = legrates_lut[legrate];

    rate_idx = legrates_lut[legrate];
    if (rate_idx < 0 || rate_idx > 11) {
        printk("%s invalid legrate: %u\n", __func__, legrate);
        return rate_kbps;
    }
    return legrates_tbl[rate_idx];
}

/*********************************************************************
 * helper
 *********************************************************************/
static void siwifi_get_cur_survey_info(struct wiphy *wiphy, struct wireless_dev *wdev,
        uint16_t center_freq, uint8_t survey_idx);

struct siwifi_sta *siwifi_get_sta(struct siwifi_hw *siwifi_hw, const u8 *mac_addr)
{
    int i;

    for (i = 0; i < NX_REMOTE_STA_MAX; i++) {
        struct siwifi_sta *sta = &siwifi_hw->sta_table[i];
        if (sta->valid && (memcmp(mac_addr, sta->mac_addr, 6) == 0))
            return sta;
    }

    return NULL;
}

void siwifi_enable_wapi(struct siwifi_hw *siwifi_hw)
{
#if DEBUG_ARRAY_CHECK
    BUG_ON(siwifi_hw->wiphy->n_cipher_suites >= sizeof(cipher_suites) / sizeof(u32));
#endif
    cipher_suites[siwifi_hw->wiphy->n_cipher_suites] = WLAN_CIPHER_SUITE_SMS4;
    siwifi_hw->wiphy->n_cipher_suites ++;
    siwifi_hw->wiphy->flags |= WIPHY_FLAG_CONTROL_PORT_PROTOCOL;
}

void siwifi_enable_mfp(struct siwifi_hw *siwifi_hw)
{
#if DEBUG_ARRAY_CHECK
    BUG_ON(siwifi_hw->wiphy->n_cipher_suites >= sizeof(cipher_suites) / sizeof(u32));
#endif
    cipher_suites[siwifi_hw->wiphy->n_cipher_suites] = WLAN_CIPHER_SUITE_AES_CMAC;
    siwifi_hw->wiphy->n_cipher_suites ++;
}

// check whether any vif is switching channel
int siwifi_is_channel_switching(struct siwifi_hw *siwifi_hw)
{
    struct siwifi_vif *vif;

    list_for_each_entry(vif, &siwifi_hw->vifs, list) {
        if(SIWIFI_VIF_TYPE(vif) != NL80211_IFTYPE_AP)
            continue;

        if (vif->ap.channel_switching) {
            return 1;
        }
    }

    return 0;
}

u8 *siwifi_build_bcn(struct siwifi_bcn *bcn, struct cfg80211_beacon_data *new, u8 **real_addr)
{
    u8 *buf, *pos;

    if (new->head) {
        u8 *head = siwifi_kmalloc(new->head_len, GFP_KERNEL);

        if (!head)
            return NULL;

        if (bcn->head)
            siwifi_kfree(bcn->head);

        bcn->head = head;
        bcn->head_len = new->head_len;
        memcpy(bcn->head, new->head, new->head_len);
    }
    if (new->tail) {
        u8 *tail = siwifi_kmalloc(new->tail_len, GFP_KERNEL);

        if (!tail)
            return NULL;

        if (bcn->tail)
            siwifi_kfree(bcn->tail);

        bcn->tail = tail;
        bcn->tail_len = new->tail_len;
        memcpy(bcn->tail, new->tail, new->tail_len);
    }

    if (!bcn->head)
        return NULL;

    bcn->tim_len = 6;
    bcn->len = bcn->head_len + bcn->tail_len + bcn->ies_len + bcn->tim_len;

    buf = siwifi_kmalloc(bcn->len + 2 * cache_line_size(), GFP_KERNEL);
    if (!buf)
        return NULL;
    *real_addr = buf;
    buf = buf + cache_line_size();
    // Build the beacon buffer
    pos = buf;
    memcpy(pos, bcn->head, bcn->head_len);
    pos += bcn->head_len;
    *pos++ = WLAN_EID_TIM;
    *pos++ = 4;
    *pos++ = 0;
    *pos++ = bcn->dtim;
    *pos++ = 0;
    *pos++ = 0;
    if (bcn->tail) {
        memcpy(pos, bcn->tail, bcn->tail_len);
        pos += bcn->tail_len;
    }
    if (bcn->ies) {
        memcpy(pos, bcn->ies, bcn->ies_len);
    }

    return buf;
}

/*
 *  @start: Called before the first netdevice attached to the hardware
 *  s enabled. The message is sent by the timer from the umac to lmac .
 *  IF the lmac don't answer in SIWIFI_HEART_BEAT_TIME_MS for twice,the module
 *  will be recovering
 *  * */
#ifdef CONFIG_HEART_BEAT
extern void lmac_dump_registers(int band_type, uint32_t fexception_base);
#ifdef CONFIG_SEND_ERR
extern int ker_err_send(char *type, int module, int code, char *text, char *path, int flag);
#endif

extern void siwifi_dump_lmac_debug_info(struct ipc_shared_env_tag *shared_env_ptr);

static void heart_beat_handler(struct work_struct *wk)
{
    struct siwifi_hw *siwifi_hw = container_of(wk, struct siwifi_hw, heart_work.work);
#ifdef CONFIG_SEND_ERR
    int err = 0;
#endif
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
    struct siwifi_debugfs *siwifi_debugfs = &siwifi_hw->debugfs;
#endif
    //SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);
    mutex_lock(&siwifi_hw->dbgdump_elem.mutex);
    if (test_bit(SIWIFI_DEV_STACK_RESTARTING, &siwifi_hw->drv_flags)
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
                    || siwifi_hw->debugfs.recoverying
#endif
                    ) {
        mutex_unlock(&siwifi_hw->dbgdump_elem.mutex);
        schedule_delayed_work(&siwifi_hw->heart_work, MSECS(SIWIFI_HEART_BEAT_TIME_MS));
        return;
    } else {
        if (siwifi_send_heart(siwifi_hw)) {
            siwifi_hw->recovery_hb_num++;
            if (siwifi_hw->recovery_hb_num == 2) {
                siwifi_hw->recovery_hb_num = 0;
                siwifi_dump_lmac_debug_info(siwifi_hw->ipc_env->shared);
                msleep(2000);
                siwifi_dump_lmac_debug_info(siwifi_hw->ipc_env->shared);
                //try to dump registers if lmac crashed
                lmac_dump_registers(siwifi_hw->mod_params->is_hb, siwifi_hw->ipc_env->shared->comp_info.fexception_base);
                siwifi_trigger_restart(siwifi_hw);
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
                siwifi_debugfs->recovery_hb_count = siwifi_debugfs->recovery_hb_count + 1;
                printk("heart_beat  restart over. count %d\n",siwifi_debugfs->recovery_hb_count);
#endif
#ifdef CONFIG_SEND_ERR
                //report to log server
                err = ker_err_send("WARNING", 5, 101, "HEAD_BEAT", "klog/system log", 0);
                printk("ker_err_send(%d) ret2=%d\n",siwifi_hw->mod_params->is_hb,err);
#endif
            }
        } else {
            siwifi_hw->recovery_hb_num = 0;
            //SIWIFI_DBG(SIWIFI_FN_EXIT_STR);
        }
    }
    schedule_delayed_work(&siwifi_hw->heart_work, MSECS(SIWIFI_HEART_BEAT_TIME_MS));
    mutex_unlock(&siwifi_hw->dbgdump_elem.mutex);
    if(!(siwifi_hw->ate_env.ate_start))
        siwifi_channel_recovery_check(siwifi_hw);
}
#endif

static void siwifi_del_bcn(struct siwifi_bcn *bcn)
{
    if (bcn->head) {
        siwifi_kfree(bcn->head);
        bcn->head = NULL;
    }
    bcn->head_len = 0;

    if (bcn->tail) {
        siwifi_kfree(bcn->tail);
        bcn->tail = NULL;
    }
    bcn->tail_len = 0;

    if (bcn->ies) {
        siwifi_kfree(bcn->ies);
        bcn->ies = NULL;
    }
    bcn->ies_len = 0;
    bcn->tim_len = 0;
    bcn->dtim = 0;
    bcn->len = 0;
}

/**
 * Check whether the sta channel is the same as the current vif channel
 */
static int siwifi_check_vif_channel_same(struct siwifi_vif *vif, u8 ch_idx,
                                        struct cfg80211_chan_def *chandef, struct siwifi_chanctx *ctxt)
{
    // NL80211_IFTYPE_STATION channel info
    u8 sta_ch_idx;
    struct siwifi_vif *vif_sta = NULL;
    struct cfg80211_chan_def *chandef_sta = NULL;
    struct siwifi_chanctx *ctxt_sta = NULL;
    struct siwifi_vif *vif_tmp = NULL;
    struct siwifi_chanctx *ctxt2 = NULL;
    if (SIWIFI_VIF_TYPE(vif) == NL80211_IFTYPE_STATION) {
        vif_sta = vif;
        sta_ch_idx = ch_idx;
        chandef_sta = chandef;
        ctxt_sta = ctxt;
    } else {
        list_for_each_entry(vif_tmp, &vif->siwifi_hw->vifs, list) {
            if (SIWIFI_VIF_TYPE(vif_tmp) == NL80211_IFTYPE_STATION) {
                vif_sta = vif_tmp;
                sta_ch_idx = vif_tmp->ch_index;
                chandef_sta = &vif_tmp->siwifi_hw->chanctx_table[vif_tmp->ch_index].chan_def;
                ctxt_sta = &vif_tmp->siwifi_hw->chanctx_table[vif_tmp->ch_index];
            }
        }
    }
    if (!vif_sta || !chandef_sta || !ctxt_sta || (vif_sta->ch_index == SIWIFI_CH_NOT_SET)){
        return 1;
    }
    list_for_each_entry(vif_tmp, &vif_sta->siwifi_hw->vifs, list) {
        if (vif_tmp == vif_sta){
            continue;
        }
        ctxt2 = &vif_tmp->siwifi_hw->chanctx_table[vif_tmp->ch_index];
        if (ctxt2->chan_def.chan == NULL || chandef_sta->chan == NULL) {
            continue;
        }
        if (ctxt2->chan_def.chan->center_freq != chandef_sta->chan->center_freq || vif_tmp->ch_index != sta_ch_idx){
            printk("NL80211_IFTYPE_AP(%s freq %d ch_idx %d) should follow NL80211_IFTYPE_STATION (%s freq %d ch_idx %d) \n",
                    vif_tmp->ndev->name, ctxt2->chan_def.chan->center_freq, vif_tmp->ch_index,
                    vif_sta->ndev->name, chandef_sta->chan->center_freq, sta_ch_idx);
            // Notify upper layer channel has changed
            cfg80211_ch_switch_notify(vif_tmp->ndev, chandef_sta);
            siwifi_chanctx_unlink(vif_tmp);
            vif_tmp->ch_index = sta_ch_idx;
            ctxt_sta->count++;
            // Txq will not start because the channel has changed in @siwifi_rx_chan_switch_ind
            siwifi_txq_vif_start(vif_tmp, SIWIFI_TXQ_STOP_CHAN, vif_tmp->siwifi_hw);
        }
    }
    return 0;
}
/**
 * Link channel ctxt to a vif and thus increments count for this context.
 */
void siwifi_chanctx_link(struct siwifi_vif *vif, u8 ch_idx,
                       struct cfg80211_chan_def *chandef)
{
    struct siwifi_chanctx *ctxt;

    if (ch_idx >= NX_CHAN_CTXT_CNT) {
        WARN(1, "Invalid channel ctxt id %d", ch_idx);
        return;
    }

    vif->ch_index = ch_idx;
    ctxt = &vif->siwifi_hw->chanctx_table[ch_idx];
    ctxt->count++;

    // For now chandef is NULL for STATION interface
    if (chandef) {
        ctxt->chan_def = *chandef;

        siwifi_check_vif_channel_same(vif, ch_idx, chandef, ctxt);
    }
}

/**
 * Unlink channel ctxt from a vif and thus decrements count for this context
 */
void siwifi_chanctx_unlink(struct siwifi_vif *vif)
{
    struct siwifi_chanctx *ctxt;

    if (vif->ch_index == SIWIFI_CH_NOT_SET)
        return;

    ctxt = &vif->siwifi_hw->chanctx_table[vif->ch_index];

    if (ctxt->count == 0) {
        WARN(1, "Chan ctxt ref count is already 0");
    } else {
        ctxt->count--;
    }

    if (ctxt->count == 0) {
        if (vif->ch_index == vif->siwifi_hw->cur_chanctx) {
            /* If current chan ctxt is no longer linked to a vif
               disable radar detection (no need to check if it was activated) */
            siwifi_radar_detection_enable(&vif->siwifi_hw->radar,
                                        SIWIFI_RADAR_DETECT_DISABLE,
                                        SIWIFI_RADAR_RIU);
        }
        /* set chan to null, so that if this ctxt is relinked to a vif that
           don't have channel information, don't use wrong information */
        ctxt->chan_def.chan = NULL;
    }
    vif->ch_index = SIWIFI_CH_NOT_SET;
}

int siwifi_chanctx_valid(struct siwifi_hw *siwifi_hw, u8 ch_idx)
{
    if (ch_idx >= NX_CHAN_CTXT_CNT ||
        siwifi_hw->chanctx_table[ch_idx].chan_def.chan == NULL) {
        return 0;
    }

    return 1;
}

static void siwifi_del_csa(struct siwifi_vif *vif)
{
    struct siwifi_hw *siwifi_hw = vif->siwifi_hw;
    struct siwifi_csa *csa = vif->ap.csa;

    if (!csa)
        return;

    siwifi_ipc_elem_var_deallocs(siwifi_hw, &csa->elem);
    siwifi_del_bcn(&csa->bcn);
    siwifi_kfree(csa);
    vif->ap.csa = NULL;
}

static void siwifi_csa_finish(struct work_struct *ws)
{
    struct siwifi_csa *csa = container_of(ws, struct siwifi_csa, work);
    struct siwifi_vif *vif = csa->vif;
    struct siwifi_hw *siwifi_hw = vif->siwifi_hw;
    int error = csa->status;

    if (!error)
        error = siwifi_send_bcn_change(siwifi_hw, vif->vif_index, csa->elem.dma_addr,
                                     csa->bcn.len, csa->bcn.head_len,
                                     csa->bcn.tim_len, NULL);

    if (error)
        cfg80211_stop_iface(siwifi_hw->wiphy, &vif->wdev, GFP_KERNEL);
    else {
        mutex_lock(&vif->wdev.mtx);
        __acquire(&vif->wdev.mtx);
        spin_lock_bh(&siwifi_hw->cb_lock);
        siwifi_chanctx_unlink(vif);
        siwifi_chanctx_link(vif, csa->ch_idx, &csa->chandef);
        if (siwifi_hw->cur_chanctx == csa->ch_idx) {
            siwifi_radar_detection_enable_on_cur_channel(siwifi_hw);
            siwifi_txq_vif_start(vif, SIWIFI_TXQ_STOP_CHAN, siwifi_hw);
        } else
            siwifi_txq_vif_stop(vif, SIWIFI_TXQ_STOP_CHAN, siwifi_hw);
        spin_unlock_bh(&siwifi_hw->cb_lock);
        cfg80211_ch_switch_notify(vif->ndev, &csa->chandef);
        mutex_unlock(&vif->wdev.mtx);
        __release(&vif->wdev.mtx);
    }
    vif->ap.channel_switching = false;
    siwifi_del_csa(vif);
}

/**
 * siwifi_external_auth_enable - Enable external authentication on a vif
 *
 * @vif: VIF on which external authentication must be enabled
 *
 * External authentication requires to start TXQ for unknown STA in
 * order to send auth frame pusehd by user space.
 * Note: It is assumed that fw is on the correct channel.
 */
void siwifi_external_auth_enable(struct siwifi_vif *vif)
{
    vif->sta.external_auth = true;
    spin_lock_bh(&vif->siwifi_hw->tx_lock);
    siwifi_txq_unk_vif_init(vif);
    siwifi_txq_start(siwifi_txq_vif_get(vif, NX_UNK_TXQ_TYPE), 0);
    spin_unlock_bh(&vif->siwifi_hw->tx_lock);
}

/**
 * siwifi_external_auth_disable - Disable external authentication on a vif
 *
 * @vif: VIF on which external authentication must be disabled
 */
void siwifi_external_auth_disable(struct siwifi_vif *vif)
{
    if (!vif->sta.external_auth)
        return;

    vif->sta.external_auth = false;
    spin_lock_bh(&vif->siwifi_hw->tx_lock);
    siwifi_txq_unk_vif_deinit(vif);
    spin_unlock_bh(&vif->siwifi_hw->tx_lock);
}

/*********************************************************************
 * netdev callbacks
 ********************************************************************/
/**
 * int (*ndo_open)(struct net_device *dev);
 *     This function is called when network device transistions to the up
 *     state.
 *
 * - Start FW if this is the first interface opened
 * - Add interface at fw level
 */
static int siwifi_open(struct net_device *dev)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_hw *siwifi_hw = siwifi_vif->siwifi_hw;
    struct mm_add_if_cfm add_if_cfm;
    int error = 0;

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

    // Check if it is the first opened VIF
    if (siwifi_hw->vif_started == 0)
    {
        // Start the FW
       if ((error = siwifi_send_start(siwifi_hw)))
           return error;

       /* Device is now started */
       set_bit(SIWIFI_DEV_STARTED, &siwifi_hw->drv_flags);
    }

    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_AP_VLAN) {
        /* For AP_vlan use same fw and drv indexes. We ensure that this index
           will not be used by fw for another vif by taking index >= NX_VIRT_DEV_MAX */
        add_if_cfm.inst_nbr = siwifi_vif->drv_vif_index;
        netif_tx_stop_all_queues(dev);
    } else {
        /* Forward the information to the LMAC,
         *     p2p value not used in FMAC configuration, iftype is sufficient */
        memset(&add_if_cfm, 0, sizeof(struct mm_add_if_cfm));
        if ((error = siwifi_send_add_if(siwifi_hw, dev->dev_addr,
                                      SIWIFI_VIF_TYPE(siwifi_vif), false, siwifi_vif->use_4addr, &add_if_cfm)))
            return error;

        if (add_if_cfm.status != 0) {
            SIWIFI_PRINT_CFM_ERR(add_if);
            return -EIO;
        }
    }

	siwifi_send_set_power_lvl(siwifi_hw, (u8)siwifi_hw->mod_params->txpower_lvl);
    /* Fix the problem that web page relay cannot take effect after private fields are set */
    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_STATION) {
        siwifi_send_assoc_req_insert_info(siwifi_hw);
    }
    /* Save the index retrieved from LMAC */
    spin_lock_bh(&siwifi_hw->cb_lock);
    siwifi_vif->vif_index = add_if_cfm.inst_nbr;
    /* Restore drv_vif_index, make sure its same as lmac vif index, this idx is used for recovery */
    siwifi_vif->drv_vif_index = add_if_cfm.inst_nbr;
    siwifi_vif->up = true;
    set_repeater_status(siwifi_vif, REPEATER_VIF_UP);
    siwifi_hw->vif_started++;
#if DEBUG_ARRAY_CHECK
    BUG_ON(add_if_cfm.inst_nbr >= NX_VIRT_DEV_MAX + NX_REMOTE_STA_MAX);
#endif
    siwifi_hw->vif_table[add_if_cfm.inst_nbr] = siwifi_vif;
    spin_unlock_bh(&siwifi_hw->cb_lock);


    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MONITOR){
        siwifi_hw->monitor_vif = siwifi_vif->vif_index;
        if (siwifi_vif->ch_index != SIWIFI_CH_NOT_SET){
#if DEBUG_ARRAY_CHECK
            BUG_ON(siwifi_vif->ch_index >= NX_CHAN_CTXT_CNT);
#endif
            //Configure the monitor channel
            error = siwifi_send_config_monitor_req(siwifi_hw, &siwifi_hw->chanctx_table[siwifi_vif->ch_index].chan_def, NULL);
        }
    }

    netif_carrier_off(dev);

    return error;
}

/**
 * int (*ndo_stop)(struct net_device *dev);
 *     This function is called when network device transistions to the down
 *     state.
 *
 * - Remove interface at fw level
 * - Reset FW if this is the last interface opened
 */
static int siwifi_close(struct net_device *dev)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_hw *siwifi_hw = siwifi_vif->siwifi_hw;

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

    netdev_info(dev, "CLOSE");

    spin_lock_bh(&siwifi_hw->cb_lock);
    siwifi_radar_cancel_cac(&siwifi_hw->radar);

    /* Abort scan request on the vif */
    if (siwifi_hw->scan_request &&
        siwifi_hw->scan_request->wdev == &siwifi_vif->wdev) {
        struct cfg80211_scan_info info = {
            .aborted = true,
        };

        cfg80211_scan_done(siwifi_hw->scan_request, &info);
        siwifi_hw->scan_request = NULL;
    }

    spin_unlock_bh(&siwifi_hw->cb_lock);
    siwifi_send_remove_if(siwifi_hw, siwifi_vif->vif_index);

    /* Ensure that we won't process disconnect ind */
    spin_lock_bh(&siwifi_hw->cb_lock);

    if (siwifi_hw->roc_elem && (siwifi_hw->roc_elem->wdev == &siwifi_vif->wdev)) {
        printk(KERN_CRIT "%s clear roc\n", __func__);
        /* Initialize RoC element pointer to NULL, indicate that RoC can be started */
        siwifi_hw->roc_elem = NULL;
    }

    siwifi_vif->up = false;
    if (netif_carrier_ok(dev)) {
        if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_STATION ||
            SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_P2P_CLIENT) {
            cfg80211_disconnected(dev, WLAN_REASON_DEAUTH_LEAVING,
                                  NULL, 0, true, GFP_ATOMIC);
            netif_tx_stop_all_queues(dev);
            netif_carrier_off(dev);
        } else if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_AP_VLAN) {
            netif_carrier_off(dev);
        } else {
            netdev_warn(dev, "AP not stopped when disabling interface");
        }
    }

#if DEBUG_ARRAY_CHECK
    BUG_ON(siwifi_vif->vif_index >= NX_VIRT_DEV_MAX + NX_REMOTE_STA_MAX);
#endif

    siwifi_hw->vif_table[siwifi_vif->vif_index] = NULL;
    siwifi_hw->vif_started--;
    spin_unlock_bh(&siwifi_hw->cb_lock);

    if (siwifi_hw->vif_started == 0) {
        /* This also lets both ipc sides remain in sync before resetting */
        siwifi_ipc_tx_drain(siwifi_hw);

        siwifi_send_reset(siwifi_hw);

        // Set parameters to firmware
        siwifi_send_me_config_req(siwifi_hw);

        // Set channel parameters to firmware
        siwifi_send_me_chan_config_req(siwifi_hw);

        clear_bit(SIWIFI_DEV_STARTED, &siwifi_hw->drv_flags);

    }

	/* hnat deinit*/
    return 0;
}

/**
 * struct net_device_stats* (*ndo_get_stats)(struct net_device *dev);
 *	Called when a user wants to get the network device usage
 *	statistics. Drivers must do one of the following:
 *	1. Define @ndo_get_stats64 to fill in a zero-initialised
 *	   rtnl_link_stats64 structure passed by the caller.
 *	2. Define @ndo_get_stats to update a net_device_stats structure
 *	   (which should normally be dev->stats) and return a pointer to
 *	   it. The structure may be changed asynchronously only if each
 *	   field is written atomically.
 *	3. Update dev->stats asynchronously and atomically, and define
 *	   neither operation.
 */
static struct net_device_stats *siwifi_get_stats(struct net_device *dev)
{
    struct siwifi_vif *vif = netdev_priv(dev);

    return &vif->net_stats;
}

/**
 * u16 (*ndo_select_queue)(struct net_device *dev, struct sk_buff *skb,
 *                         struct net_device *sb_dev,
 *                         select_queue_fallback_t fallback);
 *	Called to decide which queue to when device supports multiple
 *	transmit queues.
 */
u16 siwifi_select_queue(struct net_device *dev, struct sk_buff *skb,
                      struct net_device *sb_dev,
                      select_queue_fallback_t fallback)
{
    u16 txq_num;
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_hw *siwifi_hw = siwifi_vif->siwifi_hw;

    spin_lock_bh(&siwifi_hw->cb_lock);
    txq_num = siwifi_select_txq(siwifi_vif, skb);
    spin_unlock_bh(&siwifi_hw->cb_lock);
    return txq_num;
}

/**
 * int (*ndo_set_mac_address)(struct net_device *dev, void *addr);
 *	This function  is called when the Media Access Control address
 *	needs to be changed. If this interface is not defined, the
 *	mac address can not be changed.
 */
static int siwifi_set_mac_address(struct net_device *dev, void *addr)
{
    struct sockaddr *sa = addr;
    int ret;

    ret = eth_mac_addr(dev, sa);

    return ret;
}

#ifdef CONFIG_SF16A18_WIFI_ATE_TOOLS
static int siwifi_do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    //if some driver do not implement the ioctl function,
    //do nothing
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_hw *siwifi_hw = siwifi_vif->siwifi_hw;
    int ret = -1;
    if (siwifi_vif == NULL){
        printk(" no vif\n");
		return -1;
	}
    if (siwifi_hw == NULL){
        printk("no rwnw\n");
		return -1;
	}

    ret = siwifi_ops_ioctl(siwifi_hw, ifr, cmd);
    return ret;
}
#endif

#if defined(CONFIG_SF19A28_FULLMASK) && IS_ENABLED(CONFIG_SFAX8_HNAT_DRIVER) && IS_ENABLED(CONFIG_NF_FLOW_TABLE)
int siwifi_ndo_flow_offload_check(struct flow_offload_hw_path *path) {
	struct net_device * pndev = path->dev;
	struct siwifi_vif *siwifi_vif = netdev_priv(pndev);

	// eth_dest mac 00:00:00:00:00:00 is invalid
	if (!siwifi_vif->phnat_priv || (*(int *)path->eth_dest == 0) || (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_STATION))
		return -1;
	return 0;
}
int siwifi_ndo_flow_offload(enum flow_offload_type type,struct flow_offload *flow,struct flow_offload_hw_path *src,struct flow_offload_hw_path *dest) {
	struct siwifi_vif *siwifi_vif = netdev_priv(src->dev);
	struct sf_hnat_priv * phnat_priv = (struct sf_hnat_priv *)siwifi_vif->phnat_priv;
	return phnat_priv->ndo_flow_offload(siwifi_vif->hnat_pdev, type, flow, src, dest);
}
#endif

static const struct net_device_ops siwifi_netdev_ops = {
	.ndo_open               = siwifi_open,
	.ndo_stop               = siwifi_close,
	.ndo_start_xmit         = siwifi_start_xmit,
	.ndo_get_stats          = siwifi_get_stats,
	.ndo_select_queue       = siwifi_select_queue,
	.ndo_set_mac_address    = siwifi_set_mac_address,
	//    .ndo_set_features       = siwifi_set_features,
	//    .ndo_set_rx_mode        = siwifi_set_multicast_list,
#ifdef CONFIG_SF16A18_WIFI_ATE_TOOLS
	.ndo_do_ioctl           = siwifi_do_ioctl,
#endif
#if defined(CONFIG_SF19A28_FULLMASK) && IS_ENABLED(CONFIG_SFAX8_HNAT_DRIVER) && IS_ENABLED(CONFIG_NF_FLOW_TABLE)
    .ndo_flow_offload_check = siwifi_ndo_flow_offload_check,
	.ndo_flow_offload = siwifi_ndo_flow_offload,
#endif
};

static const struct net_device_ops siwifi_netdev_monitor_ops = {
	.ndo_open               = siwifi_open,
	.ndo_stop               = siwifi_close,
	.ndo_get_stats          = siwifi_get_stats,
	.ndo_set_mac_address    = siwifi_set_mac_address,
};

static void siwifi_netdev_setup(struct net_device *dev)
{
	ether_setup(dev);

	dev->priv_flags &= ~IFF_TX_SKB_SHARING;
	dev->netdev_ops = &siwifi_netdev_ops;
#if MY_LINUX_VERSION_CODE <  KERNEL_VERSION(4, 12, 0)
	dev->priv_destructor = free_netdev;
#else
	dev->needs_free_netdev = true;
#endif
	dev->watchdog_timeo = SIWIFI_TX_LIFETIME_MS;
    dev->needed_headroom = max(SIWIFI_TX_HEADROOM, SIWIFI_TX_AMSDU_HEADROOM);
	dev->hw_features = 0;
}


/*********************************************************************
 * Cfg80211 callbacks (and helper)
 *********************************************************************/
static struct wireless_dev *siwifi_interface_add(struct siwifi_hw *siwifi_hw,
			const char *name,
			unsigned char name_assign_type,
			enum nl80211_iftype type,
			struct vif_params *params)
{
	struct net_device *ndev;
	struct siwifi_vif *vif;
	int min_idx, max_idx;
	int vif_idx = -1;
	int i;

#if defined(CONFIG_SF19A28_FULLMASK) && IS_ENABLED(CONFIG_SFAX8_HNAT_DRIVER) && IS_ENABLED(CONFIG_NF_FLOW_TABLE)
	struct device * hnat_dev = NULL;
	struct platform_device * hnat_pdev = NULL;
	struct sf_hnat_priv * phnat_priv = NULL;
#endif

	// Look for an available VIF
	if (type == NL80211_IFTYPE_AP_VLAN) {
		min_idx = NX_VIRT_DEV_MAX;
        max_idx = NX_VIRT_DEV_MAX + NX_REMOTE_STA_MAX;
    } else {
        min_idx = 0;
        max_idx = NX_VIRT_DEV_MAX;
    }

    for (i = min_idx; i < max_idx; i++) {
        if ((siwifi_hw->avail_idx_map) & BIT(i)) {
            vif_idx = i;
            break;
        }
    }
    if (vif_idx < 0)
        return NULL;

    #ifndef CONFIG_SIWIFI_MON_DATA
    list_for_each_entry(vif, &siwifi_hw->vifs, list) {
        // Check if monitor interface already exists or type is monitor
        if ((SIWIFI_VIF_TYPE(vif) == NL80211_IFTYPE_MONITOR) ||
           (type == NL80211_IFTYPE_MONITOR)) {
            wiphy_err(siwifi_hw->wiphy,
                    "Monitor+Data interface support (MON_DATA) disabled\n");
            return NULL;
        }
    }
    #endif

    ndev = alloc_netdev_mqs(sizeof(*vif), name, name_assign_type,
                            siwifi_netdev_setup, siwifi_hw->tx_queue_num, 1);
    if (!ndev)
        return NULL;

    vif = netdev_priv(ndev);

    // Lite memory control for each vif
    vif->lm_ctl = (struct siwifi_lite_mem_ctrl *)siwifi_kzalloc(
                siwifi_hw->tx_queue_num * sizeof(struct siwifi_lite_mem_ctrl),
                GFP_KERNEL);
    if (!vif->lm_ctl) {
        printk("oom, can not alocate siwifi_lite_mem_ctrl\n");
        goto err;
    }
    /*
     *Total memory limit is 2MB = 2 * 1024 * 1024, if we are on the same queue
     */
    for (i = 0; i < siwifi_hw->tx_queue_num; i++)  {
#ifdef CONFIG_WIFI_LITE_MEMORY
        /* In the past, 1.3MB memory cache size(1375000).
         * To make tx rate and rx rate are as equal as possible,
         * now adjust tx_memory_limit = 1800000, almost 1.71MB.
         */
        vif->lm_ctl[i].tx_memory_limit = 1800000;
        // 4096 skbs number
        vif->lm_ctl[i].tx_cnt_limit = 1024 * 4;
#else
        // 6MB memory cache size
        vif->lm_ctl[i].tx_memory_limit = 6 * 1024 * 1024;
        // 4096 skbs number
        vif->lm_ctl[i].tx_cnt_limit = 1024 * 4;
#endif
    }

	ndev->ieee80211_ptr = &vif->wdev;
	vif->wdev.wiphy = siwifi_hw->wiphy;
	vif->siwifi_hw = siwifi_hw;
	vif->ndev = ndev;
	vif->drv_vif_index = vif_idx;
	SET_NETDEV_DEV(ndev, wiphy_dev(vif->wdev.wiphy));
	vif->wdev.netdev = ndev;
	vif->wdev.iftype = type;
	vif->up = false;
	vif->ch_index = SIWIFI_CH_NOT_SET;
	memset(&vif->net_stats, 0, sizeof(vif->net_stats));

	switch (type) {
	case NL80211_IFTYPE_STATION:
    case NL80211_IFTYPE_P2P_CLIENT:
        vif->sta.ap = NULL;
        vif->sta.tdls_sta = NULL;
        vif->sta.external_auth = false;
        break;
    case NL80211_IFTYPE_MESH_POINT:
        INIT_LIST_HEAD(&vif->ap.mpath_list);
        INIT_LIST_HEAD(&vif->ap.proxy_list);
        vif->ap.create_path = false;
        vif->ap.generation = 0;
        // no break
    case NL80211_IFTYPE_AP:
    case NL80211_IFTYPE_P2P_GO:
        INIT_LIST_HEAD(&vif->ap.sta_list);
#ifdef LOCAL_RECOVERY
        INIT_LIST_HEAD(&vif->ap.local_sta_list);
#endif
        memset(&vif->ap.bcn, 0, sizeof(vif->ap.bcn));
        vif->ap.channel_switching = false;
        break;
    case NL80211_IFTYPE_AP_VLAN:
    {
        struct siwifi_vif *master_vif;
        bool found = false;
        list_for_each_entry(master_vif, &siwifi_hw->vifs, list) {
            if ((SIWIFI_VIF_TYPE(master_vif) == NL80211_IFTYPE_AP) &&
                (!memcmp(master_vif->ndev->dev_addr, params->macaddr,
                           ETH_ALEN))) {
                 found=true;
                 break;
            }
        }

        if (!found)
            goto err;

         vif->ap_vlan.master = master_vif;
         vif->ap_vlan.sta_4a = NULL;
         break;
    }
    case NL80211_IFTYPE_MONITOR:
        ndev->type = ARPHRD_IEEE80211_RADIOTAP;
        ndev->netdev_ops = &siwifi_netdev_monitor_ops;
        break;
    default:
        break;
    }

    if (type == NL80211_IFTYPE_AP_VLAN)
        memcpy(ndev->dev_addr, params->macaddr, ETH_ALEN);
    else {
        memcpy(ndev->dev_addr, siwifi_hw->wiphy->perm_addr, ETH_ALEN);
        ndev->dev_addr[5] ^= vif_idx;
    }

    if (params) {
        vif->use_4addr = params->use_4addr;
        ndev->ieee80211_ptr->use_4addr = params->use_4addr;
    } else
        vif->use_4addr = false;

    if (register_netdevice(ndev))
        goto err;

#if defined(CONFIG_SF19A28_FULLMASK) && IS_ENABLED(CONFIG_SFAX8_HNAT_DRIVER) && IS_ENABLED(CONFIG_NF_FLOW_TABLE)
	hnat_dev = bus_find_device_by_name(&platform_bus_type, NULL, "sf_hnat.0.auto");
	if(hnat_dev){
		hnat_pdev = to_platform_device(hnat_dev);
		vif->phnat_priv = platform_get_drvdata(hnat_pdev);
		vif->hnat_pdev = hnat_pdev;
		phnat_priv = (struct sf_hnat_priv *)vif->phnat_priv;
		printk("found hnat device to add\n");
		phnat_priv->add_wifi_ndev(hnat_pdev, ndev);
	}else {
		printk("not found hnat device\n");
	}
#endif

    spin_lock_bh(&siwifi_hw->cb_lock);
    list_add_tail(&vif->list, &siwifi_hw->vifs);
    siwifi_hw->avail_idx_map &= ~BIT(vif_idx);
    spin_unlock_bh(&siwifi_hw->cb_lock);
    // when re-add interface, that means all information are not valid
    siwifi_hw->dying_gasp_valid = false;

    if (type == NL80211_IFTYPE_STATION) {
        spin_lock_init(&vif->src_filter_lock);
        setup_timer(&(vif->src_filter_timer), src_filter_aging, (unsigned long)vif);
        mod_timer(&(vif->src_filter_timer), jiffies + SRC_FILTER_AGING_TIME * HZ);
    }


    return &vif->wdev;

err:
    if (vif->lm_ctl)
        siwifi_kfree(vif->lm_ctl);
    free_netdev(ndev);
    return NULL;
}


/*
 * @brief Retrieve the siwifi_sta object allocated for a given MAC address
 * and a given role.
 */
static struct siwifi_sta *siwifi_retrieve_sta(struct siwifi_hw *siwifi_hw,
                                          struct siwifi_vif *siwifi_vif, u8 *addr,
                                          __le16 fc, bool ap)
{
    if (ap) {
        /* only deauth, disassoc and action are bufferable MMPDUs */
        bool bufferable = ieee80211_is_deauth(fc) ||
                          ieee80211_is_disassoc(fc) ||
                          ieee80211_is_action(fc);

        /* Check if the packet is bufferable or not */
        if (bufferable)
        {
            /* Check if address is a broadcast or a multicast address */
            if (is_broadcast_ether_addr(addr) || is_multicast_ether_addr(addr)) {
                /* Returned STA pointer */
                struct siwifi_sta *siwifi_sta = &siwifi_hw->sta_table[siwifi_vif->ap.bcmc_index];
#if DEBUG_ARRAY_CHECK
                BUG_ON(siwifi_vif->ap.bcmc_index >= NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX);
#endif

                if (siwifi_sta->valid)
                    return siwifi_sta;
            } else {
                /* Returned STA pointer */
                struct siwifi_sta *siwifi_sta;

                /* Go through list of STAs linked with the provided VIF */
                list_for_each_entry(siwifi_sta, &siwifi_vif->ap.sta_list, list) {
                    if (siwifi_sta->valid &&
                        ether_addr_equal(siwifi_sta->mac_addr, addr)) {
                        /* Return the found STA */
                        return siwifi_sta;
                    }
                }
            }
        }
    } else {
        return siwifi_vif->sta.ap;
    }

    return NULL;
}

/**
 * @add_virtual_intf: create a new virtual interface with the given name,
 *	must set the struct wireless_dev's iftype. Beware: You must create
 *	the new netdev in the wiphy's network namespace! Returns the struct
 *	wireless_dev, or an ERR_PTR. For P2P device wdevs, the driver must
 *	also set the address member in the wdev.
 */
static struct wireless_dev *siwifi_cfg80211_add_iface(struct wiphy *wiphy,
                                                    const char *name,
                                                    unsigned char name_assign_type,
                                                    enum nl80211_iftype type,
                                                    struct vif_params *params)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct wireless_dev *wdev;
#if (MY_LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0) && !defined CPTCFG_WIRELESS)
    unsigned char name_assign_type = NET_NAME_UNKNOWN;
#endif
    wdev = siwifi_interface_add(siwifi_hw, name, name_assign_type, type, params);

    if (!wdev)
        return ERR_PTR(-EINVAL);

#ifdef CONFIG_SIWIFI_REPEATER
    if (NL80211_IFTYPE_STATION == type) {
        struct siwifi_vif *vif = container_of(wdev, struct siwifi_vif, wdev);
        if ((params) && (params->use_4addr))
            return wdev;
        repeater_register(&vif->rp_info, vif->ndev, 64);
    }
#endif

    return wdev;
}

/**
 * @del_virtual_intf: remove the virtual interface
 */
static int siwifi_cfg80211_del_iface(struct wiphy *wiphy, struct wireless_dev *wdev)
{
    struct net_device *dev = wdev->netdev;
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);

#if defined(CONFIG_SF19A28_FULLMASK) && IS_ENABLED(CONFIG_SFAX8_HNAT_DRIVER) && IS_ENABLED(CONFIG_NF_FLOW_TABLE)
	struct sf_hnat_priv * phnat_priv = (struct sf_hnat_priv *)siwifi_vif->phnat_priv;
	struct device * hnat_dev = NULL;
	struct platform_device * hnat_pdev = NULL;
#endif
    netdev_info(dev, "Remove Interface");

    // We can not free any resources before unregister netdevice,
    // because unregister netdevice will call stop_ap / close
    // If any resources will be used in those(stop_ap/close function),
    // crash will happen!

    if (dev->reg_state == NETREG_REGISTERED) {
#if defined(CONFIG_SF19A28_FULLMASK) && IS_ENABLED(CONFIG_SFAX8_HNAT_DRIVER) && IS_ENABLED(CONFIG_NF_FLOW_TABLE)
		if(phnat_priv){
			hnat_dev = bus_find_device_by_name(&platform_bus_type, NULL, "sf_hnat.0.auto");
			if(hnat_dev){
				hnat_pdev = to_platform_device(hnat_dev);
				phnat_priv = platform_get_drvdata(hnat_pdev);
				printk("found hnat device to del\n");
				phnat_priv->del_wifi_ndev(hnat_pdev, dev);
			}
		}
#endif
		/* Will call siwifi_close if interface is UP */
		unregister_netdevice(dev);
	}

    spin_lock_bh(&siwifi_hw->tx_lock);
    if (siwifi_vif->lm_ctl)
        siwifi_kfree(siwifi_vif->lm_ctl);
    siwifi_vif->lm_ctl = NULL;

#ifdef CONFIG_BRIDGE_ACCELERATE
    siwifi_clear_hook(dev);
#endif
    spin_unlock_bh(&siwifi_hw->tx_lock);

	spin_lock_bh(&siwifi_hw->cb_lock);
	list_del(&siwifi_vif->list);
    siwifi_hw->avail_idx_map |= BIT(siwifi_vif->drv_vif_index);
	spin_unlock_bh(&siwifi_hw->cb_lock);

#ifdef CONFIG_WIFI_RX_THREAD
    // flush all rx elements belong to this virtual interface
    siwifi_rx_thread_flush(siwifi_hw, siwifi_vif);
#endif

	if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MONITOR) {
		siwifi_chanctx_unlink(siwifi_vif);
		siwifi_hw->monitor_vif = SIWIFI_INVALID_VIF;
	}

    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_STATION) {
	    siwifi_src_filter_hash_free(siwifi_vif);
        del_timer_sync(&(siwifi_vif->src_filter_timer));
    }

	siwifi_vif->ndev = NULL;

    /* Clear the priv in adapter */
    dev->ieee80211_ptr = NULL;
    netdev_info(dev, "Remove Interface Over");

    return 0;
}

/**
 * @change_virtual_intf: change type/configuration of virtual interface,
 *	keep the struct wireless_dev's iftype updated.
 */
static int siwifi_cfg80211_change_iface(struct wiphy *wiphy,
                                      struct net_device *dev,
                                      enum nl80211_iftype type,
                                      struct vif_params *params)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *vif = netdev_priv(dev);

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

    // If the STA is already a four address STA, there is no need to change it.
    if ((type == NL80211_IFTYPE_STATION) && (vif->use_4addr) && (params->use_4addr == 1) && (vif->up))
        return 0;

    if (vif->up)
        return (-EBUSY);

    #ifndef CONFIG_SIWIFI_MON_DATA
    if ((type == NL80211_IFTYPE_MONITOR) &&
       (SIWIFI_VIF_TYPE(vif) != NL80211_IFTYPE_MONITOR)) {
        struct siwifi_vif *vif_el;
        list_for_each_entry(vif_el, &siwifi_hw->vifs, list) {
            // Check if data interface already exists
            if ((vif_el != vif) &&
               (SIWIFI_VIF_TYPE(vif) != NL80211_IFTYPE_MONITOR)) {
                wiphy_err(siwifi_hw->wiphy,
                        "Monitor+Data interface support (MON_DATA) disabled\n");
                return -EIO;
            }
        }
    }
    #endif

    switch (type) {
    case NL80211_IFTYPE_STATION:
    case NL80211_IFTYPE_P2P_CLIENT:
        vif->sta.ap = NULL;
        vif->sta.tdls_sta = NULL;
        vif->sta.external_auth = false;
        break;
    case NL80211_IFTYPE_MESH_POINT:
        INIT_LIST_HEAD(&vif->ap.mpath_list);
        INIT_LIST_HEAD(&vif->ap.proxy_list);
        vif->ap.create_path = false;
        vif->ap.generation = 0;
        // no break
    case NL80211_IFTYPE_AP:
    case NL80211_IFTYPE_P2P_GO:
        INIT_LIST_HEAD(&vif->ap.sta_list);
#ifdef LOCAL_RECOVERY
        INIT_LIST_HEAD(&vif->ap.local_sta_list);
#endif
        memset(&vif->ap.bcn, 0, sizeof(vif->ap.bcn));
        break;
    case NL80211_IFTYPE_AP_VLAN:
        return -EPERM;
    case NL80211_IFTYPE_MONITOR:
        dev->type = ARPHRD_IEEE80211_RADIOTAP;
        dev->netdev_ops = &siwifi_netdev_monitor_ops;
        break;
    default:
        break;
    }

#ifdef CONFIG_SIWIFI_REPEATER
    if (NL80211_IFTYPE_STATION == vif->wdev.iftype && NL80211_IFTYPE_STATION != type) {
        repeater_unregister(vif->rp_info);
        vif->rp_info = NULL;
    }
#endif

    if((vif->wdev.iftype == NL80211_IFTYPE_MONITOR)
            && (type != NL80211_IFTYPE_MONITOR)){
        siwifi_hw->monitor_vif = SIWIFI_INVALID_VIF;
        dev->type = ARPHRD_ETHER;
        dev->netdev_ops = &siwifi_netdev_ops;
    }

    vif->wdev.iftype = type;
    if (params->use_4addr != -1)
        vif->use_4addr = params->use_4addr;

    return 0;
}

/**
 * @scan: Request to do a scan. If returning zero, the scan request is given
 *	the driver, and will be valid until passed to cfg80211_scan_done().
 *	For scan results, call cfg80211_inform_bss(); you can call this outside
 *	the scan/scan_done bracket too.
 */
static int siwifi_cfg80211_scan(struct wiphy *wiphy,
                              struct cfg80211_scan_request *request)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = container_of(request->wdev, struct siwifi_vif,
                                             wdev);
    int error;
#ifdef CONFIG_SIWIFI_SORT_SCAN
    int i;
#endif

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

#ifdef CONFIG_SIWIFI_SORT_SCAN
    /*unlink the bss last scanned*/
    if (!siwifi_hw->scaning) {
        for (i = 0; i < siwifi_hw->scan_num; i++) {
            if (siwifi_hw->bss_results[i]) {
                cfg80211_unlink_bss(siwifi_hw->wiphy, siwifi_hw->bss_results[i]);
                cfg80211_put_bss(siwifi_hw->wiphy, siwifi_hw->bss_results[i]);
            }
        }
        siwifi_hw->scan_num = 0;
        memset(siwifi_hw->scan_results, 0, sizeof(siwifi_hw->scan_results));
        memset(siwifi_hw->bss_results, 0, sizeof(siwifi_hw->bss_results));
    }
#endif

    if ((error = siwifi_send_scanu_req(siwifi_hw, siwifi_vif, request)))
        return error;

    siwifi_hw->scan_request = request;
    siwifi_hw->scaning = true;
    init_completion(&siwifi_hw->scan_complete);

#ifdef CONFIG_VDR_HW
    vendor_hook_scan_req();
#endif
    return 0;
}

/*
 * @update sta key_params for local recovery
 */
struct key_params *rebuild_sta_key_params(struct key_params *params)
{
    struct key_params *key_params;
    if (!params)
        return NULL;

    key_params = siwifi_kmalloc(sizeof(struct key_params) + params->key_len + params->seq_len, GFP_KERNEL);
    if (!key_params)
        return NULL;

    memcpy((u8 *)key_params, (u8 *)params, sizeof(struct key_params));
    if (params->key_len > 0) {
        key_params->key = (u8 *)(((u8 *)key_params) + sizeof(struct key_params));
        memcpy((u8 *)key_params->key, (u8 *)params->key, params->key_len);
    } else {
        key_params->key = NULL;
    }
    if (params->seq_len > 0) {
        key_params->seq = (((u8 *)key_params->key) + params->key_len);
        memcpy((u8 *)key_params->seq, (u8 *)params->seq, params->seq_len);
    } else {
        key_params->seq = NULL;
    }
    return key_params;
}

/**
 * @add_key: add a key with the given parameters. @mac_addr will be %NULL
 *	when adding a group key.
 */
static int siwifi_cfg80211_add_key(struct wiphy *wiphy, struct net_device *netdev,
                                 u8 key_index, bool pairwise, const u8 *mac_addr,
                                 struct key_params *params)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *vif = netdev_priv(netdev);
    int i, error = 0;
    struct mm_key_add_cfm key_add_cfm;
    u8_l cipher = 0;
    struct siwifi_sta *sta = NULL;
    struct siwifi_key *siwifi_key;
#ifdef LOCAL_RECOVERY
    struct siwifi_key_recovery_info *rec_key;
#endif

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

    if (!params)
        return -EINVAL;

    if (mac_addr) {
        sta = siwifi_get_sta(siwifi_hw, mac_addr);
        if (!sta)
            return -EINVAL;
        siwifi_key = &sta->key;
        netdev_info(netdev, "Add key for sta %d (%pM)", sta->sta_idx, sta->mac_addr);
    }
    else {
        if (key_index >= MAX_VIF_KEY_NUM) {
            printk("siwifi_cfg80211_add_key, invalid key index, %d\n", key_index);
            return -EINVAL;
        }
        siwifi_key = &vif->key[key_index];
        netdev_info(netdev, "Add key for vif(%d), key index : %d\n", vif->vif_index, key_index);
    }

    /* Retrieve the cipher suite selector */
    switch (params->cipher) {
    case WLAN_CIPHER_SUITE_WEP40:
        cipher = MAC_RSNIE_CIPHER_WEP40;
        break;
    case WLAN_CIPHER_SUITE_WEP104:
        cipher = MAC_RSNIE_CIPHER_WEP104;
        break;
    case WLAN_CIPHER_SUITE_TKIP:
        cipher = MAC_RSNIE_CIPHER_TKIP;
        break;
    case WLAN_CIPHER_SUITE_CCMP:
        cipher = MAC_RSNIE_CIPHER_CCMP;
        break;
    case WLAN_CIPHER_SUITE_AES_CMAC:
        cipher = MAC_RSNIE_CIPHER_AES_CMAC;
        break;
    case WLAN_CIPHER_SUITE_SMS4:
    {
        // Need to reverse key order
        u8 tmp, *key = (u8 *)params->key;
        cipher = MAC_RSNIE_CIPHER_SMS4;
        for (i = 0; i < WPI_SUBKEY_LEN/2; i++) {
            tmp = key[i];
            key[i] = key[WPI_SUBKEY_LEN - 1 - i];
            key[WPI_SUBKEY_LEN - 1 - i] = tmp;
        }
        for (i = 0; i < WPI_SUBKEY_LEN/2; i++) {
            tmp = key[i + WPI_SUBKEY_LEN];
            key[i + WPI_SUBKEY_LEN] = key[WPI_KEY_LEN - 1 - i];
            key[WPI_KEY_LEN - 1 - i] = tmp;
        }
        break;
    }
    default:
        return -EINVAL;
    }

    if ((error = siwifi_send_key_add(siwifi_hw, vif->vif_index,
                                   (sta ? sta->sta_idx : 0xFF), pairwise,
                                   (u8 *)params->key, params->key_len,
                                   key_index, cipher, &key_add_cfm)))
        return error;

    if (key_add_cfm.status != 0) {
        SIWIFI_PRINT_CFM_ERR(key_add);
        return -EIO;
    }

    /* Save the index retrieved from LMAC */
    siwifi_key->hw_idx = key_add_cfm.hw_key_idx;
    siwifi_key->valid = 1;

    set_repeater_status(vif, REPEATER_EAPOL);
#ifdef LOCAL_RECOVERY
    if (mac_addr) {
        rec_key = &sta->rec_info.rec_key;
    } else {
        rec_key = &vif->rec_key[key_index];
    }

    // Free previous point if exist
    if (rec_key->params != params) {
        if (rec_key->params) {
            siwifi_kfree(rec_key->params);
            rec_key->params = NULL;
        }
        // rebuild the new key parameters
        rec_key->params = rebuild_sta_key_params(params);
    }
    rec_key->pairwise = pairwise;
    rec_key->key_index = key_index;
    rec_key->valid = true;
#endif
    return 0;
}

/**
 * @get_key: get information about the key with the given parameters.
 *	@mac_addr will be %NULL when requesting information for a group
 *	key. All pointers given to the @callback function need not be valid
 *	after it returns. This function should return an error if it is
 *	not possible to retrieve the key, -ENOENT if it doesn't exist.
 *
 */
static int siwifi_cfg80211_get_key(struct wiphy *wiphy, struct net_device *netdev,
                                 u8 key_index, bool pairwise, const u8 *mac_addr,
                                 void *cookie,
                                 void (*callback)(void *cookie, struct key_params*))
{
    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

    return -1;
}


/**
 * @del_key: remove a key given the @mac_addr (%NULL for a group key)
 *	and @key_index, return -ENOENT if the key doesn't exist.
 */
static int siwifi_cfg80211_del_key(struct wiphy *wiphy, struct net_device *netdev,
                                 u8 key_index, bool pairwise, const u8 *mac_addr)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *vif = netdev_priv(netdev);
    struct siwifi_key *siwifi_key;
    struct siwifi_sta *sta = NULL;
    int error = 0;

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);
    if (mac_addr) {
        sta = siwifi_get_sta(siwifi_hw, mac_addr);
        if (!sta)
            return -EINVAL;
        siwifi_key = &sta->key;
#ifdef LOCAL_RECOVERY
        if (sta->rec_info.rec_key.params) {
            siwifi_kfree(sta->rec_info.rec_key.params);
            sta->rec_info.rec_key.params = NULL;
        }
        sta->rec_info.rec_key.valid = false;
#endif
        if (siwifi_key->valid)
            netdev_info(netdev, "Del key for sta %d (%pM)", sta->sta_idx, sta->mac_addr);
    }
    else {
        if (key_index >= MAX_VIF_KEY_NUM)
            return -EINVAL;
        siwifi_key = &vif->key[key_index];
#ifdef LOCAL_RECOVERY
        if (vif->rec_key[key_index].params) {
            siwifi_kfree(vif->rec_key[key_index].params);
            vif->rec_key[key_index].params = NULL;
        }
        vif->rec_key[key_index].valid = false;
#endif
        if (siwifi_key->valid)
            netdev_info(netdev, "Del key for vif(%d), key index : %d\n", vif->vif_index, key_index);
    }

    if (siwifi_key->valid) {
        error = siwifi_send_key_del(siwifi_hw, siwifi_key->hw_idx);
    }
    siwifi_key->valid = 0;

    return error;
}

/**
 * @set_default_key: set the default key on an interface
 */
static int siwifi_cfg80211_set_default_key(struct wiphy *wiphy,
                                         struct net_device *netdev,
                                         u8 key_index, bool unicast, bool multicast)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *vif = netdev_priv(netdev);
    struct mm_default_key_add_cfm default_key_add_cfm;
    int error;

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);
    if (key_index >= MAX_VIF_KEY_NUM)
        return -EINVAL;
    if ((error = siwifi_send_default_key_add(siwifi_hw, vif->vif_index, key_index, &default_key_add_cfm)))
        return error;

    if (default_key_add_cfm.status != 0) {
        SIWIFI_PRINT_CFM_ERR(default_key_add);
        return -EIO;
    }

    return 0;
}

/**
 * @set_default_mgmt_key: set the default management frame key on an interface
 */
static int siwifi_cfg80211_set_default_mgmt_key(struct wiphy *wiphy,
                                              struct net_device *netdev,
                                              u8 key_index)
{
    return 0;
}

/**
 * @connect: Connect to the ESS with the specified parameters. When connected,
 *	call cfg80211_connect_result() with status code %WLAN_STATUS_SUCCESS.
 *	If the connection fails for some reason, call cfg80211_connect_result()
 *	with the status from the AP.
 *	(invoked with the wireless_dev mutex held)
 */
static int siwifi_cfg80211_connect(struct wiphy *wiphy, struct net_device *dev,
                                 struct cfg80211_connect_params *sme)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct sm_connect_cfm sm_connect_cfm;
    int error = 0;

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

    /* For SHARED-KEY authentication, must install key first */
    if (sme->auth_type == NL80211_AUTHTYPE_SHARED_KEY && sme->key)
    {
        struct key_params key_params;
        key_params.key = sme->key;
        key_params.seq = NULL;
        key_params.key_len = sme->key_len;
        key_params.seq_len = 0;
        key_params.cipher = sme->crypto.cipher_group;
        siwifi_cfg80211_set_default_key(wiphy, dev, sme->key_idx, false, false);
        siwifi_cfg80211_add_key(wiphy, dev, sme->key_idx, false, NULL, &key_params);
    }
#if MY_LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
    else if ((sme->auth_type == NL80211_AUTHTYPE_SAE) &&
             !(sme->flags & CONNECT_REQ_EXTERNAL_AUTH_SUPPORT)) {
        netdev_err(dev, "Doesn't support SAE without external authentication\n");
        return -EINVAL;
    }
#endif

    /* Forward the information to the LMAC */
    if ((error = siwifi_send_sm_connect_req(siwifi_hw, siwifi_vif, sme, &sm_connect_cfm)))
        return error;

    // Check the status
    switch (sm_connect_cfm.status)
    {
        case CO_OK:
            error = 0;
            break;
        case CO_BUSY:
            error = -EINPROGRESS;
            break;
        case CO_OP_IN_PROGRESS:
            error = -EALREADY;
            break;
        default:
            error = -EIO;
            break;
    }

    return error;
}

/**
 * @disconnect: Disconnect from the BSS/ESS.
 *	(invoked with the wireless_dev mutex held)
 */
static int siwifi_cfg80211_disconnect(struct wiphy *wiphy, struct net_device *dev,
                                    u16 reason_code)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

    return(siwifi_send_sm_disconnect_req(siwifi_hw, siwifi_vif, reason_code));
}

#if MY_LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
/**
 * @external_auth: indicates result of offloaded authentication processing from
 *     user space
 */
static int siwifi_cfg80211_external_auth(struct wiphy *wiphy, struct net_device *dev,
                                       struct cfg80211_external_auth_params *params)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);

    if (!siwifi_vif->sta.external_auth)
        return -EINVAL;

    siwifi_external_auth_disable(siwifi_vif);
    return siwifi_send_sm_external_auth_required_rsp(siwifi_hw, siwifi_vif,
                                                   params->status);
}
#endif

/*
 *	update station parameters for local recovery
*/
struct station_parameters *siwifi_rebuild_sta_params(struct station_parameters *parameters)
{
    struct station_parameters *station_parameters;
    station_parameters = siwifi_kmalloc(sizeof(struct station_parameters) + parameters->supported_rates_len +
                                                 sizeof(struct ieee80211_ht_cap) + sizeof(struct ieee80211_vht_cap),
                    GFP_ATOMIC);
    if (!station_parameters)
        return NULL;

    memcpy((u8 *)station_parameters, (u8 *)parameters, sizeof(struct station_parameters));

    station_parameters->supported_rates = (u8 *)station_parameters + sizeof(struct station_parameters);

    station_parameters->ht_capa = (struct ieee80211_ht_cap *)((u8 *)station_parameters->supported_rates + parameters->supported_rates_len);

    station_parameters->vht_capa = (struct ieee80211_vht_cap *)((u8 *)station_parameters->ht_capa + sizeof(struct ieee80211_ht_cap));

    memcpy((u8 *)station_parameters->supported_rates, (u8 *)parameters->supported_rates, parameters->supported_rates_len);

    if (parameters->ht_capa)
        memcpy((u8 *)station_parameters->ht_capa, (u8 *)parameters->ht_capa, sizeof(struct ieee80211_ht_cap));
    else
        station_parameters->ht_capa = NULL;

    if (parameters->vht_capa)
        memcpy((u8 *)station_parameters->vht_capa, (u8 *)parameters->vht_capa, sizeof(struct ieee80211_vht_cap));
    else
        station_parameters->vht_capa = NULL;

    return station_parameters;
}

#ifdef CONFIG_SIWIFI_EASYMESH
/**
 * siwifi_notify_easymesh - Global callback function pointer for EasyMesh WiFi events.
 *
 * This variable holds the callback function pointer used for handling WiFi events
 * within the EasyMesh module. It is initialized to NULL by default and can be set
 * using the `siwifi_set_notify` function.
 */
siwifi_event_callback siwifi_notify_easymesh = NULL;

/**
 * siwifi_set_notify - Set the callback function for handling WiFi events within EasyMesh.
 *
 * @param ptr: Pointer to the callback function of type `siwifi_event_callback`.
 *
 * This function allows setting the callback function to handle WiFi events within the EasyMesh module.
 * The provided callback function (`ptr`) will be assigned to the global variable `siwifi_notify_easymesh`.
 */
void siwifi_set_notify(siwifi_event_callback ptr)
{
    siwifi_notify_easymesh = ptr;
}
EXPORT_SYMBOL(siwifi_set_notify);

/**
 * report_sf_notify_event - Report a WiFi notify event to the registered callback function.
 * @event_data: Pointer to the sf_notify_event_data structure representing the event to report.
 *
 * Reports a WiFi notify event by invoking the registered callback function with the event data.
 */
void report_sf_notify_event(const sf_notify_event_data *event_data)
{
    if (siwifi_notify_easymesh)
        siwifi_notify_easymesh(event_data);
}

/**
 * siwifi_easymesh_sta_change_hook - Hook function to handle STA (station) change events.
 *
 * @param mac: Pointer to the current MAC address of the station.
 * @param prev_mac: Pointer to the previous MAC address of the station.
 * @param change: Boolean indicating whether the station connection state has changed (true for connect, false for
 * disconnect).
 *
 * This function is used as a hook to handle STA change events within the siwifi EasyMesh module. It creates and
 * populates an instance of sf_notify_event_data representing the STA change event,then reports the event by invoking
 * report_sf_notify_event with the event data.
 */
static void siwifi_easymesh_sta_change_hook(const uint8_t *mac, uint8_t *prev_mac, bool change)
{
    /* Declare and initialize an instance of sf_notify_event_data. */
    sf_notify_event_data event_data;

    /* Populate the event data structure. */
    event_data.type = SF_NOTIFY_STA_CHANGE_EVENT;
    event_data.data.sta_change_event.sta_mac = mac;
    event_data.data.sta_change_event.prev_mac = prev_mac;
    event_data.data.sta_change_event.updown = change;

    /* Report the STA change event. */
    report_sf_notify_event(&event_data);
}

/**
 * siwifi_easymesh_sta_info_hook - Hook function to handle STA info events for EasyMesh.
 *
 * @param vif: Pointer to the virtual interface structure.
 * @param sta: Pointer to the station structure.
 */
static void siwifi_easymesh_sta_info_hook(struct siwifi_vif *vif, struct siwifi_sta *sta)
{
    /* Declare and initialize an instance of sf_notify_event_data. */
    sf_notify_event_data event_data;

    /* Populate the event data structure. */
    event_data.type = SF_NOTIFY_STA_INFO_EVENT;
    event_data.data.sta_info_event.sta_mac = sta->mac_addr;
    event_data.data.sta_info_event.prev_mac = vif->ndev->dev_addr;
    event_data.data.sta_info_event.rssi = sta->stats.last_rx.rx_vect1.rssi1;

    /* Report the STA change event. */
    report_sf_notify_event(&event_data);
}

/**
 * siwifi_notify_sta_info_timer_callback - Timer callback function to notify STA information periodically.
 *
 * @param timer Pointer to the timer_list structure.
 */
void siwifi_notify_sta_info_timer_callback(struct timer_list *timer)
{
    sf_sta_timer_info *info = container_of(timer, sf_sta_timer_info, timer);
    struct siwifi_hw *siwifi_hw = info->siwifi_hw;
    struct siwifi_vif *vif;
    struct siwifi_sta *sta;

    /* Iterate over all virtual interfaces. */
    list_for_each_entry (vif, &siwifi_hw->vifs, list) {
        /*  Continue if the VIF is not of type Access Point (AP) */
        if (SIWIFI_VIF_TYPE(vif) != NL80211_IFTYPE_AP)
            continue;

        /* Iterate over all stations associated with the AP VIF. */
        list_for_each_entry (sta, &vif->ap.sta_list, list) {
            /* Hook to handle STA info event for each STA. */
            siwifi_easymesh_sta_info_hook(vif, sta);
        }
    }

    /* Reschedule the timer for the next expiration. */
    mod_timer(&siwifi_hw->sta_timer_info.timer, jiffies + msecs_to_jiffies(info->timeout * 1000));
}
#endif /* CONFIG_SIWIFI_EASYMESH */

/**
 * @add_station: Add a new station.
 */
static int siwifi_cfg80211_add_station(struct wiphy *wiphy, struct net_device *dev,
                                     const u8 *mac, struct station_parameters *params)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct me_sta_add_cfm me_sta_add_cfm;
    struct siwifi_sta *sta = NULL;
    int error = 0;
    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);
    WARN_ON(SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_AP_VLAN);

    /* Do not add TDLS station */
    if (params->sta_flags_set & BIT(NL80211_STA_FLAG_TDLS_PEER))
        return 0;

    /* Indicate we are in a STA addition process - This will allow handling
     * potential PS mode change indications correctly
     */
    siwifi_hw->adding_sta = true;
    memset(&me_sta_add_cfm, 0, sizeof(struct me_sta_add_cfm));
    // If we are under recovering, then we use this hack to
    // make lmac use the same station index order
    if (test_bit(SIWIFI_DEV_STACK_RESTARTING, &siwifi_hw->drv_flags)) {
        me_sta_add_cfm.status = 1;
        me_sta_add_cfm.sta_idx = siwifi_hw->recovering_sta_idx;
    }

    /* Forward the information to the LMAC */
    if ((error = siwifi_send_me_sta_add(siwifi_hw, params, mac, siwifi_vif->vif_index,
                                      &me_sta_add_cfm)))
        return error;

    // Check the status
    switch (me_sta_add_cfm.status)
    {
        case CO_OK:
        {
            int tid;
            if (me_sta_add_cfm.sta_idx >= NX_REMOTE_STA_MAX)
            {
                error = -EINVAL;
                printk("invalid me_sta_add_cfm sta_idx %d\n", me_sta_add_cfm.sta_idx);
                break;
            }
            spin_lock_bh(&siwifi_hw->cb_lock);
            sta = &siwifi_hw->sta_table[me_sta_add_cfm.sta_idx];
            sta->aid = params->aid;

            sta->sta_idx = me_sta_add_cfm.sta_idx;
            sta->ch_idx = siwifi_vif->ch_index;
            sta->vif_idx = siwifi_vif->vif_index;
            sta->vlan_idx = sta->vif_idx;
            sta->qos = (params->sta_flags_set & BIT(NL80211_STA_FLAG_WME)) != 0;
            sta->ht = params->ht_capa ? 1 : 0;
            sta->vht = params->vht_capa ? 1 : 0;
			sta->stats.connected_time = ktime_get_seconds();
			sta->stats.count = 0;
			memset(sta->stats.data_rssi_old, 0 , sizeof(sta->stats.data_rssi_old));
			sta->stats.last_rx_bytes = 0;
			sta->stats.last_tx_bytes = 0;
			sta->stats.last_rx_tp = 0;
			sta->stats.last_tx_tp = 0;
			sta->stats.last_calc_tp = jiffies - HZ;
			sta->stats.last_th_tp = 0;
            sta->stats.last_tx_info = 0;

            sta->acm = 0;
            for (tid = 0; tid < NX_NB_TXQ_PER_STA; tid++) {
                int uapsd_bit = siwifi_hwq2uapsd[siwifi_tid2hwq[tid]];
                if (params->uapsd_queues & uapsd_bit)
                    sta->uapsd_tids |= 1 << tid;
                else
                    sta->uapsd_tids &= ~(1 << tid);
            }
            memcpy(sta->mac_addr, mac, ETH_ALEN);
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
            if (!test_bit(SIWIFI_DEV_STACK_RESTARTING, &siwifi_hw->drv_flags)) {
                siwifi_dbgfs_register_rc_stat(siwifi_hw, sta);
            }
#endif

            /* Ensure that we won't process PS change or channel switch ind*/
            spin_lock_bh(&siwifi_hw->tx_lock);
            siwifi_txq_sta_init(siwifi_hw, sta, siwifi_txq_vif_get_status(siwifi_vif));
            spin_unlock_bh(&siwifi_hw->tx_lock);
            list_add_tail(&sta->list, &siwifi_vif->ap.sta_list);
			/* Add sta to vif hash table */
			siwifi_sta_hash_add(siwifi_vif, sta);
            /* update sta params for local recovery */
#ifdef LOCAL_RECOVERY
            sta->rec_info.rec_key.valid = false;
            sta->rec_info.rec_key.params = NULL;
            sta->rec_info.params = siwifi_rebuild_sta_params(params);
            sta->rec_info.qos = sta->qos;
            sta->rec_info.cp_open_set = 0;
            sta->rec_info.control_port_open = 0;
            sta->rec_info.vlan = NULL;
            sta->rec_info.pstate_set = 0;
            sta->rec_info.plink_state = 0;
            sta->rec_info.sta_idx = sta->sta_idx;
            memcpy(sta->rec_info.mac_addr, mac, ETH_ALEN);
            siwifi_hw->recovering_sta = sta;
#endif
            sta->valid = true;
            sta->update_time_count = 0;
            if (sta->ps.active ||
                    me_sta_add_cfm.pm_state == MM_PS_MODE_ON ||
                    me_sta_add_cfm.pm_state == MM_PS_MODE_ON_DYN)
                siwifi_ps_bh_enable(siwifi_hw, sta, MM_PS_MODE_ON);
            else
                siwifi_ps_bh_enable(siwifi_hw, sta, MM_PS_MODE_OFF);
            spin_unlock_bh(&siwifi_hw->cb_lock);

            error = 0;

#ifdef CONFIG_SIWIFI_BFMER
            if (siwifi_hw->mod_params->bfmer)
                siwifi_send_bfmer_enable(siwifi_hw, sta, params->vht_capa);

            siwifi_mu_group_sta_init(sta, params->vht_capa);
#endif /* CONFIG_SIWIFI_BFMER */

            #define PRINT_STA_FLAG(f)                               \
                (params->sta_flags_set & BIT(NL80211_STA_FLAG_##f) ? "["#f"]" : "")

            netdev_info(dev, "Add sta %d (%pM) flags=%s%s%s%s%s%s%s",
                        sta->sta_idx, mac,
                        PRINT_STA_FLAG(AUTHORIZED),
                        PRINT_STA_FLAG(SHORT_PREAMBLE),
                        PRINT_STA_FLAG(WME),
                        PRINT_STA_FLAG(MFP),
                        PRINT_STA_FLAG(AUTHENTICATED),
                        PRINT_STA_FLAG(TDLS_PEER),
                        PRINT_STA_FLAG(ASSOCIATED));
            #undef PRINT_STA_FLAG
#ifdef CONFIG_VDR_HW
            vendor_hook_sta_added(sta, params);
#endif

#ifdef CONFIG_SIWIFI_EASYMESH
            siwifi_easymesh_sta_change_hook(mac, dev->dev_addr, true);
#endif /* CONFIG_SIWIFI_EASYMESH */

            break;
        }
        default:
            error = -EBUSY;
            break;
    }
    siwifi_hw->adding_sta = false;

    return error;
}

/**
 * @del_station: Remove a station
 */
static int siwifi_cfg80211_del_station_compat(struct wiphy *wiphy,
                                            struct net_device *dev,
                                            struct station_del_parameters *params)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_sta *cur, *tmp;
    int error = 0, found = 0;

#if MY_LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
    const u8 *mac = NULL;
    if (params)
        mac = params->mac;
#endif
    list_for_each_entry_safe(cur, tmp, &siwifi_vif->ap.sta_list, list) {
        if ((!mac) || (!memcmp(cur->mac_addr, mac, ETH_ALEN))) {
            netdev_info(dev, "Del sta %d (%pM)", cur->sta_idx, cur->mac_addr);
#ifdef CONFIG_SIWIFI_IGMP
            siwifi_del_mcgmember(siwifi_hw, cur->mac_addr);
#endif
            /* Ensure that we won't process PS change ind */
            spin_lock_bh(&siwifi_hw->cb_lock);
            spin_lock_bh(&siwifi_hw->tx_lock);
            cur->ps.active = false;
            cur->valid = false;

            if (cur->vif_idx != cur->vlan_idx) {
                struct siwifi_vif *vlan_vif;
#if DEBUG_ARRAY_CHECK
                BUG_ON(cur->vlan_idx >= NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX);
#endif
                vlan_vif = siwifi_hw->vif_table[cur->vlan_idx];
                if (vlan_vif->up) {
                    if ((SIWIFI_VIF_TYPE(vlan_vif) == NL80211_IFTYPE_AP_VLAN) &&
                        (vlan_vif->use_4addr)) {
                        vlan_vif->ap_vlan.sta_4a = NULL;
                    } else {
                        WARN(1, "Deleting sta belonging to VLAN other than AP_VLAN 4A");
                    }
                }
            }
            siwifi_txq_sta_deinit(siwifi_hw, cur);
            spin_unlock_bh(&siwifi_hw->tx_lock);
            spin_unlock_bh(&siwifi_hw->cb_lock);

            error = siwifi_send_me_sta_del(siwifi_hw, cur->sta_idx, false);
            //if in dead state del sta from list however
            if (!test_bit(SIWIFI_DEV_HW_DEAD, &siwifi_hw->drv_flags))
                if ((error != 0) && (error != -EPIPE))
                    return error;

#ifdef CONFIG_SIWIFI_BFMER
            // Disable Beamformer if supported
            siwifi_bfmer_report_del(siwifi_hw, cur);
            siwifi_mu_group_sta_del(siwifi_hw, cur);
#endif /* CONFIG_SIWIFI_BFMER */

            //lock vif->ap.sta_list to avoid list access here
            spin_lock_bh(&siwifi_hw->cb_lock);
#ifdef LOCAL_RECOVERY
            if (cur->rec_info.params) {
                siwifi_kfree(cur->rec_info.params);
                cur->rec_info.params = NULL;
            }
            if (cur->rec_info.rec_key.params) {
                siwifi_kfree(cur->rec_info.rec_key.params);
                cur->rec_info.rec_key.params = NULL;
            }
            cur->rec_info.rec_key.valid = false;
#endif
            list_del(&cur->list);
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
            siwifi_dbgfs_unregister_rc_stat(siwifi_hw, cur);
#endif
            spin_lock_bh(&siwifi_hw->tx_lock);
			siwifi_sta_hash_del(siwifi_vif, cur);
            spin_unlock_bh(&siwifi_hw->tx_lock);
            spin_unlock_bh(&siwifi_hw->cb_lock);

#ifdef CONFIG_SIWIFI_EASYMESH
            siwifi_easymesh_sta_change_hook(cur->mac_addr, dev->dev_addr, false);
#endif /* CONFIG_SIWIFI_EASYMESH */

            found ++;
            break;
        }
    }

    if (!found)
        return -ENOENT;
    else
        return 0;
}

/**
 * @change_station: Modify a given station. Note that flags changes are not much
 *	validated in cfg80211, in particular the auth/assoc/authorized flags
 *	might come to the driver in invalid combinations -- make sure to check
 *	them, also against the existing state! Drivers must call
 *	cfg80211_check_station_change() to validate the information.
 */
static int siwifi_cfg80211_change_station(struct wiphy *wiphy, struct net_device *dev,
                                        const u8 *mac, struct station_parameters *params)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *vif = netdev_priv(dev);
    struct siwifi_sta *sta;

    sta = siwifi_get_sta(siwifi_hw, mac);
    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);
    if (!sta)
    {
        /* Add the TDLS station */
        if (params->sta_flags_set & BIT(NL80211_STA_FLAG_TDLS_PEER))
        {
            struct siwifi_vif *siwifi_vif = netdev_priv(dev);
            struct me_sta_add_cfm me_sta_add_cfm;
            int error = 0;

            /* Indicate we are in a STA addition process - This will allow handling
             * potential PS mode change indications correctly
             */
            siwifi_hw->adding_sta = true;

            /* Forward the information to the LMAC */
            if ((error = siwifi_send_me_sta_add(siwifi_hw, params, mac, siwifi_vif->vif_index,
                                              &me_sta_add_cfm)))
                return error;

            // Check the status
            switch (me_sta_add_cfm.status)
            {
                case CO_OK:
                {
                    int tid;
                    spin_lock_bh(&siwifi_hw->cb_lock);
#if DEBUG_ARRAY_CHECK
                    BUG_ON(me_sta_add_cfm.sta_idx >= NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX);
#endif
                    sta = &siwifi_hw->sta_table[me_sta_add_cfm.sta_idx];
                    sta->aid = params->aid;
                    sta->sta_idx = me_sta_add_cfm.sta_idx;
                    sta->ch_idx = siwifi_vif->ch_index;
                    sta->vif_idx = siwifi_vif->vif_index;
                    sta->vlan_idx = sta->vif_idx;
                    sta->qos = (params->sta_flags_set & BIT(NL80211_STA_FLAG_WME)) != 0;
                    sta->ht = params->ht_capa ? 1 : 0;
                    sta->vht = params->vht_capa ? 1 : 0;
                    sta->acm = 0;
                    for (tid = 0; tid < NX_NB_TXQ_PER_STA; tid++) {
                        int uapsd_bit = siwifi_hwq2uapsd[siwifi_tid2hwq[tid]];
                        if (params->uapsd_queues & uapsd_bit)
                            sta->uapsd_tids |= 1 << tid;
                        else
                            sta->uapsd_tids &= ~(1 << tid);
                    }
                    memcpy(sta->mac_addr, mac, ETH_ALEN);
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
                    siwifi_dbgfs_register_rc_stat(siwifi_hw, sta);
#endif
                    /* Ensure that we won't process PS change or channel switch ind*/
                    spin_lock_bh(&siwifi_hw->tx_lock);
                    siwifi_txq_sta_init(siwifi_hw, sta, siwifi_txq_vif_get_status(siwifi_vif));
                    if (siwifi_vif->tdls_status == TDLS_SETUP_RSP_TX) {
                        siwifi_vif->tdls_status = TDLS_LINK_ACTIVE;
                        sta->tdls.initiator = true;
                        sta->tdls.active = true;
                    }
                    /* Set TDLS channel switch capability */
                    if ((params->ext_capab[3] & WLAN_EXT_CAPA4_TDLS_CHAN_SWITCH) &&
                        !siwifi_vif->tdls_chsw_prohibited)
                        sta->tdls.chsw_allowed = true;
                    siwifi_vif->sta.tdls_sta = sta;
                    sta->valid = true;
                    spin_unlock_bh(&siwifi_hw->tx_lock);
                    spin_unlock_bh(&siwifi_hw->cb_lock);
#ifdef CONFIG_SIWIFI_BFMER
                    if (siwifi_hw->mod_params->bfmer)
                        siwifi_send_bfmer_enable(siwifi_hw, sta, params->vht_capa);

                    siwifi_mu_group_sta_init(sta, NULL);
#endif /* CONFIG_SIWIFI_BFMER */

                    #define PRINT_STA_FLAG(f)                               \
                        (params->sta_flags_set & BIT(NL80211_STA_FLAG_##f) ? "["#f"]" : "")

                    netdev_info(dev, "Add %s TDLS sta %d (%pM) flags=%s%s%s%s%s%s%s",
                                sta->tdls.initiator ? "initiator" : "responder",
                                sta->sta_idx, mac,
                                PRINT_STA_FLAG(AUTHORIZED),
                                PRINT_STA_FLAG(SHORT_PREAMBLE),
                                PRINT_STA_FLAG(WME),
                                PRINT_STA_FLAG(MFP),
                                PRINT_STA_FLAG(AUTHENTICATED),
                                PRINT_STA_FLAG(TDLS_PEER),
                                PRINT_STA_FLAG(ASSOCIATED));
                    #undef PRINT_STA_FLAG

                    break;
                }
                default:
                {
                    siwifi_hw->adding_sta = false;
                    return -EBUSY;
                }
            }
            siwifi_hw->adding_sta = false;
        } else  {
            return -EINVAL;
        }
    }

    if (params->sta_flags_mask & BIT(NL80211_STA_FLAG_AUTHORIZED)) {
        siwifi_send_me_set_control_port_req(siwifi_hw,
                (params->sta_flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED)) != 0,
                sta->sta_idx);
#ifdef LOCAL_RECOVERY
        sta->rec_info.cp_open_set = 1;
        sta->rec_info.control_port_open = ((params->sta_flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED)) != 0);
#endif
    }

    if (SIWIFI_VIF_TYPE(vif) == NL80211_IFTYPE_MESH_POINT) {
        if (params->sta_modify_mask & STATION_PARAM_APPLY_PLINK_STATE) {
            if (params->plink_state < NUM_NL80211_PLINK_STATES) {
                siwifi_send_mesh_peer_update_ntf(siwifi_hw, vif, sta->sta_idx, params->plink_state);
#ifdef LOCAL_RECOVERY
                sta->rec_info.pstate_set = 1;
                sta->rec_info.plink_state = params->plink_state;
#endif
            }
        }
    }

    if (params->vlan) {
        uint8_t vlan_idx;

        spin_lock_bh(&siwifi_hw->cb_lock);
        vif = netdev_priv(params->vlan);
        vlan_idx = vif->vif_index;
        if (sta->vlan_idx != vlan_idx) {
            struct siwifi_vif *old_vif;

#if DEBUG_ARRAY_CHECK
            BUG_ON(sta->vlan_idx >= NX_VIRT_DEV_MAX + NX_REMOTE_STA_MAX);
#endif
            old_vif = siwifi_hw->vif_table[sta->vlan_idx];
            siwifi_txq_sta_switch_vif(sta, old_vif, vif);
            sta->vlan_idx = vlan_idx;

            if ((SIWIFI_VIF_TYPE(vif) == NL80211_IFTYPE_AP_VLAN) &&
                (vif->use_4addr)) {
                WARN((vif->ap_vlan.sta_4a),
                     "4A AP_VLAN interface with more than one sta");
                vif->ap_vlan.sta_4a = sta;
            }

            if ((old_vif) && (SIWIFI_VIF_TYPE(old_vif) == NL80211_IFTYPE_AP_VLAN) &&
                (old_vif->use_4addr)) {
                old_vif->ap_vlan.sta_4a = NULL;
            }
        }
#ifdef LOCAL_RECOVERY
        sta->rec_info.vlan = (void *)params->vlan;
#endif
        spin_unlock_bh(&siwifi_hw->cb_lock);
    }

    return 0;
}


/*
 * @update vif setting params for local recovery
*/
struct cfg80211_ap_settings *siwifi_rebuild_ap_settings(struct cfg80211_ap_settings *settings)
{
    struct cfg80211_ap_settings *siwifi_ap_settings;
    if (!settings)
        return NULL;

    siwifi_ap_settings = siwifi_kmalloc(sizeof(struct cfg80211_ap_settings) + sizeof(struct ieee80211_channel) +
                                               settings->beacon.head_len + settings->beacon.tail_len + settings->beacon.beacon_ies_len +
                                               settings->beacon.proberesp_ies_len + settings->beacon.assocresp_ies_len + settings->beacon.probe_resp_len +
                                               settings->ssid_len + sizeof(struct cfg80211_acl_data), GFP_KERNEL);
    if (!siwifi_ap_settings)
        return NULL;

    memcpy((u8 *)siwifi_ap_settings, (u8 *)settings, sizeof(struct cfg80211_ap_settings));
    siwifi_ap_settings->chandef.chan = (struct ieee80211_channel *)(((u8 *)siwifi_ap_settings) + sizeof(*siwifi_ap_settings));
    siwifi_ap_settings->beacon.head = ((u8 *)siwifi_ap_settings->chandef.chan) + sizeof(struct ieee80211_channel);
    siwifi_ap_settings->beacon.tail = siwifi_ap_settings->beacon.head + settings->beacon.head_len;
    siwifi_ap_settings->beacon.beacon_ies = siwifi_ap_settings->beacon.tail + settings->beacon.tail_len;
    siwifi_ap_settings->beacon.proberesp_ies = siwifi_ap_settings->beacon.beacon_ies + settings->beacon.beacon_ies_len;
    siwifi_ap_settings->beacon.assocresp_ies = siwifi_ap_settings->beacon.proberesp_ies + settings->beacon.proberesp_ies_len;
    siwifi_ap_settings->beacon.probe_resp = siwifi_ap_settings->beacon.assocresp_ies + settings->beacon.assocresp_ies_len;
    siwifi_ap_settings->ssid = siwifi_ap_settings->beacon.probe_resp + settings->beacon.probe_resp_len;
    siwifi_ap_settings->acl = (struct cfg80211_acl_data *)(((u8 *)siwifi_ap_settings->ssid) + settings->ssid_len);
    memcpy((u8 *)siwifi_ap_settings->chandef.chan, (u8 *)settings->chandef.chan, sizeof(struct ieee80211_channel));
    memcpy((u8 *)siwifi_ap_settings->beacon.head, (u8 *)settings->beacon.head, settings->beacon.head_len);
    memcpy((u8 *)siwifi_ap_settings->beacon.tail, (u8 *)settings->beacon.tail, settings->beacon.tail_len);
    memcpy((u8 *)siwifi_ap_settings->beacon.beacon_ies, (u8 *)settings->beacon.beacon_ies, settings->beacon.beacon_ies_len);
    memcpy((u8 *)siwifi_ap_settings->beacon.proberesp_ies, (u8 *)settings->beacon.proberesp_ies, settings->beacon.proberesp_ies_len);
    memcpy((u8 *)siwifi_ap_settings->beacon.assocresp_ies, (u8 *)settings->beacon.assocresp_ies, settings->beacon.assocresp_ies_len);
    memcpy((u8 *)siwifi_ap_settings->beacon.probe_resp, (u8 *)settings->beacon.probe_resp, settings->beacon.probe_resp_len);
    memcpy((u8 *)siwifi_ap_settings->ssid, (u8 *)settings->ssid, settings->ssid_len);
    if (settings->acl)
        memcpy((u8 *)siwifi_ap_settings->acl, (u8 *)settings->acl, sizeof(struct cfg80211_acl_data));

    return siwifi_ap_settings;
}

/**
 * @start_ap: Start acting in AP mode defined by the parameters.
 */
static int siwifi_cfg80211_start_ap(struct wiphy *wiphy, struct net_device *dev,
                                  struct cfg80211_ap_settings *settings)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct apm_start_cfm apm_start_cfm;
    struct siwifi_ipc_elem_var elem;
    struct siwifi_sta *sta;
    int error = 0;

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

    memset(&elem, 0, sizeof(struct siwifi_ipc_elem_var));
    /* Forward the information to the LMAC */
    if ((error = siwifi_send_apm_start_req(siwifi_hw, siwifi_vif, settings,
                                         &apm_start_cfm, &elem)))
        goto end;

    // Check the status
    switch (apm_start_cfm.status)
    {
        case CO_OK:
        {
            u8 txq_status = 0;
            /* copy cfg80211_ap_settings info used for local recovery*/
#ifdef LOCAL_RECOVERY
            if (siwifi_vif->ap_settings != settings)
                siwifi_vif->ap_settings = siwifi_rebuild_ap_settings(settings);
#endif

            spin_lock_bh(&siwifi_hw->cb_lock);
            siwifi_vif->ap.bcmc_index = apm_start_cfm.bcmc_idx;
            siwifi_vif->ap.flags = 0;
#if DEBUG_ARRAY_CHECK
            BUG_ON(apm_start_cfm.bcmc_idx >= NX_VIRT_DEV_MAX + NX_REMOTE_STA_MAX);
#endif
            sta = &siwifi_hw->sta_table[apm_start_cfm.bcmc_idx];
            sta->valid = true;
            sta->aid = 0;
            sta->sta_idx = apm_start_cfm.bcmc_idx;
            sta->ch_idx = apm_start_cfm.ch_idx;
            sta->vif_idx = siwifi_vif->vif_index;
            sta->qos = false;
            sta->acm = 0;
            sta->ps.active = false;
            siwifi_mu_group_sta_init(sta, NULL);
            siwifi_chanctx_link(siwifi_vif, apm_start_cfm.ch_idx,
                              &settings->chandef);
            if (siwifi_hw->cur_chanctx != apm_start_cfm.ch_idx) {
                txq_status = SIWIFI_TXQ_STOP_CHAN;
            }
            spin_lock_bh(&siwifi_hw->tx_lock);
            siwifi_txq_vif_init(siwifi_hw, siwifi_vif, txq_status);
            spin_unlock_bh(&siwifi_hw->tx_lock);
            spin_unlock_bh(&siwifi_hw->cb_lock);

            netif_tx_start_all_queues(dev);
            if (!test_bit(SIWIFI_DEV_STACK_RESTARTING, &siwifi_hw->drv_flags))
                netif_carrier_on(dev);
            error = 0;
            /* If the AP channel is already the active, we probably skip radar
               activation on MM_CHANNEL_SWITCH_IND (unless another vif use this
               ctxt). In anycase retest if radar detection must be activated
             */
            if (txq_status == 0) {
                spin_lock_bh(&siwifi_hw->cb_lock);
                siwifi_radar_detection_enable_on_cur_channel(siwifi_hw);
                spin_unlock_bh(&siwifi_hw->cb_lock);
            }
            break;
        }
        case CO_BUSY:
            error = -EINPROGRESS;
            break;
        case CO_OP_IN_PROGRESS:
            error = -EALREADY;
            break;
        default:
            error = -EIO;
            break;
    }

    if (error) {
        netdev_info(dev, "Failed to start AP (%d)", error);
    } else {
        netdev_info(dev, "AP started: ch=%d, bcmc_idx=%d",
                    siwifi_vif->ch_index, siwifi_vif->ap.bcmc_index);
    }

  end:
    siwifi_ipc_elem_var_deallocs(siwifi_hw, &elem);

    return error;
}


/**
 * @change_beacon: Change the beacon parameters for an access point mode
 *	interface. This should reject the call when AP mode wasn't started.
 */
static int siwifi_cfg80211_change_beacon(struct wiphy *wiphy, struct net_device *dev,
                                       struct cfg80211_beacon_data *info)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *vif = netdev_priv(dev);
    struct siwifi_bcn *bcn = &vif->ap.bcn;
    struct siwifi_ipc_elem_var elem;
    u8 *buf;
    u8 *real_addr;
    int error = 0;

    if(siwifi_is_channel_switching(siwifi_hw)) {
        return error;
    }

    // Build the beacon
    buf = siwifi_build_bcn(bcn, info, &real_addr);
    if (!buf)
        return -ENOMEM;

    // Sync buffer for FW
    if ((error = siwifi_ipc_elem_var_allocs(siwifi_hw, &elem, bcn->len, DMA_TO_DEVICE,
                                          buf, NULL, NULL, real_addr)))
        goto dealloc;

    // Forward the information to the LMAC
    error = siwifi_send_bcn_change(siwifi_hw, vif->vif_index, elem.dma_addr,
                                 bcn->len, bcn->head_len, bcn->tim_len, NULL);
dealloc:
    siwifi_ipc_elem_var_deallocs(siwifi_hw, &elem);

    return error;
}

/**
 * * @stop_ap: Stop being an AP, including stopping beaconing.
 */
static int siwifi_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *dev)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_sta *sta;
#ifdef LOCAL_RECOVERY
    int i = 0;
#endif
    spin_lock_bh(&siwifi_hw->cb_lock);
    siwifi_radar_cancel_cac(&siwifi_hw->radar);
    spin_unlock_bh(&siwifi_hw->cb_lock);
    siwifi_send_apm_stop_req(siwifi_hw, siwifi_vif);
    spin_lock_bh(&siwifi_hw->cb_lock);
    siwifi_chanctx_unlink(siwifi_vif);
    spin_unlock_bh(&siwifi_hw->cb_lock);

#ifdef LOCAL_RECOVERY
    if (siwifi_vif->ap_settings) {
        siwifi_kfree(siwifi_vif->ap_settings);
        siwifi_vif->ap_settings = NULL;
    }
    for (i = 0; i < MAX_VIF_KEY_NUM; i++) {
        if (siwifi_vif->rec_key[i].params) {
            siwifi_kfree(siwifi_vif->rec_key[i].params);
            siwifi_vif->rec_key[i].params = NULL;
        }
        siwifi_vif->rec_key[i].valid = false;
    }
#endif
    /* delete any remaining STA*/
    while (!list_empty(&siwifi_vif->ap.sta_list)) {
        siwifi_cfg80211_del_station(wiphy, dev, NULL);
    }

    /* delete BC/MC STA */
    sta = &siwifi_hw->sta_table[siwifi_vif->ap.bcmc_index];
    spin_lock_bh(&siwifi_hw->tx_lock);
    siwifi_txq_vif_deinit(siwifi_hw, siwifi_vif);
    spin_unlock_bh(&siwifi_hw->tx_lock);
    siwifi_del_bcn(&siwifi_vif->ap.bcn);
    siwifi_del_csa(siwifi_vif);

    netif_tx_stop_all_queues(dev);
    netif_carrier_off(dev);

    netdev_info(dev, "AP Stopped");

    return 0;
}

/**
 * @set_monitor_channel: Set the monitor mode channel for the device. If other
 *	interfaces are active this callback should reject the configuration.
 *	If no interfaces are active or the device is down, the channel should
 *	be stored for when a monitor interface becomes active.
 */
static int siwifi_cfg80211_set_monitor_channel(struct wiphy *wiphy,
                                             struct cfg80211_chan_def *chandef)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif;
    struct me_config_monitor_cfm cfm;
    u8 vif_started, ret = 0;
    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

    if (siwifi_hw->monitor_vif == SIWIFI_INVALID_VIF)
        return -EINVAL;

    siwifi_vif = siwifi_hw->vif_table[siwifi_hw->monitor_vif];

    // Check if channel context index is valid
    if (siwifi_chanctx_valid(siwifi_hw, siwifi_vif->ch_index)){
        struct siwifi_chanctx *ctxt;
        ctxt = &siwifi_vif->siwifi_hw->chanctx_table[siwifi_vif->ch_index];

        // Check if the monitor mode channel is already configured
        if (chandef && cfg80211_chandef_identical(&ctxt->chan_def, chandef))
            return 0;
    }

    vif_started = siwifi_hw->vif_started;

    // Forward the information to the FMAC
    ret = siwifi_send_config_monitor_req(siwifi_hw, chandef, &cfm);

    // Check if channel index is invalid
    if (cfm.chan_index == SIWIFI_CH_NOT_SET) {
        //Check if context index is valid
        if (siwifi_chanctx_valid(siwifi_hw, siwifi_vif->ch_index)){
            siwifi_chanctx_unlink(siwifi_vif);
        } else {
            //Reject if channel index is invalid
            return -EINVAL;
        }
    }

    if (!chandef) {
        chandef = &siwifi_hw->chanctx_table[cfm.chan_index].chan_def;
        if (chandef->chan == NULL) {
            struct ieee80211_channel *chan;

            //Set to default channel
            chan = &siwifi_2ghz_channels[5];
            cfg80211_chandef_create(chandef, chan, NL80211_CHAN_WIDTH_20);
        }
    }

    #ifdef CONFIG_SIWIFI_MON_DATA
    //Check if other interfaces are active
    if (vif_started > 1) {
        // Link existing channel context to monitor interface
        siwifi_chanctx_link(siwifi_vif, cfm.chan_index, chandef);

        // Reject if other interfaces are active
        return -EBUSY;
    }
    #endif

    // Check if channel context index is valid
    if (siwifi_chanctx_valid(siwifi_hw, siwifi_vif->ch_index)){
        struct siwifi_chanctx *ctxt;

        //Update channel definition
        ctxt = &siwifi_hw->chanctx_table[siwifi_vif->ch_index];
        ctxt->chan_def = *chandef;
    } else {
        siwifi_chanctx_link(siwifi_vif, cfm.chan_index, chandef);
    }

    return ret;
}

/**
  * just send null frame to associated client, then update client idle time if
  * receive an ack
  */
static int siwifi_cfg80211_probe_client(struct wiphy *wiphy, struct net_device *dev,
            const u8 *peer, u64 *cookie)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_sta *cur, *tmp;
    int error;
    if (!peer) {
        printk("mac addr should not be null when probe client\n");
        return -EINVAL;
    }
    printk("probe client to sta 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n", peer[0], peer[1], peer[2], peer[3], peer[4], peer[5]);
    list_for_each_entry_safe(cur, tmp, &siwifi_vif->ap.sta_list, list) {
        if (!memcmp(cur->mac_addr, peer, ETH_ALEN)) {
            error = siwifi_send_probe_client(siwifi_hw, cur->sta_idx);
            if (error)
                return error;
            break;
        }
    }
    return 0;
}

/**
 * @mgmt_frame_register: Notify driver that a management frame type was
 *	registered. Note that this callback may not sleep, and cannot run
 *	concurrently with itself.
 */
void siwifi_cfg80211_mgmt_frame_register(struct wiphy *wiphy,
                   struct wireless_dev *wdev,
                   u16 frame_type, bool reg)
{
}

/**
 * @set_wiphy_params: Notify that wiphy parameters have changed;
 *	@changed bitfield (see &enum wiphy_params_flags) describes which values
 *	have changed. The actual parameter values are available in
 *	struct wiphy. If returning an error, no value should be changed.
 */
static int siwifi_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
    return 0;
}


/**
 * @set_tx_power: set the transmit power according to the parameters,
 *	the power passed is in mBm, to get dBm use MBM_TO_DBM(). The
 *	wdev may be %NULL if power was set for the wiphy, and will
 *	always be %NULL unless the driver supports per-vif TX power
 *	(as advertised by the nl80211 feature flag.)
 */
static int siwifi_cfg80211_set_tx_power(struct wiphy *wiphy, struct wireless_dev *wdev,
                                      enum nl80211_tx_power_setting type, int mbm)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *vif;
    s8 pwr;
    int res = 0;
    struct mm_set_power_cfm cfm;

    if (type == NL80211_TX_POWER_AUTOMATIC) {
        pwr = 0x7f;
    } else {
        pwr = MBM_TO_DBM(mbm);
    }

    if (wdev) {
        vif = container_of(wdev, struct siwifi_vif, wdev);
        res = siwifi_send_set_power(siwifi_hw, vif->vif_index, pwr, &cfm);
        if (!res) {
            vif->txpower = cfm.power;
            vif->txpower_idx = cfm.radio_idx;
        }
    } else {
        list_for_each_entry(vif, &siwifi_hw->vifs, list) {
            res = siwifi_send_set_power(siwifi_hw, vif->vif_index, pwr, &cfm);
            if (res)
                break;
            vif->txpower = cfm.power;
            vif->txpower_idx = cfm.radio_idx;
        }
    }

    return res;
}

int siwifi_debug_set_tx_power(struct wiphy *wiphy, struct wireless_dev *wdev,
                enum nl80211_tx_power_setting type, int mbm)
{
    return siwifi_cfg80211_set_tx_power(wiphy,wdev,type,mbm);
}

/**
 * @get_tx_power: store the current TX power into the dbm variable;
 * return 0 if successful
 */
static int siwifi_cfg80211_get_tx_power(struct wiphy *wiphy, struct wireless_dev *wdev,
                                    int *dbm)
{
	struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
	struct siwifi_vif *vif;

	if (wdev) {
		vif = container_of(wdev, struct siwifi_vif, wdev);
	} else {
		list_for_each_entry(vif, &siwifi_hw->vifs, list);
	}
	*dbm = vif->txpower;

    return 0;

}


static int siwifi_cfg80211_set_txq_params(struct wiphy *wiphy, struct net_device *dev,
                                        struct ieee80211_txq_params *params)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    u8 hw_queue, aifs, cwmin, cwmax;
    u32 param;

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

#if DEBUG_ARRAY_CHECK
    BUG_ON(params->ac >= NL80211_NUM_ACS);
#endif
    hw_queue = siwifi_ac2hwq[0][params->ac];

    aifs  = params->aifs;
    cwmin = fls(params->cwmin);
    cwmax = fls(params->cwmax);

    /* Store queue information in general structure */
    param  = (u32) (aifs << 0);
    param |= (u32) (cwmin << 4);
    param |= (u32) (cwmax << 8);
    param |= (u32) (params->txop) << 12;

    /* Send the MM_SET_EDCA_REQ message to the FW */
    return siwifi_send_set_edca(siwifi_hw, hw_queue, param, false, siwifi_vif->vif_index);
}


/**
 * @remain_on_channel: Request the driver to remain awake on the specified
 *	channel for the specified duration to complete an off-channel
 *	operation (e.g., public action frame exchange). When the driver is
 *	ready on the requested channel, it must indicate this with an event
 *	notification by calling cfg80211_ready_on_channel().
 */
static int
siwifi_cfg80211_remain_on_channel(struct wiphy *wiphy, struct wireless_dev *wdev,
                                struct ieee80211_channel *chan,
                                unsigned int duration, u64 *cookie)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(wdev->netdev);
    struct siwifi_roc_elem *roc_elem;
    int error;

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

    /* For debug purpose (use ftrace kernel option) */
    trace_roc(siwifi_vif->vif_index, chan->center_freq, duration);

    /* Check that no other RoC procedure has been launched */
    if (siwifi_hw->roc_elem) {
        return -EBUSY;
    }

    /* Allocate a temporary RoC element */
    roc_elem = siwifi_kmalloc(sizeof(struct siwifi_roc_elem), GFP_KERNEL);

    /* Verify that element has well been allocated */
    if (!roc_elem) {
        return -ENOMEM;
    }

    /* Initialize the RoC information element */
    roc_elem->wdev = wdev;
    roc_elem->chan = chan;
    roc_elem->duration = duration;
    roc_elem->mgmt_roc = false;
    roc_elem->on_chan = false;

    /* Forward the information to the FMAC */
    siwifi_hw->roc_elem = roc_elem;
    error = siwifi_send_roc(siwifi_hw, siwifi_vif, chan, duration);

    spin_lock_bh(&siwifi_hw->cb_lock);
    /* If no error, keep all the information for handling of end of procedure */
    if (error == 0) {

        /* Set the cookie value */
        *cookie = (u64)(siwifi_hw->roc_cookie_cnt);
        spin_lock_bh(&siwifi_hw->tx_lock);
        // RM#14170 siwifi_rx_remain_on_channel_exp_ind may have happened
        if (siwifi_hw->roc_elem == NULL) {
            spin_unlock_bh(&siwifi_hw->tx_lock);
            spin_unlock_bh(&siwifi_hw->cb_lock);
            return -EPERM;
        }
        /* Initialize the OFFCHAN TX queue to allow off-channel transmissions */
        siwifi_txq_offchan_init(siwifi_vif);
        spin_unlock_bh(&siwifi_hw->tx_lock);
    } else {
        /* Free the allocated element */
        siwifi_hw->roc_elem = NULL;
        siwifi_kfree(roc_elem);
    }
    spin_unlock_bh(&siwifi_hw->cb_lock);

    return error;
}

/**
 * @cancel_remain_on_channel: Cancel an on-going remain-on-channel operation.
 *	This allows the operation to be terminated prior to timeout based on
 *	the duration value.
 */
static int siwifi_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
                                                  struct wireless_dev *wdev,
                                                  u64 cookie)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(wdev->netdev);

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

    /* For debug purpose (use ftrace kernel option) */
    trace_cancel_roc(siwifi_vif->vif_index);

    /* Check if a RoC procedure is pending */
    if (!siwifi_hw->roc_elem) {
        return 0;
    }

    /* Forward the information to the FMAC */
    return siwifi_send_cancel_roc(siwifi_hw);
}

/**
 * @dump_survey: get site survey information.
 */
static int siwifi_cfg80211_dump_survey(struct wiphy *wiphy, struct net_device *netdev,
                                     int idx, struct survey_info *info)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(netdev);
    struct ieee80211_supported_band *sband;
    struct siwifi_survey_info *siwifi_survey;

    if (idx >= ARRAY_SIZE(siwifi_hw->survey))
        return -ENOENT;

    spin_lock_bh(&siwifi_hw->cb_lock);
    siwifi_survey = &siwifi_hw->survey[idx];

    // Check if provided index matches with a supported 2.4GHz channel
    sband = wiphy->bands[NL80211_BAND_2GHZ];
    if (sband && idx >= sband->n_channels) {
        idx -= sband->n_channels;
        sband = NULL;
    }

    if (!sband) {
        // Check if provided index matches with a supported 5GHz channel
        sband = wiphy->bands[NL80211_BAND_5GHZ];

        if (!sband || idx >= sband->n_channels) {
            spin_unlock_bh(&siwifi_hw->cb_lock);
            return -ENOENT;
        }
    }

    // Fill the survey
    info->channel = &sband->channels[idx];
    info->filled = siwifi_survey->filled;

    // get current survey info
    spin_unlock_bh(&siwifi_hw->cb_lock);
    if (!siwifi_hw->scaning) {
        siwifi_get_cur_survey_info(wiphy, &(siwifi_vif->wdev), info->channel->center_freq, idx);
        siwifi_survey = &siwifi_hw->survey[idx];
        info->filled = siwifi_survey->filled;
    }
    spin_lock_bh(&siwifi_hw->cb_lock);

    if (siwifi_survey->filled != 0) {
        SURVEY_TIME(info) = (u64)siwifi_survey->chan_time_ms;
        SURVEY_TIME_BUSY(info) = (u64)siwifi_survey->chan_time_busy_ms;
        info->noise = siwifi_survey->noise_dbm;

        // Set the survey report as not used
        siwifi_survey->filled = 0;
    }
    spin_unlock_bh(&siwifi_hw->cb_lock);

    return 0;
}

/**
 * @get_channel: Get the current operating channel for the virtual interface.
 *	For monitor interfaces, it should return %NULL unless there's a single
 *	current monitoring channel.
 */
static int siwifi_cfg80211_get_channel(struct wiphy *wiphy,
                                     struct wireless_dev *wdev,
                                     struct cfg80211_chan_def *chandef) {
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = container_of(wdev, struct siwifi_vif, wdev);
    struct siwifi_chanctx *ctxt;

    spin_lock_bh(&siwifi_hw->cb_lock);
    if (!siwifi_vif->up) {
        spin_unlock_bh(&siwifi_hw->cb_lock);
        return -ENODATA;
    }

    if (siwifi_vif->vif_index == siwifi_hw->monitor_vif)
    {
        //retrieve channel from firmware
        siwifi_cfg80211_set_monitor_channel(wiphy, NULL);
    }

    //Check if channel context is valid
    if(!siwifi_chanctx_valid(siwifi_hw, siwifi_vif->ch_index)){
        spin_unlock_bh(&siwifi_hw->cb_lock);
        return -ENODATA;
    }

    ctxt = &siwifi_hw->chanctx_table[siwifi_vif->ch_index];
    *chandef = ctxt->chan_def;
    spin_unlock_bh(&siwifi_hw->cb_lock);

    return 0;
}

/**
 * @mgmt_tx: Transmit a management frame.
 */
static int siwifi_cfg80211_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
                                 struct cfg80211_mgmt_tx_params *params,
                                 u64 *cookie)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(wdev->netdev);
    struct siwifi_sta *siwifi_sta;
    struct ieee80211_mgmt *mgmt = (void *)params->buf;
    bool ap = false;
    bool offchan = false;
    int ret;

    spin_lock_bh(&siwifi_hw->cb_lock);
    /* Check if provided VIF is an AP or a STA one */
    switch (SIWIFI_VIF_TYPE(siwifi_vif)) {
        case NL80211_IFTYPE_AP_VLAN:
            siwifi_vif = siwifi_vif->ap_vlan.master;
        case NL80211_IFTYPE_AP:
        case NL80211_IFTYPE_P2P_GO:
        case NL80211_IFTYPE_MESH_POINT:
            ap = true;
            break;
        case NL80211_IFTYPE_STATION:
        case NL80211_IFTYPE_P2P_CLIENT:
        default:
            break;
    }

    /* Get STA on which management frame has to be sent */
    siwifi_sta = siwifi_retrieve_sta(siwifi_hw, siwifi_vif, mgmt->da,
                                 mgmt->frame_control, ap);

    trace_mgmt_tx((params->chan) ? params->chan->center_freq : 0,
                  siwifi_vif->vif_index, (siwifi_sta) ? siwifi_sta->sta_idx : 0xFF,
                  mgmt);

    if (ap || siwifi_sta) {
        goto send_frame;
    }

    /* Not an AP interface sending frame to unknown STA:
     * This is allowed for external authetication */
    if (siwifi_vif->sta.external_auth && ieee80211_is_auth(mgmt->frame_control)) {
        goto send_frame;
    }

    /* Otherwise ROC is needed */
    if (!params->chan) {
        spin_unlock_bh(&siwifi_hw->cb_lock);
        return -EINVAL;
    }

    /* Check that a RoC is already pending */
    if (siwifi_hw->roc_elem) {
        /* Get VIF used for current ROC */
        struct siwifi_vif *siwifi_roc_vif = netdev_priv(siwifi_hw->roc_elem->wdev->netdev);

        /* Check if RoC channel is the same than the required one */
        if ((siwifi_hw->roc_elem->chan->center_freq != params->chan->center_freq)
            || (siwifi_vif->vif_index != siwifi_roc_vif->vif_index)){
            spin_unlock_bh(&siwifi_hw->cb_lock);
            return -EINVAL;
		}

    } else {
        u64 cookie;
        int error;

        spin_unlock_bh(&siwifi_hw->cb_lock);
        /* Start a ROC procedure for 30ms */
        error = siwifi_cfg80211_remain_on_channel(wiphy, wdev, params->chan,
                                                30, &cookie);
        if (error) {
            return error;
        }

        /* Need to keep in mind that RoC has been launched internally in order to
         * avoid to call the cfg80211 callback once expired */
        spin_lock_bh(&siwifi_hw->cb_lock);
        if (siwifi_hw->roc_elem)
            siwifi_hw->roc_elem->mgmt_roc = true;
    }

    offchan = true;

send_frame:
    ret = siwifi_start_mgmt_xmit(siwifi_vif, siwifi_sta, params, offchan, cookie);
    spin_unlock_bh(&siwifi_hw->cb_lock);
    return ret;
}

/**
 * @start_radar_detection: Start radar detection in the driver.
 */
static
int siwifi_cfg80211_start_radar_detection(struct wiphy *wiphy,
                                        struct net_device *dev,
                                        struct cfg80211_chan_def *chandef,
                                        u32 cac_time_ms)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct apm_start_cac_cfm cfm;

    siwifi_radar_start_cac(&siwifi_hw->radar, cac_time_ms, siwifi_vif);
    siwifi_send_apm_start_cac_req(siwifi_hw, siwifi_vif, chandef, &cfm);

    if (cfm.status == CO_OK) {
        spin_lock_bh(&siwifi_hw->cb_lock);
        siwifi_chanctx_link(siwifi_vif, cfm.ch_idx, chandef);
        if (siwifi_hw->cur_chanctx == siwifi_vif->ch_index)
            siwifi_radar_detection_enable(&siwifi_hw->radar,
                                        SIWIFI_RADAR_DETECT_REPORT,
                                        SIWIFI_RADAR_RIU);
        spin_unlock_bh(&siwifi_hw->cb_lock);
    } else {
        return -EIO;
    }

    return 0;
}

/**
 * @update_ft_ies: Provide updated Fast BSS Transition information to the
 *	driver. If the SME is in the driver/firmware, this information can be
 *	used in building Authentication and Reassociation Request frames.
 */
static
int siwifi_cfg80211_update_ft_ies(struct wiphy *wiphy,
                            struct net_device *dev,
                            struct cfg80211_update_ft_ies_params *ftie)
{
    return 0;
}

/**
 * @set_cqm_rssi_config: Configure connection quality monitor RSSI threshold.
 */
static
int siwifi_cfg80211_set_cqm_rssi_config(struct wiphy *wiphy,
                                  struct net_device *dev,
                                  int32_t rssi_thold, uint32_t rssi_hyst)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);

    //For CQM, lmac only support station interface
    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_STATION)
        return siwifi_send_cfg_rssi_req(siwifi_hw, siwifi_vif->vif_index, rssi_thold, rssi_hyst);
    return 0;
}

/**
 *
 * @channel_switch: initiate channel-switch procedure (with CSA). Driver is
 *	responsible for veryfing if the switch is possible. Since this is
 *	inherently tricky driver may decide to disconnect an interface later
 *	with cfg80211_stop_iface(). This doesn't mean driver can accept
 *	everything. It should do it's best to verify requests and reject them
 *	as soon as possible.
 */
int siwifi_cfg80211_channel_switch(struct wiphy *wiphy,
                                 struct net_device *dev,
                                 struct cfg80211_csa_settings *params)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *vif = netdev_priv(dev);
    struct siwifi_ipc_elem_var elem;
    struct siwifi_bcn *bcn, *bcn_after;
    struct siwifi_csa *csa;
    u16 csa_oft[BCN_MAX_CSA_CPT];
    u8 *buf;
    u8 *real_addr;
    int i, error = 0;


    if (vif->ap.csa)
        return -EBUSY;

    if (params->n_counter_offsets_beacon > BCN_MAX_CSA_CPT)
        return -EINVAL;

    /* Build the new beacon with CSA IE */
    bcn = &vif->ap.bcn;
    buf = siwifi_build_bcn(bcn, &params->beacon_csa, &real_addr);
    if (!buf)
        return -ENOMEM;

    memset(csa_oft, 0, sizeof(csa_oft));
    for (i = 0; i < params->n_counter_offsets_beacon; i++)
    {
        csa_oft[i] = params->counter_offsets_beacon[i] + bcn->head_len +
            bcn->tim_len;
    }

    /* If count is set to 0 (i.e anytime after this beacon) force it to 2 */
    if (params->count == 0) {
        params->count = 2;
        for (i = 0; i < params->n_counter_offsets_beacon; i++)
        {
            buf[csa_oft[i]] = 2;
        }
    }

    if ((error = siwifi_ipc_elem_var_allocs(siwifi_hw, &elem, bcn->len,
                                          DMA_TO_DEVICE, buf, NULL, NULL, real_addr))) {
        goto end;
    }

    /* Build the beacon to use after CSA. It will only be sent to fw once
       CSA is over, but do it before sending the beacon as it must be ready
       when CSA is finished. */
    csa = siwifi_kzalloc(sizeof(struct siwifi_csa), GFP_KERNEL);
    if (!csa) {
        error = -ENOMEM;
        goto end;
    }

    bcn_after = &csa->bcn;
    buf = siwifi_build_bcn(bcn_after, &params->beacon_after, &real_addr);
    if (!buf) {
        error = -ENOMEM;
        siwifi_del_csa(vif);
        goto end;
    }

    if ((error = siwifi_ipc_elem_var_allocs(siwifi_hw, &csa->elem, bcn_after->len,
                                          DMA_TO_DEVICE, buf, NULL, NULL, real_addr))) {
        goto end;
    }

    vif->ap.csa = csa;
    csa->vif = vif;
    csa->chandef = params->chandef;

    if(SIWIFI_VIF_TYPE(vif) != NL80211_IFTYPE_AP)
        printk("**** err: vif is not ap\n");
    else
        vif->ap.channel_switching = true;
    /* Send new Beacon. FW will extract channel and count from the beacon */
    error = siwifi_send_bcn_change(siwifi_hw, vif->vif_index, elem.dma_addr,
                                 bcn->len, bcn->head_len, bcn->tim_len, csa_oft);

    if (error) {
        if(SIWIFI_VIF_TYPE(vif) == NL80211_IFTYPE_AP)
            vif->ap.channel_switching = false;
        siwifi_del_csa(vif);
        goto end;
    } else {
        INIT_WORK(&csa->work, siwifi_csa_finish);
        cfg80211_ch_switch_started_notify(dev, &csa->chandef, params->count);
    }

  end:
    siwifi_ipc_elem_var_deallocs(siwifi_hw, &elem);
    return error;
}


/*
 * @tdls_mgmt: prepare TDLS action frame packets and forward them to FW
 */
static int
siwifi_cfg80211_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
                        const u8 *peer, u8 action_code,  u8 dialog_token,
                        u16 status_code, u32 peer_capability,
                        bool initiator, const u8 *buf, size_t len)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    int ret = 0;

    /* make sure we support TDLS */
    if (!(wiphy->flags & WIPHY_FLAG_SUPPORTS_TDLS))
        return -ENOTSUPP;

    /* make sure we are in station mode (and connected) */
    if ((SIWIFI_VIF_TYPE(siwifi_vif) != NL80211_IFTYPE_STATION) ||
        (!siwifi_vif->up) || (!siwifi_vif->sta.ap))
        return -ENOTSUPP;

    /* only one TDLS link is supported */
    if ((action_code == WLAN_TDLS_SETUP_REQUEST) &&
        (siwifi_vif->sta.tdls_sta) &&
        (siwifi_vif->tdls_status == TDLS_LINK_ACTIVE)) {
        printk("%s: only one TDLS link is supported!\n", __func__);
        return -ENOTSUPP;
    }

    if ((action_code == WLAN_TDLS_DISCOVERY_REQUEST) &&
        (siwifi_hw->mod_params->ps_on)) {
        printk("%s: discovery request is not supported when "
                "power-save is enabled!\n", __func__);
        return -ENOTSUPP;
    }

    switch (action_code) {
    case WLAN_TDLS_SETUP_RESPONSE:
        /* only one TDLS link is supported */
        if ((status_code == 0) &&
            (siwifi_vif->sta.tdls_sta) &&
            (siwifi_vif->tdls_status == TDLS_LINK_ACTIVE)) {
            printk("%s: only one TDLS link is supported!\n", __func__);
            status_code = WLAN_STATUS_REQUEST_DECLINED;
        }
        /* fall-through */
    case WLAN_TDLS_SETUP_REQUEST:
    case WLAN_TDLS_TEARDOWN:
    case WLAN_TDLS_DISCOVERY_REQUEST:
    case WLAN_TDLS_SETUP_CONFIRM:
    case WLAN_PUB_ACTION_TDLS_DISCOVER_RES:
        ret = siwifi_tdls_send_mgmt_packet_data(siwifi_hw, siwifi_vif, peer, action_code,
                dialog_token, status_code, peer_capability, initiator, buf, len, 0, NULL);
        break;

    default:
        printk("%s: Unknown TDLS mgmt/action frame %pM\n",
                __func__, peer);
        ret = -EOPNOTSUPP;
        break;
    }

    if (action_code == WLAN_TDLS_SETUP_REQUEST) {
        siwifi_vif->tdls_status = TDLS_SETUP_REQ_TX;
    } else if (action_code == WLAN_TDLS_SETUP_RESPONSE) {
        siwifi_vif->tdls_status = TDLS_SETUP_RSP_TX;
    } else if ((action_code == WLAN_TDLS_SETUP_CONFIRM) && (ret == CO_OK)) {
        siwifi_vif->tdls_status = TDLS_LINK_ACTIVE;
        /* Set TDLS active */
        if (siwifi_vif->sta.tdls_sta)
            siwifi_vif->sta.tdls_sta->tdls.active = true;
    }

    return ret;
}

/*
 * @tdls_oper: execute TDLS operation
 */
static int
siwifi_cfg80211_tdls_oper(struct wiphy *wiphy, struct net_device *dev,
        const u8 *peer, enum nl80211_tdls_operation oper)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    int error;

    if (oper != NL80211_TDLS_DISABLE_LINK)
        return 0;

    if (!siwifi_vif->sta.tdls_sta) {
        printk("%s: TDLS station %pM does not exist\n", __func__, peer);
        return -ENOLINK;
    }

    if (memcmp(siwifi_vif->sta.tdls_sta->mac_addr, peer, ETH_ALEN) == 0) {
        /* Disable Channel Switch */
        if (!siwifi_send_tdls_cancel_chan_switch_req(siwifi_hw, siwifi_vif,
                                                   siwifi_vif->sta.tdls_sta,
                                                   NULL))
            siwifi_vif->sta.tdls_sta->tdls.chsw_en = false;

        netdev_info(dev, "Del TDLS sta %d (%pM)",
                siwifi_vif->sta.tdls_sta->sta_idx,
                siwifi_vif->sta.tdls_sta->mac_addr);
        /* Ensure that we won't process PS change ind */
        spin_lock_bh(&siwifi_hw->cb_lock);
        spin_lock_bh(&siwifi_hw->tx_lock);
        siwifi_vif->sta.tdls_sta->ps.active = false;
        siwifi_vif->sta.tdls_sta->valid = false;
        siwifi_txq_sta_deinit(siwifi_hw, siwifi_vif->sta.tdls_sta);
        spin_unlock_bh(&siwifi_hw->tx_lock);
        spin_unlock_bh(&siwifi_hw->cb_lock);
        error = siwifi_send_me_sta_del(siwifi_hw, siwifi_vif->sta.tdls_sta->sta_idx, true);
        if ((error != 0) && (error != -EPIPE))
            return error;

#ifdef CONFIG_SIWIFI_BFMER
            // Disable Beamformer if supported
            siwifi_bfmer_report_del(siwifi_hw, siwifi_vif->sta.tdls_sta);
            siwifi_mu_group_sta_del(siwifi_hw, siwifi_vif->sta.tdls_sta);
#endif /* CONFIG_SIWIFI_BFMER */

#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
        /* Set TDLS not active */
        siwifi_vif->sta.tdls_sta->tdls.active = false;
        siwifi_dbgfs_unregister_rc_stat(siwifi_hw, siwifi_vif->sta.tdls_sta);
#endif
        // Remove TDLS station
        siwifi_vif->tdls_status = TDLS_LINK_IDLE;
        siwifi_vif->sta.tdls_sta = NULL;
    }

    return 0;
}

#if MY_LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
/*
 * @tdls_channel_switch: enable TDLS channel switch
 */
static int
siwifi_cfg80211_tdls_channel_switch(struct wiphy *wiphy,
                                      struct net_device *dev,
                                      const u8 *addr, u8 oper_class,
                                      struct cfg80211_chan_def *chandef)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_sta *siwifi_sta = siwifi_vif->sta.tdls_sta;
    struct tdls_chan_switch_cfm cfm;
    int error;

    if ((!siwifi_sta) || (memcmp(addr, siwifi_sta->mac_addr, ETH_ALEN))) {
        printk("%s: TDLS station %pM doesn't exist\n", __func__, addr);
        return -ENOLINK;
    }

    if (!siwifi_sta->tdls.chsw_allowed) {
        printk("%s: TDLS station %pM does not support TDLS channel switch\n", __func__, addr);
        return -ENOTSUPP;
    }

    error = siwifi_send_tdls_chan_switch_req(siwifi_hw, siwifi_vif, siwifi_sta,
                                           siwifi_sta->tdls.initiator,
                                           oper_class, chandef, &cfm);
    if (error)
        return error;

    if (!cfm.status) {
        siwifi_sta->tdls.chsw_en = true;
        return 0;
    } else {
        printk("%s: TDLS channel switch already enabled and only one is supported\n", __func__);
        return -EALREADY;
    }
}

/*
 * @tdls_cancel_channel_switch: disable TDLS channel switch
 */
static void
siwifi_cfg80211_tdls_cancel_channel_switch(struct wiphy *wiphy,
                                              struct net_device *dev,
                                              const u8 *addr)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_sta *siwifi_sta = siwifi_vif->sta.tdls_sta;
    struct tdls_cancel_chan_switch_cfm cfm;

    if (!siwifi_sta)
        return;

    if (!siwifi_send_tdls_cancel_chan_switch_req(siwifi_hw, siwifi_vif,
                                               siwifi_sta, &cfm))
        siwifi_sta->tdls.chsw_en = false;
}
#endif /* version >= 3.19 */

/**
 * @change_bss: Modify parameters for a given BSS (mainly for AP mode).
 */
int siwifi_cfg80211_change_bss(struct wiphy *wiphy, struct net_device *dev,
                             struct bss_parameters *params)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    int res =  -EOPNOTSUPP;

    if (((SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_AP) ||
         (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_P2P_GO)) &&
        (params->ap_isolate > -1)) {

        if (params->ap_isolate)
            siwifi_vif->ap.flags |= SIWIFI_AP_ISOLATE;
        else
            siwifi_vif->ap.flags &= ~SIWIFI_AP_ISOLATE;

        res = 0;
    }

    return res;
}

/**
 * This function is to reduce repeated code,
 * extract the frequently repeated code between MESH, AP and STATION,
 * the fields filled in the front are private, and the fields filled in the back are public
 */
static int siwifi_dump_station_info (struct net_device *dev, struct wiphy *wiphy,
		u8 *mac, struct siwifi_sta *found_sta, struct station_info *sinfo)
{
	struct siwifi_vif *siwifi_vif = netdev_priv(dev);
	struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
	uint32_t tx_bytes, rx_bytes;
	uint32_t cur_jiffies = jiffies;
	union siwifi_rate_ctrl_info *r_cfg = NULL;
	union siwifi_mcs_index *mcs_index = NULL;
	uint32_t delta_jiffies;
	int status = -ENOENT;

	if (found_sta == NULL)
		return -ENOENT;
	memcpy(mac, found_sta->mac_addr, ETH_ALEN);
	//to calculate throughput
	delta_jiffies = cur_jiffies - found_sta->stats.last_calc_tp;
	if (delta_jiffies < HZ) {
		sinfo->expected_throughput = found_sta->stats.last_th_tp;
		goto tag;
	}
	/* Fill station information */
	if (cur_jiffies > found_sta->stats.last_calc_tp) {
		tx_bytes = (uint32_t)(found_sta->stats.tx_bytes - found_sta->stats.last_tx_bytes);
		rx_bytes = (uint32_t)(found_sta->stats.rx_bytes - found_sta->stats.last_rx_bytes);
		found_sta->stats.last_tx_tp = tx_bytes * 8 / delta_jiffies * HZ;
		found_sta->stats.last_rx_tp = rx_bytes * 8 / delta_jiffies * HZ;
	}
	found_sta->stats.last_th_tp = found_sta->stats.last_tx_tp + found_sta->stats.last_rx_tp;
	found_sta->stats.last_calc_tp = cur_jiffies;
	found_sta->stats.last_tx_bytes = found_sta->stats.tx_bytes;
	found_sta->stats.last_rx_bytes = found_sta->stats.rx_bytes;
	sinfo->expected_throughput = found_sta->stats.last_th_tp;
tag:
	sinfo->connected_time = ktime_get_seconds() - found_sta->stats.connected_time;
	sinfo->inactive_time = (ktime_get_seconds() - found_sta->stats.idle) * 1000;
	if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_AP || SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_STATION) {
		if (found_sta == NULL)
		{
			sinfo->sta_flags.set = 0;
			return status;
		}else{
			sinfo->sta_flags.set = BIT(NL80211_STA_FLAG_AUTHORIZED) |
				BIT(NL80211_STA_FLAG_AUTHENTICATED) |
				BIT(NL80211_STA_FLAG_ASSOCIATED) |
				BIT(NL80211_STA_FLAG_WME) |
				BIT(NL80211_STA_FLAG_MFP) ;
			sinfo->sta_flags.mask = BIT(NL80211_STA_FLAG_AUTHORIZED) |
				BIT(NL80211_STA_FLAG_WME) |
				BIT(NL80211_STA_FLAG_MFP) |
				BIT(NL80211_STA_FLAG_AUTHENTICATED) |
				BIT(NL80211_STA_FLAG_ASSOCIATED) ;
		}
	}

	if (found_sta->stats.rx_bytes) {
		sinfo->rx_bytes = found_sta->stats.rx_bytes;
		sinfo->rx_packets = found_sta->stats.rx_packets;
		sinfo->rx_dropped_misc = found_sta->stats.rx_dropped;
		//rssi
		sinfo->signal = found_sta->stats.data_rssi;
		sinfo->chains = 0x3;
		sinfo->chain_signal[0] = found_sta->stats.data_rssi_max;
		sinfo->chain_signal[1] = found_sta->stats.data_rssi_min;
		sinfo->signal_avg = found_sta->stats.data_rssi_avg;
		if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_AP || SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_STATION){
			// Todo: add HE
			if (found_sta->stats.format_mod == FORMATMOD_NON_HT || \
					found_sta->stats.format_mod == FORMATMOD_NON_HT_DUP_OFDM){
                sinfo->rxrate.legacy = siwifi_calculate_legrate(found_sta->stats.leg_rate & 0xF, 0);
			}
			else{
				//HT and VHT
				sinfo->rxrate.flags = (found_sta->stats.format_mod == FORMATMOD_VHT) ? RATE_INFO_FLAGS_VHT_MCS \
									  : RATE_INFO_FLAGS_MCS;
				if (found_sta->stats.short_gi)
					sinfo->rxrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
				switch (found_sta->stats.data_rx_bw)
				{
					case 0:
						sinfo->rxrate.bw = RATE_INFO_BW_20;
						break;
					case 1:
						sinfo->rxrate.bw = RATE_INFO_BW_40;
						break;
					case 2:
						if ((sinfo->rxrate.flags & RATE_INFO_FLAGS_MCS) == RATE_INFO_FLAGS_MCS)
							return -EINVAL;
						sinfo->rxrate.bw = RATE_INFO_BW_80;
						break;
					default:
						return -EINVAL;
				}
				sinfo->rxrate.mcs = found_sta->stats.data_rx_mcs;
				sinfo->rxrate.nss = found_sta->stats.data_rx_nss + 1;
			}
			sinfo->filled |= BIT(NL80211_STA_INFO_RX_BITRATE);
		}
	}
	if (found_sta->stats.tx_bytes) {
		sinfo->tx_bytes = found_sta->stats.tx_bytes;
		sinfo->tx_packets = found_sta->stats.tx_packets;
		//tx retries and failed
		sinfo->tx_retries = siwifi_hw->stats.tx_retry;
		sinfo->tx_failed = siwifi_hw->stats.tx_failed;
		if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_AP || SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_STATION){
            r_cfg = (union siwifi_rate_ctrl_info *)(&(found_sta->stats.last_tx_rate_config));
			mcs_index = (union siwifi_mcs_index *)(&(found_sta->stats.last_tx_rate_config));
			if (r_cfg->formatModTx == FORMATMOD_NON_HT || \
					r_cfg->formatModTx == FORMATMOD_NON_HT_DUP_OFDM){
                sinfo->txrate.legacy = siwifi_calculate_legrate(mcs_index->legacy & 0xF, 1);
			}else {
				sinfo->txrate.flags = 0;
				if (r_cfg->giAndPreTypeTx){
					sinfo->txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
				}
				//HT and VHT , consider HE
				if (r_cfg->formatModTx == FORMATMOD_VHT){
					sinfo->txrate.flags |= RATE_INFO_FLAGS_VHT_MCS;
					sinfo->txrate.mcs = mcs_index->vht.mcs;
					sinfo->txrate.nss = mcs_index->vht.nss + 1;
				}else if (r_cfg->formatModTx == FORMATMOD_HT_MF || \
						r_cfg->formatModTx == FORMATMOD_HT_GF ){
					sinfo->txrate.flags |= RATE_INFO_FLAGS_MCS;
					sinfo->txrate.mcs = mcs_index->ht.mcs + 8 * mcs_index->ht.nss;
					sinfo->txrate.nss = mcs_index->ht.nss + 1;
				}
				switch (r_cfg->bwTx)
				{
					case 0:
						sinfo->txrate.bw = RATE_INFO_BW_20;
						break;
					case 1:
						sinfo->txrate.bw = RATE_INFO_BW_40;
						break;
					case 2:
						if ((sinfo->txrate.flags & RATE_INFO_FLAGS_MCS) == RATE_INFO_FLAGS_MCS)
							return -EINVAL;
						sinfo->txrate.bw = RATE_INFO_BW_80;
						break;
					default:
						return -EINVAL;
				}
			}
			sinfo->filled |= BIT(NL80211_STA_INFO_TX_BITRATE);
		}
	}
#ifdef CPTCFG_WIRELESS
	sinfo->filled =	BIT(NL80211_STA_INFO_CONNECTED_TIME) |
		BIT(NL80211_STA_INFO_INACTIVE_TIME) |
		BIT(NL80211_STA_INFO_RX_BYTES64) |
		BIT(NL80211_STA_INFO_RX_PACKETS) |
		BIT(NL80211_STA_INFO_RX_DROP_MISC) |
		BIT(NL80211_STA_INFO_SIGNAL) |
		BIT(NL80211_STA_INFO_CHAIN_SIGNAL) |
		BIT(NL80211_STA_INFO_SIGNAL_AVG) |
		BIT(NL80211_STA_INFO_TX_BYTES64) |
		BIT(NL80211_STA_INFO_TX_PACKETS) |
		BIT(NL80211_STA_INFO_TX_RETRIES) |
		BIT(NL80211_STA_INFO_TX_FAILED) |
		BIT(NL80211_STA_INFO_TX_BITRATE) |
		BIT(NL80211_STA_INFO_RX_BITRATE) |
		BIT(NL80211_STA_INFO_EXPECTED_THROUGHPUT) |
		BIT(NL80211_STA_INFO_STA_FLAGS) |
		BIT(NL80211_STA_INFO_BSS_PARAM);
#else
	sinfo->filled = STATION_INFO_CONNECTED_TIME |
		STATION_INFO_INACTIVE_TIME |
		STATION_INFO_RX_BYTES64 |
		STATION_INFO_RX_PACKETS |
		STATION_INFO_RX_DROP_MISC |
		STATION_INFO_INFO_SIGNAL |
		STATION_INFO_CHAIN_SIGNAL |
		STATION_INFO_SIGNAL_AVG |
		STATION_INFO_TX_BYTES64 |
		STATION_INFO_TX_PACKETS |
		STATION_INFO_TX_RETRIES |
		STATION_INFO_TX_FAILED |
		STATION_INFO_TX_BITRATE |
		STATION_INFO_RX_BITRATE |
		STATION_INFO_RX_EXPECTED_THROUGHPUT |
		STATION_INFO_STA_FLAGS |
		STATION_INFO_BSS_PARAM;
#endif
	if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MESH_POINT) {
		struct mesh_peer_info_cfm peer_info_cfm;
		// Forward the information to the UMAC
		if ((status = siwifi_send_mesh_peer_info_req(siwifi_hw, siwifi_vif, found_sta->sta_idx,
						&peer_info_cfm))) {
			return status = 0;
		}
		// Fill station information
		sinfo->llid = peer_info_cfm.local_link_id;
		sinfo->plid = peer_info_cfm.peer_link_id;
		sinfo->plink_state = peer_info_cfm.link_state;
		sinfo->local_pm = peer_info_cfm.local_ps_mode;
		sinfo->peer_pm = peer_info_cfm.peer_ps_mode;
		sinfo->nonpeer_pm = peer_info_cfm.non_peer_ps_mode;

#ifdef CPTCFG_WIRELESS
		sinfo->filled |= BIT(NL80211_STA_INFO_LLID) |
			BIT(NL80211_STA_INFO_PLID) |
			BIT(NL80211_STA_INFO_PLINK_STATE) |
			BIT(NL80211_STA_INFO_LOCAL_PM) |
			BIT(NL80211_STA_INFO_PEER_PM) |
			BIT(NL80211_STA_INFO_NONPEER_PM) ;
#else
		sinfo->filled |= STATION_INFO_LLID |
			STATION_INFO_PLID |
			STATION_INFO_PLINK_STATE |
			STATION_INFO_LOCAL_PM |
			STATION_INFO_PEER_PM |
			STATION_INFO_NONPEER_PM;
#endif
		status = 0;
	}else if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_AP){
		if ((siwifi_vif->ap_settings) || (siwifi_vif->up)) {
			//WME
			sinfo->sta_flags.set |= BIT(NL80211_STA_FLAG_WME);
			//beacon_interval
			sinfo->bss_param.beacon_interval |= siwifi_vif->ap_settings->beacon_interval;
			//dtim_period
			sinfo->bss_param.dtim_period |= siwifi_vif->ap_settings->dtim_period;
		}
		if (!found_sta->stats.preamble) {
			sinfo->sta_flags.set |= BIT(NL80211_STA_FLAG_SHORT_PREAMBLE);
			sinfo->bss_param.flags |= BSS_PARAM_FLAGS_SHORT_PREAMBLE;
		} else {
			sinfo->sta_flags.set = 0;
		}
		//short_slot_time
		if (found_sta->stats.short_gi) {
			sinfo->bss_param.flags |= BSS_PARAM_FLAGS_SHORT_SLOT_TIME;
		} else {
			sinfo->bss_param.flags = 0;
		}
	}
	return status = 0;
}

/**
 * @get_station: get station information for the station identified by @mac
 */
static int siwifi_cfg80211_get_station(struct wiphy *wiphy, struct net_device *dev,
		const u8 *mac, struct station_info *sinfo)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_sta *found_sta = NULL;

    if (!siwifi_vif)
        return -ENOENT;

    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MONITOR)
        return -EINVAL;

    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_STATION ||
        SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_P2P_CLIENT) {
        if (siwifi_vif->sta.ap && ether_addr_equal(siwifi_vif->sta.ap->mac_addr, mac))
            found_sta = siwifi_vif->sta.ap;
    } else if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_AP_VLAN) {
        if (siwifi_vif->ap_vlan.sta_4a && ether_addr_equal(siwifi_vif->ap_vlan.sta_4a->mac_addr, mac))
            found_sta = siwifi_vif->ap_vlan.sta_4a;
    } else {
        struct siwifi_sta *sta;
        spin_lock_bh(&siwifi_hw->cb_lock);
        list_for_each_entry(sta, &siwifi_vif->ap.sta_list, list) {
            if (sta->valid || ether_addr_equal(sta->mac_addr, mac)){
                found_sta = sta;
                break;
            }
        }
        spin_unlock_bh(&siwifi_hw->cb_lock);
    }

    if (found_sta)
        return siwifi_dump_station_info(dev, wiphy, (u8 *)mac, found_sta, sinfo);

    return -EINVAL;
}

/**
 * @dump_station: dump station callback -- resume dump at index @idx
 */
static int siwifi_cfg80211_dump_station(struct wiphy *wiphy, struct net_device *dev,
        int idx, u8 *mac, struct station_info *sinfo)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_sta *found_sta = NULL;

    if (!siwifi_vif)
        return -ENOENT;

    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MONITOR)
        return -EINVAL;

    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_STATION ||
        SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_P2P_CLIENT) {
        // One sta will only connect to one ap.
        if ((idx == 0) && siwifi_vif->sta.ap && siwifi_vif->sta.ap->valid)
		    found_sta = siwifi_vif->sta.ap;
	} else if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_AP_VLAN) {
        // One AP_VLAN will only connect to one sta.
		if ((idx == 0) && siwifi_vif->ap_vlan.sta_4a && siwifi_vif->ap_vlan.sta_4a->valid)
            found_sta = siwifi_vif->ap_vlan.sta_4a;
	} else {
        struct siwifi_sta *sta;
        int i = 0;
		spin_lock_bh(&siwifi_hw->cb_lock);
		list_for_each_entry(sta, &siwifi_vif->ap.sta_list, list) {
			if (i == idx) {
                found_sta = sta;
                break;
			}
			i++;
		}
		spin_unlock_bh(&siwifi_hw->cb_lock);
	}

    if (found_sta == NULL)
        return -ENOENT;

    /* Copy peer mac address*/
    memcpy(mac, &found_sta->mac_addr, ETH_ALEN);

	return siwifi_dump_station_info(dev, wiphy, mac, found_sta, sinfo);
}

/**
 * @add_mpath: add a fixed mesh path
 */
static int siwifi_cfg80211_add_mpath(struct wiphy *wiphy, struct net_device *dev,
                                   const u8 *dst, const u8 *next_hop)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    int status = -ENOENT;

    /* Check if provided interface is a Mesh Point interface */
    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MESH_POINT) {
        struct mesh_path_update_cfm cfm;

        /* Forward the information to the UMAC */
        status = siwifi_send_mesh_path_update_req(siwifi_hw, siwifi_vif, dst, next_hop, &cfm);
    }

    return status;
}

/**
 * @del_mpath: delete a given mesh path
 */
static int siwifi_cfg80211_del_mpath(struct wiphy *wiphy, struct net_device *dev,
                                   const u8 *dst)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    int status = -ENOENT;

    /* Check if provided interface is a Mesh Point interface */
    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MESH_POINT) {
        struct mesh_path_update_cfm cfm;

        /* Forward the information to the UMAC */
        status = siwifi_send_mesh_path_update_req(siwifi_hw, siwifi_vif, dst, NULL, &cfm);
    }

    return status;
}

/**
 * @change_mpath: change a given mesh path
 */
static int siwifi_cfg80211_change_mpath(struct wiphy *wiphy, struct net_device *dev,
                                      const u8 *dst, const u8 *next_hop)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    int status = -ENOENT;

    /* Check if provided interface is a Mesh Point interface */
    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MESH_POINT) {
        struct mesh_path_update_cfm cfm;

        /* Forward the information to the UMAC */
        status = siwifi_send_mesh_path_update_req(siwifi_hw, siwifi_vif, dst, next_hop, &cfm);
    }

    return status;
}

/**
 * @get_mpath: get a mesh path for the given parameters
 */
static int siwifi_cfg80211_get_mpath(struct wiphy *wiphy, struct net_device *dev,
                                   u8 *dst, u8 *next_hop, struct mpath_info *pinfo)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    int status = -ENOENT;

    /* Check if provided interface is a Mesh Point interface */
    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MESH_POINT) {
        struct siwifi_mesh_path *mesh_path = NULL;
        struct siwifi_mesh_path *cur;

        list_for_each_entry(cur, &siwifi_vif->ap.mpath_list, list) {
            /* Compare the path target address and the provided destination address */
            if (memcmp(dst, &cur->tgt_mac_addr, ETH_ALEN)) {
                continue;
            }

            mesh_path = cur;
            break;
        }

        do
        {
            /* Check if path has been found */
            if (mesh_path == NULL) {
                break;
            }

            /* Copy target MAC address */
            if (mesh_path->p_nhop_sta) {
                /* Copy next HOP MAC address */
                memcpy(next_hop, mesh_path->p_nhop_sta->mac_addr, ETH_ALEN);
            }

            /* Fill path information */
            pinfo->filled = 0;
            pinfo->generation = siwifi_vif->ap.generation;

            status = 0;
        } while (0);
    }

    return status;
}

/**
 * @dump_mpath: dump mesh path callback -- resume dump at index @idx
 */
static int siwifi_cfg80211_dump_mpath(struct wiphy *wiphy, struct net_device *dev,
                                    int idx, u8 *dst, u8 *next_hop,
                                    struct mpath_info *pinfo)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    int status = -ENOENT;

    /* Check if provided interface is a Mesh Point interface */
    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MESH_POINT) {
        struct siwifi_mesh_path *mesh_path = NULL;
        struct siwifi_mesh_path *cur;
        int i = 0;

        list_for_each_entry(cur, &siwifi_vif->ap.mpath_list, list) {
            if (i < idx) {
                i++;
                continue;
            }

            mesh_path = cur;
            break;
        }

        do
        {
            if (mesh_path == NULL) {
                break;
            }

            /* Copy target MAC address */
            memcpy(dst, &mesh_path->tgt_mac_addr, ETH_ALEN);
            if (mesh_path->p_nhop_sta) {
                /* Copy next HOP MAC address */
                memcpy(next_hop, mesh_path->p_nhop_sta->mac_addr, ETH_ALEN);
            }

            /* Fill path information */
            pinfo->filled = 0;
            pinfo->generation = siwifi_vif->ap.generation;

            status = 0;
        } while (0);
    }

    return status;
}

#if MY_LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
/**
 * @get_mpp: get a mesh proxy path for the given parameters
 */
static int siwifi_cfg80211_get_mpp(struct wiphy *wiphy, struct net_device *dev,
                                 u8 *dst, u8 *mpp, struct mpath_info *pinfo)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    int status = -ENOENT;

    /* Check if provided interface is a Mesh Point interface */
    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MESH_POINT) {
        struct siwifi_mesh_proxy *mesh_proxy = NULL;
        struct siwifi_mesh_proxy *cur;

        list_for_each_entry(cur, &siwifi_vif->ap.proxy_list, list) {
            if (cur->local) {
                continue;
            }

            /* Compare the path target address and the provided destination address */
            if (memcmp(dst, &cur->ext_sta_addr, ETH_ALEN)) {
                continue;
            }

            mesh_proxy = cur;
            break;
        }

        do
        {
            /* Check if proxied device has been found */
            if (mesh_proxy == NULL) {
                break;
            }

            memcpy(mpp, &mesh_proxy->proxy_addr, ETH_ALEN);

            /* Fill path information */
            pinfo->filled = 0;
            pinfo->generation = siwifi_vif->ap.generation;

            status = 0;
        } while (0);
    }

    return status;
}

/**
 * @dump_mpp: dump mesh proxy path callback -- resume dump at index @idx
 */
static int siwifi_cfg80211_dump_mpp(struct wiphy *wiphy, struct net_device *dev,
                                  int idx, u8 *dst, u8 *mpp, struct mpath_info *pinfo)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    int status = -ENOENT;

    /* Check if provided interface is a Mesh Point interface */
    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MESH_POINT) {
        struct siwifi_mesh_proxy *mesh_proxy = NULL;
        struct siwifi_mesh_proxy *cur;
        int i = 0;

        list_for_each_entry(cur, &siwifi_vif->ap.proxy_list, list) {
            if (cur->local) {
                continue;
            }

            if (i < idx) {
                i++;
                continue;
            }

            mesh_proxy = cur;
            break;
        }

        do {
            if (mesh_proxy == NULL) {
                break;
            }

            /* Copy target MAC address */
            memcpy(dst, &mesh_proxy->ext_sta_addr, ETH_ALEN);
            memcpy(mpp, &mesh_proxy->proxy_addr, ETH_ALEN);

            /* Fill path information */
            pinfo->filled = 0;
            pinfo->generation = siwifi_vif->ap.generation;

            status = 0;
        } while (0);
    }

    return status;
}
#endif /* version >= 3.19 */

/**
 * @get_mesh_config: Get the current mesh configuration
 */
static int siwifi_cfg80211_get_mesh_config(struct wiphy *wiphy, struct net_device *dev,
                                         struct mesh_config *conf)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    int status = -ENOENT;

    /* Check if provided interface is a Mesh Point interface */
    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MESH_POINT) {
        status = 0;
    }

    return status;
}

/**
 * @update_mesh_config: Update mesh parameters on a running mesh.
 */
static int siwifi_cfg80211_update_mesh_config(struct wiphy *wiphy, struct net_device *dev,
                                            u32 mask, const struct mesh_config *nconf)
{
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    int status = -ENOENT;

    /* Check if provided interface is a Mesh Point interface */
    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MESH_POINT) {
        struct mesh_update_cfm cfm;

        /* Forward the information to the UMAC */
        status = siwifi_send_mesh_update_req(siwifi_hw, siwifi_vif, mask, nconf, &cfm);

        if (!status) {
            if (cfm.status != 0) {
                status = -ENOENT;
            }
        }
    }

    return status;
}

/**
 * @join_mesh: join the mesh network with the specified parameters
 * (invoked with the wireless_dev mutex held)
 */
static int siwifi_cfg80211_join_mesh(struct wiphy *wiphy, struct net_device *dev,
                                   const struct mesh_config *conf, const struct mesh_setup *setup)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct mesh_start_cfm mesh_start_cfm;
    int error = 0;
    u8 txq_status = 0;
    /* STA for BC/MC traffic */
    struct siwifi_sta *sta;

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

    /* Forward the information to the UMAC */
    if ((error = siwifi_send_mesh_start_req(siwifi_hw, siwifi_vif, conf, setup, &mesh_start_cfm))) {
        return error;
    }

    /* Check the status */
    switch (mesh_start_cfm.status) {
        case CO_OK:
            siwifi_vif->ap.bcmc_index = mesh_start_cfm.bcmc_idx;
            siwifi_vif->ap.flags = 0;
            siwifi_vif->use_4addr = true;
            siwifi_vif->user_mpm = setup->user_mpm;

#if DEBUG_ARRAY_CHECK
            BUG_ON(mesh_start_cfm.bcmc_idx >= NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX);
#endif
            sta = &siwifi_hw->sta_table[mesh_start_cfm.bcmc_idx];
            sta->valid = true;
            sta->aid = 0;
            sta->sta_idx = mesh_start_cfm.bcmc_idx;
            sta->ch_idx = mesh_start_cfm.ch_idx;
            sta->vif_idx = siwifi_vif->vif_index;
            sta->qos = true;
            sta->acm = 0;
            sta->ps.active = false;
            siwifi_mu_group_sta_init(sta, NULL);
            spin_lock_bh(&siwifi_hw->cb_lock);
            siwifi_chanctx_link(siwifi_vif, mesh_start_cfm.ch_idx,
                              (struct cfg80211_chan_def *)(&setup->chandef));
            if (siwifi_hw->cur_chanctx != mesh_start_cfm.ch_idx) {
                txq_status = SIWIFI_TXQ_STOP_CHAN;
            }
            spin_lock_bh(&siwifi_hw->tx_lock);
            siwifi_txq_vif_init(siwifi_hw, siwifi_vif, txq_status);
            spin_unlock_bh(&siwifi_hw->tx_lock);
            spin_unlock_bh(&siwifi_hw->cb_lock);

            netif_tx_start_all_queues(dev);
            netif_carrier_on(dev);

            /* If the AP channel is already the active, we probably skip radar
               activation on MM_CHANNEL_SWITCH_IND (unless another vif use this
               ctxt). In anycase retest if radar detection must be activated
             */
            spin_lock_bh(&siwifi_hw->cb_lock);
            if (siwifi_hw->cur_chanctx == mesh_start_cfm.ch_idx) {
                siwifi_radar_detection_enable_on_cur_channel(siwifi_hw);
            }
            spin_unlock_bh(&siwifi_hw->cb_lock);
            break;

        case CO_BUSY:
            error = -EINPROGRESS;
            break;

        default:
            error = -EIO;
            break;
    }

    /* Print information about the operation */
    if (error) {
        netdev_info(dev, "Failed to start MP (%d)", error);
    } else {
        netdev_info(dev, "MP started: ch=%d, bcmc_idx=%d",
                    siwifi_vif->ch_index, siwifi_vif->ap.bcmc_index);
    }

    return error;
}

/**
 * @leave_mesh: leave the current mesh network
 * (invoked with the wireless_dev mutex held)
 */
static int siwifi_cfg80211_leave_mesh(struct wiphy *wiphy, struct net_device *dev)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);
    struct mesh_stop_cfm mesh_stop_cfm;
    int error = 0;

    error = siwifi_send_mesh_stop_req(siwifi_hw, siwifi_vif, &mesh_stop_cfm);

    if (error == 0) {
        /* Check the status */
        switch (mesh_stop_cfm.status) {
            case CO_OK:
                spin_lock_bh(&siwifi_hw->cb_lock);
                siwifi_chanctx_unlink(siwifi_vif);
                siwifi_radar_cancel_cac(&siwifi_hw->radar);
                spin_unlock_bh(&siwifi_hw->cb_lock);
                /* delete any remaining STA*/
                while (!list_empty(&siwifi_vif->ap.sta_list)) {
                    siwifi_cfg80211_del_station(wiphy, dev, NULL);
                }
                spin_lock_bh(&siwifi_hw->tx_lock);
                /* delete BC/MC STA */
                siwifi_txq_vif_deinit(siwifi_hw, siwifi_vif);
                spin_unlock_bh(&siwifi_hw->tx_lock);
                siwifi_del_bcn(&siwifi_vif->ap.bcn);

                netif_tx_stop_all_queues(dev);
                netif_carrier_off(dev);

                break;

            default:
                error = -EIO;
                break;
        }
    }

    if (error) {
        netdev_info(dev, "Failed to stop MP");
    } else {
        netdev_info(dev, "MP Stopped");
    }

    return 0;
}

static void siwifi_get_cur_survey_info(struct wiphy *wiphy, struct wireless_dev *wdev, uint16_t center_freq, uint8_t survey_idx)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);
    struct siwifi_vif *siwifi_vif = container_of(wdev, struct siwifi_vif, wdev);
    struct cfg80211_chan_def chandef;
    struct dbg_get_vendor_mp_info_cfm cfm;

    // judge siwifi_hw & siwifi_vif whether up
    if ((siwifi_vif == NULL) || (siwifi_hw == NULL)) {
        if (siwifi_hw->debug_get_survey == 1)
            printk(" no vif or siwifi hw\n");
        return;
    }
    if (!siwifi_vif->up) {
        if (siwifi_hw->debug_get_survey == 1)
            printk("vif is not up\n");
        return;
    }
    // get current center freq
    if (siwifi_cfg80211_get_channel(wiphy, wdev, &chandef) != 0) {
        if (siwifi_hw->debug_get_survey == 1)
            printk("Get channel failed !\n");
        return;
    }
    // judge the freq whether equal
    if (center_freq == chandef.chan->center_freq)
    {
        // get current survey info
        memset(&cfm, 0, sizeof(struct dbg_get_vendor_mp_info_cfm));
        if (siwifi_send_dbg_get_vendor_mp_info_req(siwifi_hw, &cfm) == 0) {
#if DEBUG_ARRAY_CHECK
            BUG_ON(survey_idx >= SCAN_CHANNEL_MAX);
#endif
            // fill survey info
            siwifi_hw->survey[survey_idx].chan_time_ms = (cfm.last_cal_time_us * 30 ) / (1000 * 1024);
            siwifi_hw->survey[survey_idx].chan_time_busy_ms = (cfm.cca_busy_time_us * 30) / (1000 * 1024);
            siwifi_hw->survey[survey_idx].noise_dbm = (cfm.last_noise0_dbm + cfm.last_noise1_dbm) / 2;
            siwifi_hw->survey[survey_idx].filled = (SURVEY_INFO_IN_USE | SURVEY_INFO_TIME
                    | SURVEY_INFO_TIME_BUSY | SURVEY_INFO_NOISE_DBM);
        } else {
            printk("siwifi_send_dbg_get_vendor_mp_info_req send msg fail\n");
            return;
        }
    }

    return;
}

extern void siwifi_reprobe_driver(struct siwifi_hw *siwifi_hw);
#ifdef LOCAL_RECOVERY
static void dump_station(struct siwifi_sta *sta)
{
    if (!sta)
        return;
    printk("dump_station: \n");;
    printk("sta_idx : %d\n", sta->sta_idx);
    printk("aid : %d\n", sta->aid);
    printk("vif_idx: %d\n", sta->vif_idx);
    printk("vlan_idx : %d\n", sta->vlan_idx);
    printk("band : %d\n", sta->band);
    printk("width : %d\n", sta->width);
    printk("center_freq : %d\n", sta->center_freq);
    printk("center_freq1: %d\n", sta->center_freq1);
    printk("center_freq2: %d\n", sta->center_freq2);
    printk("ch_idx: %d\n", sta->ch_idx);
    printk("qos : %d\n", sta->qos);
    printk("acm: %d\n", sta->acm);
    printk("uapsd_tids : %d\n", sta->uapsd_tids);
    printk("mac_addr : %pM\n", sta->mac_addr);
    printk("key.index: %d\n", sta->key.hw_idx);
    printk("valid : %d\n", sta->valid);
    printk("ps.active: %d\n", sta->ps.active);
    printk("ht: %d\n", sta->ht);
    printk("vht: %d\n", sta->vht);
    printk("ac_param[0]: %d\n", sta->ac_param[0]);
    printk("ac_param[1]: %d\n", sta->ac_param[1]);
    printk("ac_param[2]: %d\n", sta->ac_param[2]);
    printk("ac_param[3]: %d\n", sta->ac_param[3]);
}

static void dump_vif(struct siwifi_vif *vif)
{

    if (!vif)
        return;
    printk("dump_vif: \n");;
    printk("vif_index : %d\n", vif->vif_index);
    printk("siwifi_hw : %p\n", vif->siwifi_hw);
    printk("ndev : %p\n", vif->ndev);
    printk("drv_vif_index: %d\n", vif->drv_vif_index);
    printk("ch_index: %d\n", vif->ch_index);
    printk("up : %d\n", vif->up);
    printk("use_4addr: %d\n", vif->use_4addr);
    printk("is_resending: %d\n", vif->is_resending);
    printk("user_mpm: %d\n", vif->user_mpm);
    printk("roc_tdls: %d\n", vif->roc_tdls);
    printk("tdls_status: %d\n", vif->tdls_status);
    printk("tdls_chsw_prohibited: %d\n", vif->tdls_chsw_prohibited);
    printk("txpower: %d\n", vif->txpower);
    printk("txpower_idx: %d\n", vif->txpower_idx);
    if (SIWIFI_VIF_TYPE(vif) == NL80211_IFTYPE_AP) {
        printk("ap.flags : %d\n", vif->ap.flags);
        printk("ap.bcmc_index: %d\n", vif->ap.bcmc_index);
        printk("ap.csa: %p\n", vif->ap.csa);
        printk("ap.create_path: %d\n", vif->ap.create_path);
        printk("ap.generation: %d\n", vif->ap.generation);
    }
}

static int siwifi_recovery_station(struct siwifi_hw *siwifi_hw, struct net_device *dev, struct siwifi_sta_rec_info *rec)
{

    int error;
    struct siwifi_sta *sta;
    struct siwifi_vif *siwifi_vif = netdev_priv(dev);

    // add station
    siwifi_hw->recovering_sta_idx = rec->sta_idx;
    error = siwifi_cfg80211_add_station(siwifi_hw->wiphy, dev, (u8 *)(rec->mac_addr), rec->params);
    if (error) {
        printk("recovery station failed to add station, %pM\n", rec->mac_addr);
        return error;
    }
    sta = siwifi_hw->recovering_sta;
    sta->key.valid = 0;

    //open the ctrl port
    if (rec->cp_open_set) {
        error = siwifi_send_me_set_control_port_req(siwifi_hw, rec->control_port_open, sta->sta_idx);
        if (error) {
            printk("siwifi_send_me_set_control_port_req failed\n");
        }
        sta->rec_info.cp_open_set = 1;
        sta->rec_info.control_port_open = rec->control_port_open;
    }

    // recovery the plink state
    if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MESH_POINT) {
        if (rec->pstate_set && rec->plink_state < NUM_NL80211_PLINK_STATES) {
            siwifi_send_mesh_peer_update_ntf(siwifi_hw, siwifi_vif, sta->sta_idx, rec->plink_state);
            sta->rec_info.pstate_set = 1;
            sta->rec_info.plink_state = rec->plink_state;
        }
    }

    // recovery vlan information
    if (rec->vlan) {
        struct siwifi_vif *vif;
        uint8_t vlan_idx;

        vif = netdev_priv(rec->vlan);
        vlan_idx = vif->vif_index;

        if (sta->vlan_idx != vlan_idx) {
            struct siwifi_vif *old_vif;
#if DEBUG_ARRAY_CHECK
            BUG_ON(sta->vlan_idx >= NX_VIRT_DEV_MAX + NX_REMOTE_STA_MAX);
#endif
            old_vif = siwifi_hw->vif_table[sta->vlan_idx];
            siwifi_txq_sta_switch_vif(sta, old_vif, vif);
            sta->vlan_idx = vlan_idx;

            if ((SIWIFI_VIF_TYPE(vif) == NL80211_IFTYPE_AP_VLAN) &&
                    (vif->use_4addr)) {
                WARN((vif->ap_vlan.sta_4a),
                        "4A AP_VLAN interface with more than one sta");
                vif->ap_vlan.sta_4a = sta;
            }

            if ((SIWIFI_VIF_TYPE(old_vif) == NL80211_IFTYPE_AP_VLAN) &&
                    (old_vif->use_4addr)) {
                old_vif->ap_vlan.sta_4a = NULL;
            }
        }
        sta->rec_info.vlan = rec->vlan;
    }
    sta->rec_info.qos = rec->qos;

    return 0;
}

static void siwifi_disconnect_all(struct siwifi_hw *siwifi_hw, struct siwifi_vif *siwifi_vif)
{
    struct ieee80211_mgmt mgmt;
    struct cfg80211_mgmt_tx_params params;
    u8 bc_addr[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    u64 cookie;

    memset(&mgmt, 0, sizeof(mgmt));
    memset(&params, 0, sizeof(params));
    mgmt.frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT |
            IEEE80211_STYPE_DISASSOC);
    memcpy(mgmt.da, bc_addr, ETH_ALEN);
    memcpy(mgmt.sa, siwifi_vif->ndev->dev_addr, ETH_ALEN);
    memcpy(mgmt.bssid, siwifi_vif->ndev->dev_addr, ETH_ALEN);
    mgmt.u.disassoc.reason_code = cpu_to_le16(WLAN_REASON_UNSPECIFIED);

    params.buf = (void *)&mgmt;
    params.len = sizeof(struct ieee80211_hdr_3addr) + sizeof(mgmt.u.disassoc);

    spin_lock_bh(&siwifi_hw->cb_lock);
    if (siwifi_start_mgmt_xmit(siwifi_vif, NULL, &params, false, &cookie)) {
        printk("failed to send DISASSOC frames\n");
    }
    spin_unlock_bh(&siwifi_hw->cb_lock);
}

static void siwifi_notify_disconnect(struct siwifi_hw *siwifi_hw, struct siwifi_vif *siwifi_vif)
{

}

static int siwifi_start_driver(struct siwifi_hw *siwifi_hw)
{
    struct siwifi_vif *siwifi_vif;
    struct mm_add_if_cfm add_if_cfm;
    struct mm_set_power_cfm pwr_cfm;
    struct net_device *dev;

    int error = 0;
    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
    flush_work(&siwifi_hw->debugfs.helper_work);
#endif
    //before start hardware reset the hwq
    //reset the hwq in start is correct,
    //because no matter in stop or in restart driver, there always contains ipc_txq_flush skb operations
    //but no one increase hwq's credits.

    siwifi_hwq_init(siwifi_hw);
    siwifi_hw->scaning = false;
#ifdef CONFIG_SIWIFI_SORT_SCAN
    siwifi_hw->scan_num = 0;
    memset(siwifi_hw->scan_results, 0, sizeof(siwifi_hw->scan_results));
    memset(siwifi_hw->bss_results, 0, sizeof(siwifi_hw->bss_results));
#endif
    // Start the FW
    if ((error = siwifi_send_start(siwifi_hw)))
        return error;

    /* Device is now started */
    set_bit(SIWIFI_DEV_STARTED, &siwifi_hw->drv_flags);

    //set default txpower lvl
    siwifi_send_set_power_lvl(siwifi_hw, (u8)siwifi_hw->mod_params->txpower_lvl);

    list_for_each_entry(siwifi_vif, &siwifi_hw->vifs, list)
    {
        if (!siwifi_vif->up)
            continue;
        dev = siwifi_vif->ndev;
        if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_AP_VLAN) {
            /* For AP_vlan use same fw and drv indexes. We ensure that this index
               will not be used by fw for another vif by taking index >= NX_VIRT_DEV_MAX */
            add_if_cfm.inst_nbr = siwifi_vif->drv_vif_index;
            netif_tx_stop_all_queues(dev);
        } else {
            /* Forward the information to the LMAC,
             *     p2p value not used in FMAC configuration, iftype is sufficient */
            add_if_cfm.status = 1;
            add_if_cfm.inst_nbr = siwifi_vif->drv_vif_index;
            if ((error = siwifi_send_add_if(siwifi_hw, dev->dev_addr,
                                 SIWIFI_VIF_TYPE(siwifi_vif), false,siwifi_vif->use_4addr, &add_if_cfm)))
                return error;

            if (add_if_cfm.status != 0) {
                SIWIFI_PRINT_CFM_ERR(add_if);
                return -EIO;
            }
        }
        siwifi_vif->vif_index = add_if_cfm.inst_nbr;
        //siwifi_vif->up = true;
        siwifi_hw->vif_started ++;
#if DEBUG_ARRAY_CHECK
        BUG_ON(add_if_cfm.inst_nbr >= NX_VIRT_DEV_MAX + NX_REMOTE_STA_MAX);
#endif
        siwifi_hw->vif_table[add_if_cfm.inst_nbr] = siwifi_vif;

        if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_AP) {
            struct siwifi_key_recovery_info *rec_key;
            int vif_key_index;
            /* siwifi_cfg80211_start_ap */
            if (siwifi_vif->ap_settings) {
                for (vif_key_index = 0; vif_key_index < MAX_VIF_KEY_NUM; vif_key_index++)
                    siwifi_vif->key[vif_key_index].valid = 0;
                siwifi_cfg80211_start_ap(siwifi_hw->wiphy, dev, siwifi_vif->ap_settings);
            }
            siwifi_vif->ap.flags = siwifi_vif->ap.flags_recovery;

            // Restore group key for this vif
            for (vif_key_index = 0; vif_key_index < MAX_VIF_KEY_NUM; vif_key_index++) {
                rec_key = &siwifi_vif->rec_key[vif_key_index];
                if (rec_key->valid && rec_key->params) {
                    siwifi_cfg80211_add_key(siwifi_hw->wiphy, dev,
                            rec_key->key_index, rec_key->pairwise, NULL, rec_key->params);
                    siwifi_cfg80211_set_default_key(siwifi_hw->wiphy, dev, rec_key->key_index, false, false);
                }
            }
        } else if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_STATION) {
            if (siwifi_vif->sta.ap) {
                /*  siwifi_send_sm_set_channel_req(siwifi_hw,siwifi_vif);
                 siwifi_chanctx_link(siwifi_vif, siwifi_vif->sta.ap->ch_idx, NULL);
                 siwifi_txq_sta_init(siwifi_hw, siwifi_vif->sta.ap, 0);
                 siwifi_txq_tdls_vif_init(siwifi_hw, siwifi_vif);
                 siwifi_dbgfs_register_rc_stat(siwifi_hw, siwifi_vif->sta.ap);
                 siwifi_mu_group_sta_init(siwifi_vif->sta.ap, NULL);*/
                cfg80211_disconnected(dev, WLAN_REASON_DEAUTH_LEAVING, NULL, 0, false, GFP_ATOMIC);
            }

            // Only this vif has carrier, then start txqueue
            if (netif_carrier_ok(siwifi_vif->ndev)) {
                netif_tx_start_all_queues(dev);
                //netif_carrier_on(dev);
            }
        } else if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_MONITOR) {
            siwifi_hw->monitor_vif = siwifi_vif->vif_index;
            if (siwifi_vif->ch_index != SIWIFI_CH_NOT_SET){
                // Configure the monitor channel
                error = siwifi_send_config_monitor_req(siwifi_hw, &siwifi_hw->chanctx_table[siwifi_vif->ch_index].chan_def, NULL);
                if (error) {
                    printk("siwifi_send_config_monitor_req failed\n");
                }
            }
        }

        // Restore the vif power
        error = siwifi_send_set_power(siwifi_hw, siwifi_vif->vif_index, siwifi_vif->txpower, &pwr_cfm);
        if (!error) {
            siwifi_vif->txpower = pwr_cfm.power;
            siwifi_vif->txpower_idx = pwr_cfm.radio_idx;
        }
        // dump_vif(siwifi_vif);
    } // vif while end

    // This must be done after all vif prepared, can not put this code into previous vif loop
    // some station are switch txq from one vif to another if vlan is enable
    // So station must be prepared after all vifs ready
    list_for_each_entry(siwifi_vif, &siwifi_hw->vifs, list)
    {
        if (!siwifi_vif->up)
            continue;
        if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_AP) {
            struct siwifi_sta_rec_info *cur, *tmp;
            struct siwifi_key_recovery_info *rec_key;
            int local_sta_list_cnt = 0;

            if (!siwifi_hw->dying_gasp_valid && 0) {
                printk("dying gasp information is not valid, it's better to disconnect all stations\n");
                // 1, send deauth frame dst address FF,FF,FF,FF,FF,FF
                siwifi_disconnect_all(siwifi_hw, siwifi_vif);
                // 2, notified hostapd to change the
                siwifi_notify_disconnect(siwifi_hw, siwifi_vif);
            } else {
                dev = siwifi_vif->ndev;
                /* add sta if have sta connected to this ap*/
                list_for_each_entry_safe(cur, tmp, &siwifi_vif->ap.local_sta_list, list)
                {
                    local_sta_list_cnt ++;
                    WARN_ON(local_sta_list_cnt > NX_REMOTE_STA_MAX);

                    siwifi_recovery_station(siwifi_hw, dev, cur);
                    siwifi_kfree(cur->params);

                    rec_key = &cur->rec_key;
                    /* add sta key */
                    if (rec_key->valid && rec_key->params) {
                        siwifi_cfg80211_add_key(siwifi_hw->wiphy, dev,
                                        /*u8 key_index0*/ rec_key->key_index, rec_key->pairwise, (u8 *)(cur->mac_addr),
                                        rec_key->params);
                        siwifi_kfree(rec_key->params);
                        rec_key->params = NULL;
                        rec_key->valid = false;
                    }
                    list_del(&cur->list);
                }
                /*{
                    struct siwifi_sta *cur_sta;
                    printk("try to dump station after recovering......\n\n");

                    list_for_each_entry(cur_sta, &siwifi_vif->ap.sta_list, list)
                    {
                        dump_station(cur_sta);
                    }
                }*/
            }
        }
    }

    SIWIFI_DBG("start driver end \n");

    return 0;
}

static void siwifi_restart_driver(struct work_struct *ws)
{
    struct siwifi_vif *siwifi_vif, *__siwifi_vif;
    struct siwifi_sta *cur, *tmp;
    struct net_device *dev;
    int ret = 0;
    int i = 0;
    struct siwifi_hw *siwifi_hw;

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);
    siwifi_hw = container_of(ws, struct siwifi_hw, restart_work);

    //wait the dump to be finished
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
    flush_work(&siwifi_hw->debugfs.helper_work);
#endif
    rtnl_lock();
    spin_lock_bh(&siwifi_hw->cb_lock);
    // RM#12017 If an assert occurs during scanning, a cfg80211_scan_done message should be sent.
    // Otherwise, the upper layer thinks that the resource is busy
    if (siwifi_hw->scan_request) {
        struct cfg80211_scan_info info = {
            .aborted = true,
        };
        cfg80211_scan_done(siwifi_hw->scan_request, &info);
        siwifi_hw->scan_request = NULL;
    }
    if(siwifi_hw->scaning){
        complete_all(&siwifi_hw->scan_complete);
        siwifi_hw->scaning = false;
    }
    siwifi_radar_cancel_cac(&siwifi_hw->radar);

    list_for_each_entry_safe(siwifi_vif, __siwifi_vif, &siwifi_hw->vifs, list)
    {
        //dump_vif(siwifi_vif);
        dev = siwifi_vif->ndev;
        if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_AP) {
            struct siwifi_sta_rec_info *new_sta;
            // restore flags
            siwifi_vif->ap.flags_recovery = siwifi_vif->ap.flags;
            /* copy from siwifi_cfg80211_stop_ap */
            /* maybe send msg to lmac,this time lmac interrupt is disable so not care */
            siwifi_chanctx_unlink(siwifi_vif);
            //siwifi_vif->vif = NULL;
            //deinit txqs from hwqs case siwifi_vif will be reinit latter when add_interface
            spin_lock_bh(&siwifi_hw->tx_lock);
            siwifi_txq_vif_deinit(siwifi_hw, siwifi_vif);
            spin_unlock_bh(&siwifi_hw->tx_lock);
            //deinit stations
            list_for_each_entry_safe(cur, tmp, &siwifi_vif->ap.sta_list, list)
            {
                netdev_info(dev, "restart Del sta %d (%pM)", cur->sta_idx, cur->mac_addr);
                //dump_station(cur);
                spin_lock_bh(&siwifi_hw->tx_lock);
                siwifi_txq_sta_deinit(siwifi_hw, cur);
				siwifi_sta_hash_del(siwifi_vif, cur);
                spin_unlock_bh(&siwifi_hw->tx_lock);
                //copy sta info to local
                new_sta = &siwifi_hw->local_sta_table[i];
                i++;
                memcpy(new_sta, &cur->rec_info, sizeof(struct siwifi_sta_rec_info));
                list_add_tail(&new_sta->list, &siwifi_vif->ap.local_sta_list);
                list_del(&cur->list);
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
                //siwifi_dbgfs_unregister_rc_stat(siwifi_hw, cur);
#endif
                cur->valid = false;
            }
            siwifi_del_bcn(&siwifi_vif->ap.bcn);
            siwifi_del_csa(siwifi_vif);
        } else if (SIWIFI_VIF_TYPE(siwifi_vif) == NL80211_IFTYPE_STATION) {
            if (siwifi_vif->sta.ap) {
                spin_lock_bh(&siwifi_hw->tx_lock);
                siwifi_txq_sta_deinit(siwifi_hw, siwifi_vif->sta.ap);
                siwifi_txq_tdls_vif_deinit(siwifi_vif);
                spin_unlock_bh(&siwifi_hw->tx_lock);
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
                siwifi_dbgfs_unregister_rc_stat(siwifi_hw, siwifi_vif->sta.ap);
#endif
                siwifi_chanctx_unlink(siwifi_vif);
            }
        }
        //reset to default value
        siwifi_vif->vif_index = 0;
        // we should keep siwifi_vif->up same as it originally is.
        //siwifi_vif->up = false;
#if DEBUG_ARRAY_CHECK
      	BUG_ON(siwifi_vif->vif_index >= NX_VIRT_DEV_MAX + NX_REMOTE_STA_MAX);
#endif
        siwifi_hw->vif_table[siwifi_vif->vif_index] = NULL;

        //If carrier is on, then we need to stop uplayer txqueue, bu
        if (netif_carrier_ok(siwifi_vif->ndev)) {
            netif_tx_stop_all_queues(siwifi_vif->ndev);
            /*Franklin, there is no need to tell uplayer that carrier is off, this maybe cause
             lots of uncessary consumption
             We'd better to make recovery be quiet
             */
            // netif_carrier_off(siwifi_vif->ndev);
        }
    } //vif list end

    siwifi_hw->vif_started = 0;
    // reset the stuck check list
    spin_lock_bh(&siwifi_hw->tx_lock);
    INIT_LIST_HEAD(&siwifi_hw->stuck_check_list);
    spin_unlock_bh(&siwifi_hw->tx_lock);
    spin_unlock_bh(&siwifi_hw->cb_lock);

    if((ret = siwifi_platform_restart(siwifi_hw))){
        printk(KERN_CRIT "%s: Couldn't restart the platform(), ret : %d\n", __func__, ret);
        goto REPROBE;
    }
    if ((ret = siwifi_send_reset(siwifi_hw))) {
        printk(KERN_CRIT "%s: Couldn't reset the LMAC, ret : %d .. aborting(%d)\n", __func__, ret, siwifi_hw->mod_params->is_hb);
        goto REPROBE;
    }
    // There is no need to init radar detection again
    // siwifi_radar_detection_init(&siwifi_hw->radar);

    /* Set parameters to firmware */
    siwifi_send_me_config_req(siwifi_hw);

    /* Set channel parameters to firmware (must be done after WiPHY registration) */
    siwifi_send_me_chan_config_req(siwifi_hw);

    siwifi_start_driver(siwifi_hw);
    clear_bit(SIWIFI_DEV_STACK_RESTARTING, &siwifi_hw->drv_flags);
    siwifi_hw->dying_gasp_valid = false;
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
    siwifi_hw->debugfs.recoverying = false;
#endif
    rtnl_unlock();
    return;

REPROBE:
    siwifi_reprobe_driver(siwifi_hw);
    rtnl_unlock();
    return;
}
#endif

static struct cfg80211_ops siwifi_cfg80211_ops = {
    .add_virtual_intf = siwifi_cfg80211_add_iface,
    .del_virtual_intf = siwifi_cfg80211_del_iface,
    .change_virtual_intf = siwifi_cfg80211_change_iface,
    .scan = siwifi_cfg80211_scan,
    .connect = siwifi_cfg80211_connect,
    .disconnect = siwifi_cfg80211_disconnect,
    .add_key = siwifi_cfg80211_add_key,
    .get_key = siwifi_cfg80211_get_key,
    .del_key = siwifi_cfg80211_del_key,
    .set_default_key = siwifi_cfg80211_set_default_key,
    .set_default_mgmt_key = siwifi_cfg80211_set_default_mgmt_key,
    .add_station = siwifi_cfg80211_add_station,
    .del_station = siwifi_cfg80211_del_station,
    .change_station = siwifi_cfg80211_change_station,
    .mgmt_tx = siwifi_cfg80211_mgmt_tx,
    .start_ap = siwifi_cfg80211_start_ap,
    .change_beacon = siwifi_cfg80211_change_beacon,
    .stop_ap = siwifi_cfg80211_stop_ap,
    .set_monitor_channel = siwifi_cfg80211_set_monitor_channel,
    .probe_client = siwifi_cfg80211_probe_client,
    .mgmt_frame_register = siwifi_cfg80211_mgmt_frame_register,
    .set_wiphy_params = siwifi_cfg80211_set_wiphy_params,
    .set_txq_params = siwifi_cfg80211_set_txq_params,
    .set_tx_power = siwifi_cfg80211_set_tx_power,
    .get_tx_power = siwifi_cfg80211_get_tx_power,
//    .set_power_mgmt = siwifi_cfg80211_set_power_mgmt,
    .get_station = siwifi_cfg80211_get_station,
    .dump_station = siwifi_cfg80211_dump_station,
    .remain_on_channel = siwifi_cfg80211_remain_on_channel,
    .cancel_remain_on_channel = siwifi_cfg80211_cancel_remain_on_channel,
    .dump_survey = siwifi_cfg80211_dump_survey,
    .get_channel = siwifi_cfg80211_get_channel,
    .start_radar_detection = siwifi_cfg80211_start_radar_detection,
    .update_ft_ies = siwifi_cfg80211_update_ft_ies,
    .set_cqm_rssi_config = siwifi_cfg80211_set_cqm_rssi_config,
    .channel_switch = siwifi_cfg80211_channel_switch,
#if MY_LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
    .tdls_channel_switch = siwifi_cfg80211_tdls_channel_switch,
    .tdls_cancel_channel_switch = siwifi_cfg80211_tdls_cancel_channel_switch,
#endif
    .tdls_mgmt = siwifi_cfg80211_tdls_mgmt,
    .tdls_oper = siwifi_cfg80211_tdls_oper,
    .change_bss = siwifi_cfg80211_change_bss,
#if MY_LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
    .external_auth = siwifi_cfg80211_external_auth,
#endif
};


/*********************************************************************
 * Init/Exit functions
 *********************************************************************/
static void siwifi_wdev_unregister(struct siwifi_hw *siwifi_hw)
{
    struct siwifi_vif *siwifi_vif, *tmp;

    rtnl_lock();
    list_for_each_entry_safe(siwifi_vif, tmp, &siwifi_hw->vifs, list) {
        siwifi_cfg80211_del_iface(siwifi_hw->wiphy, &siwifi_vif->wdev);
    }
    rtnl_unlock();
}

static void siwifi_set_vers(struct siwifi_hw *siwifi_hw)
{
    u32 vers = siwifi_hw->version_cfm.version_lmac;

    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

    snprintf(siwifi_hw->wiphy->fw_version,
             sizeof(siwifi_hw->wiphy->fw_version), "%d.%d.%d.%d",
             (vers & (0xff << 24)) >> 24, (vers & (0xff << 16)) >> 16,
             (vers & (0xff <<  8)) >>  8, (vers & (0xff <<  0)) >>  0);
}

static void siwifi_reg_notifier(struct wiphy *wiphy,
                              struct regulatory_request *request)
{
    struct siwifi_hw *siwifi_hw = wiphy_priv(wiphy);

    // For now trust all initiator
    siwifi_radar_set_domain(&siwifi_hw->radar, request->dfs_region);
    siwifi_send_me_chan_config_req(siwifi_hw);
}

static void siwifi_enable_mesh(struct siwifi_hw *siwifi_hw)
{
    struct wiphy *wiphy = siwifi_hw->wiphy;

    if (!siwifi_hw->mod_params->mesh)
        return;

    siwifi_cfg80211_ops.get_station = siwifi_cfg80211_get_station;
    siwifi_cfg80211_ops.dump_station = siwifi_cfg80211_dump_station;
    siwifi_cfg80211_ops.add_mpath = siwifi_cfg80211_add_mpath;
    siwifi_cfg80211_ops.del_mpath = siwifi_cfg80211_del_mpath;
    siwifi_cfg80211_ops.change_mpath = siwifi_cfg80211_change_mpath;
    siwifi_cfg80211_ops.get_mpath = siwifi_cfg80211_get_mpath;
    siwifi_cfg80211_ops.dump_mpath = siwifi_cfg80211_dump_mpath;
#if MY_LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
    siwifi_cfg80211_ops.get_mpp = siwifi_cfg80211_get_mpp;
    siwifi_cfg80211_ops.dump_mpp = siwifi_cfg80211_dump_mpp;
#endif
    siwifi_cfg80211_ops.get_mesh_config = siwifi_cfg80211_get_mesh_config;
    siwifi_cfg80211_ops.update_mesh_config = siwifi_cfg80211_update_mesh_config;
    siwifi_cfg80211_ops.join_mesh = siwifi_cfg80211_join_mesh;
    siwifi_cfg80211_ops.leave_mesh = siwifi_cfg80211_leave_mesh;

    wiphy->flags |= (WIPHY_FLAG_MESH_AUTH | WIPHY_FLAG_IBSS_RSN);
    wiphy->features |= NL80211_FEATURE_USERSPACE_MPM;
    wiphy->interface_modes |= BIT(NL80211_IFTYPE_MESH_POINT);

    siwifi_limits[0].types |= BIT(NL80211_IFTYPE_MESH_POINT);
    siwifi_limits_dfs[0].types |= BIT(NL80211_IFTYPE_MESH_POINT);
}


static void inc_sf_mac_addr(char *mac, int inc)
{
    unsigned int mac_calc[ETH_ALEN] = {0};
    int i = 0;
    for (i = 0; i < ETH_ALEN; i++) {
        mac_calc[i] = (unsigned int)(mac[i]) & 0xFF;
    }
    mac_calc[ETH_ALEN - 1] += inc;
    mac_calc[ETH_ALEN - 2] += ((mac_calc[ETH_ALEN - 1] & (0x100)) >> 8);
    mac[ETH_ALEN - 1] = mac_calc[ETH_ALEN - 1] & (0xff);

    mac_calc[ETH_ALEN - 3] += ((mac_calc[ETH_ALEN - 2] & (0x100)) >> 8);
    mac[ETH_ALEN - 2] = mac_calc[ETH_ALEN - 2] & (0xff);
    mac[ETH_ALEN - 3] = mac_calc[ETH_ALEN - 3] & (0xff);
    //the first 3 char is reserved
}

#ifdef CONFIG_SF_SKB_POOL
bool sfmac_skb_pool_alloc_fail(struct net_device *ndev, unsigned int size){
    unsigned long long free_mem;

    free_mem = global_zone_page_state(NR_FREE_PAGES) << (PAGE_SHIFT -10);
    if (free_mem > SIWIFI_RX_LOW_MEM_SIZE) {
        return 1;
    }
    return 0;
}
#endif

/**
 *
 */
int siwifi_cfg80211_init(struct siwifi_plat *siwifi_plat, void **platform_data)
{
    struct siwifi_hw *siwifi_hw;
    struct siwifi_conf_file init_conf;
    int ret = 0;
    struct wiphy *wiphy;
    struct wireless_dev *wdev;
    int i;
#ifdef CONFIG_ENABLE_DIGGAINTABLE
    struct phy_aetnensis_cfg_tag *phy_tag;
#endif
    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

    /* create a new wiphy for use with cfg80211 */
    wiphy = wiphy_new(&siwifi_cfg80211_ops, sizeof(struct siwifi_hw));

    if (!wiphy) {
        dev_err(siwifi_platform_get_dev(siwifi_plat), "Failed to create new wiphy\n");
        ret = -ENOMEM;
        goto err_out;
    }

    siwifi_hw = wiphy_priv(wiphy);
    siwifi_hw->wiphy = wiphy;
    siwifi_hw->plat = siwifi_plat;
    siwifi_hw->dev = siwifi_platform_get_dev(siwifi_plat);
    siwifi_hw->mod_params = (struct siwifi_mod_params *)of_device_get_match_data(siwifi_platform_get_dev(siwifi_plat));
    siwifi_hw->tcp_pacing_shift = 7;
    siwifi_hw->task_max_process_time = 0;
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
#ifdef CONFIG_SIWIFI_COOLING_TEMP
    siwifi_hw->temp_disable = 0;
#endif
    siwifi_hw->enable_rc = 0;
    siwifi_hw->tx_ctrl = 0;
    siwifi_hw->rx_ctrl = 0;
    siwifi_hw->debug_get_survey = 0;
    siwifi_hw->disable_wmm_edca = 1;
    siwifi_hw->disable_cca_channel_switch = 1;
    siwifi_hw->amsdu_threshold = AMSDU_THRESHOLD_M;
    siwifi_hw->wmm_edca_interval = EDCA_CHECK_INTERVAL;
    siwifi_hw->wmm_edca_counter_drop = EDCA_WMM_COUNTER;
    siwifi_hw->wmm_edca_pkt_threshold = EDCA_BE_THRESHOLD;
    siwifi_hw->wmm_debug_enable = 0;
    siwifi_hw->amsdu_nb_disable = 0;
    siwifi_hw->amsdu_nb_percent = 10;
    siwifi_hw->amsdu_nb_cleanup = 2;
    siwifi_hw->amsdu_nb_threshold = 2000;
#endif
    siwifi_hw->scan_timeout = 5000;
    siwifi_hw->enable_dbg_sta_conn = 0;
    siwifi_hw->atf.enable = 0;
#ifdef CONFIG_SIWIFI_PROCFS
    siwifi_hw->procfsdir = proc_mkdir(siwifi_hw->mod_params->is_hb ? "hb" : "lb", NULL);
#endif
    siwifi_hw->rx_skb_alloc_fail_cnt = 0;
    siwifi_trace_init(siwifi_hw);
#ifdef CONFIG_BRIDGE_ACCELERATE
    if(siwifi_hw->mod_params->is_hb){
        siwifi_device_traffic_init();
        siwifi_hook_xmit_reigster();
    }
#endif
    /* set device pointer for wiphy */
    set_wiphy_dev(wiphy, siwifi_hw->dev);

    if ((ret = siwifi_parse_configfile(siwifi_hw, SIWIFI_CONFIG_FW_NAME, &init_conf))) {
        wiphy_err(wiphy, "siwifi_parse_configfile failed\n");
        goto err_config;
    }

    //get mac addres from disk
#ifdef CONFIG_SFAX8_FACTORY_READ
    if (!sf_get_value_from_factory(
                ((1 << siwifi_hw->mod_params->is_hb) == LB_MODULE) ? READ_WIFI_LB_MAC_ADDRESS : READ_WIFI_HB_MAC_ADDRESS,
                init_conf.mac_addr, 6)) {
        printk("get wifi address from factory\n");
    } else if (eth_platform_get_mac_address(siwifi_hw->dev, init_conf.mac_addr) >= 0) {
#else
    if (eth_platform_get_mac_address(siwifi_hw->dev, init_conf.mac_addr) >= 0) {
#endif
        //init_conf.mac_addr[ETH_ALEN - 1] = (init_conf.mac_addr[ETH_ALEN - 1] + (((1 << siwifi_hw->mod_params->is_hb) == LB_MODULE) ? 2 : 3)) & 0xFF;
        inc_sf_mac_addr(&init_conf.mac_addr[0], (((1 << siwifi_hw->mod_params->is_hb) == LB_MODULE) ? 2 : 6));
    } else {
        //use random address if fail get mac address
        eth_random_addr(init_conf.mac_addr);
    }

    siwifi_hw->sec_phy_chan.band = NL80211_BAND_5GHZ;
    siwifi_hw->sec_phy_chan.type = PHY_CHNL_BW_20;
    siwifi_hw->sec_phy_chan.prim20_freq = 5500;
    siwifi_hw->sec_phy_chan.center_freq1 = 5500;
    siwifi_hw->sec_phy_chan.center_freq2 = 0;
    siwifi_hw->vif_started = 0;
    siwifi_hw->monitor_vif = SIWIFI_INVALID_VIF;
    siwifi_hw->adding_sta = false;

    siwifi_hw->scan_ie.addr = NULL;
#ifdef CONFIG_SIWIFI_TEMPERATURE_CONTROL
    siwifi_parse_temperature_control_configfile(siwifi_hw, SIWIFI_TEMPERATURE_CONTROL_NAME);
    siwifi_hw->temp_ctl.temp_ctl_level = 0;
    siwifi_hw->temp_ctl.last_change_temp = 0;
#endif

    for (i = 0; i < NX_VIRT_DEV_MAX + NX_REMOTE_STA_MAX; i++)
        siwifi_hw->avail_idx_map |= BIT(i);

#ifdef CONFIG_SF19A28_WIFI_LED
    ret = of_get_named_gpio(siwifi_hw->dev->of_node, LED_GPIO_LABEL, 0);
    if (ret < 0) {
        printk(KERN_INFO "info: gpio get failed from dts!<<<<<<<<<<<<<<<<<<<<<<<\n");
    } else {
        siwifi_local_led_init(siwifi_hw);
        ret = siwifi_led_init(siwifi_hw);
        if (ret)
            printk(KERN_ERR "error: siwifi_led_init failed!<<<<<<<<<<<<<<<<<<<<<<<\n");
        else
            printk(KERN_INFO "info: siwifi_led_init successful!<<<<<<<<<<<<<<<<<<<<<<<\n");
    }
#endif
    // We did not want to allocate two much txq, that will create lots of skb cache when system
    // throughput become slow!!
    if (siwifi_hw->mod_params->tx_queue_num > (IEEE80211_NUM_ACS + 1))
        siwifi_hw->mod_params->tx_queue_num = IEEE80211_NUM_ACS + 1;
    else if (siwifi_hw->mod_params->tx_queue_num == 0)
        siwifi_hw->mod_params->tx_queue_num = 1;
#ifdef CONFIG_WIFI_LITE_MEMORY
    /* For Lite memory platform, we have two reason to make ndevq size to 1:
     1, multi queue take lots of memory, but can not be run in parallel totally, because in driver xmit
         function use global lock to make the xmit calls in serial
     2, tx memory limit and cnt limit are global, if use multi txq, it's better to have tx_memory_limit and
        tx_cnt_limit per txq*/
    siwifi_hw->tx_queue_num = 1;
#else
    siwifi_hw->tx_queue_num = siwifi_hw->mod_params->tx_queue_num;
#endif

    // define if lm ctrl limit is active or not
    siwifi_hw->lm_ctrl_enable = true;
    // Init hwq
    siwifi_hwq_init(siwifi_hw);
    // Init stuck check list
    INIT_LIST_HEAD(&siwifi_hw->stuck_check_list);

    for (i = 0; i < NX_NB_TXQ; i++) {
        siwifi_hw->txq[i].idx = TXQ_INACTIVE;
    }

#ifdef CONFIG_WIFI_RX_THREAD
    siwifi_hw->rx_kthread = NULL;
    spin_lock_init(&siwifi_hw->rx_lock);
    INIT_LIST_HEAD(&siwifi_hw->rx_skbs);
    atomic_set(&siwifi_hw->rx_pending_count, 0);
    init_waitqueue_head(&siwifi_hw->rx_wq);
    if (siwifi_rx_thread_setup(siwifi_hw)) {
        printk("siwifi_rx_thread_setup failed\n");
        goto err_config;
    }
#endif

    siwifi_mu_group_init(siwifi_hw);

    /* Initialize RoC element pointer to NULL, indicate that RoC can be started */
    siwifi_hw->roc_elem = NULL;
    /* Cookie can not be 0 */
    siwifi_hw->roc_cookie_cnt = 1;

    memcpy(wiphy->perm_addr, init_conf.mac_addr, ETH_ALEN);
    if (siwifi_hw->mod_params->addr_maskall) {
        memset(&wiphy->addr_mask[0], 0xFF, ETH_ALEN);
    } else {
        /* ATM - NX_VIRT_DEV_MAX is a power of 2
         *     - mac80211 doesn't handle non-contiguous masks */
        //if (!WARN_ON(NX_VIRT_DEV_MAX & (NX_VIRT_DEV_MAX - 1)))
        //    *(u32 *)(wiphy->addr_mask + 2) = cpu_to_be32(NX_VIRT_DEV_MAX - 1);
        // Franklin : 2021.4.25
        // I did not see the reason why we make vif num a power of 2
        *(u32 *)(wiphy->addr_mask + 2) = cpu_to_be32(siwifi_wiphy_addmask[NX_VIRT_DEV_MAX]);
    }

    wiphy->mgmt_stypes = siwifi_default_mgmt_stypes;

    wiphy->bands[NL80211_BAND_2GHZ] = &siwifi_band_2GHz;
    wiphy->bands[NL80211_BAND_5GHZ] = &siwifi_band_5GHz;
    wiphy->interface_modes =
        BIT(NL80211_IFTYPE_STATION)     |
        BIT(NL80211_IFTYPE_AP)          |
        BIT(NL80211_IFTYPE_AP_VLAN)     |
        BIT(NL80211_IFTYPE_P2P_CLIENT)  |
        BIT(NL80211_IFTYPE_P2P_GO)      |
        BIT(NL80211_IFTYPE_MONITOR);
    wiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL |
        WIPHY_FLAG_HAS_CHANNEL_SWITCH |
        WIPHY_FLAG_4ADDR_STATION |
        WIPHY_FLAG_4ADDR_AP;

    wiphy->max_num_csa_counters = BCN_MAX_CSA_CPT;

    wiphy->max_remain_on_channel_duration = siwifi_hw->mod_params->roc_dur_max;

    wiphy->features |= NL80211_FEATURE_NEED_OBSS_SCAN |
        NL80211_FEATURE_SK_TX_STATUS |
        NL80211_FEATURE_VIF_TXPOWER |
        NL80211_FEATURE_ACTIVE_MONITOR |
        NL80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE;

#if MY_LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
    wiphy->features |= NL80211_FEATURE_SAE;
#endif

    //if (siwifi_mod_params.tdls)
    //    /* TDLS support */
    //    wiphy->features |= NL80211_FEATURE_TDLS_CHANNEL_SWITCH;

    wiphy->iface_combinations   = siwifi_combinations;
    /* -1 not to include combination with radar detection, will be re-added in
       siwifi_handle_dynparams if supported */
    wiphy->n_iface_combinations = ARRAY_SIZE(siwifi_combinations) - 1;
    wiphy->reg_notifier = siwifi_reg_notifier;

    wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;

    wiphy->cipher_suites = cipher_suites;
    wiphy->n_cipher_suites = ARRAY_SIZE(cipher_suites) - NB_RESERVED_CIPHER;

    siwifi_hw->ext_capa[0] = WLAN_EXT_CAPA1_EXT_CHANNEL_SWITCHING;
    siwifi_hw->ext_capa[7] = WLAN_EXT_CAPA8_OPMODE_NOTIF;

    wiphy->extended_capabilities = siwifi_hw->ext_capa;
    wiphy->extended_capabilities_mask = siwifi_hw->ext_capa;
    wiphy->extended_capabilities_len = ARRAY_SIZE(siwifi_hw->ext_capa);

    //tasklet_init(&siwifi_hw->task, siwifi_task, (unsigned long)siwifi_hw);

    INIT_LIST_HEAD(&siwifi_hw->vifs);

    mutex_init(&siwifi_hw->mutex);
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
    mutex_init(&siwifi_hw->dbgdump_elem.mutex);
#endif
    spin_lock_init(&siwifi_hw->tx_lock);
    spin_lock_init(&siwifi_hw->cb_lock);
#ifdef LOCAL_RECOVERY
    INIT_WORK(&siwifi_hw->restart_work, siwifi_restart_driver);
#endif
#ifdef CONFIG_SF_SKB_POOL
    // this skb_size equal bufsz add  increate size in __netdev_alloc_skb
	//  len += NET_SKB_PAD; len = SKB_DATA_ALIGN(len);  len += SKB_DATA_ALIGN(sizeof(struct skb_shared_info));
    siwifi_hw->skb_pool_dev_param = skb_pool_init((((1 << siwifi_hw->mod_params->is_hb) == LB_MODULE) ? SKB_POOL_WIFI_LB_ID : SKB_POOL_WIFI_HB_ID),
            (((1 << siwifi_hw->mod_params->is_hb) == LB_MODULE) ? 500 : 700),
            MAX_WIFI_POOL_SKB_RAW_SIZE);

	if(siwifi_hw->skb_pool_dev_param){
		siwifi_hw->skb_pool_dev_param->enable_skb_pool = 0;
		// return ture means keep alloc ,false means abort alloc
		siwifi_hw->skb_pool_dev_param->device_skb_pool_alloc_fail = sfmac_skb_pool_alloc_fail;
		siwifi_hw->skb_pool_dev_param->use_skb_pool = 1;
	}
#endif
    if ((ret = siwifi_platform_on(siwifi_hw))) {
        siwifi_reprobe_driver(siwifi_hw);
        goto err_platon;
    }
    /* For dying gasp information, we need to allocate here
        We dont't want to reallocate this during restarting, because
        we need dying gasp information to do recovering
     */
    if (siwifi_ipc_elem_var_allocs(siwifi_hw, &siwifi_hw->dying_gasp_info_elem,
                sizeof(struct mm_dying_gasp), DMA_FROM_DEVICE,
                NULL, NULL, ipc_host_dying_gasp_addr_push, NULL))
    {
        printk("can not allocate dying gasp information elem\n");
        goto err_lmac_reqs;
    }

    /* Reset FW */
    if ((ret = siwifi_send_reset(siwifi_hw)))
        goto err_lmac_reqs;
    if ((ret = siwifi_send_version_req(siwifi_hw, &siwifi_hw->version_cfm)))
        goto err_lmac_reqs;
    siwifi_set_vers(siwifi_hw);

    if ((ret = siwifi_handle_dynparams(siwifi_hw, siwifi_hw->wiphy)))
        goto err_lmac_reqs;

    siwifi_enable_mesh(siwifi_hw);
    siwifi_radar_detection_init(&siwifi_hw->radar);

    /* Set parameters to firmware */
    siwifi_send_me_config_req(siwifi_hw);

    if ((ret = wiphy_register(wiphy))) {
        wiphy_err(wiphy, "Could not register wiphy device\n");
        goto err_register_wiphy;
    }

    /* Set channel parameters to firmware (must be done after WiPHY registration) */
    siwifi_send_me_chan_config_req(siwifi_hw);

    if (platform_data)
        *platform_data = siwifi_hw;

#ifdef CONFIG_HEART_BEAT
    siwifi_hw->recovery_hb_num = 0;
    INIT_DELAYED_WORK(&siwifi_hw->heart_work, heart_beat_handler);
    schedule_delayed_work(&siwifi_hw->heart_work, MSECS(SIWIFI_HEART_BEAT_TIME_MS));
#endif

    INIT_DELAYED_WORK(&siwifi_hw->txq_stat_work, txq_stat_handler);
    schedule_delayed_work(&siwifi_hw->txq_stat_work, MSECS(SIWIFI_TXQ_STAT_TIME_MS));

#ifdef CONFIG_ENABLE_DIGGAINTABLE
    phy_tag = (struct phy_aetnensis_cfg_tag *)&siwifi_hw->phy_config;
    if(!(phy_tag->flag & RF_EXTERNAL_PA_ENABLE))
        siwifi_parse_digtable_configfile(siwifi_hw,SIWIFI_DIG_GAIN_TABLE_NAME);
    else
        siwifi_parse_digtable_configfile(siwifi_hw,SIWIFI_DIG_GAIN_TABLE_EXPA_NAME);
#endif

#ifdef CONFIG_ENABLE_TXPOWERTABLE
    phy_tag = (struct phy_aetnensis_cfg_tag *)&siwifi_hw->phy_config;
    if(!(phy_tag->flag & RF_EXTERNAL_PA_ENABLE))
        siwifi_parse_txpower_gain_table_configfile(siwifi_hw,0);
    else
        siwifi_parse_txpower_gain_table_configfile(siwifi_hw,1);
#endif

#ifdef CONFIG_ENABLE_RFGAINTABLE
    siwifi_parse_rf_gain_table_configfile(siwifi_hw);
#endif

#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
    if ((ret = siwifi_dbgfs_register(siwifi_hw, "siwifi"))) {
        wiphy_err(wiphy, "Failed to register debugfs entries");
        goto err_debugfs;
    }
#endif

    rtnl_lock();

    /* Add an initial station interface */
    wdev = siwifi_interface_add(siwifi_hw, "wlan%d", NET_NAME_UNKNOWN,
                              NL80211_IFTYPE_STATION, NULL);

    rtnl_unlock();

    if (!wdev) {
        wiphy_err(wiphy, "Failed to instantiate a network device\n");
        ret = -ENOMEM;
        goto err_add_interface;
    }

    wiphy_info(wiphy, "New interface create %s", wdev->netdev->name);
    siwifi_set_debug_mem_ctx((void *)siwifi_hw);
#ifdef CONFIG_SIWIFI_ACS
    siwifi_set_vendor_commands(wiphy);
#endif

#ifdef CONFIG_SIWIFI_EASYMESH
    /* Timer is used to notify the easymesh driver of the connection information to sta, such as RSSI, bitrate etc. */
    siwifi_hw->sta_timer_info.timeout = 1;
    siwifi_hw->sta_timer_info.siwifi_hw = siwifi_hw;
    timer_setup(&siwifi_hw->sta_timer_info.timer, siwifi_notify_sta_info_timer_callback, 0);
    // mod_timer(&siwifi_hw->sta_timer_info.timer, jiffies + msecs_to_jiffies(5 * 1000));
#endif /* CONFIG_SIWIFI_EASYMESH */

#ifdef CONFIG_SIWIFI_ACS_INTERNAL
    INIT_WORK(&siwifi_hw->csa_work, siwifi_channel_ctxt_switch);
#endif
    return 0;

err_add_interface:
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
err_debugfs:
#endif
#ifdef CONFIG_HEART_BEAT
    cancel_delayed_work_sync(&siwifi_hw->heart_work);
#endif
    cancel_delayed_work_sync(&siwifi_hw->txq_stat_work);
    wiphy_unregister(siwifi_hw->wiphy);
err_register_wiphy:
err_lmac_reqs:
    siwifi_ipc_elem_var_deallocs(siwifi_hw, &siwifi_hw->dying_gasp_info_elem);

#ifdef CONFIG_SIWIFI_FW_TRACE
    siwifi_fw_trace_dump(siwifi_hw);
#endif
    siwifi_platform_off(siwifi_hw);
#ifdef CONFIG_PRIV_RX_BUFFER_POOL
    siwifi_deinit_private_rxskbs();
#endif
err_platon:
#ifdef CONFIG_WIFI_RX_THREAD
    siwifi_rx_thread_release(siwifi_hw);
#endif
err_config:
    wiphy_free(wiphy);
err_out:
    return ret;
}

/**
 *
 */
void siwifi_cfg80211_deinit(struct siwifi_hw *siwifi_hw)
{
    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

#ifdef CONFIG_WIFI_RX_THREAD
    siwifi_rx_thread_release(siwifi_hw);
#endif
#ifdef CONFIG_SIWIFI_EASYMESH
    del_timer(&siwifi_hw->sta_timer_info.timer);
#endif /* CONFIG_SIWIFI_EASYMESH */
#ifdef CONFIG_HEART_BEAT
    cancel_delayed_work_sync(&siwifi_hw->heart_work);
#endif
    cancel_delayed_work_sync(&siwifi_hw->txq_stat_work);
#ifdef CONFIG_SF19A28_WIFI_LED
    if (of_get_named_gpio(siwifi_hw->dev->of_node, LED_GPIO_LABEL, 0) >= 0)
        siwifi_led_deinit(siwifi_hw);
#endif
#ifdef CONFIG_SIWIFI_IGMP
    siwifi_mcg_free(NULL, siwifi_hw, FREE_MCG_ALL);
#endif
    if (siwifi_hw->assoc_req_insert_info)
        siwifi_kfree(siwifi_hw->assoc_req_insert_info);
#if defined (CONFIG_SIWIFI_DEBUGFS) || defined (CONFIG_SIWIFI_PROCFS)
    siwifi_dbgfs_unregister(siwifi_hw);
#endif

    siwifi_wdev_unregister(siwifi_hw);
    wiphy_unregister(siwifi_hw->wiphy);
    siwifi_radar_detection_deinit(&siwifi_hw->radar);
    siwifi_ipc_elem_var_deallocs(siwifi_hw, &siwifi_hw->dying_gasp_info_elem);
    siwifi_platform_off(siwifi_hw);
#ifdef CONFIG_PRIV_RX_BUFFER_POOL
    siwifi_deinit_private_rxskbs();
#endif
    siwifi_trace_deinit(siwifi_hw);
    wiphy_free(siwifi_hw->wiphy);
    siwifi_set_debug_mem_ctx(NULL);
    SIWIFI_DBG(SIWIFI_FN_EXIT_STR);
}

/**
 *
 */
static int __init siwifi_mod_init(void)
{
#ifdef CONFIG_SIWIFI_REPEATER
    if (repeater_init())
       printk("repeater_init failed\n");
#endif

#ifdef CONFIG_SIWIFI_AMSDUS_TX
    traffic_info_init();
#endif
#ifdef CONFIG_SIWIFI_CACHE_ALLOC
    /* Create cache to allocate sw_txhdr */
    sw_txhdr_cache = KMEM_CACHE(siwifi_sw_txhdr, 0);
    if (!sw_txhdr_cache) {
        pr_err("%s: Cannot allocate cache for sw TX header\n", __func__);
        return -ENOMEM;
    }
#endif
    return siwifi_platform_register_drv();
}

/**
 *
 */
static void __exit siwifi_mod_exit(void)
{
    SIWIFI_DBG(SIWIFI_FN_ENTRY_STR);

#ifdef CONFIG_SIWIFI_REPEATER
    repeater_exit();
#endif
    siwifi_platform_unregister_drv();
#ifdef CONFIG_SIWIFI_CACHE_ALLOC
    kmem_cache_destroy(sw_txhdr_cache);
#endif
}

module_init(siwifi_mod_init);
module_exit(siwifi_mod_exit);

MODULE_FIRMWARE(SIWIFI_CONFIG_FW_NAME);

MODULE_DESCRIPTION(SF_DRV_DESCRIPTION);
MODULE_VERSION(SIWIFI_VERS_MOD);
MODULE_AUTHOR(SF_DRV_COPYRIGHT " " SF_DRV_AUTHOR);
MODULE_LICENSE("GPL");
