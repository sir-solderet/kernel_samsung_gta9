// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *      Storage Driver <storage.sec@samsung.com>
 */

#include "ufs-sec-feature.h"
#include "ufs-sec-sysfs.h"

#include <trace/hooks/ufshcd.h>

struct ufs_sec_feature_info ufs_sec_features;

void ufs_sec_get_health_desc(struct ufs_hba *hba)
{
	struct ufs_vendor_dev_info *vdi = ufs_sec_features.vdi;
	int buff_len;
	u8 *desc_buf = NULL;
	int err;

	buff_len = hba->desc_size[QUERY_DESC_IDN_HEALTH];
	desc_buf = kmalloc(buff_len, GFP_KERNEL);
	if (!desc_buf)
		return;

	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
			QUERY_DESC_IDN_HEALTH, 0, 0,
			desc_buf, &buff_len);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading health descriptor. err %d",
				__func__, err);
		goto out;
	}

	/* getting Life Time at Device Health DESC*/
	vdi->lifetime = desc_buf[HEALTH_DESC_PARAM_LIFE_TIME_EST_A];

	dev_info(hba->dev, "LT: 0x%02x\n", (desc_buf[3] << 4) | desc_buf[4]);
out:
	kfree(desc_buf);
}

static void ufs_sec_set_unique_number(struct ufs_hba *hba, u8 *desc_buf)
{
	struct ufs_vendor_dev_info *vdi = ufs_sec_features.vdi;
	u8 manid;
	u8 serial_num_index;
	u8 snum_buf[UFS_UN_MAX_DIGITS];
	int buff_len;
	u8 *str_desc_buf = NULL;
	int err;

	/* read string desc */
	buff_len = QUERY_DESC_MAX_SIZE;
	str_desc_buf = kzalloc(buff_len, GFP_KERNEL);
	if (!str_desc_buf)
		goto out;

	serial_num_index = desc_buf[DEVICE_DESC_PARAM_SN];

	/* spec is unicode but sec uses hex data */
	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
			QUERY_DESC_IDN_STRING, serial_num_index, 0,
			str_desc_buf, &buff_len);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading string descriptor. err %d",
				__func__, err);
		goto out;
	}

	/* setup unique_number */
	manid = desc_buf[DEVICE_DESC_PARAM_MANF_ID + 1];
	memset(snum_buf, 0, sizeof(snum_buf));
	memcpy(snum_buf, str_desc_buf + QUERY_DESC_HDR_SIZE, SERIAL_NUM_SIZE);
	memset(vdi->unique_number, 0, sizeof(vdi->unique_number));

	sprintf(vdi->unique_number, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
			manid,
			desc_buf[DEVICE_DESC_PARAM_MANF_DATE],
			desc_buf[DEVICE_DESC_PARAM_MANF_DATE + 1],
			snum_buf[0], snum_buf[1], snum_buf[2],
			snum_buf[3], snum_buf[4], snum_buf[5], snum_buf[6]);

	/* Null terminate the unique number string */
	vdi->unique_number[UFS_UN_20_DIGITS] = '\0';

	dev_dbg(hba->dev, "%s: ufs un : %s\n", __func__, vdi->unique_number);
out:
	kfree(str_desc_buf);
}

void ufs_sec_set_features(struct ufs_hba *hba)
{
	struct ufs_vendor_dev_info *vdi = NULL;
	int buff_len;
	u8 *desc_buf = NULL;
	int err;

	if (ufs_sec_features.vdi)
		return;

	vdi = devm_kzalloc(hba->dev, sizeof(struct ufs_vendor_dev_info),
			GFP_KERNEL);
	if (!vdi) {
		dev_err(hba->dev, "%s: Failed allocating ufs_vdi(%lu)",
				__func__, sizeof(struct ufs_vendor_dev_info));
		return;
	}

	vdi->hba = hba;

	ufs_sec_features.vdi = vdi;

	/* read device desc */
	buff_len = hba->desc_size[QUERY_DESC_IDN_DEVICE];
	desc_buf = kmalloc(buff_len, GFP_KERNEL);
	if (!desc_buf)
		return;

	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
			QUERY_DESC_IDN_DEVICE, 0, 0,
			desc_buf, &buff_len);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading device descriptor. err %d",
				__func__, err);
		goto out;
	}

	ufs_sec_set_unique_number(hba, desc_buf);
	ufs_sec_get_health_desc(hba);

	ufs_sec_add_sysfs_nodes(hba);
out:
	kfree(desc_buf);
}

void ufs_sec_remove_features(struct ufs_hba *hba)
{
	ufs_sec_remove_sysfs_nodes(hba);
}

/* check error info : begin */
inline bool ufs_sec_is_err_cnt_allowed(void)
{
	return ufs_sec_features.ufs_err;
}

static void ufs_sec_check_hwrst_cnt(void)
{
	struct SEC_UFS_op_count *op_cnt = NULL;

	if (!ufs_sec_is_err_cnt_allowed())
		return;

	if (ufs_sec_features.skip_flush)
		return;

	ufs_sec_features.skip_flush = true;

	op_cnt = &get_err_member(op_count);

	SEC_UFS_ERR_COUNT_INC(op_cnt->HW_RESET_count, UINT_MAX);
	SEC_UFS_ERR_COUNT_INC(op_cnt->op_err, UINT_MAX);
}

static void ufs_sec_check_link_startup_error_cnt(void)
{
	struct SEC_UFS_op_count *op_cnt = &get_err_member(op_count);

	SEC_UFS_ERR_COUNT_INC(op_cnt->link_startup_count, UINT_MAX);
	SEC_UFS_ERR_COUNT_INC(op_cnt->op_err, UINT_MAX);
}

static void ufs_sec_uic_cmd_error_check(struct ufs_hba *hba,
		u32 cmd, bool timeout)
{
	struct SEC_UFS_UIC_cmd_count *uic_cmd_cnt = NULL;
	struct SEC_UFS_op_count *op_cnt = NULL;

	if (!ufs_sec_is_err_cnt_allowed())
		return;

	if (((hba->active_uic_cmd->argument2 & MASK_UIC_COMMAND_RESULT)
			== UIC_CMD_RESULT_SUCCESS)
			&& !timeout)
		return;

	uic_cmd_cnt = &get_err_member(UIC_cmd_count);
	op_cnt = &get_err_member(op_count);

	switch (cmd & COMMAND_OPCODE_MASK) {
	case UIC_CMD_DME_GET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_GET_err, U8_MAX);
		break;
	case UIC_CMD_DME_SET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_SET_err, U8_MAX);
		break;
	case UIC_CMD_DME_PEER_GET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_PEER_GET_err, U8_MAX);
		break;
	case UIC_CMD_DME_PEER_SET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_PEER_SET_err, U8_MAX);
		break;
	case UIC_CMD_DME_POWERON:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_POWERON_err, U8_MAX);
		break;
	case UIC_CMD_DME_POWEROFF:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_POWEROFF_err, U8_MAX);
		break;
	case UIC_CMD_DME_ENABLE:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_ENABLE_err, U8_MAX);
		break;
	case UIC_CMD_DME_RESET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_RESET_err, U8_MAX);
		break;
	case UIC_CMD_DME_END_PT_RST:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_END_PT_RST_err, U8_MAX);
		break;
	case UIC_CMD_DME_LINK_STARTUP:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_LINK_STARTUP_err, U8_MAX);
		break;
	case UIC_CMD_DME_HIBER_ENTER:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_HIBER_ENTER_err, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(op_cnt->Hibern8_enter_count, UINT_MAX);
		SEC_UFS_ERR_COUNT_INC(op_cnt->op_err, UINT_MAX);
		break;
	case UIC_CMD_DME_HIBER_EXIT:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_HIBER_EXIT_err, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(op_cnt->Hibern8_exit_count, UINT_MAX);
		SEC_UFS_ERR_COUNT_INC(op_cnt->op_err, UINT_MAX);
		break;
	case UIC_CMD_DME_TEST_MODE:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_TEST_MODE_err, U8_MAX);
		break;
	default:
		break;
	}

	SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->UIC_cmd_err, UINT_MAX);
}

static void ufs_sec_uic_fatal_check(u32 errors)
{
	struct SEC_UFS_Fatal_err_count *fatal_err_cnt = &get_err_member(Fatal_err_count);

	if (errors & DEVICE_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->DFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	if (errors & CONTROLLER_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->CFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	if (errors & SYSTEM_BUS_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->SBFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	if (errors & CRYPTO_ENGINE_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->CEFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	/* UIC_LINK_LOST : can not be checked in ufshcd.c */
	if (errors & UIC_LINK_LOST) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->LLE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
}

static void ufs_sec_uic_error_check(enum ufs_event_type evt, u32 reg)
{
	struct SEC_UFS_UIC_err_count *uic_err_cnt = &get_err_member(UIC_err_count);
	int check_val = 0;

	switch (evt) {
	case UFS_EVT_PA_ERR:
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->PA_ERR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);

		if (reg & UIC_PHY_ADAPTER_LAYER_GENERIC_ERROR)
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->PA_ERR_linereset, UINT_MAX);

		check_val = reg & UIC_PHY_ADAPTER_LAYER_LANE_ERR_MASK;
		if (check_val)
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->PA_ERR_lane[check_val - 1], UINT_MAX);
		break;
	case UFS_EVT_DL_ERR:
		if (reg & UIC_DATA_LINK_LAYER_ERROR_PA_INIT) {
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DL_PA_INIT_ERROR_cnt, U8_MAX);
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		}
		if (reg & UIC_DATA_LINK_LAYER_ERROR_NAC_RECEIVED) {
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DL_NAC_RECEIVED_ERROR_cnt, U8_MAX);
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		}
		if (reg & UIC_DATA_LINK_LAYER_ERROR_TCx_REPLAY_TIMEOUT) {
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DL_TC_REPLAY_ERROR_cnt, U8_MAX);
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		}
		break;
	case UFS_EVT_NL_ERR:
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->NL_ERROR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		break;
	case UFS_EVT_TL_ERR:
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->TL_ERROR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		break;
	case UFS_EVT_DME_ERR:
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DME_ERROR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		break;
	default:
		break;
	}
}

static void ufs_sec_tm_error_check(u8 tm_cmd)
{
	struct SEC_UFS_UTP_count *utp_err = &get_err_member(UTP_count);

	switch (tm_cmd) {
	case UFS_QUERY_TASK:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTMR_query_task_count, U8_MAX);
		break;
	case UFS_ABORT_TASK:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTMR_abort_task_count, U8_MAX);
		break;
	case UFS_LOGICAL_RESET:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTMR_logical_reset_count, U8_MAX);
		break;
	default:
		break;
	}

	SEC_UFS_ERR_COUNT_INC(utp_err->UTP_err, UINT_MAX);
}

static void ufs_sec_utp_error_check(struct ufs_hba *hba, int tag)
{
	struct SEC_UFS_UTP_count *utp_err = &get_err_member(UTP_count);
	struct ufshcd_lrb *lrbp = NULL;
	int opcode = 0;

	/* check tag value */
	if (tag >= hba->nutrs)
		return;

	lrbp = &hba->lrb[tag];
	/* check lrbp */
	if (!lrbp || !lrbp->cmd || (lrbp->task_tag != tag))
		return;

	opcode = lrbp->cmd->cmnd[0];

	switch (opcode) {
	case WRITE_10:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_write_err, U8_MAX);
		break;
	case READ_10:
	case READ_16:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_read_err, U8_MAX);
		break;
	case SYNCHRONIZE_CACHE:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_sync_cache_err, U8_MAX);
		break;
	case UNMAP:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_unmap_err, U8_MAX);
		break;
	default:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_etc_err, U8_MAX);
		break;
	}

	SEC_UFS_ERR_COUNT_INC(utp_err->UTP_err, UINT_MAX);
}

static void ufs_sec_query_error_check(struct ufs_hba *hba,
		struct ufshcd_lrb *lrbp, bool timeout)
{
	struct SEC_UFS_QUERY_count *query_cnt = NULL;
	struct ufs_query_req *request = &hba->dev_cmd.query.request;
	enum query_opcode opcode = request->upiu_req.opcode;
	enum dev_cmd_type cmd_type = hba->dev_cmd.type;

	if (!ufs_sec_is_err_cnt_allowed())
		return;

	if (((le32_to_cpu(lrbp->utr_descriptor_ptr->header.dword_2) & MASK_OCS)
			== OCS_SUCCESS)
			&& !timeout)
		return;

	/* get last query cmd information when timeout occurs */
	if (timeout) {
		opcode = ufs_sec_features.last_qcmd;
		cmd_type = ufs_sec_features.qcmd_type;
	}

	query_cnt = &get_err_member(Query_count);

	if (cmd_type == DEV_CMD_TYPE_NOP) {
		SEC_UFS_ERR_COUNT_INC(query_cnt->NOP_err, U8_MAX);
	} else {
		switch (opcode) {
		case UPIU_QUERY_OPCODE_READ_DESC:
			SEC_UFS_ERR_COUNT_INC(query_cnt->R_Desc_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_WRITE_DESC:
			SEC_UFS_ERR_COUNT_INC(query_cnt->W_Desc_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_READ_ATTR:
			SEC_UFS_ERR_COUNT_INC(query_cnt->R_Attr_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_WRITE_ATTR:
			SEC_UFS_ERR_COUNT_INC(query_cnt->W_Attr_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_READ_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->R_Flag_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_SET_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->Set_Flag_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_CLEAR_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->Clear_Flag_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_TOGGLE_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->Toggle_Flag_err, U8_MAX);
			break;
		default:
			break;
		}
	}

	SEC_UFS_ERR_COUNT_INC(query_cnt->Query_err, UINT_MAX);
}

void ufs_sec_op_err_check(struct ufs_hba *hba,
		enum ufs_event_type evt, void *data)
{
	u32 error_val = *(u32 *)data;

	if (!ufs_sec_is_err_cnt_allowed())
		return;

	switch (evt) {
	case UFS_EVT_LINK_STARTUP_FAIL:
		ufs_sec_check_link_startup_error_cnt();
		break;
	case UFS_EVT_DEV_RESET:
		if (hba->eh_flags)
			ufs_sec_check_hwrst_cnt();
		break;
	case UFS_EVT_PA_ERR:
	case UFS_EVT_DL_ERR:
	case UFS_EVT_NL_ERR:
	case UFS_EVT_TL_ERR:
	case UFS_EVT_DME_ERR:
		if (error_val)
			ufs_sec_uic_error_check(evt, error_val);
		break;
	case UFS_EVT_FATAL_ERR:
		if (error_val)
			ufs_sec_uic_fatal_check(error_val);
		break;
	case UFS_EVT_ABORT:
		ufs_sec_utp_error_check(hba, (int)error_val);
		break;
	case UFS_EVT_HOST_RESET:
		if (!error_val)
			ufs_sec_features.skip_flush = false;
		break;
	case UFS_EVT_SUSPEND_ERR:
	case UFS_EVT_RESUME_ERR:
		break;
	case UFS_EVT_AUTO_HIBERN8_ERR:
		break;
	default:
		break;
	}
}

static void ufs_sec_sense_err_check(struct ufshcd_lrb *lrbp,
		struct ufs_sec_cmd_info *ufs_cmd)
{
	struct SEC_SCSI_SENSE_count *sense_err = NULL;
	u8 sense_key = 0;
	u8 asc = 0;
	u8 ascq = 0;

	sense_key = lrbp->ucd_rsp_ptr->sr.sense_data[2] & 0x0F;
	if (sense_key != MEDIUM_ERROR && sense_key != HARDWARE_ERROR)
		return;

	asc = lrbp->ucd_rsp_ptr->sr.sense_data[12];
	ascq = lrbp->ucd_rsp_ptr->sr.sense_data[13];

	pr_err("UFS: LU%u: sense key 0x%x(asc 0x%x, ascq 0x%x),"
			"opcode 0x%x, lba 0x%x, len 0x%x.\n",
			ufs_cmd->lun, sense_key, asc, ascq,
			ufs_cmd->opcode, ufs_cmd->lba, ufs_cmd->transfer_len);

	if (!ufs_sec_is_err_cnt_allowed())
		return;

	sense_err = &get_err_member(Sense_count);

	if (sense_key == MEDIUM_ERROR)
		sense_err->scsi_medium_err++;
	else
		sense_err->scsi_hw_err++;
}

void ufs_sec_print_err_info(struct ufs_hba *hba)
{
	dev_err(hba->dev, "Count: %u UIC: %u UTP: %u QUERY: %u\n",
		get_err_member(op_count).HW_RESET_count,
		get_err_member(UIC_err_count).UIC_err,
		get_err_member(UTP_count).UTP_err,
		get_err_member(Query_count).Query_err);

	dev_err(hba->dev, "Sense Key: medium: %u, hw: %u\n",
		get_err_member(Sense_count).scsi_medium_err,
		get_err_member(Sense_count).scsi_hw_err);
}

static void ufs_sec_init_error_logging(struct device *dev)
{
	struct ufs_sec_err_info *ufs_err = NULL;

	ufs_err = devm_kzalloc(dev, sizeof(struct ufs_sec_err_info),
			GFP_KERNEL);
	if (!ufs_err) {
		dev_err(dev, "%s: Failed allocating ufs_err(%lu)",
				__func__, sizeof(struct ufs_sec_err_info));
		devm_kfree(dev, ufs_err);
		return;
	}

	ufs_sec_features.ufs_err = ufs_err;

	ufs_sec_features.ucmd_complete = true;
	ufs_sec_features.qcmd_complete = true;
}
/* check error info : end */

void ufs_sec_init_logging(struct device *dev)
{
	ufs_sec_init_error_logging(dev);
}

static bool ufs_sec_get_scsi_cmd_info(struct ufshcd_lrb *lrbp,
		struct ufs_sec_cmd_info *ufs_cmd)
{
	struct scsi_cmnd *cmd;

	if (!lrbp || !lrbp->cmd || !ufs_cmd)
		return false;

	cmd = lrbp->cmd;

	ufs_cmd->opcode = (u8)(*cmd->cmnd);
	ufs_cmd->lba = ((cmd->cmnd[2] << 24) | (cmd->cmnd[3] << 16) |
			(cmd->cmnd[4] << 8) | cmd->cmnd[5]);
	ufs_cmd->transfer_len = (cmd->cmnd[7] << 8) | cmd->cmnd[8];
	ufs_cmd->lun = ufshcd_scsi_to_upiu_lun(cmd->device->lun);

	return true;
}

/*
 * vendor hooks
 * check include/trace/hooks/ufshcd.h
 */
static void sec_android_vh_ufs_send_command(void *data,
		struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufs_sec_cmd_info ufs_cmd = { 0, };
	struct ufs_query_req *request = NULL;
	enum dev_cmd_type cmd_type;
	enum query_opcode opcode;
	bool is_scsi_cmd = false;

	is_scsi_cmd = ufs_sec_get_scsi_cmd_info(lrbp, &ufs_cmd);

	if (!is_scsi_cmd) {
		/* in timeout error case, last cmd is not completed */
		if (!ufs_sec_features.qcmd_complete)
			ufs_sec_query_error_check(hba, lrbp, true);

		request = &hba->dev_cmd.query.request;
		opcode = request->upiu_req.opcode;
		cmd_type = hba->dev_cmd.type;

		ufs_sec_features.last_qcmd = opcode;
		ufs_sec_features.qcmd_type = cmd_type;
		ufs_sec_features.qcmd_complete = false;
	}
}

static void sec_android_vh_ufs_compl_command(void *data,
		struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufs_sec_cmd_info ufs_cmd = { 0, };
	bool is_scsi_cmd = false;

	is_scsi_cmd = ufs_sec_get_scsi_cmd_info(lrbp, &ufs_cmd);

	if (is_scsi_cmd) {
		ufs_sec_sense_err_check(lrbp, &ufs_cmd);
		/*
		 * check hba->req_abort_count, if the cmd is aborting
		 * it's the one way to check aborting
		 * hba->req_abort_count is cleared in queuecommand and after
		 * error handling
		 */
		if (hba->req_abort_count > 0 && ufs_sec_is_err_cnt_allowed())
			ufs_sec_utp_error_check(hba, lrbp->task_tag);
	} else {
		ufs_sec_features.qcmd_complete = true;

		/* check and count error, except timeout */
		ufs_sec_query_error_check(hba, lrbp, false);
	}
}

static void sec_android_vh_ufs_send_uic_command(void *data,
		struct ufs_hba *hba, struct uic_command *ucmd, const char *str)
{
	u32 cmd;

	if (!strcmp(str, "send")) {
		/* in timeout error case, last cmd is not completed */
		if (!ufs_sec_features.ucmd_complete) {
			ufs_sec_uic_cmd_error_check(hba,
					ufs_sec_features.last_ucmd, true);
		}

		cmd = ucmd->command;
		ufs_sec_features.last_ucmd = cmd;
		ufs_sec_features.ucmd_complete = false;
	} else {
		cmd = ufshcd_readl(hba, REG_UIC_COMMAND);

		ufs_sec_features.ucmd_complete = true;

		/* check and count error, except timeout */
		ufs_sec_uic_cmd_error_check(hba, cmd, false);
	}
}

static void sec_android_vh_ufs_send_tm_command(void *data,
		struct ufs_hba *hba, int tag, const char *str)
{
	struct utp_task_req_desc treq = { { 0 }, };
	u8 tm_func = 0;

	memcpy(&treq, hba->utmrdl_base_addr + tag, sizeof(treq));

	tm_func = (be32_to_cpu(treq.req_header.dword_1) >> 16) & 0xFF;

	if (!strncmp(str, "tm_complete_err", sizeof("tm_complete_err"))
									&& ufs_sec_is_err_cnt_allowed())
		ufs_sec_tm_error_check(tm_func);
}

void ufs_sec_register_vendor_hooks(void)
{
	register_trace_android_vh_ufs_send_command(sec_android_vh_ufs_send_command, NULL);
	register_trace_android_vh_ufs_compl_command(sec_android_vh_ufs_compl_command, NULL);
	register_trace_android_vh_ufs_send_uic_command(sec_android_vh_ufs_send_uic_command, NULL);
	register_trace_android_vh_ufs_send_tm_command(sec_android_vh_ufs_send_tm_command, NULL);
}
MODULE_LICENSE("GPL v2");
