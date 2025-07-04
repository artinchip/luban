/******************************************************************************
 *
 * Copyright(c) 2007 - 2017  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

/*@************************************************************
 * include files
 ************************************************************/
#include "mp_precomp.h"
#include "phydm_precomp.h"

#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
	#if WPP_SOFTWARE_TRACE
		#include "PhyDM_Adaptivity.tmh"
	#endif
#endif
#ifdef PHYDM_SUPPORT_ADAPTIVITY
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
boolean
phydm_check_channel_plan(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;
	void *adapter = dm->adapter;
	PMGNT_INFO mgnt_info = &((PADAPTER)adapter)->MgntInfo;

	if (mgnt_info->RegEnableAdaptivity != 2)
		return false;

	if (!dm->carrier_sense_enable) { /*@check domain Code for adaptivity or CarrierSense*/
		if ((*dm->band_type == ODM_BAND_5G) &&
		    !(adapt->regulation_5g == REGULATION_ETSI || adapt->regulation_5g == REGULATION_WW)) {
			PHYDM_DBG(dm, DBG_ADPTVTY,
				  "adaptivity skip 5G domain code : %d\n",
				  adapt->regulation_5g);
			return true;
		} else if ((*dm->band_type == ODM_BAND_2_4G) &&
			   !(adapt->regulation_2g == REGULATION_ETSI || adapt->regulation_2g == REGULATION_WW)) {
			PHYDM_DBG(dm, DBG_ADPTVTY,
				  "adaptivity skip 2.4G domain code : %d\n",
				  adapt->regulation_2g);
			return true;
		} else if ((*dm->band_type != ODM_BAND_2_4G) && (*dm->band_type != ODM_BAND_5G)) {
			PHYDM_DBG(dm, DBG_ADPTVTY,
				  "adaptivity neither 2G nor 5G band, return\n");
			return true;
		}
	} else {
		if ((*dm->band_type == ODM_BAND_5G) &&
		    !(adapt->regulation_5g == REGULATION_MKK || adapt->regulation_5g == REGULATION_WW)) {
			PHYDM_DBG(dm, DBG_ADPTVTY,
				  "CarrierSense skip 5G domain code : %d\n",
				  adapt->regulation_5g);
			return true;
		} else if ((*dm->band_type == ODM_BAND_2_4G) &&
			   !(adapt->regulation_2g == REGULATION_MKK || adapt->regulation_2g == REGULATION_WW)) {
			PHYDM_DBG(dm, DBG_ADPTVTY,
				  "CarrierSense skip 2.4G domain code : %d\n",
				  adapt->regulation_2g);
			return true;
		} else if ((*dm->band_type != ODM_BAND_2_4G) && (*dm->band_type != ODM_BAND_5G)) {
			PHYDM_DBG(dm, DBG_ADPTVTY,
				  "CarrierSense neither 2G nor 5G band, return\n");
			return true;
		}
	}

	return false;
}

boolean
phydm_soft_ap_special_set(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;
	boolean disable_ap_adapt_setting = false;

	if (dm->soft_ap_mode != NULL) {
		if (*dm->soft_ap_mode != 0 &&
		    (dm->soft_ap_special_setting & BIT(0)))
			disable_ap_adapt_setting = true;
		else
			disable_ap_adapt_setting = false;
		PHYDM_DBG(dm, DBG_ADPTVTY,
			  "soft_ap_setting = %x, soft_ap = %d, dis_ap_adapt = %d\n",
			  dm->soft_ap_special_setting, *dm->soft_ap_mode,
			  disable_ap_adapt_setting);
	}

	return disable_ap_adapt_setting;
}

boolean
phydm_ap_num_check(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;
	boolean dis_adapt = false;

	if  (dm->ap_total_num > adapt->ap_num_th)
		dis_adapt = true;
	else
		dis_adapt = false;

	PHYDM_DBG(dm, DBG_ADPTVTY, "AP total num = %d, AP num threshold = %d\n",
		  dm->ap_total_num, adapt->ap_num_th);
	return dis_adapt;
}

void phydm_check_adaptivity(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;
	boolean disable_adapt = false;

	if (!adapt->mode_cvrt_en)
		return;

	if (phydm_check_channel_plan(dm) || phydm_ap_num_check(dm) ||
	    phydm_soft_ap_special_set(dm))
		disable_adapt = true;

	if (*dm->edcca_mode == PHYDM_EDCCA_ADAPT_MODE && disable_adapt)
		*dm->edcca_mode = PHYDM_EDCCA_NORMAL_MODE;
	else if (*dm->edcca_mode == PHYDM_EDCCA_NORMAL_MODE && !disable_adapt)
		*dm->edcca_mode = PHYDM_EDCCA_ADAPT_MODE;
}

void phydm_set_l2h_th_ini_win(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	 /*@ [New Format: JGR3]IGI-idx:45 = RSSI:35 = -65dBm*/
	if (dm->support_ic_type & ODM_IC_JGR3_SERIES) {
		if (dm->support_ic_type & (ODM_RTL8822C | ODM_RTL8733B |\
		    ODM_RTL8735B | ODM_RTL8730A | ODM_RTL8822E))
			dm->th_l2h_ini = 45;
		else if (dm->support_ic_type & (ODM_RTL8814B | ODM_RTL8814C))
			dm->th_l2h_ini = 49;
	} else if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
	 /*@ [Old Format] -11+base(50) = IGI_idx:39 = RSSI:29 = -71dBm*/
		if (dm->support_ic_type & (ODM_RTL8821 | ODM_RTL8812)) {
			dm->th_l2h_ini = -17;
		} else {
			if (*dm->band_type == ODM_BAND_5G)
				dm->th_l2h_ini = -14;
			else if (*dm->band_type == ODM_BAND_2_4G)
				dm->th_l2h_ini = -9;
		}
	} else { /*ODM_IC_11N_SERIES*/
		dm->th_l2h_ini = -9;
	}
}

void phydm_l2h_ini_recorder_reset(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;
	struct phydm_l2h_ini_recorder_strcut *adapt_rc = &adapt->l2h_ini_recorder_t;

	PHYDM_DBG(dm, DBG_ADPTVTY, "%s ======>\n", __func__);

	odm_memory_set(dm, &adapt_rc->l2h_ini_bitmap, 0,
		       sizeof(struct phydm_l2h_ini_recorder_strcut));
}


void phydm_l2h_ini_recorder(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;
	struct phydm_l2h_ini_recorder_strcut *adapt_rc = &adapt->l2h_ini_recorder_t;
	u32 low_rate_tx_fail_cnt = dm->low_rate_tx_fail_cnt;
	s8 l2h_ini_curr = dm->th_l2h_ini;
	s8 l2h_ini_pre = adapt_rc->l2h_ini_hist[0];
	s8 l2h_ini_down = 0;

	if (!dm->is_linked || adapt->is_adapt_by_dig || adapt->rts_drop_en)
		return;

	PHYDM_DBG(dm, DBG_ADPTVTY, "%s ======>\n", __func__);

	if (dm->first_connect) {
		phydm_l2h_ini_recorder_reset(dm);
		adapt_rc->l2h_ini_hist[0] = l2h_ini_curr;
		return;
	}

	l2h_ini_down = (l2h_ini_curr < l2h_ini_pre) ? 1 : 0;
	adapt_rc->l2h_ini_bitmap = ((adapt_rc->l2h_ini_bitmap << 1) & 0xfe) | l2h_ini_down;

	adapt_rc->l2h_ini_hist[3] = adapt_rc->l2h_ini_hist[2];
	adapt_rc->l2h_ini_hist[2] = adapt_rc->l2h_ini_hist[1];
	adapt_rc->l2h_ini_hist[1] = adapt_rc->l2h_ini_hist[0];
	adapt_rc->l2h_ini_hist[0] = l2h_ini_curr;

	adapt_rc->low_rate_tx_fail_hist[3] = adapt_rc->low_rate_tx_fail_hist[2];
	adapt_rc->low_rate_tx_fail_hist[2] = adapt_rc->low_rate_tx_fail_hist[1];
	adapt_rc->low_rate_tx_fail_hist[1] = adapt_rc->low_rate_tx_fail_hist[0];
	adapt_rc->low_rate_tx_fail_hist[0] = low_rate_tx_fail_cnt;

	PHYDM_DBG(dm, DBG_ADPTVTY, "l2h_ini_hist[3:0] = {0x%x, 0x%x, 0x%x, 0x%x}\n",
		  adapt_rc->l2h_ini_hist[3], adapt_rc->l2h_ini_hist[2],
		  adapt_rc->l2h_ini_hist[1], adapt_rc->l2h_ini_hist[0]);
	PHYDM_DBG(dm, DBG_ADPTVTY, "low_rate_tx_fail_hist[3:0] = {%d, %d, %d, %d}\n",
		  adapt_rc->low_rate_tx_fail_hist[3], adapt_rc->low_rate_tx_fail_hist[2],
		  adapt_rc->low_rate_tx_fail_hist[1], adapt_rc->low_rate_tx_fail_hist[0]);
	PHYDM_DBG(dm, DBG_ADPTVTY, "l2h_ini_bitmap = {%d, %d, %d, %d} = 0x%x\n",
		  (u8)((adapt_rc->l2h_ini_bitmap & BIT(3)) >> 3),
		  (u8)((adapt_rc->l2h_ini_bitmap & BIT(2)) >> 2),
		  (u8)((adapt_rc->l2h_ini_bitmap & BIT(1)) >> 1),
		  (u8)(adapt_rc->l2h_ini_bitmap & BIT(0)),
		  adapt_rc->l2h_ini_bitmap);
}

void phydm_rts_drop_chk(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;
	u32 time_tmp = 0;

	if (!dm->is_linked || adapt->is_adapt_by_dig)
		return;

	PHYDM_DBG(dm, DBG_ADPTVTY, "%s ======>\n", __func__);

	if (dm->rts_drop_cnt > 0) {
		adapt->rts_drop_en = 1;
		adapt->rts_drop_limit_time = dm->phydm_sys_up_time;
	}

	/*@== Fix l2h_ini to l2h_ini_range_min================================*/
	if (adapt->rts_drop_en) {
		PHYDM_DBG(dm, DBG_ADPTVTY,
			  "[RTS DROP!] rts_drop_limit_time=%d, phydm_sys_up_time=%d\n",
			  adapt->rts_drop_limit_time, dm->phydm_sys_up_time);

		time_tmp = adapt->rts_drop_limit_time + L2H_INI_LIMIT_PERIOD;
		dm->th_l2h_ini = adapt->l2h_ini_range_min;

		if (time_tmp < dm->phydm_sys_up_time && dm->rts_drop_cnt == 0) {
			adapt->rts_drop_en = 0;
			PHYDM_DBG(dm, DBG_ADPTVTY, "rts_drop_cnt=%d\n",
				  dm->rts_drop_cnt);
		}
	}
	return;
}


void phydm_l2h_ini_damping_chk(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;
	struct phydm_l2h_ini_recorder_strcut *adapt_rc = &adapt->l2h_ini_recorder_t;
	u8 l2h_ini_bitmap_4bit = adapt_rc->l2h_ini_bitmap & 0xf;
	s8 diff1 = 0, diff2 = 0, min_l2h_ini = 0x7f;
	u32 tx_fail_low_th = adapt->low_rate_tx_fail_th[0];
	u32 tx_fail_high_th = adapt->low_rate_tx_fail_th[1];
	u32 tx_fail_high_th2 = adapt->low_rate_tx_fail_th[2];
	u8 tx_fail_pattern_match = 0;
	u32 time_tmp = 0;
	u8 i = 0;

	if (!dm->is_linked || adapt->is_adapt_by_dig || adapt->rts_drop_en)
		return;

	PHYDM_DBG(dm, DBG_ADPTVTY, "%s ======>\n", __func__);

	/*@== Release Damping ================================================*/
	if (adapt_rc->damping_limit_en) {
		PHYDM_DBG(dm, DBG_ADPTVTY,
			  "[Damping Limit!] limit_time=%d, phydm_sys_up_time=%d\n",
			  adapt_rc->limit_time, dm->phydm_sys_up_time);

		time_tmp = adapt_rc->limit_time + L2H_INI_LIMIT_PERIOD;

		if (DIFF_2(dm->low_rate_tx_fail_cnt, adapt_rc->limit_low_rate_tx_fail) > 3 ||
		    time_tmp < dm->phydm_sys_up_time) {
			adapt_rc->damping_limit_en = 0;
			PHYDM_DBG(dm, DBG_ADPTVTY, "low_rate_tx_fail_cnt=%d, limit_low_rate_tx_fail=%d\n",
				  dm->low_rate_tx_fail_cnt, adapt_rc->limit_low_rate_tx_fail);
		}
		return;
	}

	/*@== Damping Pattern Check===========================================*/
	PHYDM_DBG(dm, DBG_ADPTVTY, "low_rate_tx_fail_th{H, L}= {%d,%d}\n", tx_fail_high_th, tx_fail_low_th);

	switch (l2h_ini_bitmap_4bit) {
	case 0x5:
	/*@ 4b'0101 
	* L2H_INI:     [3]up(40)  ->[2]down(38)->[1]up(40)  ->[0]down(38)->[new](Lock @ 38)
	* low_rate_tx_fail_cnt: [3] >high1 ->[2] <low   ->[1] >high1 ->[0] <low   ->[new]   <low
	*
	* L2H_INI:     [3]up(45)  ->[2]down(40)->[1]up(42)  ->[0]down(37)->[new](Lock @ 37)
	* low_rate_tx_fail_cnt: [3] >high2 ->[2] <low   ->[1] >high2 ->[0] <low   ->[new]   <low
	*/
		if (adapt_rc->l2h_ini_hist[0] < adapt_rc->l2h_ini_hist[1])
			diff1 = adapt_rc->l2h_ini_hist[1] - adapt_rc->l2h_ini_hist[0];

		if (adapt_rc->l2h_ini_hist[2] < adapt_rc->l2h_ini_hist[3])
			diff2 = adapt_rc->l2h_ini_hist[3] - adapt_rc->l2h_ini_hist[2];

		if (adapt_rc->low_rate_tx_fail_hist[0] < tx_fail_low_th &&
		    adapt_rc->low_rate_tx_fail_hist[1] > tx_fail_high_th &&
		    adapt_rc->low_rate_tx_fail_hist[2] < tx_fail_low_th &&
		    adapt_rc->low_rate_tx_fail_hist[3] > tx_fail_high_th) {
		    /*@Check each rts drop element*/
			tx_fail_pattern_match = 1;
		}
		break;
	case 0x9:
	/*@ 4b'1001
	* L2H_INI:     [3]down(40)->[2]up(42)->[1]up(44)  ->[0]down(39)->[new](Lock @ 39)
	* low_rate_tx_fail_cnt: [3]  <low  ->[2] <low ->[1] >high2 ->[0] <low   ->[new]  <low
	*/
		if (adapt_rc->l2h_ini_hist[0] < adapt_rc->l2h_ini_hist[1])
			diff1 = adapt_rc->l2h_ini_hist[1] - adapt_rc->l2h_ini_hist[0];

		if (adapt_rc->l2h_ini_hist[2] > adapt_rc->l2h_ini_hist[3])
			diff2 = adapt_rc->l2h_ini_hist[2] - adapt_rc->l2h_ini_hist[3];

		if (adapt_rc->low_rate_tx_fail_hist[0] < tx_fail_low_th &&
		    adapt_rc->low_rate_tx_fail_hist[1] > tx_fail_high_th2 &&
		    adapt_rc->low_rate_tx_fail_hist[2] < tx_fail_low_th &&
		    adapt_rc->low_rate_tx_fail_hist[3] < tx_fail_low_th) {
		    /*@Check each fa element*/
			tx_fail_pattern_match = 1;
		}
		break;
	default:
		break;
	}

	if (diff1 >= 2 && diff2 >= 2 && tx_fail_pattern_match) {
		for (i = 0; i < L2H_INI_RECORD_NUM; i++) {
			if (min_l2h_ini > adapt_rc->l2h_ini_hist[i])
				min_l2h_ini = adapt_rc->l2h_ini_hist[i];
		}

		adapt_rc->damping_limit_en = 1;
		adapt_rc->damping_limit_val = min_l2h_ini;
		adapt_rc->limit_time = dm->phydm_sys_up_time;
		adapt_rc->limit_low_rate_tx_fail = dm->low_rate_tx_fail_cnt;

		PHYDM_DBG(dm, DBG_ADPTVTY,
			  "[Start damping_limit!] l2h_ini_min=0x%x, limit_time=%d, limit_low_rate_tx_fail=%d\n",
			  adapt_rc->damping_limit_val,
			  adapt_rc->limit_time, adapt_rc->limit_low_rate_tx_fail);
	}

	PHYDM_DBG(dm, DBG_ADPTVTY, "damping_limit=%d\n", adapt_rc->damping_limit_en);
}

void phydm_low_rate_tx_fail_threshold_check(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;

	if (adapt->is_dbg_low_rate_tx_fail_th) {
		PHYDM_DBG(dm, DBG_ADPTVTY, "Manual Fix low_rate_tx_fail_th\n");
	} else {
		adapt->low_rate_tx_fail_th[0] = 2;
		adapt->low_rate_tx_fail_th[1] = 2;
		adapt->low_rate_tx_fail_th[2] = 5;
	}

	PHYDM_DBG(dm, DBG_ADPTVTY, "low_rate_tx_fail_th={%d,%d,%d}\n", adapt->low_rate_tx_fail_th[0],
		  adapt->low_rate_tx_fail_th[1], adapt->low_rate_tx_fail_th[2]);
}

s8 phydm_new_l2h_ini_by_low_rate_tx_fail(void *dm_void, s8 l2h_ini,
				  u8 *step_size)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;
	u32 low_rate_tx_fail_cnt = dm->low_rate_tx_fail_cnt;

	if (low_rate_tx_fail_cnt > adapt->low_rate_tx_fail_th[2])
		l2h_ini = l2h_ini - step_size[2];
	else if (low_rate_tx_fail_cnt > adapt->low_rate_tx_fail_th[1])
		l2h_ini = l2h_ini - step_size[1];
	else if (low_rate_tx_fail_cnt < adapt->low_rate_tx_fail_th[0])
		l2h_ini = l2h_ini + step_size[0];

	return l2h_ini;
}


void phydm_get_new_l2h_ini(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;
	struct phydm_l2h_ini_recorder_strcut *adapt_rc = &adapt->l2h_ini_recorder_t;
	u8 step[3] = {1, 2, 5};
	u32 low_rate_tx_fail_cnt = dm->low_rate_tx_fail_cnt;
	u32 low_rate_tx_ok_cnt = dm->low_rate_tx_ok_cnt;
	s8 l2h_ini = dm->th_l2h_ini;

	if (!dm->is_linked || adapt->is_adapt_by_dig || adapt->rts_drop_en)
		return;

	if (adapt_rc->damping_limit_en) {
		dm->th_l2h_ini = adapt_rc->damping_limit_val;
		PHYDM_DBG(dm, DBG_ADPTVTY, "[Limit by Damping] l2h_ini: 0x%x -> 0x%x\n",
			  l2h_ini, dm->th_l2h_ini);
		return;
	}

	l2h_ini = phydm_new_l2h_ini_by_low_rate_tx_fail(dm, l2h_ini, step);

	PHYDM_DBG(dm, DBG_ADPTVTY, "step = {-%d, -%d, +%d}\n", step[2], step[1],
		  step[0]);

	/*@Check th_l2h_ini by dyn-upper/lower bound */
	if (l2h_ini < adapt->l2h_ini_range_min)
		l2h_ini = adapt->l2h_ini_range_min;

	if (l2h_ini > adapt->l2h_ini_range_max)
		l2h_ini = adapt->l2h_ini_range_max;

	PHYDM_DBG(dm, DBG_ADPTVTY, "low_rate_tx_fail_cnt = %d, low_rate_tx_ok_cnt = %d, l2h_ini: 0x%x -> 0x%x\n",
		  low_rate_tx_fail_cnt, low_rate_tx_ok_cnt, dm->th_l2h_ini,
		  l2h_ini);

	dm->th_l2h_ini = l2h_ini;
}

void phydm_dyn_l2h_ini(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;

	if (dm->rssi_min <= 20 || !dm->is_linked) {
		phydm_set_l2h_th_ini_win(dm);
		PHYDM_DBG(dm, DBG_ADPTVTY, "th_l2h_ini = %d\n", dm->th_l2h_ini);
		return;
	}

	/*Check rts drop*/
	phydm_rts_drop_chk(dm);

	/*Record l2h_ini History*/
	phydm_l2h_ini_recorder(dm);

	/*@l2h_ini Damping Check*/
	phydm_l2h_ini_damping_chk(dm);

	/*@low_rate_tx_fail threshold decision */
	phydm_low_rate_tx_fail_threshold_check(dm);

	/*Select new l2h_ini by tx_fail */
	phydm_get_new_l2h_ini(dm);
	PHYDM_DBG(dm, DBG_ADPTVTY, "Adjust l2h init @ linked, th_l2h_ini = %d\n",
		  dm->th_l2h_ini);
}

#endif

void phydm_dig_up_bound_lmt_en(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;

	if (*dm->edcca_mode != PHYDM_EDCCA_ADAPT_MODE ||
	    !dm->is_linked) {
		adapt->igi_up_bound_lmt_cnt = 0;
		adapt->igi_lmt_en = false;
		return;
	}

	if (dm->total_tp > 1) {
		adapt->igi_lmt_en = true;
		adapt->igi_up_bound_lmt_cnt = adapt->igi_up_bound_lmt_val;
		PHYDM_DBG(dm, DBG_ADPTVTY,
			  "TP >1, Start limit IGI upper bound\n");
	} else {
		if (adapt->igi_up_bound_lmt_cnt == 0)
			adapt->igi_lmt_en = false;
		else
			adapt->igi_up_bound_lmt_cnt--;
	}

	PHYDM_DBG(dm, DBG_ADPTVTY, "IGI_lmt_cnt = %d\n",
		  adapt->igi_up_bound_lmt_cnt);
}

void phydm_set_edcca_threshold(void *dm_void, s8 H2L, s8 L2H)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (dm->support_ic_type & ODM_IC_JGR3_SERIES) {
		odm_set_bb_reg(dm, R_0x84c, MASKBYTE2, (u8)L2H + 0x80);
		odm_set_bb_reg(dm, R_0x84c, MASKBYTE3, (u8)H2L + 0x80);
	} else if (dm->support_ic_type & ODM_IC_11N_SERIES) {
		odm_set_bb_reg(dm, R_0xc4c, MASKBYTE0, (u8)L2H);
		odm_set_bb_reg(dm, R_0xc4c, MASKBYTE2, (u8)H2L);
	} else if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
		odm_set_bb_reg(dm, R_0x8a4, MASKBYTE0, (u8)L2H);
		odm_set_bb_reg(dm, R_0x8a4, MASKBYTE1, (u8)H2L);
	}
}

void phydm_mac_edcca_state(void *dm_void, enum phydm_mac_edcca_type state)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (state == PHYDM_IGNORE_EDCCA) {
		/*@ignore EDCCA*/
		odm_set_mac_reg(dm, R_0x520, BIT(15), 1);
		/*@enable EDCCA count down*/
		odm_set_mac_reg(dm, R_0x524, BIT(11), 0);
	} else { /*@don't set MAC ignore EDCCA signal*/
		/*@don't ignore EDCCA*/
		odm_set_mac_reg(dm, R_0x520, BIT(15), 0);
		/*@disable EDCCA count down*/
		odm_set_mac_reg(dm, R_0x524, BIT(11), 1);
	}
	PHYDM_DBG(dm, DBG_ADPTVTY, "EDCCA enable state = %d\n", state);
}

void phydm_search_pwdb_lower_bound(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;
	u32 value32 = 0, reg_value32 = 0;
	u8 cnt = 0, try_count = 0;
	u8 tx_edcca1 = 0;
	boolean is_adjust = true;
	s8 th_l2h, th_h2l, igi_target_dc = 0x32;
	s8 diff = 0;
	s8 IGI = adapt->igi_base + 30 + dm->th_l2h_ini - dm->th_edcca_hl_diff;

	halrf_rf_lna_setting(dm, HALRF_LNA_DISABLE);
	diff = igi_target_dc - IGI;
	th_l2h = dm->th_l2h_ini + diff;
	if (th_l2h > 10)
		th_l2h = 10;

	th_h2l = th_l2h - dm->th_edcca_hl_diff;
	phydm_set_edcca_threshold(dm, th_h2l, th_l2h);
	ODM_delay_ms(30);

	while (is_adjust) {
		/*@check CCA status*/
		/*set debug port to 0x0*/
		if (phydm_set_bb_dbg_port(dm, DBGPORT_PRI_1, 0x0)) {
			reg_value32 = phydm_get_bb_dbg_port_val(dm);

			while (reg_value32 & BIT(3) && try_count < 3) {
				ODM_delay_ms(3);
				try_count = try_count + 1;
				reg_value32 = phydm_get_bb_dbg_port_val(dm);
			}
			phydm_release_bb_dbg_port(dm);
			try_count = 0;
		}

		/*@count EDCCA signal = 1 times*/
		for (cnt = 0; cnt < 20; cnt++) {
			if (phydm_set_bb_dbg_port(dm, DBGPORT_PRI_1,
						  adapt->adaptivity_dbg_port)) {
				value32 = phydm_get_bb_dbg_port_val(dm);
				phydm_release_bb_dbg_port(dm);
			}

			if (value32 & BIT(30) && dm->support_ic_type &
						 (ODM_RTL8723B | ODM_RTL8188E))
				tx_edcca1 = tx_edcca1 + 1;
			else if (value32 & BIT(29))
				tx_edcca1 = tx_edcca1 + 1;
		}

		if (tx_edcca1 > 1) {
			IGI = IGI - 1;
			th_l2h = th_l2h + 1;
			if (th_l2h > 10)
				th_l2h = 10;

			th_h2l = th_l2h - dm->th_edcca_hl_diff;
			phydm_set_edcca_threshold(dm, th_h2l, th_l2h);
			tx_edcca1 = 0;
			if (th_l2h == 10)
				is_adjust = false;

		} else {
			is_adjust = false;
		}
	}

	adapt->adapt_igi_up = IGI - ADAPT_DC_BACKOFF;
	adapt->h2l_lb = th_h2l + ADAPT_DC_BACKOFF;
	adapt->l2h_lb = th_l2h + ADAPT_DC_BACKOFF;

	halrf_rf_lna_setting(dm, HALRF_LNA_ENABLE);
	phydm_set_edcca_threshold(dm, 0x7f, 0x7f); /*resume to no link state*/
}

boolean phydm_re_search_condition(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adaptivity = &dm->adaptivity;
	u8 adaptivity_igi_upper = adaptivity->adapt_igi_up + ADAPT_DC_BACKOFF;

	if (adaptivity_igi_upper <= 0x26)
		return true;
	else
		return false;
}

void phydm_set_l2h_th_ini(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	 /*@ [New Format: JGR3]IGI-idx:45 = RSSI:35 = -65dBm*/
	if (dm->support_ic_type & ODM_IC_JGR3_SERIES) {
		if (dm->support_ic_type & (ODM_RTL8822C | ODM_RTL8733B |\
		    ODM_RTL8735B | ODM_RTL8730A | ODM_RTL8822E))
			dm->th_l2h_ini = 45;
		else if (dm->support_ic_type & (ODM_RTL8814B | ODM_RTL8814C))
			dm->th_l2h_ini = 49;
	} else if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
	 /*@ [Old Format] -11+base(50) = IGI_idx:39 = RSSI:29 = -71dBm*/
		if (dm->support_ic_type & (ODM_RTL8821 | ODM_RTL8812))
			dm->th_l2h_ini = -17;
		else
			dm->th_l2h_ini = -14;
	} else { /*ODM_IC_11N_SERIES*/
		if (dm->support_ic_type & ODM_RTL8721D)
			dm->th_l2h_ini = -14;
		else
			dm->th_l2h_ini = -11;
	}
}

void phydm_set_l2h_th_ini_carrier_sense(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (dm->support_ic_type & ODM_IC_JGR3_SERIES)
		dm->th_l2h_ini = 60; /*@ -50dBm*/
	else
		dm->th_l2h_ini = 10; /*@ -50dBm*/
}

void phydm_set_forgetting_factor(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (*dm->edcca_mode != PHYDM_EDCCA_ADAPT_MODE)
		return;

	if (dm->support_ic_type & (ODM_RTL8821C | ODM_RTL8822B | ODM_RTL8814A |
				  ODM_RTL8195B))
		odm_set_bb_reg(dm, R_0x8a0, BIT(1) | BIT(0), 0);
}

void phydm_edcca_decision_opt(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (*dm->edcca_mode != PHYDM_EDCCA_ADAPT_MODE)
		return;

	if (dm->support_ic_type & ODM_RTL8822B)
		odm_set_bb_reg(dm, R_0x8dc, BIT(5), 0x1);
	else if (dm->support_ic_type & (ODM_RTL8197F | ODM_RTL8192F))
		odm_set_bb_reg(dm, R_0xce8, BIT(13), 0x1);
	else if (dm->support_ic_type & ODM_IC_JGR3_SERIES)
		odm_set_bb_reg(dm, R_0x844, BIT(30) | BIT(29), 0x0);
}

void phydm_adaptivity_debug(void *dm_void, char input[][16], u32 *_used,
			    char *output, u32 *_out_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adaptivity = &dm->adaptivity;
	struct phydm_l2h_ini_recorder_strcut *adapt_rc = &adaptivity->l2h_ini_recorder_t;
	u32 used = *_used;
	u32 out_len = *_out_len;
	char help[] = "-h";
	u32 dm_value[10] = {0};
	u8 i = 0, input_idx = 0;
	u32 reg_value32 = 0;
	s8 h2l_diff = 0;

	for (i = 0; i < 6; i++) {
		PHYDM_SSCANF(input[i + 1], DCMD_HEX, &dm_value[i]);
		input_idx++;
	}
	if (strcmp(input[1], help) == 0) {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "Show adaptivity message: {0}\n");
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "Enter debug mode: {1} {th_l2h_ini} {th_edcca_hl_diff}\n");
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "Leave debug mode: {2}\n");
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "RTS drop debug mode: {3} {en} {low_rate_tx_fail_th[0]} {low_rate_tx_fail_th[1]} {low_rate_tx_fail_th[2]}\n");
		goto out;
	}

	if (input_idx == 0)
		return;

	if (dm_value[0] == PHYDM_ADAPT_DEBUG) {
		adaptivity->debug_mode = true;
		if (dm_value[1] != 0)
			dm->th_l2h_ini = (s8)dm_value[1];
		if (dm_value[2] != 0)
			dm->th_edcca_hl_diff = (s8)dm_value[2];
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "th_l2h_ini = %d, th_edcca_hl_diff = %d\n",
			 dm->th_l2h_ini, dm->th_edcca_hl_diff);
	} else if (dm_value[0] == PHYDM_ADAPT_RESUME) {
		adaptivity->debug_mode = false;
		dm->th_l2h_ini = adaptivity->th_l2h_ini_backup;
		dm->th_edcca_hl_diff = adaptivity->th_edcca_hl_diff_backup;
	} else if (dm_value[0] == PHYDM_ADAPT_MSG) {
		PDM_SNPF(out_len, used, output + used, out_len - used,
			 "debug_mode = %s, th_l2h_ini = %d\n",
			 (adaptivity->debug_mode ? "TRUE" : "FALSE"),
			 dm->th_l2h_ini);
		if (dm->support_ic_type & ODM_IC_JGR3_SERIES) {
			reg_value32 = odm_get_bb_reg(dm, R_0x84c, MASKDWORD);
			h2l_diff = (s8)((0x00ff0000 & reg_value32) >> 16) -
				   (s8)((0xff000000 & reg_value32) >> 24);
		} else if (dm->support_ic_type & ODM_IC_11N_SERIES) {
			reg_value32 = odm_get_bb_reg(dm, R_0xc4c, MASKDWORD);
			h2l_diff = (s8)(0x000000ff & reg_value32) -
				   (s8)((0x00ff0000 & reg_value32) >> 16);
		} else if (dm->support_ic_type & ODM_IC_11AC_SERIES) {
			reg_value32 = odm_get_bb_reg(dm, R_0x8a4, MASKDWORD);
			h2l_diff = (s8)(0x000000ff & reg_value32) -
				   (s8)((0x0000ff00 & reg_value32) >> 8);
		}

		if (h2l_diff == 7)
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "adaptivity enable\n");
		else
			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "adaptivity disable\n");
	} else if (dm_value[0] == PHYDM_L2H_INI_DEBUG) {
		if (dm_value[1] == 1) {
			adaptivity->is_dbg_low_rate_tx_fail_th = true;
			adaptivity->low_rate_tx_fail_th[0] = (u32)dm_value[2];
			adaptivity->low_rate_tx_fail_th[1] = (u32)dm_value[3];
			adaptivity->low_rate_tx_fail_th[2] = (u32)dm_value[4];

			PDM_SNPF(out_len, used, output + used, out_len - used,
				 "Set low_rate_tx_fail_th={%d,%d,%d}\n",
				 adaptivity->low_rate_tx_fail_th[0],
				 adaptivity->low_rate_tx_fail_th[1],
				 adaptivity->low_rate_tx_fail_th[2]);
		} else {
			adaptivity->is_dbg_low_rate_tx_fail_th = false;
		}
	}

out:
	*_used = used;
	*_out_len = out_len;
}

void phydm_set_edcca_val(void *dm_void, u32 *val_buf, u8 val_len)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	if (val_len != 2) {
		PHYDM_DBG(dm, ODM_COMP_API,
			  "[Error][adaptivity]Need val_len = 2\n");
		return;
	}
	phydm_set_edcca_threshold(dm, (s8)val_buf[1], (s8)val_buf[0]);
}

boolean phydm_edcca_abort(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	void *adapter = dm->adapter;
	u32 is_fw_in_psmode = false;
#endif

	if (!(dm->support_ability & ODM_BB_ADAPTIVITY)) {
		PHYDM_DBG(dm, DBG_ADPTVTY, "adaptivity disable\n");
		return true;
	}

	if (dm->pause_ability & ODM_BB_ADAPTIVITY) {
		PHYDM_DBG(dm, DBG_ADPTVTY, "Return: Pause ADPTVTY in LV=%d\n",
			  dm->pause_lv_table.lv_adapt);
		return true;
	}

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	((PADAPTER)adapter)->HalFunc.GetHwRegHandler(adapter,
						      HW_VAR_FW_PSMODE_STATUS,
						      (u8 *)(&is_fw_in_psmode));

	/*@Disable EDCCA while under LPS mode, added by Roger, 2012.09.14.*/
	if (is_fw_in_psmode)
		return true;
#endif

	return false;
}

void phydm_edcca_thre_calc_jgr3(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;
	u8 igi = dig_t->cur_ig_value;
	s8 th_l2h = 0, th_h2l = 0;

	if (*dm->edcca_mode == PHYDM_EDCCA_ADAPT_MODE) {
		/*prevent pwdB clipping and result in Miss Detection*/
		adapt->l2h_dyn_min = (u8)(dm->th_l2h_ini - ADC_BACKOFF);

		if (igi < adapt->l2h_dyn_min)
			th_l2h = igi + ADC_BACKOFF;
		else
			th_l2h = dm->th_l2h_ini;

		th_h2l = th_l2h - dm->th_edcca_hl_diff;
	} else {
		th_l2h = MAX_2(igi + TH_L2H_DIFF_IGI, EDCCA_TH_L2H_LB);
		th_h2l = th_l2h - EDCCA_HL_DIFF_NORMAL;
	}
	adapt->th_l2h = th_l2h;
	adapt->th_h2l = th_h2l;

	phydm_set_edcca_threshold(dm, adapt->th_h2l, adapt->th_l2h);
}

void phydm_edcca_thre_calc(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;
	u8 igi = dig_t->cur_ig_value;
	s8 th_l2h = 0, th_h2l = 0;
	s8 diff = 0, igi_target = adapt->igi_base;

	if (dm->support_ic_type & ODM_IC_PWDB_EDCCA) {
		/*@fix EDCCA hang issue*/
		if (dm->support_ic_type & ODM_RTL8812) {
			/*@ADC_mask disable*/
			odm_set_bb_reg(dm, R_0x800, BIT(10), 1);
			/*@ADC_mask enable*/
			odm_set_bb_reg(dm, R_0x800, BIT(10), 0);
		}

		if (*dm->edcca_mode == PHYDM_EDCCA_ADAPT_MODE) {
			/*@Limit IGI upper bound for adaptivity*/
			phydm_dig_up_bound_lmt_en(dm);
			diff = igi_target - (s8)igi;
			th_l2h = dm->th_l2h_ini + diff;
			if (th_l2h > 10)
				th_l2h = 10;

			th_h2l = th_l2h - dm->th_edcca_hl_diff;
		} else {
			th_l2h = 70 - igi;
			th_h2l = th_l2h - EDCCA_HL_DIFF_NORMAL;
		}
		/*replace lower bound to prevent EDCCA always equal 1*/
		if (th_h2l < adapt->h2l_lb)
			th_h2l = adapt->h2l_lb;
		if (th_l2h < adapt->l2h_lb)
			th_l2h = adapt->l2h_lb;
		PHYDM_DBG(dm, DBG_ADPTVTY,
			  "adapt_igi_up=0x%x, l2h_lb = %d dBm, h2l_lb = %d dBm\n",
			  adapt->adapt_igi_up,
			  IGI_2_DBM(adapt->l2h_lb + adapt->adapt_igi_up),
			  IGI_2_DBM(adapt->h2l_lb + adapt->adapt_igi_up));
	} else { /* < JGR2 & N*/
		if (*dm->edcca_mode == PHYDM_EDCCA_ADAPT_MODE) {
			/*need to consider PwdB upper bound for 8814 later IC*/
			adapt->l2h_dyn_min = (u8)(dm->th_l2h_ini + igi_target);

			if (igi < adapt->l2h_dyn_min)
				th_l2h = igi;
			else
				th_l2h = adapt->l2h_dyn_min;

			th_h2l = th_l2h - dm->th_edcca_hl_diff;
		} else {
			th_l2h = MAX_2(igi + TH_L2H_DIFF_IGI, EDCCA_TH_L2H_LB);
			th_h2l = th_l2h - EDCCA_HL_DIFF_NORMAL;
		}
	}

	adapt->th_l2h = th_l2h;
	adapt->th_h2l = th_h2l;

	phydm_set_edcca_threshold(dm, adapt->th_h2l, adapt->th_l2h);
}
#endif

void phydm_set_edcca_threshold_api(void *dm_void)
{
#ifdef PHYDM_SUPPORT_ADAPTIVITY
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;

	if (*dm->edcca_mode != PHYDM_EDCCA_ADAPT_MODE)
		return;

	if (dm->support_ic_type & ODM_IC_JGR3_SERIES)
		phydm_edcca_thre_calc_jgr3(dm);
	else
		phydm_edcca_thre_calc(dm);

	PHYDM_DBG(dm, DBG_ADPTVTY,
		  "API :IGI = 0x%x, th_l2h = %d, th_h2l = %d\n",
		  dm->dm_dig_table.cur_ig_value, adapt->th_l2h, adapt->th_h2l);
#endif
}

void phydm_adaptivity_info_init(void *dm_void, enum phydm_adapinfo cmn_info,
				u32 value)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adaptivity = &dm->adaptivity;

	switch (cmn_info) {
	case PHYDM_ADAPINFO_CARRIER_SENSE_ENABLE:
		dm->carrier_sense_enable = (boolean)value;
		break;
	case PHYDM_ADAPINFO_TH_L2H_INI:
		dm->th_l2h_ini = (s8)value;
		dm->th_l2h_ini_custom = (s8)value;
		break;
	case PHYDM_ADAPINFO_TH_EDCCA_HL_DIFF:
		dm->th_edcca_hl_diff = (s8)value;
		break;
	case PHYDM_ADAPINFO_AP_NUM_TH:
		adaptivity->ap_num_th = (u8)value;
		break;
	case PHYDM_ADAPINFO_SWITCH_TH_L2H_INI_IN_BAND:
		adaptivity->switch_th_l2h_ini_in_band = (u8)value;
		break;
	default:
		break;
	}
}

void phydm_adaptivity_info_update(void *dm_void, enum phydm_adapinfo cmn_info,
				  u32 value)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;

	/*This init variable may be changed in run time.*/
	switch (cmn_info) {
	case PHYDM_ADAPINFO_DOMAIN_CODE_2G:
		adapt->regulation_2g = (u8)value;
		break;
	case PHYDM_ADAPINFO_DOMAIN_CODE_5G:
		adapt->regulation_5g = (u8)value;
		break;
	default:
		break;
	}
}

void phydm_adaptivity_init(void *dm_void)
{
#ifdef PHYDM_SUPPORT_ADAPTIVITY
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adaptivity = &dm->adaptivity;

	/* @[Config Adaptivity]*/
	if (!dm->edcca_mode) {
		pr_debug("[%s] warning!\n", __func__);
		dm->edcca_mode = &dm->u8_dummy;
		dm->support_ability &= ~ODM_BB_ADAPTIVITY;
		return;
	}

#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
	if (!dm->carrier_sense_enable) {
		if (dm->th_l2h_ini == 0 &&
		    !adaptivity->switch_th_l2h_ini_in_band)
			phydm_set_l2h_th_ini(dm);
	} else {
		phydm_set_l2h_th_ini_carrier_sense(dm);
	}

	if (dm->th_edcca_hl_diff == 0)
		dm->th_edcca_hl_diff = 7;

	if (dm->wifi_test & RT_WIFI_LOGO)
		dm->support_ability &= ~ODM_BB_ADAPTIVITY;

	if (*dm->edcca_mode == PHYDM_EDCCA_ADAPT_MODE)
		adaptivity->mode_cvrt_en = true;
	else
		adaptivity->mode_cvrt_en = false;

	if (dm->support_ic_type & ODM_RTL8822C) {
		adaptivity->l2h_ini_range_max = 45;
		adaptivity->l2h_ini_range_min = 45;
	} else {
		adaptivity->l2h_ini_range_max = dm->th_l2h_ini;
		adaptivity->l2h_ini_range_min = dm->th_l2h_ini;
	}
	adaptivity->low_rate_tx_fail_th[0] = 2;
	adaptivity->low_rate_tx_fail_th[1] = 2;
	adaptivity->low_rate_tx_fail_th[2] = 5;
	adaptivity->is_dbg_low_rate_tx_fail_th = false;
	adaptivity->rts_drop_en = false;
	phydm_l2h_ini_recorder_reset(dm);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
	if (!dm->carrier_sense_enable) {
		if (dm->th_l2h_ini_custom == 0)
			phydm_set_l2h_th_ini(dm);
		else
			dm->th_l2h_ini = dm->th_l2h_ini_custom;
	} else {
		phydm_set_l2h_th_ini_carrier_sense(dm);
	}

	if (dm->th_edcca_hl_diff == 0)
		dm->th_edcca_hl_diff = 7;

	if (dm->wifi_test || *dm->mp_mode)
		dm->support_ability &= ~ODM_BB_ADAPTIVITY;
#elif (DM_ODM_SUPPORT_TYPE & ODM_AP)
	if (dm->carrier_sense_enable) {
		phydm_set_l2h_th_ini_carrier_sense(dm);
		dm->th_edcca_hl_diff = 7;
	} else {
		dm->th_l2h_ini = dm->TH_L2H_default; /*set by mib*/
		dm->th_edcca_hl_diff = dm->th_edcca_hl_diff_default;
	}
#elif (DM_ODM_SUPPORT_TYPE & ODM_IOT)
	if (!dm->carrier_sense_enable) {
		if (dm->th_l2h_ini == 0)
			phydm_set_l2h_th_ini(dm);
	} else {
		phydm_set_l2h_th_ini_carrier_sense(dm);
	}

	if (dm->th_edcca_hl_diff == 0)
		dm->th_edcca_hl_diff = 7;
#endif

	adaptivity->debug_mode = false;
	adaptivity->th_l2h_ini_backup = dm->th_l2h_ini;
	adaptivity->th_edcca_hl_diff_backup = dm->th_edcca_hl_diff;
	adaptivity->igi_base = 0x32;
	adaptivity->adapt_igi_up = 0;
	adaptivity->h2l_lb = 0;
	adaptivity->l2h_lb = 0;
	adaptivity->l2h_dyn_min = 0;
	adaptivity->th_l2h = 0x7f;
	adaptivity->th_h2l = 0x7f;

	if (dm->support_ic_type & ODM_IC_11N_SERIES)
		adaptivity->adaptivity_dbg_port = 0x208;
	else if (dm->support_ic_type & ODM_IC_11AC_SERIES)
		adaptivity->adaptivity_dbg_port = 0x209;

	if (dm->support_ic_type & ODM_IC_11N_SERIES &&
	    !(dm->support_ic_type & ODM_IC_PWDB_EDCCA)) {
		if (dm->support_ic_type & (ODM_RTL8197F | ODM_RTL8192F)) {
			/*set to page B1*/
			odm_set_bb_reg(dm, R_0xe28, BIT(30), 0x1);
			/*@0:rx_dfir, 1: dcnf_out, 2 :rx_iq, 3: rx_nbi_nf_out*/
			odm_set_bb_reg(dm, R_0xbc0, BIT(27) | BIT(26), 0x1);
			odm_set_bb_reg(dm, R_0xe28, BIT(30), 0x0);
		} else {
			/*@0:rx_dfir, 1: dcnf_out, 2 :rx_iq, 3: rx_nbi_nf_out*/
			odm_set_bb_reg(dm, R_0xe24, BIT(21) | BIT(20), 0x1);
		}
	} else if (dm->support_ic_type & ODM_IC_11AC_SERIES &&
		   !(dm->support_ic_type & ODM_IC_PWDB_EDCCA)) {
		/*@0:rx_dfir, 1: dcnf_out, 2 :rx_iq, 3: rx_nbi_nf_out*/
		odm_set_bb_reg(dm, R_0x944, BIT(29) | BIT(28), 0x1);
	}

	if (dm->support_ic_type & ODM_IC_PWDB_EDCCA) {
		phydm_search_pwdb_lower_bound(dm);
		if (phydm_re_search_condition(dm))
			phydm_search_pwdb_lower_bound(dm);
	} else {
		/*resume to no link state*/
		phydm_set_edcca_threshold(dm, 0x7f, 0x7f);
	}

	/*@whether to ignore EDCCA*/
	phydm_mac_edcca_state(dm, PHYDM_DONT_IGNORE_EDCCA);

	/*@forgetting factor setting*/
	phydm_set_forgetting_factor(dm);

	/*@EDCCA behavior based on maximum or mean power*/
	phydm_edcca_decision_opt(dm);

#if (DM_ODM_SUPPORT_TYPE == ODM_AP)
	adaptivity->igi_up_bound_lmt_val = 180;
#else
	adaptivity->igi_up_bound_lmt_val = 90;
#endif
	adaptivity->igi_up_bound_lmt_cnt = 0;
	adaptivity->igi_lmt_en = false;
#endif
}

void phydm_adaptivity(void *dm_void)
{
#ifdef PHYDM_SUPPORT_ADAPTIVITY
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct phydm_adaptivity_struct *adapt = &dm->adaptivity;
	struct phydm_dig_struct *dig_t = &dm->dm_dig_table;

	if (phydm_edcca_abort(dm))
		return;

#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	phydm_check_adaptivity(dm); /*@Check adaptivity enable*/

	if (!dm->carrier_sense_enable &&
	    !adapt->debug_mode &&
	    adapt->switch_th_l2h_ini_in_band)
		phydm_set_l2h_th_ini_win(dm);

	if (dm->support_ic_type & ODM_RTL8822C)
		phydm_dyn_l2h_ini(dm);
#endif
#if (DM_ODM_SUPPORT_TYPE == ODM_CE)
	if (!adapt->debug_mode) {
		if (*dm->edcca_mode == PHYDM_EDCCA_ADAPT_MODE &&
		    dm->carrier_sense_enable)
			phydm_set_l2h_th_ini_carrier_sense(dm);
		else if (*dm->edcca_mode == PHYDM_EDCCA_ADAPT_MODE){
			if (dm->th_l2h_ini_custom == 0)
				phydm_set_l2h_th_ini(dm);
			else
				dm->th_l2h_ini = dm->th_l2h_ini_custom;
		}
	}
#endif
	PHYDM_DBG(dm, DBG_ADPTVTY, "%s ====>\n", __func__);
	PHYDM_DBG(dm, DBG_ADPTVTY, "mode = %s, debug_mode = %d\n",
		  (*dm->edcca_mode ?
		  (dm->carrier_sense_enable ?
		  "CARRIER SENSE" :
		  "ADAPTIVITY") :
		  "NORMAL"),
		  adapt->debug_mode);

	if (dm->support_ic_type & ODM_IC_JGR3_SERIES)
		phydm_edcca_thre_calc_jgr3(dm);
	else
		phydm_edcca_thre_calc(dm);

	if (*dm->edcca_mode == PHYDM_EDCCA_ADAPT_MODE)
		PHYDM_DBG(dm, DBG_ADPTVTY,
			  "th_l2h_ini = %d, th_edcca_hl_diff = %d\n",
			  dm->th_l2h_ini, dm->th_edcca_hl_diff);
	if (dm->support_ic_type & ODM_IC_PWDB_EDCCA)
		PHYDM_DBG(dm, DBG_ADPTVTY,
			  "IGI = 0x%x, th_l2h = %d dBm, th_h2l = %d dBm\n",
			  dig_t->cur_ig_value,
			  IGI_2_DBM(adapt->th_l2h + dig_t->cur_ig_value),
			  IGI_2_DBM(adapt->th_h2l + dig_t->cur_ig_value));
	else
		PHYDM_DBG(dm, DBG_ADPTVTY,
			  "IGI = 0x%x, th_l2h = %d dBm, th_h2l = %d dBm\n",
			  dig_t->cur_ig_value,
			  IGI_2_DBM(adapt->th_l2h),
			  IGI_2_DBM(adapt->th_h2l));
#endif
}

