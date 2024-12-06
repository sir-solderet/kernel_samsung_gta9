#include "touch_feature.h"

static struct lcm_info *gs_common_lcm_info = NULL;

static struct platform_device hwinfo_device = {
    .name = HWINFO_NAME,
    .id = -1,
};

static struct proc_dir_entry *proc_tpinfo_file = NULL;

static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void aot_enable(void *device_data);
/*Tab A9 code for SR-AX6739A-01-343 by wenghailong at 20230606 start*/
static void high_sensitivity_mode(void *device_data);
/*Tab A9 code for SR-AX6739A-01-343 by wenghailong at 20230606 end*/
static void not_support_cmd(void *device_data);

struct sec_cmd sec_cmds[] = {
    {SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
    {SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
    {SEC_CMD("aot_enable", aot_enable),},
    /*Tab A9 code for SR-AX6739A-01-343 by wenghailong at 20230606 start*/
    {SEC_CMD("glove_mode", high_sensitivity_mode),},
    /*Tab A9 code for SR-AX6739A-01-343 by wenghailong at 20230606 end*/
    {SEC_CMD("not_support_cmd", not_support_cmd),},
};
/**
 * This function get_fw_ver_bin
 *
 * @Param[]：void *device_data
 * @Return ：
 */
static void get_fw_ver_bin(void *device_data)
{
    char buff[SEC_CMD_STR_LEN] = { 0 };
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    sec_cmd_set_default_result(sec);
    /*Tab A9 code for AX6739A-241 by suyurui at 20230526 start*/
    if (gs_common_lcm_info->module_name != NULL) {
        /*Tab A9 code for AX6739A-241 by suyurui at 20230530 start*/
        snprintf(buff, sizeof(buff), "%s_BIN=V%02x", gs_common_lcm_info->module_name,
            *(gs_common_lcm_info->fw_version) & 0xFF);
        /*Tab A9 code for AX6739A-241 by suyurui at 20230530 end*/
    } else {
        snprintf(buff, sizeof(buff), "NA_BIN=VNA");
    }
    /*Tab A9 code for AX6739A-241 by suyurui at 20230526 end*/
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_OK;
    sec_cmd_set_cmd_exit(sec);
    TP_INFO("[SEC]: %s", buff);
}
/**
 * This function get_fw_ver_ic
 *
 * @Param[]：void *device_data
 * @Return ：
 */
static void get_fw_ver_ic(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };
    sec_cmd_set_default_result(sec);
    /*Tab A9 code for AX6739A-241 by suyurui at 20230526 start*/
    if (gs_common_lcm_info->module_name != NULL) {
        snprintf(buff, sizeof(buff), "IC=%s",gs_common_lcm_info->module_name);
    } else {
        snprintf(buff, sizeof(buff), "IC=NA");
    }
    /*Tab A9 code for AX6739A-241 by suyurui at 20230526 end*/
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_OK;
    sec_cmd_set_cmd_exit(sec);
    TP_INFO("[SEC]: %s\n",buff);
}
/**
 * This function disable_gesture_wakeup
 *
 * @Param[]：void
 * @Return ：
 */
void disable_gesture_wakeup(void)
{
    TP_INFO("Disable gesture wakeup");
    if (gs_common_lcm_info->gesture_wakeup_disable != NULL) {
        gs_common_lcm_info->gesture_wakeup_disable();
    }
}
/**
 * This function enable_gesture_wakeup
 *
 * @Param[]：void
 * @Return ：
 */
void enable_gesture_wakeup(void)
{
    TP_INFO("Enable gesture wakeup");
    if (gs_common_lcm_info->gesture_wakeup_enable != NULL) {
        gs_common_lcm_info->gesture_wakeup_enable();
    }
}
/**
 * This function aot_enable
 *
 * @Param[]：void *device_data
 * @Return ：
 */
static void aot_enable(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };

    sec_cmd_set_default_result(sec);

    if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
        snprintf(buff, sizeof(buff), "%s", "NG");
        sec->cmd_state = SEC_CMD_STATUS_FAIL;
    } else {
        if (sec->cmd_param[0] == 0) {
            disable_gesture_wakeup();
        } else {
            enable_gesture_wakeup();
        }
        snprintf(buff, sizeof(buff), "%s", "OK");
        sec->cmd_state = SEC_CMD_STATUS_OK;
    }
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_WAITING;
    sec_cmd_set_cmd_exit(sec);

    /*Tab A9 code for AX6739A-2490 by zhawei at 20230726 start*/
    TP_INFO("[SEC]%s: %s\n", __func__, buff);
    /*Tab A9 code for AX6739A-2490 by zhawei at 20230726 end*/
}
/*Tab A9 code for SR-AX6739A-01-343 by wenghailong at 20230606 start*/
/**
 * This function disable_high_sensitivity
 *
 * @Param[]：void
 * @Return ：
 */
void disable_high_sensitivity(void)
{
    TP_INFO("Disable high sensitivity");
}
/**
 * This function enable_high_sensitivity
 *
 * @Param[]：void
 * @Return ：
 */
void enable_high_sensitivity(void)
{
    TP_INFO("Enable high sensitivity");
}
/**
 * This function high_sensitivity_mode
 *
 * @Param[]：void *device_data
 * @Return ：
 */
static void high_sensitivity_mode(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };

    sec_cmd_set_default_result(sec);

    if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
        snprintf(buff, sizeof(buff), "%s", "NG");
        sec->cmd_state = SEC_CMD_STATUS_FAIL;
    } else {
        if (sec->cmd_param[0] == 0) {
            disable_high_sensitivity();
        } else {
            enable_high_sensitivity();
        }
        snprintf(buff, sizeof(buff), "%s", "OK");
        sec->cmd_state = SEC_CMD_STATUS_OK;
    }
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_WAITING;
    sec_cmd_set_cmd_exit(sec);
    /*Tab A9 code for AX6739A-2490 by zhawei at 20230726 start*/
    TP_INFO("[SEC]%s: %s\n", __func__, buff);
    /*Tab A9 code for AX6739A-2490 by zhawei at 20230726 end*/
}
/*Tab A9 code for SR-AX6739A-01-343 by wenghailong at 20230606 end*/
/**
 * This function not_support_cmd
 *
 * @Param[]：void *device_data
 * @Return ：
 */
static void not_support_cmd(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };

    sec_cmd_set_default_result(sec);
    snprintf(buff, sizeof(buff), "%s", "NA");
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
    sec_cmd_set_cmd_exit(sec);

    /*Tab A9 code for AX6739A-2490 by zhawei at 20230726 start*/
    TP_INFO("[SEC]%s: %s\n", __func__, buff);
    /*Tab A9 code for AX6739A-2490 by zhawei at 20230726 end*/
}

int sec_fn_init(struct sec_cmd_data *sec)
{
    int ret = 0;
    /*Tab A9 code for AX6739A-241 by suyurui at 20230526 start*/
    if (sec == NULL) {
        return -EFAULT;
    }
    /*Tab A9 code for AX6739A-241 by suyurui at 20230526 end*/

    ret = sec_cmd_init(sec, sec_cmds, ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
    if (ret !=0) {
        TP_ERROR("failed to sec cmd init");
        sec_cmd_exit(sec, SEC_CLASS_DEVT_TSP);
        return ret;
    }

    TP_INFO("OK\n");
    return 0;

}

static ssize_t ito_test_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    int count = 0;
    int ret = 0;

    TP_INFO("Run MP test with LCM on");
    if (gs_common_lcm_info->ito_test != NULL) {
        ret = gs_common_lcm_info->ito_test ();
        if(ret != 0) {
            TP_ERROR("ITO test failed!");
        } else {
            TP_INFO("ITO test success!");
        }
    }
    count = sprintf(buf, "%s\n", (ret == 0) ? "1" : "0");//return test  result
    return count;
}

static ssize_t ito_test_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
    return count;
}

static DEVICE_ATTR(factory_check, 0644, ito_test_show, ito_test_store);
static struct attribute *factory_sysfs_attrs[] = {
    &dev_attr_factory_check.attr,
    NULL,
};

static const struct attribute_group factory_group = {
    .attrs = factory_sysfs_attrs,
};

int ito_test_node_init(struct platform_device *tp_device)
{
    int ret = 0;
    ret = platform_device_register(tp_device);
    if (ret != 0) {
        platform_device_unregister(tp_device);
        return ret;
    }
    ret = sysfs_create_group(&tp_device->dev.kobj, &factory_group);
    if (ret != 0) {
        sysfs_remove_group(&tp_device->dev.kobj, &factory_group);
        return ret;
    }
    return 0;
}

static ssize_t tp_info_read(struct file *file, char *buff,
                                size_t len, loff_t *pos)
{
    char buf[TPINFO_STR_LEN] = {0};
    int rc = 0;
    /*Tab A9 code for AX6739A-241 by suyurui at 20230526 start*/
    if (gs_common_lcm_info->module_name != NULL) {
        /*Tab A9 code for AX6739A-241 by suyurui at 20230530 start*/
        snprintf(buf, sizeof(buf), "Module_name: %s\nTouch_FW_ver:V%02x\n",
            gs_common_lcm_info->module_name, *(gs_common_lcm_info->fw_version) & 0xFF);
        /*Tab A9 code for AX6739A-241 by suyurui at 20230530 end*/
    } else {
        return rc;
    }
    /*Tab A9 code for AX6739A-241 by suyurui at 20230526 end*/
    rc = simple_read_from_buffer(buff, len, pos, buf, strlen(buf));
    return rc;
}

static const struct proc_ops proc_tp_info_ops = {
    .proc_read = tp_info_read,
};

static int proc_tpinfo_init(void)
{
    proc_tpinfo_file = proc_create(PROC_TPINFO_FILE, 0666, NULL, &proc_tp_info_ops);
    if (proc_tpinfo_file == NULL) {
        TP_ERROR("proc tp_info file create failed!\n");
        return -EPERM;
    }
    return 0;
}
/*Tab A9 code for SR-AX6739A-01-260 by wenghailong at 20230513 start*/
bool get_tp_enable(void)
{
    return g_tp_is_enabled;
}
EXPORT_SYMBOL(get_tp_enable);
/*Tab A9 code for SR-AX6739A-01-260 by wenghailong at 20230513 end*/
/*Tab A9 code for AX6739A-241 by suyurui at 20230526 start*/
void tp_common_init(struct lcm_info *g_lcm_info)
{
    int ret = 0;
    gs_common_lcm_info = g_lcm_info;

    if (gs_common_lcm_info == NULL) {
        TP_ERROR("gs_common_lcm_info is NULL\n");
    } else {
        g_tp_is_enabled = true;

        ret = proc_tpinfo_init();
        if (ret) {
            TP_ERROR("proc_tpinfo_init failed\n");
        }

        ret = ito_test_node_init(&hwinfo_device);
        if (ret) {
            TP_ERROR("ito_test_node_init failed\n");
        }

        ret = sec_fn_init(&gs_common_lcm_info->sec);
        if (ret) {
            TP_ERROR("sec_fn_init failed\n");
        }

        #if defined(CONFIG_ODM_CUSTOM_D85_BUILD)
        if (gs_common_lcm_info->gesture_wakeup_enable != NULL) {
            gs_common_lcm_info->gesture_wakeup_enable();
            TP_INFO("Double click gesture default on in D85 version\n");
        }
        #endif
    }
}
EXPORT_SYMBOL(tp_common_init);
/*Tab A9 code for AX6739A-241 by suyurui at 20230526 end*/

/*Tab A9 code for AX6739A-241 by suyurui at 20230526 start*/
const char* tp_choose_panel(void)
{
    struct device_node *chosen = NULL;
    const char* panel_name = NULL;

    chosen = of_find_node_by_name(NULL, "chosen");
    if (NULL == chosen) {
        TP_ERROR("chosen node is not found\n");
    } else {
        of_property_read_string(chosen, "panel_name", &panel_name);
        TP_INFO("panel_name: %s\n", panel_name);
    }

    return panel_name;
}
EXPORT_SYMBOL(tp_choose_panel);

int tp_detect_panel(const char *tp_ic)
{
    const char *panel_name = tp_choose_panel();
    enum boot_mode_t boot_mode = tp_get_boot_mode();
    /*Tab A9 code for AX6739A-1684 by suyurui at 20230628 start*/
    if ((boot_mode != NORMAL_BOOT) && (boot_mode != ALARM_BOOT)) {
    /*Tab A9 code for AX6739A-1684 by suyurui at 20230628 end*/
        TP_ERROR("tp init fail because boot_mode = %d\n", boot_mode);
        return -EINVAL;
    }

    if (panel_name == NULL) {
        TP_ERROR("panel_name is NULL\n");
        return -EFAULT;
    }
    if (strstr(panel_name, tp_ic)) {
        TP_INFO("tp_ic = %s\n", tp_ic);
        return 0;
    }

    return -EFAULT;
}
EXPORT_SYMBOL(tp_detect_panel);
/*Tab A9 code for AX6739A-241 by suyurui at 20230526 end*/
