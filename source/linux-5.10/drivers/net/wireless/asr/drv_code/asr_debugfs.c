/**
 ****************************************************************************************
 *
 * @file asr_debugfs.c
 *
 * @brief RX function definitions
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include <linux/sort.h>

#include "asr_debugfs.h"
#include "asr_msg_tx.h"
//#include "asr_radar.h"
#include "asr_tx.h"

static ssize_t asr_dbgfs_stats_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char *buf;
	int ret;
	int i, skipped;
#ifdef CONFIG_ASR_SPLIT_TX_BUF
	int per;
#endif
	ssize_t read;
	int bufsz =
	    (NX_TXQ_CNT) * 20 + (ARRAY_SIZE(priv->stats.amsdus_rx) + 1) * 40 + (ARRAY_SIZE(priv->stats.ampdus_tx) * 30);

	if (*ppos)
		return 0;

	buf = kmalloc(bufsz, GFP_ATOMIC);
	if (buf == NULL)
		return 0;

	ret = scnprintf(buf, bufsz, "TXQs CFM balances ");
	for (i = 0; i < NX_TXQ_CNT; i++)
		ret += scnprintf(&buf[ret], bufsz - ret, "  [%1d]:%3d", i, priv->stats.cfm_balance[i]);

	ret += scnprintf(&buf[ret], bufsz - ret, "\n");

#ifdef CONFIG_ASR_SPLIT_TX_BUF
	ret += scnprintf(&buf[ret], bufsz - ret, "\nAMSDU[len]       done         failed   received\n");
	for (i = skipped = 0; i < NX_TX_PAYLOAD_MAX; i++) {
		if (priv->stats.amsdus[i].done) {
			per = DIV_ROUND_UP((priv->stats.amsdus[i].failed) * 100, priv->stats.amsdus[i].done);
		} else if (priv->stats.amsdus_rx[i]) {
			per = 0;
		} else {
			per = 0;
			skipped = 1;
			continue;
		}
		if (skipped) {
			ret += scnprintf(&buf[ret], bufsz - ret, "   ...\n");
			skipped = 0;
		}

		ret += scnprintf(&buf[ret], bufsz - ret,
				 "   [%2d]    %10d %8d(%3d%%) %10d\n",
				 i ? i + 1 : i, priv->stats.amsdus[i].done,
				 priv->stats.amsdus[i].failed, per, priv->stats.amsdus_rx[i]);
	}

	for (; i < ARRAY_SIZE(priv->stats.amsdus_rx); i++) {
		if (!priv->stats.amsdus_rx[i]) {
			skipped = 1;
			continue;
		}
		if (skipped) {
			ret += scnprintf(&buf[ret], bufsz - ret, "   ...\n");
			skipped = 0;
		}

		ret += scnprintf(&buf[ret], bufsz - ret,
				 "   [%2d]                              %10d\n", i + 1, priv->stats.amsdus_rx[i]);
	}
#else
	ret += scnprintf(&buf[ret], bufsz - ret, "\nAMSDU[len]   received\n");
	for (i = skipped = 0; i < ARRAY_SIZE(priv->stats.amsdus_rx); i++) {
		if (!priv->stats.amsdus_rx[i]) {
			skipped = 1;
			continue;
		}
		if (skipped) {
			ret += scnprintf(&buf[ret], bufsz - ret, "   ...\n");
			skipped = 0;
		}

		ret += scnprintf(&buf[ret], bufsz - ret, "   [%2d]    %10d\n", i + 1, priv->stats.amsdus_rx[i]);
	}

#endif /* CONFIG_ASR_SPLIT_TX_BUF */

	ret += scnprintf(&buf[ret], bufsz - ret, "\nAMPDU[len]     done  received\n");
	for (i = skipped = 0; i < ARRAY_SIZE(priv->stats.ampdus_tx); i++) {
		if (!priv->stats.ampdus_tx[i] && !priv->stats.ampdus_rx[i]) {
			skipped = 1;
			continue;
		}
		if (skipped) {
			ret += scnprintf(&buf[ret], bufsz - ret, "    ...\n");
			skipped = 0;
		}

		ret += scnprintf(&buf[ret], bufsz - ret,
				 "   [%2d]   %9d %9d\n", i ? i + 1 : i,
				 priv->stats.ampdus_tx[i], priv->stats.ampdus_rx[i]);
	}

	ret += scnprintf(&buf[ret], bufsz - ret, "#mpdu missed        %9d\n", priv->stats.ampdus_rx_miss);
	read = simple_read_from_buffer(user_buf, count, ppos, buf, ret);

	kfree(buf);

	return read;
}

static ssize_t asr_dbgfs_stats_write(struct file *file, const char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;

	/* Prevent from interrupt preemption as these statistics are updated under
	 * interrupt */
	spin_lock_bh(&priv->tx_lock);

	memset(&priv->stats, 0, sizeof(priv->stats));

	spin_unlock_bh(&priv->tx_lock);

	return count;
}

DEBUGFS_READ_WRITE_FILE_OPS(stats);

#define TXQ_STA_PREF "tid|"
#define TXQ_STA_PREF_FMT "%3d|"

#define TXQ_VIF_PREF "type|"
#define TXQ_VIF_PREF_FMT "%4s|"

#define TXQ_HDR "idx| status|credit|ready|retry"
#define TXQ_HDR_FMT "%3d|%s%s%s%s%s%s%s|%6d|%5d|%5d"

#ifdef CONFIG_ASR_AMSDUS_TX
#define TXQ_HDR_SUFF "|amsdu"
#define TXQ_HDR_SUFF_FMT "|%5d"
#else
#define TXQ_HDR_SUFF ""
#define TXQ_HDR_SUF_FMT ""
#endif /* CONFIG_ASR_AMSDUS_TX */

#define TXQ_HDR_MAX_LEN (sizeof(TXQ_STA_PREF) + sizeof(TXQ_HDR) + sizeof(TXQ_HDR_SUFF) + 1)

#define PS_HDR  "Legacy PS: ready=%d, sp=%d / UAPSD: ready=%d, sp=%d"
#define PS_HDR_LEGACY "Legacy PS: ready=%d, sp=%d"
#define PS_HDR_UAPSD  "UAPSD: ready=%d, sp=%d"
#define PS_HDR_MAX_LEN  sizeof("Legacy PS: ready=xxx, sp=xxx / UAPSD: ready=xxx, sp=xxx\n")

#define STA_HDR "** STA %d (%pM)\n"
#define STA_HDR_MAX_LEN sizeof("- STA xx (xx:xx:xx:xx:xx:xx)\n") + PS_HDR_MAX_LEN

#define VIF_HDR "* VIF [%d] %s\n"
#define VIF_HDR_MAX_LEN sizeof(VIF_HDR) + IFNAMSIZ

#ifdef CONFIG_ASR_AMSDUS_TX
#define VIF_SEP "---------------------------------------\n"
#else /* ! CONFIG_ASR_AMSDUS_TX */
#define VIF_SEP "---------------------------------\n"
#endif /* CONFIG_ASR_AMSDUS_TX */

#define VIF_SEP_LEN sizeof(VIF_SEP)

#define CAPTION "status: L=in hwq list, F=stop full, P=stop sta PS, V=stop vif PS, C=stop channel, S=stop CSA, M=stop MU"
#define CAPTION_LEN sizeof(CAPTION)

#define STA_TXQ 0
#define VIF_TXQ 1

static int asr_dbgfs_txq(char *buf, size_t size, struct asr_txq *txq, int type, int tid, char *name)
{
	int res, idx = 0;

	if (type == STA_TXQ) {
		res = scnprintf(&buf[idx], size, TXQ_STA_PREF_FMT, tid);
		idx += res;
		size -= res;
	} else {
		res = scnprintf(&buf[idx], size, TXQ_VIF_PREF_FMT, name);
		idx += res;
		size -= res;
	}

	res = scnprintf(&buf[idx], size, TXQ_HDR_FMT, txq->idx,
			(txq->status & ASR_TXQ_IN_HWQ_LIST) ? "L" : " ",
			(txq->status & ASR_TXQ_STOP_FULL) ? "F" : " ",
			(txq->status & ASR_TXQ_STOP_STA_PS) ? "P" : " ",
			(txq->status & ASR_TXQ_STOP_VIF_PS) ? "V" : " ",
			(txq->status & ASR_TXQ_STOP_CHAN) ? "C" : " ", (txq->status & ASR_TXQ_STOP_CSA) ? "S" : " ",
#ifdef CONFIG_TWT
			(txq->status & ASR_TXQ_STOP_TWT) ? "T" : " ",
#else
			(txq->status & ASR_TXQ_STOP_MU_POS) ? "M" : " ",
#endif
			txq->credits, skb_queue_len(&txq->sk_list), txq->nb_retry);
	idx += res;
	size -= res;

#ifdef CONFIG_ASR_AMSDUS_TX
	if (type == STA_TXQ) {
		res = scnprintf(&buf[idx], size, TXQ_HDR_SUFF_FMT, txq->amsdu_len);
		idx += res;
		size -= res;
	}
#endif

	res = scnprintf(&buf[idx], size, "\n");
	idx += res;
	size -= res;

	return idx;
}

static int asr_dbgfs_txq_sta(char *buf, size_t size, struct asr_sta *asr_sta, struct asr_hw *asr_hw)
{
	int tid, res, idx = 0;
	struct asr_txq *txq;

	res = scnprintf(&buf[idx], size, "\n" STA_HDR, asr_sta->sta_idx, asr_sta->mac_addr);
	idx += res;
	size -= res;

	if (asr_sta->ps.active) {
		if (asr_sta->uapsd_tids && (asr_sta->uapsd_tids == ((1 << NX_NB_TXQ_PER_STA) - 1)))
			res = scnprintf(&buf[idx], size, PS_HDR_UAPSD "\n",
					asr_sta->ps.pkt_ready[UAPSD_ID], asr_sta->ps.sp_cnt[UAPSD_ID]);
		else if (asr_sta->uapsd_tids)
			res = scnprintf(&buf[idx], size, PS_HDR "\n",
					asr_sta->ps.pkt_ready[LEGACY_PS_ID],
					asr_sta->ps.sp_cnt[LEGACY_PS_ID],
					asr_sta->ps.pkt_ready[UAPSD_ID], asr_sta->ps.sp_cnt[UAPSD_ID]);
		else
			res = scnprintf(&buf[idx], size, PS_HDR_LEGACY "\n",
					asr_sta->ps.pkt_ready[LEGACY_PS_ID], asr_sta->ps.sp_cnt[LEGACY_PS_ID]);
		idx += res;
		size -= res;
	} else {
		res = scnprintf(&buf[idx], size, "\n");
		idx += res;
		size -= res;
	}

	res = scnprintf(&buf[idx], size, TXQ_STA_PREF TXQ_HDR TXQ_HDR_SUFF "\n");
	idx += res;
	size -= res;

	txq = asr_txq_sta_get(asr_sta, 0, NULL, asr_hw);

	for (tid = 0; tid < NX_NB_TXQ_PER_STA; tid++, txq++) {
		res = asr_dbgfs_txq(&buf[idx], size, txq, STA_TXQ, tid, NULL);
		idx += res;
		size -= res;
	}

	return idx;
}

static int asr_dbgfs_txq_vif(char *buf, size_t size, struct asr_vif *asr_vif, struct asr_hw *asr_hw)
{
	int res, idx = 0;
	struct asr_txq *txq = NULL;
	struct asr_sta *asr_sta = NULL;

	if (!asr_vif || !asr_vif->up || !asr_vif->ndev) {
		return idx;
	}

	res = scnprintf(&buf[idx], size, VIF_HDR, asr_vif->vif_index, asr_vif->ndev->name);
	idx += res;
	size -= res;

	if (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_AP ||
	    ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_P2P_GO || ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_MESH_POINT) {
		res = scnprintf(&buf[idx], size, TXQ_VIF_PREF TXQ_HDR "\n");
		idx += res;
		size -= res;
		txq = asr_txq_vif_get(asr_vif, NX_UNK_TXQ_TYPE, NULL);
		res = asr_dbgfs_txq(&buf[idx], size, txq, VIF_TXQ, 0, "UNK");
		idx += res;
		size -= res;
		txq = asr_txq_vif_get(asr_vif, NX_BCMC_TXQ_TYPE, NULL);
		res = asr_dbgfs_txq(&buf[idx], size, txq, VIF_TXQ, 0, "BCMC");
		idx += res;
		size -= res;
		asr_sta = &asr_hw->sta_table[asr_vif->ap.bcmc_index];
		if (asr_sta->ps.active) {
			res = scnprintf(&buf[idx], size, PS_HDR_LEGACY "\n",
					asr_sta->ps.sp_cnt[LEGACY_PS_ID], asr_sta->ps.sp_cnt[LEGACY_PS_ID]);
			idx += res;
			size -= res;
		} else {
			res = scnprintf(&buf[idx], size, "\n");
			idx += res;
			size -= res;
		}

		list_for_each_entry(asr_sta, &asr_vif->ap.sta_list, list) {
			res = asr_dbgfs_txq_sta(&buf[idx], size, asr_sta, asr_hw);
			idx += res;
			size -= res;
		}
	} else if (ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_STATION ||
		   ASR_VIF_TYPE(asr_vif) == NL80211_IFTYPE_P2P_CLIENT) {
		if (asr_vif->sta.ap) {
			res = asr_dbgfs_txq_sta(&buf[idx], size, asr_vif->sta.ap, asr_hw);
			idx += res;
			size -= res;
		}
	}

	return idx;
}

static ssize_t asr_dbgfs_txq_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *asr_hw = file->private_data;
	struct asr_vif *vif;
	char *buf;
	int idx, res;
	ssize_t read;
	size_t bufsz =
	    ((NX_VIRT_DEV_MAX * (VIF_HDR_MAX_LEN + 2 * VIF_SEP_LEN)) +
	     (NX_REMOTE_STA_MAX * STA_HDR_MAX_LEN) +
	     ((NX_REMOTE_STA_MAX + NX_VIRT_DEV_MAX + NX_NB_TXQ) * TXQ_HDR_MAX_LEN) + CAPTION_LEN);

	/* everything is read in one go */
	if (*ppos)
		return 0;

	bufsz = min_t(size_t, bufsz, count);
	buf = kmalloc(bufsz, GFP_ATOMIC);
	if (buf == NULL)
		return 0;

	bufsz--;
	idx = 0;

	res = scnprintf(&buf[idx], bufsz, CAPTION);
	idx += res;
	bufsz -= res;

	//mutex_lock(&asr_hw->tx_lock);
	list_for_each_entry(vif, &asr_hw->vifs, list) {
		res = scnprintf(&buf[idx], bufsz, "\n" VIF_SEP);
		idx += res;
		bufsz -= res;
		res = asr_dbgfs_txq_vif(&buf[idx], bufsz, vif, asr_hw);
		idx += res;
		bufsz -= res;
		res = scnprintf(&buf[idx], bufsz, VIF_SEP);
		idx += res;
		bufsz -= res;
	}
	//mutex_unlock(&asr_hw->tx_lock);

	read = simple_read_from_buffer(user_buf, count, ppos, buf, idx);
	kfree(buf);

	return read;
}

DEBUGFS_READ_FILE_OPS(txq);

static ssize_t asr_dbgfs_acsinfo_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	struct wiphy *wiphy = priv->wiphy;

	char buf[(SCAN_CHANNEL_MAX + 1) * 43];
	int survey_cnt = 0;
	int len = 0;
	int band, chan_cnt;
	int band_dix_max;

	mutex_lock(&priv->dbgdump_elem.mutex);

	len += scnprintf(buf, min_t(size_t, sizeof(buf) - 1, count), "FREQ    TIME(ms)    BUSY(ms)    NOISE(dBm)\n");

	band_dix_max = NL80211_BAND_2GHZ;
	for (band = NL80211_BAND_2GHZ; band <= band_dix_max; band++) {
		for (chan_cnt = 0; chan_cnt < wiphy->bands[band]->n_channels; chan_cnt++) {
			struct asr_survey_info *p_survey_info = &priv->survey[survey_cnt];
			struct ieee80211_channel *p_chan = &wiphy->bands[band]->channels[chan_cnt];

			if (p_survey_info->filled) {
				len +=
				    scnprintf(&buf[len],
					      min_t(size_t,
						    sizeof(buf) - len - 1,
						    count),
					      "%d    %03d         %03d         %d\n",
					      p_chan->center_freq,
					      p_survey_info->chan_time_ms,
					      p_survey_info->chan_time_busy_ms, p_survey_info->noise_dbm);
			} else {
				len +=
				    scnprintf(&buf[len],
					      min_t(size_t,
						    sizeof(buf) - len - 1,
						    count), "%d    NOT AVAILABLE\n", p_chan->center_freq);
			}

			survey_cnt++;
		}
	}

	mutex_unlock(&priv->dbgdump_elem.mutex);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

DEBUGFS_READ_FILE_OPS(acsinfo);

static ssize_t asr_dbgfs_fw_dbg_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	char help[] = "usage: [MOD:<ALL|KE|DBG|IPC|DMA|MM|TX|RX|PHY|HOST|FW>]* " "[DBG:<NONE|CRT|ERR|WRN|INF|VRB>]\n";

	return simple_read_from_buffer(user_buf, count, ppos, help, sizeof(help));
}

static ssize_t asr_dbgfs_fw_dbg_write(struct file *file, const char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char buf[32];
	int idx = 0;
	u32 mod = 0;
	size_t len = min_t(size_t, count, sizeof(buf) - 1);

	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;
	buf[len] = '\0';

#define ASR_MOD_TOKEN(str, val)                                        \
    if (strncmp(&buf[idx], str, sizeof(str) - 1 ) == 0) {               \
        idx += sizeof(str) - 1;                                         \
        mod |= val;                                                     \
        continue;                                                       \
    }

#define ASR_DBG_TOKEN(str, val)                                \
    if (strncmp(&buf[idx], str, sizeof(str) - 1) == 0) {        \
        idx += sizeof(str) - 1;                                 \
        dbg = val;                                              \
        goto dbg_done;                                          \
    }

	while ((idx + 4) < len) {
		if (strncmp(&buf[idx], "MOD:", 4) == 0) {
			idx += 4;
			ASR_MOD_TOKEN("ALL", 0xffffffff);
			ASR_MOD_TOKEN("KE", BIT(0));
			ASR_MOD_TOKEN("DBG", BIT(1));
			ASR_MOD_TOKEN("IPC", BIT(2));
			ASR_MOD_TOKEN("DMA", BIT(3));
			ASR_MOD_TOKEN("MM", BIT(4));
			ASR_MOD_TOKEN("TX", BIT(5));
			ASR_MOD_TOKEN("RX", BIT(6));
			ASR_MOD_TOKEN("PHY", BIT(7));
			ASR_MOD_TOKEN("HOST", BIT(14));
			ASR_MOD_TOKEN("FW", BIT(15));
			idx++;
		} else if (strncmp(&buf[idx], "DBG:", 4) == 0) {
			u32 dbg = 0;
			idx += 4;
			ASR_DBG_TOKEN("NONE", 0);
			ASR_DBG_TOKEN("CRT", 1);
			ASR_DBG_TOKEN("ERR", 2);
			ASR_DBG_TOKEN("WRN", 3);
			ASR_DBG_TOKEN("INF", 4);
			ASR_DBG_TOKEN("VRB", 5);
			idx++;
			continue;
dbg_done:
			asr_send_dbg_set_sev_filter_req(priv, dbg);
		} else {
			idx++;
		}
	}

	if (mod) {
		asr_send_dbg_set_mod_filter_req(priv, mod);
	}

	return count;
}

DEBUGFS_READ_WRITE_FILE_OPS(fw_dbg);

static ssize_t asr_dbgfs_sys_stats_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char buf[3 * 64];
	int len = 0;
	ssize_t read;
	int error = 0;
	struct dbg_get_sys_stat_cfm cfm;
	u32 sleep_int, sleep_frac, doze_int, doze_frac;

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Get the information from the FW */
	if ((error = asr_send_dbg_get_sys_stat_req(priv, &cfm)))
		return error;

	if (cfm.stats_time == 0)
		return 0;

	sleep_int = ((cfm.cpu_sleep_time * 100) / cfm.stats_time);
	sleep_frac = (((cfm.cpu_sleep_time * 100) % cfm.stats_time) * 10) / cfm.stats_time;
	doze_int = ((cfm.doze_time * 100) / cfm.stats_time);
	doze_frac = (((cfm.doze_time * 100) % cfm.stats_time) * 10) / cfm.stats_time;

	len += scnprintf(buf, min_t(size_t, sizeof(buf) - 1, count), "\nSystem statistics:\n");
	len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
			 "  CPU sleep [%%]: %d.%d\n", sleep_int, sleep_frac);
	len += scnprintf(&buf[len], min_t(size_t, sizeof(buf) - 1, count),
			 "  Doze      [%%]: %d.%d\n", doze_int, doze_frac);

	read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

	return read;
}

DEBUGFS_READ_FILE_OPS(sys_stats);

#if 0				//def CONFIG_ASR_MUMIMO_TX
static ssize_t asr_dbgfs_mu_group_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *asr_hw = file->private_data;
	struct asr_mu_info *mu = &asr_hw->mu;
	struct asr_mu_group *group;
	size_t bufsz = NX_MU_GROUP_MAX * sizeof("xx = (xx - xx - xx - xx)\n") + 50;
	char *buf;
	int j, res, idx = 0;

	if (*ppos)
		return 0;

	buf = kmalloc(bufsz, GFP_ATOMIC);
	if (buf == NULL)
		return 0;

	res =
	    scnprintf(&buf[idx], bufsz, "MU Group list (%d groups, %d users max)\n", NX_MU_GROUP_MAX, CONFIG_USER_MAX);
	idx += res;
	bufsz -= res;

	list_for_each_entry(group, &mu->active_groups, list) {
		if (group->user_cnt) {
			res = scnprintf(&buf[idx], bufsz, "%2d = (", group->group_id);
			idx += res;
			bufsz -= res;
			for (j = 0; j < (CONFIG_USER_MAX - 1); j++) {
				if (group->users[j])
					res = scnprintf(&buf[idx], bufsz, "%2d - ", group->users[j]->sta_idx);
				else
					res = scnprintf(&buf[idx], bufsz, ".. - ");

				idx += res;
				bufsz -= res;
			}

			if (group->users[j])
				res = scnprintf(&buf[idx], bufsz, "%2d)\n", group->users[j]->sta_idx);
			else
				res = scnprintf(&buf[idx], bufsz, "..)\n");

			idx += res;
			bufsz -= res;
		}
	}

	res = simple_read_from_buffer(user_buf, count, ppos, buf, idx);
	kfree(buf);

	return res;
}

DEBUGFS_READ_FILE_OPS(mu_group);
#endif

#ifdef CONFIG_ASR_P2P_DEBUGFS
static ssize_t asr_dbgfs_oppps_write(struct file *file, const char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *rw_hw = file->private_data;
	struct asr_vif *rw_vif;
	char buf[32];
	size_t len = min_t(size_t, count, sizeof(buf) - 1);
	int ctw;

	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;
	buf[len] = '\0';

	/* Read the written CT Window (provided in ms) value */
	if (sscanf(buf, "ctw=%d", &ctw) > 0) {
		/* Check if at least one VIF is configured as P2P GO */
		list_for_each_entry(rw_vif, &rw_hw->vifs, list) {
			if (ASR_VIF_TYPE(rw_vif) == NL80211_IFTYPE_P2P_GO) {
				struct mm_set_p2p_oppps_cfm cfm;

				/* Forward request to the embedded and wait for confirmation */
				asr_send_p2p_oppps_req(rw_hw, rw_vif, (u8) ctw, &cfm);

				break;
			}
		}
	}

	return count;
}

DEBUGFS_WRITE_FILE_OPS(oppps);

static ssize_t asr_dbgfs_noa_write(struct file *file, const char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *rw_hw = file->private_data;
	struct asr_vif *rw_vif;
	char buf[64];
	size_t len = min_t(size_t, count, sizeof(buf) - 1);
	int noa_count, interval, duration, dyn_noa;

	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;
	buf[len] = '\0';

	/* Read the written NOA information */
	if (sscanf(buf, "count=%d interval=%d duration=%d dyn=%d", &noa_count, &interval, &duration, &dyn_noa) > 0) {
		/* Check if at least one VIF is configured as P2P GO */
		list_for_each_entry(rw_vif, &rw_hw->vifs, list) {
			if (ASR_VIF_TYPE(rw_vif) == NL80211_IFTYPE_P2P_GO) {
				struct mm_set_p2p_noa_cfm cfm;

				/* Forward request to the embedded and wait for confirmation */
				asr_send_p2p_noa_req(rw_hw, rw_vif, noa_count, interval, duration, (dyn_noa > 0), &cfm);

				break;
			}
		}
	}

	return count;
}

DEBUGFS_WRITE_FILE_OPS(noa);
#endif /* CONFIG_ASR_P2P_DEBUGFS */

#if 0
struct asr_dbgfs_fw_trace {
	struct asr_fw_trace_local_buf lbuf;
	struct asr_fw_trace *trace;
};

static int asr_dbgfs_fw_trace_open(struct inode *inode, struct file *file)
{
	struct asr_dbgfs_fw_trace *ltrace = kmalloc(sizeof(*ltrace), GFP_KERNEL);
	struct asr_hw *priv = inode->i_private;

	if (!ltrace)
		return -ENOMEM;

	if (asr_fw_trace_alloc_local(&ltrace->lbuf, 5120)) {
		kfree(ltrace);
	}

	ltrace->trace = &priv->debugfs.fw_trace;
	file->private_data = ltrace;
	return 0;
}

static int asr_dbgfs_fw_trace_release(struct inode *inode, struct file *file)
{
	struct asr_dbgfs_fw_trace *ltrace = file->private_data;

	if (ltrace) {
		asr_fw_trace_free_local(&ltrace->lbuf);
		kfree(ltrace);
	}

	return 0;
}

static ssize_t asr_dbgfs_fw_trace_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_dbgfs_fw_trace *ltrace = file->private_data;
	return asr_fw_trace_read(ltrace->trace, &ltrace->lbuf, (file->f_flags & O_NONBLOCK), user_buf, count);
}

static ssize_t asr_dbgfs_fw_trace_write(struct file *file, const char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_dbgfs_fw_trace *ltrace = file->private_data;
	int ret;

	ret = _asr_fw_trace_reset(ltrace->trace, true);
	if (ret)
		return ret;

	return count;
}

DEBUGFS_READ_WRITE_OPEN_RELEASE_FILE_OPS(fw_trace);

static ssize_t asr_dbgfs_fw_trace_level_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	return asr_fw_trace_level_read(&priv->debugfs.fw_trace, user_buf, count, ppos);
}

static ssize_t asr_dbgfs_fw_trace_level_write(struct file *file,
					      const char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	return asr_fw_trace_level_write(&priv->debugfs.fw_trace, user_buf, count);
}

DEBUGFS_READ_WRITE_FILE_OPS(fw_trace_level);
#endif

#if 0				//def CONFIG_ASR_RADAR
static ssize_t asr_dbgfs_pulses_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos, int rd_idx)
{
	struct asr_hw *priv = file->private_data;
	char *buf;
	int len = 0;
	int bufsz;
	int i;
	int index;
	struct asr_radar_pulses *p = &priv->radar.pulses[rd_idx];
	ssize_t read;

	if (*ppos != 0)
		return 0;

	/* Prevent from interrupt preemption */
	spin_lock_bh(&priv->radar.lock);
	bufsz = p->count * 34 + 51;
	bufsz += asr_radar_dump_pattern_detector(NULL, 0, &priv->radar, rd_idx);
	buf = kmalloc(bufsz, GFP_ATOMIC);
	if (buf == NULL) {
		spin_unlock_bh(&priv->radar.lock);
		return 0;
	}

	if (p->count) {
		len += scnprintf(&buf[len], bufsz - len, " PRI     WIDTH     FOM     FREQ\n");
		index = p->index;
		for (i = 0; i < p->count; i++) {
			struct radar_pulse *pulse;

			if (index > 0)
				index--;
			else
				index = ASR_RADAR_PULSE_MAX - 1;

			pulse = (struct radar_pulse *)&p->buffer[index];

			len += scnprintf(&buf[len], bufsz - len,
					 "%05dus  %03dus     %2d%%    %+3dMHz\n",
					 pulse->rep, 2 * pulse->len, 6 * pulse->fom, 2 * pulse->freq);
		}
	}

	len += asr_radar_dump_pattern_detector(&buf[len], bufsz - len, &priv->radar, rd_idx);

	spin_unlock_bh(&priv->radar.lock);

	read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

	kfree(buf);

	return read;
}

static ssize_t asr_dbgfs_pulses_prim_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	return asr_dbgfs_pulses_read(file, user_buf, count, ppos, 0);
}

DEBUGFS_READ_FILE_OPS(pulses_prim);

static ssize_t asr_dbgfs_pulses_sec_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	return asr_dbgfs_pulses_read(file, user_buf, count, ppos, 1);
}

DEBUGFS_READ_FILE_OPS(pulses_sec);

static ssize_t asr_dbgfs_detected_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char *buf;
	int bufsz, len = 0;
	ssize_t read;

	if (*ppos != 0)
		return 0;

	bufsz = 5;		// RIU:\n
	bufsz += asr_radar_dump_radar_detected(NULL, 0, &priv->radar, ASR_RADAR_RIU);

	if (priv->phy_cnt > 1) {
		bufsz += 5;	// FCU:\n
		bufsz += asr_radar_dump_radar_detected(NULL, 0, &priv->radar, ASR_RADAR_FCU);
	}

	buf = kmalloc(bufsz, GFP_KERNEL);
	if (buf == NULL) {
		return 0;
	}

	len = scnprintf(&buf[len], bufsz, "RIU:\n");
	len += asr_radar_dump_radar_detected(&buf[len], bufsz - len, &priv->radar, ASR_RADAR_RIU);

	if (priv->phy_cnt > 1) {
		len += scnprintf(&buf[len], bufsz - len, "FCU:\n");
		len += asr_radar_dump_radar_detected(&buf[len], bufsz - len, &priv->radar, ASR_RADAR_FCU);
	}

	read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

	kfree(buf);

	return read;
}

DEBUGFS_READ_FILE_OPS(detected);

static ssize_t asr_dbgfs_enable_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char buf[32];
	int ret;
	ssize_t read;

	ret = scnprintf(buf, min_t(size_t, sizeof(buf) - 1, count),
			"RIU=%d FCU=%d\n",
			priv->radar.dpd[ASR_RADAR_RIU]->enabled, priv->radar.dpd[ASR_RADAR_FCU]->enabled);

	read = simple_read_from_buffer(user_buf, count, ppos, buf, ret);

	return read;
}

static ssize_t asr_dbgfs_enable_write(struct file *file, const char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char buf[32];
	int val;
	size_t len = min_t(size_t, count, sizeof(buf) - 1);

	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';

	if (sscanf(buf, "RIU=%d", &val) > 0)
		asr_radar_detection_enable(&priv->radar, val, ASR_RADAR_RIU);

	if (sscanf(buf, "FCU=%d", &val) > 0)
		asr_radar_detection_enable(&priv->radar, val, ASR_RADAR_FCU);

	return count;
}

DEBUGFS_READ_WRITE_FILE_OPS(enable);

static ssize_t asr_dbgfs_band_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char buf[32];
	int ret;
	ssize_t read;

	ret = scnprintf(buf, min_t(size_t, sizeof(buf) - 1, count), "BAND=%d\n", priv->sec_phy_chan.band);

	read = simple_read_from_buffer(user_buf, count, ppos, buf, ret);

	return read;
}

static ssize_t asr_dbgfs_band_write(struct file *file, const char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char buf[32];
	int val;
	size_t len = min_t(size_t, count, sizeof(buf) - 1);

	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';

	if ((sscanf(buf, "%d", &val) > 0) && (val >= 0)
	    && (val <= NL80211_BAND_5GHZ))
		priv->sec_phy_chan.band = val;

	return count;
}

DEBUGFS_READ_WRITE_FILE_OPS(band);

static ssize_t asr_dbgfs_type_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char buf[32];
	int ret;
	ssize_t read;

	ret = scnprintf(buf, min_t(size_t, sizeof(buf) - 1, count), "TYPE=%d\n", priv->sec_phy_chan.type);

	read = simple_read_from_buffer(user_buf, count, ppos, buf, ret);

	return read;
}

static ssize_t asr_dbgfs_type_write(struct file *file, const char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char buf[32];
	int val;
	size_t len = min_t(size_t, count, sizeof(buf) - 1);

	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';

	if ((sscanf(buf, "%d", &val) > 0) && (val >= PHY_CHNL_BW_20) && (val <= PHY_CHNL_BW_80P80))
		priv->sec_phy_chan.type = val;

	return count;
}

DEBUGFS_READ_WRITE_FILE_OPS(type);

static ssize_t asr_dbgfs_prim20_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char buf[32];
	int ret;
	ssize_t read;

	ret = scnprintf(buf, min_t(size_t, sizeof(buf) - 1, count), "PRIM20=%dMHz\n", priv->sec_phy_chan.prim20_freq);

	read = simple_read_from_buffer(user_buf, count, ppos, buf, ret);

	return read;
}

static ssize_t asr_dbgfs_prim20_write(struct file *file, const char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char buf[32];
	int val;
	size_t len = min_t(size_t, count, sizeof(buf) - 1);

	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';

	if (sscanf(buf, "%d", &val) > 0)
		priv->sec_phy_chan.prim20_freq = val;

	return count;
}

DEBUGFS_READ_WRITE_FILE_OPS(prim20);

static ssize_t asr_dbgfs_center1_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char buf[32];
	int ret;
	ssize_t read;

	ret = scnprintf(buf, min_t(size_t, sizeof(buf) - 1, count), "CENTER1=%dMHz\n", priv->sec_phy_chan.center_freq1);

	read = simple_read_from_buffer(user_buf, count, ppos, buf, ret);

	return read;
}

static ssize_t asr_dbgfs_center1_write(struct file *file, const char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char buf[32];
	int val;
	size_t len = min_t(size_t, count, sizeof(buf) - 1);

	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';

	if (sscanf(buf, "%d", &val) > 0)
		priv->sec_phy_chan.center_freq1 = val;

	return count;
}

DEBUGFS_READ_WRITE_FILE_OPS(center1);

static ssize_t asr_dbgfs_center2_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char buf[32];
	int ret;
	ssize_t read;

	ret = scnprintf(buf, min_t(size_t, sizeof(buf) - 1, count), "CENTER2=%dMHz\n", priv->sec_phy_chan.center_freq2);

	read = simple_read_from_buffer(user_buf, count, ppos, buf, ret);

	return read;
}

static ssize_t asr_dbgfs_center2_write(struct file *file, const char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;
	char buf[32];
	int val;
	size_t len = min_t(size_t, count, sizeof(buf) - 1);

	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';

	if (sscanf(buf, "%d", &val) > 0)
		priv->sec_phy_chan.center_freq2 = val;

	return count;
}

DEBUGFS_READ_WRITE_FILE_OPS(center2);

static ssize_t asr_dbgfs_set_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	return 0;
}

static ssize_t asr_dbgfs_set_write(struct file *file, const char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_hw *priv = file->private_data;

	asr_send_set_channel(priv, 1, NULL);
	asr_radar_detection_enable(&priv->radar, ASR_RADAR_DETECT_ENABLE, ASR_RADAR_FCU);

	return count;
}

DEBUGFS_READ_WRITE_FILE_OPS(set);
#endif /* CONFIG_ASR_RADAR */

#define LINE_MAX_SZ 150

struct st {
	char line[LINE_MAX_SZ + 1];
	unsigned int r_idx;
};

#if 0
static int compare_idx(const void *st1, const void *st2)
{
	int index1 = ((struct st *)st1)->r_idx;
	int index2 = ((struct st *)st2)->r_idx;

	if (index1 > index2)
		return 1;
	if (index1 < index2)
		return -1;

	return 0;
}
#endif

static const int ru_size[] = {
	26,
	52,
	106,
	242,
	484,
	996
};

static int print_rate(char *buf, int size, int format, int nss, int mcs, int bw, int sgi, int pre, int *r_idx)
{
	int res = 0;
	int bitrates_cck[4] = { 10, 20, 55, 110 };
	int bitrates_ofdm[8] = { 6, 9, 12, 18, 24, 36, 48, 54 };
#ifdef CONFIG_ASR595X
	char he_gi[3][4] = { "0.8", "1.6", "3.2" };
#endif

	if (format <= FORMATMOD_NON_HT_DUP_OFDM) {
		if (mcs < 4) {
			if (r_idx) {
				*r_idx = (mcs * 2) + pre;
				res = scnprintf(buf, size - res, "%3d ", *r_idx);
			}
			res +=
			    scnprintf(&buf[res], size - res,
				      "L-CCK/%cP   %2u.%1uM ",
				      pre > 0 ? 'L' : 'S', bitrates_cck[mcs] / 10, bitrates_cck[mcs] % 10);
		} else {
			mcs -= 4;
			if (r_idx) {
				*r_idx = N_CCK + mcs;
				res = scnprintf(buf, size - res, "%3d ", *r_idx);
			}
			res += scnprintf(&buf[res], size - res, "L-OFDM     %2u.0M ", bitrates_ofdm[mcs]);
		}
	} else if (format <= FORMATMOD_HT_GF) {
		if (r_idx) {
			*r_idx = N_CCK + N_OFDM + nss * 32 + mcs * 4 + bw * 2 + sgi;
			res = scnprintf(buf, size - res, "%3d ", *r_idx);
		}
		res += scnprintf(&buf[res], size - res, "HT%d/%cGI   MCS%-2d ",
				 20 * (1 << bw), sgi ? 'S' : 'L', nss * 8 + mcs);
	} else if (format == FORMATMOD_VHT) {
		if (r_idx) {
			*r_idx = N_CCK + N_OFDM + N_HT + nss * 80 + mcs * 8 + bw * 2 + sgi;
			res = scnprintf(buf, size - res, "%3d ", *r_idx);
		}
		res +=
		    scnprintf(&buf[res], size - res, "VHT%d/%cGI%*cMCS%d/%1d",
			      20 * (1 << bw), sgi ? 'S' : 'L', bw > 2 ? 1 : 2, ' ', mcs, nss + 1);
	}
#ifdef CONFIG_ASR595X
	else if (format == FORMATMOD_HE_SU) {
		if (r_idx) {
			*r_idx = N_CCK + N_OFDM + N_HT + N_VHT + nss * 144 + mcs * 12 + bw * 3 + sgi;
			res = scnprintf(buf, size - res, "%3d ", *r_idx);
		}
		res +=
		    scnprintf(&buf[res], size - res, "HE%d/GI%s%*cMCS%d/%1d%*c",
			      20 * (1 << bw), he_gi[sgi], bw > 2 ? 4 : 5, ' ', mcs, nss + 1, mcs > 9 ? 1 : 2, ' ');
	} else {
		if (r_idx) {
			*r_idx = N_CCK + N_OFDM + N_HT + N_VHT + N_HE_SU + nss * 216 + mcs * 18 + bw * 3 + sgi;
			res = scnprintf(buf, size - res, "%3d ", *r_idx);
		}
		res +=
		    scnprintf(&buf[res], size - res, "HEMU-%d/GI%s%*cMCS%d/%1d%*c",
			      ru_size[bw], he_gi[sgi], bw > 1 ? 1 : 2, ' ', mcs, nss + 1, mcs > 9 ? 1 : 2, ' ');
	}
#endif

	return res;
}

#ifdef CONFIG_ASR595X
static int print_rate_from_cfg(char *buf, int size, u32 rate_config, int *r_idx, int ru_size)
{
	union asr_rate_ctrl_info *r_cfg = (union asr_rate_ctrl_info *)&rate_config;
	union asr_mcs_index *mcs_index = (union asr_mcs_index *)&rate_config;
	unsigned int ft, pre, gi, bw, nss, mcs, len;

	ft = r_cfg->formatModTx;
	pre = r_cfg->giAndPreTypeTx >> 1;
	gi = r_cfg->giAndPreTypeTx;
	bw = r_cfg->bwTx;
	if (ft == FORMATMOD_HE_MU) {
		mcs = mcs_index->he.mcs;
		nss = mcs_index->he.nss;
		bw = ru_size;
	} else if (ft == FORMATMOD_HE_SU) {
		mcs = mcs_index->he.mcs;
		nss = mcs_index->he.nss;
	} else if (ft == FORMATMOD_VHT) {
		mcs = mcs_index->vht.mcs;
		nss = mcs_index->vht.nss;
	} else if (ft >= FORMATMOD_HT_MF) {
		mcs = mcs_index->ht.mcs;
		nss = mcs_index->ht.nss;
	} else {
		mcs = mcs_index->legacy;
		nss = 0;
	}

	len = print_rate(buf, size, ft, nss, mcs, bw, gi, pre, r_idx);
	return len;
}

static void idx_to_rate_cfg(int idx, union asr_rate_ctrl_info *r_cfg, int *ru_size)
{
	r_cfg->value = 0;
	if (idx < N_CCK) {
		r_cfg->formatModTx = FORMATMOD_NON_HT;
		r_cfg->giAndPreTypeTx = (idx & 1) << 1;
		r_cfg->mcsIndexTx = idx / 2;
	} else if (idx < (N_CCK + N_OFDM)) {
		r_cfg->formatModTx = FORMATMOD_NON_HT;
		r_cfg->mcsIndexTx = idx - N_CCK + 4;
	} else if (idx < (N_CCK + N_OFDM + N_HT)) {
		union asr_mcs_index *r = (union asr_mcs_index *)r_cfg;

		idx -= (N_CCK + N_OFDM);
		r_cfg->formatModTx = FORMATMOD_HT_MF;
		r->ht.nss = idx / (8 * 2 * 2);
		r->ht.mcs = (idx % (8 * 2 * 2)) / (2 * 2);
		r_cfg->bwTx = ((idx % (8 * 2 * 2)) % (2 * 2)) / 2;
		r_cfg->giAndPreTypeTx = idx & 1;
	} else if (idx < (N_CCK + N_OFDM + N_HT + N_VHT)) {
		union asr_mcs_index *r = (union asr_mcs_index *)r_cfg;

		idx -= (N_CCK + N_OFDM + N_HT);
		r_cfg->formatModTx = FORMATMOD_VHT;
		r->vht.nss = idx / (10 * 4 * 2);
		r->vht.mcs = (idx % (10 * 4 * 2)) / (4 * 2);
		r_cfg->bwTx = ((idx % (10 * 4 * 2)) % (4 * 2)) / 2;
		r_cfg->giAndPreTypeTx = idx & 1;
	} else if (idx < (N_CCK + N_OFDM + N_HT + N_VHT + N_HE_SU)) {
		union asr_mcs_index *r = (union asr_mcs_index *)r_cfg;

		idx -= (N_CCK + N_OFDM + N_HT + N_VHT);
		r_cfg->formatModTx = FORMATMOD_HE_SU;
		r->vht.nss = idx / (12 * 4 * 3);
		r->vht.mcs = (idx % (12 * 4 * 3)) / (4 * 3);
		r_cfg->bwTx = ((idx % (12 * 4 * 3)) % (4 * 3)) / 3;
		r_cfg->giAndPreTypeTx = idx % 3;
	} else {
		union asr_mcs_index *r = (union asr_mcs_index *)r_cfg;
		BUG_ON(ru_size == NULL);

		idx -= (N_CCK + N_OFDM + N_HT + N_VHT + N_HE_SU);
		r_cfg->formatModTx = FORMATMOD_HE_MU;
		r->vht.nss = idx / (12 * 6 * 3);
		r->vht.mcs = (idx % (12 * 6 * 3)) / (6 * 3);
		*ru_size = ((idx % (12 * 6 * 3)) % (6 * 3)) / 3;
		r_cfg->giAndPreTypeTx = idx % 3;
		r_cfg->bwTx = 0;
	}
}
#else
static int print_rate_from_cfg(char *buf, int size, u32 rate_config, int *r_idx)
{
	union asr_rate_ctrl_info *r_cfg = (union asr_rate_ctrl_info *)&rate_config;
	union asr_mcs_index *mcs_index = (union asr_mcs_index *)&rate_config;
	unsigned int ft, pre, gi, bw, nss, mcs, len;

	ft = r_cfg->formatModTx;
	pre = r_cfg->preTypeTx;
	if (ft == FORMATMOD_VHT) {
		mcs = mcs_index->vht.mcs;
		nss = mcs_index->vht.nss;
	} else if (ft >= FORMATMOD_HT_MF) {
		mcs = mcs_index->ht.mcs;
		nss = mcs_index->ht.nss;
	} else {
		mcs = mcs_index->legacy;
		nss = 0;
	}
	gi = r_cfg->shortGITx;
	bw = r_cfg->bwTx;

	len = print_rate(buf, size, ft, nss, mcs, bw, gi, pre, r_idx);
	return len;
}

static void idx_to_rate_cfg(int idx, union asr_rate_ctrl_info *r_cfg)
{
	r_cfg->value = 0;
	if (idx < N_CCK) {
		r_cfg->formatModTx = FORMATMOD_NON_HT;
		r_cfg->preTypeTx = idx & 1;
		r_cfg->mcsIndexTx = idx / 2;
	} else if (idx < (N_CCK + N_OFDM)) {
		r_cfg->formatModTx = FORMATMOD_NON_HT;
		r_cfg->mcsIndexTx = idx - N_CCK + 4;
	} else if (idx < (N_CCK + N_OFDM + N_HT)) {
		union asr_mcs_index *r = (union asr_mcs_index *)r_cfg;

		idx -= (N_CCK + N_OFDM);
		r_cfg->formatModTx = FORMATMOD_HT_MF;
		r->ht.nss = idx / (8 * 2 * 2);
		r->ht.mcs = (idx % (8 * 2 * 2)) / (2 * 2);
		r_cfg->bwTx = ((idx % (8 * 2 * 2)) % (2 * 2)) / 2;
		r_cfg->shortGITx = idx & 1;
	} else {
		union asr_mcs_index *r = (union asr_mcs_index *)r_cfg;

		idx -= (N_CCK + N_OFDM + N_HT);
		r_cfg->formatModTx = FORMATMOD_VHT;
		r->vht.nss = idx / (10 * 4 * 2);
		r->vht.mcs = (idx % (10 * 4 * 2)) / (4 * 2);
		r_cfg->bwTx = ((idx % (10 * 4 * 2)) % (4 * 2)) / 2;
		r_cfg->shortGITx = idx & 1;
	}
}
#endif

static ssize_t asr_dbgfs_rc_stats_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_sta *sta = NULL;
	struct asr_hw *priv = file->private_data;
	char *buf;
	int bufsz, len = 0;
	ssize_t read;
	int i = 0;
	int error = 0;
	struct me_rc_stats_cfm me_rc_stats_cfm;
	unsigned int no_samples;
	struct st *st;
	u8 mac[6];

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* everything should fit in one call */
	if (*ppos)
		return 0;

	/* Get the station index from MAC address */
	sscanf(file->f_path.dentry->d_parent->d_iname,
	       "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

	sta = asr_get_sta(priv, mac);
	if (sta == NULL)
		return 0;

	/* Forward the information to the LMAC */
	if ((error = asr_send_me_rc_stats(priv, sta->sta_idx, &me_rc_stats_cfm)))
		return error;

	no_samples = me_rc_stats_cfm.no_samples;
	if (no_samples == 0)
		return 0;

	bufsz = no_samples * LINE_MAX_SZ + 500;

	buf = kmalloc(bufsz + 1, GFP_ATOMIC);
	if (buf == NULL)
		return 0;

	st = kmalloc(sizeof(struct st) * no_samples, GFP_ATOMIC);
	if (st == NULL) {
		kfree(buf);
		return 0;
	}

	for (i = 0; i < no_samples; i++) {
		unsigned int tp, eprob;
#ifdef CONFIG_ASR595X
		len = print_rate_from_cfg(st[i].line, LINE_MAX_SZ,
					  me_rc_stats_cfm.rate_stats[i].rate_config, &st[i].r_idx, 0);
#else
		len = print_rate_from_cfg(st[i].line, LINE_MAX_SZ,
					  me_rc_stats_cfm.rate_stats[i].rate_config, &st[i].r_idx);
#endif

		if (me_rc_stats_cfm.sw_retry_step != 0) {
			len +=
			    scnprintf(&st[i].line[len], LINE_MAX_SZ - len, "%c",
				      me_rc_stats_cfm.retry_step_idx[me_rc_stats_cfm.sw_retry_step] == i ? '*' : ' ');
		} else {
			len += scnprintf(&st[i].line[len], LINE_MAX_SZ - len, " ");
		}

		len += scnprintf(&st[i].line[len], LINE_MAX_SZ - len, "%c",
				 me_rc_stats_cfm.retry_step_idx[0] == i ? 'T' : ' ');
		len += scnprintf(&st[i].line[len], LINE_MAX_SZ - len, "%c",
				 me_rc_stats_cfm.retry_step_idx[1] == i ? 't' : ' ');
		len += scnprintf(&st[i].line[len], LINE_MAX_SZ - len, "%c ",
				 me_rc_stats_cfm.retry_step_idx[2] == i ? 'P' : ' ');
		tp = me_rc_stats_cfm.tp[i] / 10;
		len += scnprintf(&st[i].line[len], LINE_MAX_SZ - len, "  %4u.%1u", tp / 10, tp % 10);

		eprob = ((me_rc_stats_cfm.rate_stats[i].probability * 1000) >> 16) + 1;
		len +=
		    scnprintf(&st[i].line[len], LINE_MAX_SZ - len,
			      " %4u.%1u %5u(%6u)  %6u",
			      eprob / 10, eprob % 10,
			      me_rc_stats_cfm.rate_stats[i].success,
			      me_rc_stats_cfm.rate_stats[i].attempts, me_rc_stats_cfm.rate_stats[i].sample_skipped);
	}

	dev_info(priv->dev, "%s: st line len=%d\n", __func__, len);

	len = scnprintf(buf, bufsz,
			"\nTX rate info for %02X:%02X:%02X:%02X:%02X:%02X:\n",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	len += scnprintf(&buf[len], bufsz - len,
			 " #  type       rate            tpt  eprob     ok(   tot) skipped nRetry\n");

	// add sorted statistics to the buffer
	//sort(st, no_samples, sizeof(st[0]), compare_idx, NULL);
	for (i = 0; i < no_samples; i++) {
		len += scnprintf(&buf[len], bufsz - len, "%s\n", st[i].line);
	}

#ifdef CONFIG_ASR595X
	// display HE TB statistic if any
	if (me_rc_stats_cfm.rate_stats[RC_HE_STATS_IDX].rate_config != 0) {
		unsigned int tp, eprob;
		struct rc_rate_stats *rate_stats = &me_rc_stats_cfm.rate_stats[RC_HE_STATS_IDX];
		int ru_index = rate_stats->ru_and_length & 0x07;
		int ul_length = rate_stats->ru_and_length >> 3;

		len += scnprintf(&buf[len], bufsz - len, "\nHE TB rate info:\n");

		len += scnprintf(&buf[len], bufsz - len,
				 " #  type       rate            tpt  eprob     ok(   tot)  ul_length\n");

		len += print_rate_from_cfg(&buf[len], bufsz - len, rate_stats->rate_config, NULL, ru_index);

		tp = me_rc_stats_cfm.tp[RC_HE_STATS_IDX] / 10;
		len += scnprintf(&buf[len], bufsz - len, "     %4u.%1u", tp / 10, tp % 10);

		eprob = ((rate_stats->probability * 1000) >> 16) + 1;
		len += scnprintf(&buf[len], bufsz - len,
				 "     %4u.%1u %5u(%6u)  %6u\n",
				 eprob / 10, eprob % 10, rate_stats->success, rate_stats->attempts, ul_length);
	}
#endif

	len += scnprintf(&buf[len], bufsz - len, "\n MPDUs AMPDUs AvLen trialP");
	len +=
	    scnprintf(&buf[len], bufsz - len, "\n%6u %6u %3d.%1d %6u\n",
		      me_rc_stats_cfm.ampdu_len, me_rc_stats_cfm.ampdu_packets,
		      me_rc_stats_cfm.avg_ampdu_len >> 16,
		      ((me_rc_stats_cfm.avg_ampdu_len * 10) >> 16) % 10, me_rc_stats_cfm.sample_wait);

	read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

	kfree(buf);
	kfree(st);

	return read;
}

DEBUGFS_READ_FILE_OPS(rc_stats);

static ssize_t asr_dbgfs_rc_fixed_rate_idx_write(struct file *file,
						 const char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_sta *sta = NULL;
	struct asr_hw *priv = file->private_data;
	u8 mac[6];
	char buf[10];
	int fixed_rate_idx = -1;
	union asr_rate_ctrl_info rate_config;
	int error = 0;
	size_t len = min_t(size_t, count, sizeof(buf) - 1);

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* Get the station index from MAC address */
	sscanf(file->f_path.dentry->d_parent->d_iname,
	       "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

	sta = asr_get_sta(priv, mac);
	if (sta == NULL)
		return 0;

	/* Get the content of the file */
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;
	buf[len] = '\0';
	sscanf(buf, "%i\n", &fixed_rate_idx);

	/* Convert rate index into rate configuration */
	if ((fixed_rate_idx < 0)
	    || (fixed_rate_idx >= (N_CCK + N_OFDM + N_HT + N_VHT))) {
		// disable fixed rate
		rate_config.value = (u32) - 1;
	} else {
#ifdef CONFIG_ASR595X
		idx_to_rate_cfg(fixed_rate_idx, &rate_config, NULL);
#else
		idx_to_rate_cfg(fixed_rate_idx, &rate_config);
#endif
	}

	// Forward the request to the LMAC
	if ((error = asr_send_me_rc_set_rate(priv, sta->sta_idx, (u16) rate_config.value)) != 0) {
		return error;
	}

	priv->debugfs.rc_config[sta->sta_idx] = (int)rate_config.value;
	return len;
}

DEBUGFS_WRITE_FILE_OPS(rc_fixed_rate_idx);

static ssize_t asr_dbgfs_last_rx_read(struct file *file, char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_sta *sta = NULL;
	struct asr_hw *priv = file->private_data;
	struct asr_rx_rate_stats *rate_stats;
	char *buf;
	int bufsz, i, len = 0;
	ssize_t read;
	unsigned int fmt, pre, bw, nss, mcs;
	u8 mac[6];
	struct rx_vector_1 *last_rx;
	char hist[] = "##################################################";
	int hist_len = sizeof(hist) - 1;
	u8 nrx;
#ifdef CONFIG_ASR595X
	unsigned int gi;
#else
	unsigned int sgi;
#endif

	ASR_DBG(ASR_FN_ENTRY_STR);

	/* everything should fit in one call */
	if (*ppos)
		return 0;

	/* Get the station index from MAC address */
	sscanf(file->f_path.dentry->d_parent->d_iname,
	       "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

	sta = asr_get_sta(priv, mac);
	if (sta == NULL)
		return 0;

	rate_stats = &sta->stats.rx_rate;
#ifdef CONFIG_ASR595X
	bufsz = (rate_stats->rate_cnt * (50 * hist_len) + 200);
#else
	bufsz = (rate_stats->size * (30 * hist_len) + 200);
#endif
	buf = kmalloc(bufsz + 1, GFP_ATOMIC);
	if (buf == NULL)
		return 0;

	// Get number of RX paths
	nrx = (priv->version_cfm.version_phy_1 & MDM_NRX_MASK) >> MDM_NRX_LSB;

	len += scnprintf(buf, bufsz,
			 "\nRX rate info for %02X:%02X:%02X:%02X:%02X:%02X:\n",
			 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	// Display Statistics
	for (i = 0; i < rate_stats->size; i++) {
		if (rate_stats->table[i]) {
			union asr_rate_ctrl_info rate_config;
			int percent = (rate_stats->table[i] * 1000) / rate_stats->cpt;
			int p;

#ifdef CONFIG_ASR595X
			int ru_size;
			idx_to_rate_cfg(i, &rate_config, &ru_size);
			len += print_rate_from_cfg(&buf[len], bufsz - len, rate_config.value, NULL, ru_size);
#else
			idx_to_rate_cfg(i, &rate_config);
			len += print_rate_from_cfg(&buf[len], bufsz - len, rate_config.value, NULL);
#endif
			p = (percent * hist_len) / 1000;
			len +=
			    scnprintf(&buf[len], bufsz - len,
				      ": %6d(%3d.%1d%%)%.*s\n",
				      rate_stats->table[i], percent / 10, percent % 10, p, hist);
		}
	}

	// Display detailled info of the last received rate
	last_rx = &sta->stats.last_rx.rx_vect1;

	len += scnprintf(&buf[len], bufsz - len, "\nLast received rate\n"
			 "  type     rate   LDPC STBC BEAMFM %s\n", (nrx > 1) ? "rssi1(dBm) rssi2(dBm)" : "rssi(dBm)");

#ifdef CONFIG_ASR595X
	fmt = last_rx->format_mod;
	bw = last_rx->ch_bw;
	pre = last_rx->pre_type;
	if (fmt >= FORMATMOD_HE_SU) {
		mcs = last_rx->he.mcs;
		nss = last_rx->he.nss;
		gi = last_rx->he.gi_type;
		if (fmt == FORMATMOD_HE_MU)
			bw = last_rx->he.ru_size;
	} else if (fmt == FORMATMOD_VHT) {
		mcs = last_rx->vht.mcs;
		nss = last_rx->vht.nss;
		gi = last_rx->vht.short_gi;
	} else if (fmt >= FORMATMOD_HT_MF) {
		mcs = last_rx->ht.mcs % 8;
		nss = last_rx->ht.mcs / 8;
		gi = last_rx->ht.short_gi;
	} else {
		BUG_ON((mcs = legrates_lut[last_rx->leg_rate]) == -1);
		//BUG_ON(1);
		nss = 0;
		gi = 0;
	}

	len += print_rate(&buf[len], bufsz - len, fmt, nss, mcs, bw, gi, pre, NULL);

	/* flags for HT/VHT/HE */
	if (fmt >= FORMATMOD_HE_SU) {
		len += scnprintf(&buf[len], bufsz - len, "    %c    %c      %c      %c     %c",
				 last_rx->he.fec ? 'L' : ' ',
				 last_rx->he.stbc ? 'S' : ' ',
				 last_rx->he.beamformed ? 'B' : ' ',
				 last_rx->he.dcm ? 'D' : ' ', last_rx->he.doppler ? 'D' : ' ');
	} else if (fmt == FORMATMOD_VHT) {
		len += scnprintf(&buf[len], bufsz - len, "    %c    %c      %c",
				 last_rx->vht.fec ? 'L' : ' ',
				 last_rx->vht.stbc ? 'S' : ' ', last_rx->vht.beamformed ? 'B' : ' ');
	} else if (fmt >= FORMATMOD_HT_MF) {
		len += scnprintf(&buf[len], bufsz - len, "    %c    %c       ",
				 last_rx->ht.fec ? 'L' : ' ', last_rx->ht.stbc ? 'S' : ' ');
	} else {
		len += scnprintf(&buf[len], bufsz - len, "                 ");
	}
	if (nrx > 1) {
		len += scnprintf(&buf[len], bufsz - len, "    %-4d       %d\n", last_rx->rssi1, last_rx->rssi1);
	} else {
		len += scnprintf(&buf[len], bufsz - len, "   %d\n", last_rx->rssi1);
	}

#else
	fmt = last_rx->format_mod;
	bw = last_rx->ch_bw;
	pre = last_rx->pre_type;
	sgi = last_rx->short_gi;
	if (fmt == FORMATMOD_VHT) {
		mcs = last_rx->mcs;
		nss = last_rx->stbc ? last_rx->n_sts / 2 : last_rx->n_sts;
	} else if (fmt >= FORMATMOD_HT_MF) {
		mcs = last_rx->mcs;
		nss = last_rx->stbc ? last_rx->stbc : last_rx->n_sts;
	} else {
		BUG_ON((mcs = legrates_lut[last_rx->leg_rate]) == -1);
		//BUG_ON(1);
		nss = 0;
	}

	len += print_rate(&buf[len], bufsz - len, fmt, nss, mcs, bw, sgi, pre, NULL);

	/* flags for HT/VHT */
	if (fmt == FORMATMOD_VHT) {
		len += scnprintf(&buf[len], bufsz - len, "    %c    %c      %c",
				 last_rx->fec_coding ? 'L' : ' ',
				 last_rx->stbc ? 'S' : ' ', last_rx->smoothing ? ' ' : 'B');
	} else if (fmt >= FORMATMOD_HT_MF) {
		len += scnprintf(&buf[len], bufsz - len, "    %c    %c       ",
				 last_rx->fec_coding ? 'L' : ' ', last_rx->stbc ? 'S' : ' ');
	} else {
		len += scnprintf(&buf[len], bufsz - len, "                 ");
	}
	if (nrx > 1) {
		len += scnprintf(&buf[len], bufsz - len, "    %-4d       %d\n", last_rx->rssi1, last_rx->rssi2);
	} else {
		len += scnprintf(&buf[len], bufsz - len, "   %d\n", last_rx->rssi1);
	}
#endif

	read = simple_read_from_buffer(user_buf, count, ppos, buf, len);

	kfree(buf);
	return read;
}

static ssize_t asr_dbgfs_last_rx_write(struct file *file, const char __user * user_buf, size_t count, loff_t * ppos)
{
	struct asr_sta *sta = NULL;
	struct asr_hw *priv = file->private_data;
	u8 mac[6];

	/* Get the station index from MAC address */
	sscanf(file->f_path.dentry->d_parent->d_iname,
	       "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

	sta = asr_get_sta(priv, mac);
	if (sta == NULL)
		return 0;

	/* Prevent from interrupt preemption as these statistics are updated under
	 * interrupt */
	spin_lock_bh(&priv->tx_lock);
	memset(sta->stats.rx_rate.table, 0, sta->stats.rx_rate.size * sizeof(sta->stats.rx_rate.table[0]));
	sta->stats.rx_rate.cpt = 0;
#ifdef CONFIG_ASR595X
	sta->stats.rx_rate.rate_cnt = 0;
#endif
	spin_unlock_bh(&priv->tx_lock);

	return count;
}

DEBUGFS_READ_WRITE_FILE_OPS(last_rx);

/*
 * trace helper
 */
#if 0
void asr_fw_trace_dump(struct asr_hw *asr_hw)
{
	/* may be called before asr_dbgfs_register */
	if (asr_hw->plat->enabled && !asr_hw->debugfs.fw_trace.buf.data) {
		asr_fw_trace_buf_init(&asr_hw->debugfs.fw_trace.buf, asr_ipc_fw_trace_desc_get(asr_hw));
	}

	if (!asr_hw->debugfs.fw_trace.buf.data)
		return;

	_asr_fw_trace_dump(&asr_hw->debugfs.fw_trace.buf);
}

void asr_fw_trace_reset(struct asr_hw *asr_hw)
{
	_asr_fw_trace_reset(&asr_hw->debugfs.fw_trace, true);
}

void asr_dbgfs_trigger_fw_dump(struct asr_hw *asr_hw, char *reason)
{
	asr_send_dbg_trigger_req(asr_hw, reason);
}
#endif

static void asr_rc_stat_work(struct work_struct *ws)
{
	struct asr_debugfs *asr_debugfs = container_of(ws, struct asr_debugfs,
						       rc_stat_work);
	struct asr_hw *asr_hw = container_of(asr_debugfs, struct asr_hw,
					     debugfs);
	struct asr_sta *sta;
	uint8_t ridx, sta_idx;

	ridx = asr_debugfs->rc_read;
	sta_idx = asr_debugfs->rc_sta[ridx];
	if (sta_idx >= (NX_REMOTE_STA_MAX)) {
		WARN(1, "Invalid sta index %d", sta_idx);
		return;
	}

	asr_debugfs->rc_sta[ridx] = 0xFF;
	ridx = (ridx + 1) % ARRAY_SIZE(asr_debugfs->rc_sta);
	asr_debugfs->rc_read = ridx;
	sta = &asr_hw->sta_table[sta_idx];
	if (!sta) {
		WARN(1, "Invalid sta %d", sta_idx);
		return;
	}

	if (asr_debugfs->dir_sta[sta_idx] == NULL) {
		/* register the sta */
		struct dentry *dir_rc = asr_debugfs->dir_rc;
		struct dentry *dir_sta;
		struct dentry *file;
		char sta_name[18];
		struct asr_rx_rate_stats *rate_stats = &sta->stats.rx_rate;
		int nb_rx_rate = N_CCK + N_OFDM;
		struct asr_rc_config_save *rc_cfg, *next;

		//if (sta->sta_idx >= NX_REMOTE_STA_MAX) {
		//      scnprintf(sta_name, sizeof(sta_name), "bc_mc");
		//} else {
		scnprintf(sta_name, sizeof(sta_name), "%pM", sta->mac_addr);
		//}

		if (!(dir_sta = debugfs_create_dir(sta_name, dir_rc)))
			goto error;

		asr_debugfs->dir_sta[sta->sta_idx] = dir_sta;

		file = debugfs_create_file("stats", S_IRUSR, dir_sta, asr_hw, &asr_dbgfs_rc_stats_ops);
		if (IS_ERR_OR_NULL(file))
			goto error_after_dir;

		file =
		    debugfs_create_file("fixed_rate_idx", S_IWUSR, dir_sta, asr_hw, &asr_dbgfs_rc_fixed_rate_idx_ops);
		if (IS_ERR_OR_NULL(file))
			goto error_after_dir;

		file = debugfs_create_file("rx_rate", S_IRUSR | S_IWUSR, dir_sta, asr_hw, &asr_dbgfs_last_rx_ops);
		if (IS_ERR_OR_NULL(file))
			goto error_after_dir;

		if (asr_hw->mod_params->ht_on)
			nb_rx_rate += N_HT;

		//if (asr_hw->mod_params->vht_on)
		//    nb_rx_rate += N_VHT;

#ifdef CONFIG_ASR595X
		if (asr_hw->mod_params->he_on)
			nb_rx_rate += N_HE_SU + N_HE_MU;
#endif
		rate_stats->table = kzalloc(nb_rx_rate * sizeof(rate_stats->table[0]), GFP_KERNEL);
		if (!rate_stats->table)
			goto error_after_dir;

		rate_stats->size = nb_rx_rate;
		rate_stats->cpt = 0;
#ifdef CONFIG_ASR595X
		rate_stats->rate_cnt = 0;
#endif

		/* By default enable rate contoller */
		asr_debugfs->rc_config[sta_idx] = -1;

		/* Unless we already fix the rate for this station */
		list_for_each_entry_safe(rc_cfg, next, &asr_debugfs->rc_config_save, list) {
			if (jiffies_to_msecs(jiffies - rc_cfg->timestamp) > RC_CONFIG_DUR) {
				list_del(&rc_cfg->list);
				kfree(rc_cfg);
			} else if (!memcmp(rc_cfg->mac_addr, sta->mac_addr, ETH_ALEN)) {
				asr_debugfs->rc_config[sta_idx] = rc_cfg->rate;
				list_del(&rc_cfg->list);
				kfree(rc_cfg);
				break;
			}
		}

		if ((asr_debugfs->rc_config[sta_idx] >= 0) && asr_send_me_rc_set_rate(asr_hw, sta_idx, (u16)
										      asr_debugfs->rc_config[sta_idx]))
			asr_debugfs->rc_config[sta_idx] = -1;

	} else {
		/* unregister the sta */
		if (sta->stats.rx_rate.table) {
			kfree(sta->stats.rx_rate.table);
			sta->stats.rx_rate.table = NULL;
		}
		sta->stats.rx_rate.size = 0;
		sta->stats.rx_rate.cpt = 0;
#ifdef CONFIG_ASR595X
		sta->stats.rx_rate.rate_cnt = 0;
#endif

		/* If fix rate was set for this station, save the configuration in case
		   we reconnect to this station within RC_CONFIG_DUR msec */
		if (asr_debugfs->rc_config[sta_idx] >= 0) {
			struct asr_rc_config_save *rc_cfg;
			rc_cfg = kmalloc(sizeof(*rc_cfg), GFP_KERNEL);
			if (rc_cfg) {
				rc_cfg->rate = asr_debugfs->rc_config[sta_idx];
				rc_cfg->timestamp = jiffies;
				memcpy(rc_cfg->mac_addr, sta->mac_addr, ETH_ALEN);
				list_add_tail(&rc_cfg->list, &asr_debugfs->rc_config_save);
			}
		}

		debugfs_remove_recursive(asr_debugfs->dir_sta[sta_idx]);
		asr_debugfs->dir_sta[sta->sta_idx] = NULL;
	}

	return;

error_after_dir:
	debugfs_remove_recursive(asr_debugfs->dir_sta[sta_idx]);
	asr_debugfs->dir_sta[sta->sta_idx] = NULL;
error:
	dev_err(asr_hw->dev, "Error while (un)registering debug entry for sta %d\n", sta_idx);
}

void _asr_dbgfs_rc_stat_write(struct asr_debugfs *asr_debugfs, uint8_t sta_idx)
{
	uint8_t widx = asr_debugfs->rc_write;
	if (asr_debugfs->rc_sta[widx] != 0XFF) {
		WARN(1, "Overlap in debugfs rc_sta table\n");
	}

	asr_debugfs->rc_sta[widx] = sta_idx;
	widx = (widx + 1) % ARRAY_SIZE(asr_debugfs->rc_sta);
	asr_debugfs->rc_write = widx;

	schedule_work(&asr_debugfs->rc_stat_work);
}

void asr_dbgfs_register_rc_stat(struct asr_hw *asr_hw, struct asr_sta *sta)
{
	_asr_dbgfs_rc_stat_write(&asr_hw->debugfs, sta->sta_idx);
}

void asr_dbgfs_unregister_rc_stat(struct asr_hw *asr_hw, struct asr_sta *sta)
{
	_asr_dbgfs_rc_stat_write(&asr_hw->debugfs, sta->sta_idx);
}

int asr_dbgfs_register(struct asr_hw *asr_hw, const char *name)
{

	struct dentry *phyd = asr_hw->wiphy->debugfsdir;
	struct dentry *dir_rc;
	struct asr_debugfs *asr_debugfs = &asr_hw->debugfs;
	struct dentry *dir_drv, *dir_diags;

	if (!(dir_drv = debugfs_create_dir(name, phyd)))
		return -ENOMEM;

	asr_debugfs->dir = dir_drv;
	asr_debugfs->unregistering = false;

	if (!(dir_diags = debugfs_create_dir("diags", dir_drv)))
		goto err;

	if (!(dir_rc = debugfs_create_dir("rc", dir_drv)))
		goto err;
	asr_debugfs->dir_rc = dir_rc;
	INIT_WORK(&asr_debugfs->rc_stat_work, asr_rc_stat_work);
	INIT_LIST_HEAD(&asr_debugfs->rc_config_save);
	asr_debugfs->rc_write = asr_debugfs->rc_read = 0;
	memset(asr_debugfs->rc_sta, 0xFF, sizeof(asr_debugfs->rc_sta));

	DEBUGFS_ADD_U32(tcp_pacing_shift, dir_drv, &asr_hw->tcp_pacing_shift, S_IWUSR | S_IRUSR);
	DEBUGFS_ADD_FILE(stats, dir_drv, S_IWUSR | S_IRUSR);
	DEBUGFS_ADD_FILE(sys_stats, dir_drv, S_IRUSR);
	DEBUGFS_ADD_FILE(txq, dir_drv, S_IRUSR);
	DEBUGFS_ADD_FILE(acsinfo, dir_drv, S_IRUSR);

#if 0				//def CONFIG_ASR_MUMIMO_TX
	DEBUGFS_ADD_FILE(mu_group, dir_drv, S_IRUSR);
#endif

#ifdef CONFIG_ASR_P2P_DEBUGFS
	{
		/* Create a p2p directory */
		struct dentry *dir_p2p;
		if (!(dir_p2p = debugfs_create_dir("p2p", dir_drv)))
			goto err;

		/* Add file allowing to control Opportunistic PS */
		DEBUGFS_ADD_FILE(oppps, dir_p2p, S_IRUSR);
		/* Add file allowing to control Notice of Absence */
		DEBUGFS_ADD_FILE(noa, dir_p2p, S_IRUSR);
	}
#endif /* CONFIG_ASR_P2P_DEBUGFS */

#if 0
	if (asr_dbgfs_register_fw_dump(asr_hw, dir_drv, dir_diags))
		goto err;
#endif

	DEBUGFS_ADD_FILE(fw_dbg, dir_diags, S_IWUSR | S_IRUSR);

#if 0
	if (!asr_fw_trace_init(&asr_hw->debugfs.fw_trace, asr_ipc_fw_trace_desc_get(asr_hw))) {
		DEBUGFS_ADD_FILE(fw_trace, dir_diags, S_IWUSR | S_IRUSR);
		if (asr_hw->debugfs.fw_trace.buf.nb_compo)
			DEBUGFS_ADD_FILE(fw_trace_level, dir_diags, S_IWUSR | S_IRUSR);
	} else {
		asr_debugfs->fw_trace.buf.data = NULL;
	}
#endif

#if 0				//def CONFIG_ASR_RADAR
	{
		struct dentry *dir_radar, *dir_sec;
		if (!(dir_radar = debugfs_create_dir("radar", dir_drv)))
			goto err;

		DEBUGFS_ADD_FILE(pulses_prim, dir_radar, S_IRUSR);
		DEBUGFS_ADD_FILE(detected, dir_radar, S_IRUSR);
		DEBUGFS_ADD_FILE(enable, dir_radar, S_IRUSR);

		if (asr_hw->phy_cnt == 2) {
			DEBUGFS_ADD_FILE(pulses_sec, dir_radar, S_IRUSR);

			if (!(dir_sec = debugfs_create_dir("sec", dir_radar)))
				goto err;

			DEBUGFS_ADD_FILE(band, dir_sec, S_IWUSR | S_IRUSR);
			DEBUGFS_ADD_FILE(type, dir_sec, S_IWUSR | S_IRUSR);
			DEBUGFS_ADD_FILE(prim20, dir_sec, S_IWUSR | S_IRUSR);
			DEBUGFS_ADD_FILE(center1, dir_sec, S_IWUSR | S_IRUSR);
			DEBUGFS_ADD_FILE(center2, dir_sec, S_IWUSR | S_IRUSR);
			DEBUGFS_ADD_FILE(set, dir_sec, S_IWUSR | S_IRUSR);
		}
	}
#endif /* CONFIG_ASR_RADAR */
	return 0;

err:
	asr_dbgfs_unregister(asr_hw);
	return -ENOMEM;
}

void asr_dbgfs_unregister(struct asr_hw *asr_hw)
{
	struct asr_debugfs *asr_debugfs = &asr_hw->debugfs;
	struct asr_rc_config_save *cfg, *next;
	list_for_each_entry_safe(cfg, next, &asr_debugfs->rc_config_save, list) {
		list_del(&cfg->list);
		kfree(cfg);
	}

#if 0
	asr_fw_trace_deinit(&asr_hw->debugfs.fw_trace);
#endif

	if (!asr_hw->debugfs.dir)
		return;

	asr_debugfs->unregistering = true;
	//flush_work(&asr_debugfs->helper_work);
	flush_work(&asr_debugfs->rc_stat_work);
	debugfs_remove_recursive(asr_hw->debugfs.dir);
	asr_hw->debugfs.dir = NULL;
}
