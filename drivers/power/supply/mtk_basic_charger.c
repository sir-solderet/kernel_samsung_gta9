// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_basic_charger.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of Battery charging
 *
 * Author:
 * -------
 * Wy Chuang
 *
 */
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>

#include "mtk_charger.h"
/*Tab A9 code for SR-AX6739A-01-522 by gaozhengwei at 20230607 start*/
#include "mtk_battery.h"
/*Tab A9 code for SR-AX6739A-01-522 by gaozhengwei at 20230607 end*/

static int _uA_to_mA(int uA)
{
	if (uA == -1)
		return -1;
	else
		return uA / 1000;
}

/*Tab A9 code for SR-AX6739A-01-522 by gaozhengwei at 20230607 start*/
#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
static int get_val(struct range_data *range, int threshold, int *val)
{
	int i = 0;

	/* First try to find the matching index without hysteresis */
	for (i = 0; i < MAX_CV_ENTRIES; i++) {
		if (!range[i].high_threshold && !range[i].low_threshold) {
			/* First invalid table entry; exit loop */
			break;
		}

		if (is_between(range[i].low_threshold,
			range[i].high_threshold, threshold)) {
			*val = range[i].value;
			break;
		}
	}

	/*
	 * If the threshold is lesser than the minimum allowed range,
	 * return minimum. So is the maximum.
	 */
	if (threshold > 9999) {
		*val = range[4].value;
	}
	if (threshold < 0) {
		*val = range[0].value;
	}

	return 0;
}

static int gxy_battery_aging_update(struct mtk_charger *info)
{
	int rc = 0;
	int cycle_count = 0;
	int vbat = 0;
	union power_supply_propval prop = {0, };
	static int s_battery_cv_old = 0;
	static struct mtk_battery *s_info = NULL;
	struct power_supply *psy = NULL;

	if (s_info == NULL) {
		psy = power_supply_get_by_name("battery");
		if (psy == NULL) {
			chr_err("%s: get battery psy failed\n", __func__);
			return cycle_count;
		} else {
			s_info = (struct mtk_battery *)power_supply_get_drvdata(psy);
			if (s_info == NULL) {
				chr_err("%s: get mtk_battery failed\n", __func__);
				return cycle_count;
			}
		}
	}

	cycle_count = s_info->bat_cycle;

	if (s_info->bat_cycle_debug != 0) {
		cycle_count = s_info->bat_cycle_debug;
		chr_err("%s: use bat_cycle_debug %d\n", __func__, cycle_count);
	}

	rc = get_val(info->data.batt_cv_data, cycle_count, &vbat);
	if (rc < 0) {
		chr_err("Failed to get batt_cv_data, rc=%d\n", rc);
		return -ENODEV;
	}

	if (s_battery_cv_old == vbat) {
		return 0;
	}
	prop.intval = vbat;

	if (info->sw_jeita.cv != 0) {
		prop.intval = min(prop.intval, info->sw_jeita.cv);
	}

	info->data.battery_cv = prop.intval;
	s_battery_cv_old = info->data.battery_cv;
	chr_err("%s cycle_count=%d, battery_cv=%d\n", __func__,
		cycle_count, info->data.battery_cv);

	return 0;
}
#endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
/*Tab A9 code for SR-AX6739A-01-522 by gaozhengwei at 20230607 end*/

static void select_cv(struct mtk_charger *info)
{
	u32 constant_voltage;

	/*Tab A9 code for SR-AX6739A-01-522 by gaozhengwei at 20230607 start*/
	#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
	if (info->data.gxy_batt_aging_enable) {
		gxy_battery_aging_update(info);
	}
	#endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
	/*Tab A9 code for SR-AX6739A-01-522 by gaozhengwei at 20230607 end*/

	if (info->enable_sw_jeita)
		if (info->sw_jeita.cv != 0) {
			/*Tab A9 code for SR-AX6739A-01-522 by gaozhengwei at 20230607 start*/
			#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
			if (info->data.gxy_batt_aging_enable) {
				info->setting.cv = min(info->data.battery_cv, info->sw_jeita.cv);
			}
			else {
				info->setting.cv = info->sw_jeita.cv;
			}
			#else
			info->setting.cv = info->sw_jeita.cv;
			#endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
			/*Tab A9 code for SR-AX6739A-01-522 by gaozhengwei at 20230607 end*/
			return;
		}

	constant_voltage = info->data.battery_cv;
	info->setting.cv = constant_voltage;
}

static bool is_typec_adapter(struct mtk_charger *info)
{
	int rp;

	rp = adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL);
	if (info->pd_type == MTK_PD_CONNECT_TYPEC_ONLY_SNK &&
			rp != 500 &&
			info->chr_type != POWER_SUPPLY_TYPE_USB &&
			info->chr_type != POWER_SUPPLY_TYPE_USB_CDP)
		return true;

	return false;
}

static bool support_fast_charging(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	int i = 0, state = 0;
	bool ret = false;

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL)
			continue;

		if (info->enable_fast_charging_indicator &&
		    ((alg->alg_id & info->fast_charging_indicator) == 0))
			continue;

		chg_alg_set_current_limit(alg, &info->setting);
		state = chg_alg_is_algo_ready(alg);
		chr_debug("%s %s ret:%s\n", __func__, dev_name(&alg->dev),
			chg_alg_state_to_str(state));

		if (state == ALG_READY || state == ALG_RUNNING) {
			ret = true;
			break;
		}
	}
	return ret;
}

static bool select_charging_current_limit(struct mtk_charger *info,
	struct chg_limit_setting *setting)
{
	struct charger_data *pdata, *pdata2, *pdata_dvchg, *pdata_dvchg2;
	bool is_basic = false;
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret;

	select_cv(info);

	pdata = &info->chg_data[CHG1_SETTING];
	pdata2 = &info->chg_data[CHG2_SETTING];
	pdata_dvchg = &info->chg_data[DVCHG1_SETTING];
	pdata_dvchg2 = &info->chg_data[DVCHG2_SETTING];
	if (info->usb_unlimited) {
		pdata->input_current_limit =
					info->data.ac_charger_input_current;
		pdata->charging_current_limit =
					info->data.ac_charger_current;
		is_basic = true;
		goto done;
	}

	if (info->water_detected) {
		pdata->input_current_limit = info->data.usb_charger_current;
		pdata->charging_current_limit = info->data.usb_charger_current;
		is_basic = true;
		goto done;
	}

	if (((info->bootmode == 1) ||
	    (info->bootmode == 5)) && info->enable_meta_current_limit != 0) {
		pdata->input_current_limit = 200000; // 200mA
		is_basic = true;
		goto done;
	}

	if (info->atm_enabled == true
		&& (info->chr_type == POWER_SUPPLY_TYPE_USB ||
		info->chr_type == POWER_SUPPLY_TYPE_USB_CDP)
		) {
		pdata->input_current_limit = 100000; /* 100mA */
		is_basic = true;
		goto done;
	}

	if (info->chr_type == POWER_SUPPLY_TYPE_USB &&
	    info->usb_type == POWER_SUPPLY_USB_TYPE_SDP) {
		pdata->input_current_limit =
				info->data.usb_charger_current;
		/* it can be larger */
		pdata->charging_current_limit =
				info->data.usb_charger_current;
		is_basic = true;
	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_CDP) {
		pdata->input_current_limit =
			info->data.charging_host_charger_current;
		pdata->charging_current_limit =
			info->data.charging_host_charger_current;
		is_basic = true;

	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_DCP) {
		pdata->input_current_limit =
			info->data.ac_charger_input_current;
		pdata->charging_current_limit =
			info->data.ac_charger_current;
		if (info->config == DUAL_CHARGERS_IN_SERIES) {
			pdata2->input_current_limit =
				pdata->input_current_limit;
			pdata2->charging_current_limit = 2000000;
		}
	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB &&
	    info->usb_type == POWER_SUPPLY_USB_TYPE_DCP) {
		/* NONSTANDARD_CHARGER */
		pdata->input_current_limit =
			info->data.usb_charger_current;
		pdata->charging_current_limit =
			info->data.usb_charger_current;
		is_basic = true;
	} else {
		/*chr_type && usb_type cannot match above, set 500mA*/
		pdata->input_current_limit =
				info->data.usb_charger_current;
		pdata->charging_current_limit =
				info->data.usb_charger_current;
		is_basic = true;
	}

	if (support_fast_charging(info))
		is_basic = false;
	else {
		is_basic = true;
		/* AICL */
		if (!info->disable_aicl)
			charger_dev_run_aicl(info->chg1_dev,
				&pdata->input_current_limit_by_aicl);
		if (info->enable_dynamic_mivr) {
			if (pdata->input_current_limit_by_aicl >
				info->data.max_dmivr_charger_current)
				pdata->input_current_limit_by_aicl =
					info->data.max_dmivr_charger_current;
		}
		if (is_typec_adapter(info)) {
			if (adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL)
				== 3000) {
				pdata->input_current_limit = 3000000;
				pdata->charging_current_limit = 3000000;
			} else if (adapter_dev_get_property(info->pd_adapter,
				TYPEC_RP_LEVEL) == 1500) {
				pdata->input_current_limit = 1500000;
				pdata->charging_current_limit = 2000000;
			} else {
				chr_err("type-C: inquire rp error\n");
				pdata->input_current_limit = 500000;
				pdata->charging_current_limit = 500000;
			}

			chr_err("type-C:%d current:%d\n",
				info->pd_type,
				adapter_dev_get_property(info->pd_adapter,
					TYPEC_RP_LEVEL));
		}
	}

	if (info->enable_sw_jeita) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)
			&& info->chr_type == POWER_SUPPLY_TYPE_USB)
			chr_debug("USBIF & STAND_HOST skip current check\n");
		else {
			/*Tab A9 code for SR-AX6739A-01-504 by qiaodan at 20230531 start*/
			#if defined(CONFIG_CUSTOM_PROJECT_OT11)
			switch (info->sw_jeita.sm) {
				case TEMP_ABOVE_T4:
					pdata->charging_current_limit =
						min(pdata->charging_current_limit, info->data.jeita_temp_above_t4_cur);
					info->setting.charging_current_limit1 = info->data.jeita_temp_above_t4_cur;
					break;
				case TEMP_T3_TO_T4:
					pdata->charging_current_limit =
						min(pdata->charging_current_limit, info->data.jeita_temp_t3_to_t4_cur);
					info->setting.charging_current_limit1 = info->data.jeita_temp_t3_to_t4_cur;
					break;
				case TEMP_T2_TO_T3:
					pdata->charging_current_limit =
						min(pdata->charging_current_limit, info->data.jeita_temp_t2_to_t3_cur);
					info->setting.charging_current_limit1 = info->data.jeita_temp_t2_to_t3_cur;
					break;
				case TEMP_T1_TO_T2:
					pdata->charging_current_limit =
						min(pdata->charging_current_limit, info->data.jeita_temp_t1_to_t2_cur);
					info->setting.charging_current_limit1 = info->data.jeita_temp_t1_to_t2_cur;
					break;
				case TEMP_T0_TO_T1:
					pdata->charging_current_limit =
						min(pdata->charging_current_limit, info->data.jeita_temp_t0_to_t1_cur);
					info->setting.charging_current_limit1 = info->data.jeita_temp_t0_to_t1_cur;
					break;
				case TEMP_BELOW_T0:
					pdata->charging_current_limit =
						min(pdata->charging_current_limit, info->data.jeita_temp_below_t0_cur);
					info->setting.charging_current_limit1 = info->data.jeita_temp_below_t0_cur;
					break;
				default:
					break;
			}
			chr_err("[%s] [SW_JEITA] sw_jeita->sm:%d charging_current_limit:%d\n",
				__func__, info->sw_jeita.sm, _uA_to_mA(pdata->charging_current_limit));
			#else
			if (info->sw_jeita.sm == TEMP_T0_TO_T1) {
				pdata->input_current_limit = 500000;
				pdata->charging_current_limit = 350000;
			}
			#endif /*end of CONFIG_CUSTOM_PROJECT_OT11*/
			/*Tab A9 code for SR-AX6739A-01-504 by qiaodan at 20230531 end*/
		}
	}

	sc_select_charging_current(info, pdata);

	/*Tab A9 code for SR-AX6739A-01-504 by qiaodan at 20230531 start*/
	#if defined(CONFIG_CUSTOM_PROJECT_OT11)
	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <=
			pdata->charging_current_limit) {
			pdata->charging_current_limit =
					pdata->thermal_charging_current_limit;
			info->setting.charging_current_limit1 =
					min(info->setting.charging_current_limit1, pdata->thermal_charging_current_limit);
		}
	}
	#else
	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <=
			pdata->charging_current_limit) {
			pdata->charging_current_limit =
					pdata->thermal_charging_current_limit;
			info->setting.charging_current_limit1 =
					pdata->thermal_charging_current_limit;
		}
	} else
		info->setting.charging_current_limit1 = info->sc.sc_ibat;
	#endif //CONFIG_CUSTOM_PROJECT_OT11
	/*Tab A9 code for SR-AX6739A-01-504 by qiaodan at 20230531 end*/

	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <=
			pdata->input_current_limit) {
			pdata->input_current_limit =
					pdata->thermal_input_current_limit;
			info->setting.input_current_limit1 =
					pdata->input_current_limit;
		}
	} else
		info->setting.input_current_limit1 = -1;

	if (pdata2->thermal_charging_current_limit != -1) {
		if (pdata2->thermal_charging_current_limit <=
			pdata2->charging_current_limit) {
			pdata2->charging_current_limit =
					pdata2->thermal_charging_current_limit;
			info->setting.charging_current_limit2 =
					pdata2->charging_current_limit;
		}
	} else
		info->setting.charging_current_limit2 = info->sc.sc_ibat;

	if (pdata2->thermal_input_current_limit != -1) {
		if (pdata2->thermal_input_current_limit <=
			pdata2->input_current_limit) {
			pdata2->input_current_limit =
					pdata2->thermal_input_current_limit;
			info->setting.input_current_limit2 =
					pdata2->input_current_limit;
		}
	} else
		info->setting.input_current_limit2 = -1;

	if (is_basic == true && pdata->input_current_limit_by_aicl != -1
		&& !info->charger_unlimited
		&& !info->disable_aicl) {
		if (pdata->input_current_limit_by_aicl <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->input_current_limit_by_aicl;
	}
	info->setting.input_current_limit_dvchg1 =
		pdata_dvchg->thermal_input_current_limit;

done:
	/*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 start*/
	#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
	if (info->gxy_discharge_store_mode) {
		pdata->charging_current_limit = STORE_MODE_FCC_MAX;
		chr_err("Be in store_mode,FCC set to be 500mA\r\n");
		is_basic = true;
	}
	#endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
	/*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 end*/

	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -EOPNOTSUPP && pdata->charging_current_limit < ichg1_min) {
		pdata->charging_current_limit = 0;
		/* For TC_018, pleasae don't modify the format */
		chr_err("min_charging_current is too low %d %d\n",
			pdata->charging_current_limit, ichg1_min);
		is_basic = true;
	}

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -EOPNOTSUPP && pdata->input_current_limit < aicr1_min) {
		pdata->input_current_limit = 0;
		/* For TC_018, pleasae don't modify the format */
		chr_err("min_input_current is too low %d %d\n",
			pdata->input_current_limit, aicr1_min);
		is_basic = true;
	}

	/*Tab A9 code for SR-AX6739A-01-504 by qiaodan at 20230531 start*/
	/*debug for fast charging current setting*/
	chr_err("[%s] fast charging current = %d\n", __func__, info->setting.charging_current_limit1);
	/*Tab A9 code for SR-AX6739A-01-504 by qiaodan at 20230531 end*/

	/* For TC_018, pleasae don't modify the format */
	chr_err("m:%d chg1:%d,%d,%d,%d chg2:%d,%d,%d,%d dvchg1:%d sc:%d %d %d type:%d:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d bm:%d b:%d\n",
		info->config,
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		_uA_to_mA(pdata2->thermal_input_current_limit),
		_uA_to_mA(pdata2->thermal_charging_current_limit),
		_uA_to_mA(pdata2->input_current_limit),
		_uA_to_mA(pdata2->charging_current_limit),
		_uA_to_mA(pdata_dvchg->thermal_input_current_limit),
		info->sc.pre_ibat,
		info->sc.sc_ibat,
		info->sc.solution,
		info->chr_type, info->pd_type,
		info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled,
		info->bootmode, is_basic);

	return is_basic;
}

static int do_algorithm(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	struct charger_data *pdata;
	struct chg_alg_notify notify;
	bool is_basic = true;
	bool chg_done = false;
	int i;
	int ret;
	int val = 0;

	pdata = &info->chg_data[CHG1_SETTING];
	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
	is_basic = select_charging_current_limit(info, &info->setting);

	if (info->is_chg_done != chg_done) {
		if (chg_done) {
			charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
			info->polling_interval = CHARGING_FULL_INTERVAL;
			chr_err("%s battery full\n", __func__);
		} else {
			charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
			info->polling_interval = CHARGING_INTERVAL;
			chr_err("%s battery recharge\n", __func__);
		}
	}
	/*Tab A9 code for AX6739A-2539 by liufurong at 20230728 start*/
	if (chg_done) {
		ret = get_uisoc(info);
		if (ret < 100) {
			charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
		}
	}
	/*Tab A9 code for AX6739A-2539 by liufurong at 20230728 end*/

	chr_err("%s is_basic:%d\n", __func__, is_basic);
	if (is_basic != true) {
		is_basic = true;
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = info->alg[i];
			if (alg == NULL)
				continue;

			if (info->enable_fast_charging_indicator &&
			    ((alg->alg_id & info->fast_charging_indicator) == 0))
				continue;

			if (!info->enable_hv_charging ||
			    pdata->charging_current_limit == 0 ||
			    pdata->input_current_limit == 0) {
				chg_alg_get_prop(alg, ALG_MAX_VBUS, &val);
				if (val > 5000)
					chg_alg_stop_algo(alg);
				chr_err("%s: alg:%s alg_vbus:%d\n", __func__,
					dev_name(&alg->dev), val);
				continue;
			}

			if (chg_done != info->is_chg_done) {
				if (chg_done) {
					notify.evt = EVT_FULL;
					notify.value = 0;
				} else {
					notify.evt = EVT_RECHARGE;
					notify.value = 0;
				}
				chg_alg_notifier_call(alg, &notify);
				chr_err("%s notify:%d\n", __func__, notify.evt);
			}

			chg_alg_set_current_limit(alg, &info->setting);
			ret = chg_alg_is_algo_ready(alg);

			chr_err("%s %s ret:%s\n", __func__,
				dev_name(&alg->dev),
				chg_alg_state_to_str(ret));

			if (ret == ALG_INIT_FAIL || ret == ALG_TA_NOT_SUPPORT) {
				/* try next algorithm */
				continue;
			} else if (ret == ALG_TA_CHECKING || ret == ALG_DONE ||
						ret == ALG_NOT_READY) {
				/* wait checking , use basic first */
				is_basic = true;
				break;
			} else if (ret == ALG_READY || ret == ALG_RUNNING) {
				is_basic = false;
				//chg_alg_set_setting(alg, &info->setting);
				chg_alg_start_algo(alg);
				break;
			} else {
				chr_err("algorithm ret is error");
				is_basic = true;
			}
		}
	} else {
		if (info->enable_hv_charging != true ||
		    pdata->charging_current_limit == 0 ||
		    pdata->input_current_limit == 0) {
			for (i = 0; i < MAX_ALG_NO; i++) {
				alg = info->alg[i];
				if (alg == NULL)
					continue;

				chg_alg_get_prop(alg, ALG_MAX_VBUS, &val);
				if (val > 5000 && chg_alg_is_algo_running(alg))
					chg_alg_stop_algo(alg);

				chr_err("%s: Stop hv charging. en_hv:%d alg:%s alg_vbus:%d\n",
					__func__, info->enable_hv_charging,
					dev_name(&alg->dev), val);
			}
		}
	}
	info->is_chg_done = chg_done;

	if (is_basic == true) {
		charger_dev_set_input_current(info->chg1_dev,
			pdata->input_current_limit);
		charger_dev_set_charging_current(info->chg1_dev,
			pdata->charging_current_limit);

		chr_debug("%s:old_cv=%d,cv=%d, vbat_mon_en=%d\n",
			__func__,
			info->old_cv,
			info->setting.cv,
			info->setting.vbat_mon_en);
		if (info->old_cv == 0 || (info->old_cv != info->setting.cv)
		    || info->setting.vbat_mon_en == 0) {
			charger_dev_enable_6pin_battery_charging(
				info->chg1_dev, false);
			charger_dev_set_constant_voltage(info->chg1_dev,
				info->setting.cv);
			if (info->setting.vbat_mon_en && info->stop_6pin_re_en != 1)
				charger_dev_enable_6pin_battery_charging(
					info->chg1_dev, true);
			info->old_cv = info->setting.cv;
		} else {
			if (info->setting.vbat_mon_en && info->stop_6pin_re_en != 1) {
				info->stop_6pin_re_en = 1;
				charger_dev_enable_6pin_battery_charging(
					info->chg1_dev, true);
			}
		}
	}

	if (pdata->input_current_limit == 0 ||
	    pdata->charging_current_limit == 0)
		charger_dev_enable(info->chg1_dev, false);
	else {
		alg = get_chg_alg_by_name("pe5");
		ret = chg_alg_is_algo_ready(alg);
		if (!(ret == ALG_READY || ret == ALG_RUNNING))
			charger_dev_enable(info->chg1_dev, true);
	}

	if (info->chg1_dev != NULL)
		charger_dev_dump_registers(info->chg1_dev);

	if (info->chg2_dev != NULL)
		charger_dev_dump_registers(info->chg2_dev);

	return 0;
}

/*Tab A9 code for SR-AX6739A-01-514 by qiaodan at 20230601 start*/
static int enable_charging(struct mtk_charger *info,
						bool en)
{
	#if defined(CONFIG_CUSTOM_PROJECT_OT11)
	//skip
	#else
	int i;
	struct chg_alg_device *alg;
	#endif //CONFIG_CUSTOM_PROJECT_OT11


	chr_err("%s %d\n", __func__, en);

	if (en == false) {
		#if defined(CONFIG_CUSTOM_PROJECT_OT11)
		//skip
		#else
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = info->alg[i];
			if (alg == NULL)
				continue;
			chg_alg_stop_algo(alg);
		}
		#endif //CONFIG_CUSTOM_PROJECT_OT11
		charger_dev_enable(info->chg1_dev, false);
		charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
	} else {
		charger_dev_enable(info->chg1_dev, true);
		charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
	}

	return 0;
}
/*Tab A9 code for SR-AX6739A-01-514 by qiaodan at 20230601 end*/

/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
static int enable_port_charging(struct mtk_charger *info,
						bool en)
{
	chr_err("%s %d\n", __func__, en);

	charger_dev_enable_port_charging(info->chg1_dev, en);

	return 0;
}
/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/

static int charger_dev_event(struct notifier_block *nb, unsigned long event,
				void *v)
{
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	struct mtk_charger *info =
			container_of(nb, struct mtk_charger, chg1_nb);
	struct chgdev_notify *data = v;
	int i;

	chr_err("%s %lu\n", __func__, event);

	switch (event) {
	case CHARGER_DEV_NOTIFY_EOC:
		info->stop_6pin_re_en = 1;
		notify.evt = EVT_FULL;
		notify.value = 0;
		for (i = 0; i < 10; i++) {
			alg = info->alg[i];
			chg_alg_notifier_call(alg, &notify);
		}

		break;
	case CHARGER_DEV_NOTIFY_RECHG:
		pr_info("%s: recharge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT:
		info->safety_timeout = true;
		pr_info("%s: safety timer timeout\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		info->vbusov_stat = data->vbusov_stat;
		pr_info("%s: vbus ovp = %d\n", __func__, info->vbusov_stat);
		break;
	case CHARGER_DEV_NOTIFY_BATPRO_DONE:
		info->batpro_done = true;
		info->setting.vbat_mon_en = 0;
		notify.evt = EVT_BATPRO_DONE;
		notify.value = 0;
		for (i = 0; i < 10; i++) {
			alg = info->alg[i];
			chg_alg_notifier_call(alg, &notify);
		}
		pr_info("%s: batpro_done = %d\n", __func__, info->batpro_done);
		break;
	default:
		return NOTIFY_DONE;
	}

	if (info->chg1_dev->is_polling_mode == false)
		_wake_up_charger(info);

	return NOTIFY_DONE;
}

static int to_alg_notify_evt(unsigned long evt)
{
	switch (evt) {
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		return EVT_VBUSOVP;
	case CHARGER_DEV_NOTIFY_IBUSOCP:
		return EVT_IBUSOCP;
	case CHARGER_DEV_NOTIFY_IBUSUCP_FALL:
		return EVT_IBUSUCP_FALL;
	case CHARGER_DEV_NOTIFY_BAT_OVP:
		return EVT_VBATOVP;
	case CHARGER_DEV_NOTIFY_IBATOCP:
		return EVT_IBATOCP;
	case CHARGER_DEV_NOTIFY_VBATOVP_ALARM:
		return EVT_VBATOVP_ALARM;
	case CHARGER_DEV_NOTIFY_VBUSOVP_ALARM:
		return EVT_VBUSOVP_ALARM;
	case CHARGER_DEV_NOTIFY_VOUTOVP:
		return EVT_VOUTOVP;
	case CHARGER_DEV_NOTIFY_VDROVP:
		return EVT_VDROVP;
	default:
		return -EINVAL;
	}
}

static int dvchg1_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct mtk_charger *info =
		container_of(nb, struct mtk_charger, dvchg1_nb);
	int alg_evt = to_alg_notify_evt(event);

	chr_info("%s %ld", __func__, event);
	if (alg_evt < 0)
		return NOTIFY_DONE;
	mtk_chg_alg_notify_call(info, alg_evt, 0);
	return NOTIFY_OK;
}

static int dvchg2_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct mtk_charger *info =
		container_of(nb, struct mtk_charger, dvchg1_nb);
	int alg_evt = to_alg_notify_evt(event);

	chr_info("%s %ld", __func__, event);
	if (alg_evt < 0)
		return NOTIFY_DONE;
	mtk_chg_alg_notify_call(info, alg_evt, 0);
	return NOTIFY_OK;
}


int mtk_basic_charger_init(struct mtk_charger *info)
{

	info->algo.do_algorithm = do_algorithm;
	info->algo.enable_charging = enable_charging;
	/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
	info->algo.enable_port_charging = enable_port_charging;
	/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/
	info->algo.do_event = charger_dev_event;
	info->algo.do_dvchg1_event = dvchg1_dev_event;
	info->algo.do_dvchg2_event = dvchg2_dev_event;
	//info->change_current_setting = mtk_basic_charging_current;
	/*Tab A9 code for AX6739A-2727 by wenyaqi at 20230906 start*/
	// if current_limit != -1, it will influence the value of support_fast_charging
	// pd_value will be ALG_NOT_READY in _pd_is_algo_ready in mtk_pd.c
	info->setting.input_current_limit1 = -1;
	info->setting.charging_current_limit1 = -1;
	info->setting.input_current_limit2 = -1;
	info->setting.charging_current_limit2 = -1;
	/*Tab A9 code for AX6739A-2727 by wenyaqi at 20230906 end*/
	return 0;
}
