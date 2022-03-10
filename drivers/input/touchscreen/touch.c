/***************************************************
 * File:touch.c
 * VENDOR_EDIT
 * Copyright (c)  2008- 2030  Oppo Mobile communication Corp.ltd.
 * Description:
 *             tp dev
 * Version:1.0:
 * Date created:2016/09/02
 * Author: hao.wang@Bsp.Driver
 * TAG: BSP.TP.Init
*/


#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/serio.h>
#include "oppo_touchscreen/tp_devices.h"
#include "oppo_touchscreen/touchpanel_common.h"

//#include <soc/oppo/oppo_project.h>
#include "touch.h"

#define MAX_LIMIT_DATA_LENGTH         100

struct tp_dev_name tp_dev_names[] = {
    {TP_INNOLUX, "INNOLUX"},
    {TP_HLT, "HLT"},
    {TP_TXD, "TXD"},
    {TP_DJN, "DJN"},
    {TP_TXD_BOE,"TXD_BOE"},
    {TP_UNKNOWN, "UNKNOWN"},
};

#define GET_TP_DEV_NAME(tp_type) ((tp_dev_names[tp_type].type == (tp_type))?tp_dev_names[tp_type].name:"UNMATCH")

int g_tp_dev_vendor = TP_UNKNOWN;
int g_chip_name = UNKNOWN_IC;   //add by wanglongfei  distinguish IC
typedef enum {
    TP_INDEX_NULL,
    nt36525b_djn,
    nt36525b_hlt,
    ili9881_inx,
    ili9881_txd,
    ili9881_hlt,
    ili9881_txd_boe,
    hx83102d_hlt,
} TP_USED_INDEX;
TP_USED_INDEX tp_used_index  = TP_INDEX_NULL;
extern char* saved_command_line;

/*
* this function is used to judge whether the ic driver should be loaded
* For incell module, tp is defined by lcd module, so if we judge the tp ic
* by the boot command line of containing lcd string, we can also get tp type.
*/
bool no_flash = false;
TP_USED_IC __init tp_judge_ic_match(void)
{
    pr_err("[TP] saved_command_line = %s \n", saved_command_line);


    if (strstr(saved_command_line, "nt36525b_hdp_dsi_vdo_hlt_al2350")) {
        g_tp_dev_vendor = TP_HLT;
        tp_used_index = nt36525b_hlt;
        return NT36525B;
    }

    if (strstr(saved_command_line, "nt36525b_hdp_dsi_vdo_djn_al2350")) {
        g_tp_dev_vendor = TP_DJN;
        tp_used_index = nt36525b_djn;
        return NT36525B;
    }

    if (strstr(saved_command_line, "ili9881h_hdp_dsi_vdo_inx_al2350")) {
        g_tp_dev_vendor = TP_INNOLUX;
        tp_used_index = ili9881_inx;
        return ILI9881H;
    }

//Longfei.Wang@ODM_WT.BSP.TP.NoBug, 2020/01/03, add for TXD_BOE TP bringup
    if (strstr(saved_command_line, "ili9881h_hdp_dsi_vdo_txd_boe")) {
        g_tp_dev_vendor = TP_TXD_BOE;
        tp_used_index = ili9881_txd_boe;
        g_chip_name = ILI9881H;
        printk("g_tp_dev_vendor: %d, tp_used_index: %d\n", g_tp_dev_vendor, tp_used_index);
        return g_chip_name;
    }

//Zhenzhen.Wu@ODM_WT.BSP.TP.NoBug, 2019/12/14, add for TP bringup
    if (strstr(saved_command_line, "ili9881h_hdp_dsi_vdo_txd")) {
        g_tp_dev_vendor = TP_TXD;
        tp_used_index = ili9881_txd;
        g_chip_name = ILI9881H;
        printk("g_tp_dev_vendor: %d, tp_used_index: %d\n", g_tp_dev_vendor, tp_used_index);
        return g_chip_name;
    }

//Longfei.Wang@ODM_WT.BSP.TP.NoBug, 2019/12/30, add for TP bringup
	if (strstr(saved_command_line, "ili9881h_hdp_dsi_vdo_hlt")) {
        g_tp_dev_vendor = TP_HLT;
        tp_used_index = ili9881_hlt;
        g_chip_name = ILI9881H;
        printk("g_tp_dev_vendor: %d, tp_used_index: %d\n", g_tp_dev_vendor, tp_used_index);
        return g_chip_name;
    }

//Longfei.Wang@ODM_WT.BSP.TP.NoBug, 2020/02/10, add for TP bringup
	if (strstr(saved_command_line, "hx83102d_hdp_dsi_vdo_hlt")) {
		g_tp_dev_vendor = TP_HLT;
		tp_used_index = hx83102d_hlt;
        g_chip_name = HX83102D;
        printk("[TP]touchpanel: g_tp_dev_vendor: %d, tp_used_index: %d\n", g_tp_dev_vendor, tp_used_index);
		return g_chip_name;
		}

	pr_err("Lcd module not found\n");
    return UNKNOWN_IC;
}

int tp_util_get_vendor(struct hw_resource *hw_res, struct panel_info *panel_data)
{

    char *vendor = NULL;
    panel_data->test_limit_name = kzalloc(MAX_LIMIT_DATA_LENGTH, GFP_KERNEL);
    if (panel_data->test_limit_name == NULL) {
        pr_err("[TP]panel_data.test_limit_name kzalloc error\n");
    }
    panel_data->tp_type = g_tp_dev_vendor;
    vendor = GET_TP_DEV_NAME(panel_data->tp_type);
    strncpy(panel_data->manufacture_info.manufacture, vendor, MAX_DEVICE_MANU_LENGTH);

    if (panel_data->tp_type == TP_UNKNOWN) {
        pr_err("[TP]%s type is unknown\n", __func__);
        return 0;
    }

    panel_data->vid_len = 7;
    if (panel_data->fw_name) {

        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
                 "tp/19451/FW_%s_%s.bin",
                 panel_data->chip_name, vendor);
    }

    if (panel_data->test_limit_name) {
        snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH,
                 "/tp/19451/LIMIT_%s_%s.img",
                 panel_data->chip_name, vendor);
        printk("panel_data->test_limit_name = %s\n", panel_data->test_limit_name);
    }

   if (tp_used_index == nt36525b_hlt) {
        memcpy(panel_data->manufacture_info.version, "FA148HN", 7);
        panel_data->firmware_headfile.firmware_data = FW_19181_NT36525B_HLT;
        panel_data->firmware_headfile.firmware_size = sizeof(FW_19181_NT36525B_HLT);
    } else if (tp_used_index == ili9881_inx) {
        memcpy(panel_data->manufacture_info.version, "FA148II", 7);
        panel_data->firmware_headfile.firmware_data = FW_19181_ILI9881H_INX;
        panel_data->firmware_headfile.firmware_size = sizeof(FW_19181_ILI9881H_INX);
    } else if (tp_used_index == ili9881_txd) {
        memcpy(panel_data->manufacture_info.version, "FA206TI", 7);
        panel_data->firmware_headfile.firmware_data = FW_19451_ILI9881H_TXD;
        panel_data->firmware_headfile.firmware_size = sizeof(FW_19451_ILI9881H_TXD);
    } else if (tp_used_index == ili9881_hlt) {
        memcpy(panel_data->manufacture_info.version, "FA206HI", 7);
        panel_data->firmware_headfile.firmware_data = FW_19451_ILI9881H_HLT;
        panel_data->firmware_headfile.firmware_size = sizeof(FW_19451_ILI9881H_HLT);
    } else if (tp_used_index == ili9881_txd_boe) {
        memcpy(panel_data->manufacture_info.version, "FA206XI", 7);
        panel_data->firmware_headfile.firmware_data = FW_19451_ILI9881H_TXD_BOE;
        panel_data->firmware_headfile.firmware_size = sizeof(FW_19451_ILI9881H_TXD_BOE);
    } else if (tp_used_index == hx83102d_hlt) {
        memcpy(panel_data->manufacture_info.version, "FA206HH", 7);
        panel_data->firmware_headfile.firmware_data = FW_19451_HX83102D_HLT;
        panel_data->firmware_headfile.firmware_size = sizeof(FW_19451_HX83102D_HLT);
    } else if (tp_used_index == nt36525b_djn) {
        memcpy(panel_data->manufacture_info.version, "FA148jn", 7);
        panel_data->firmware_headfile.firmware_data = FW_19181_NT36525B_DJN;
        panel_data->firmware_headfile.firmware_size = sizeof(FW_19181_NT36525B_DJN);
    }else {
        panel_data->firmware_headfile.firmware_data = NULL;
        panel_data->firmware_headfile.firmware_size = 0;
    }


    panel_data->manufacture_info.fw_path = panel_data->fw_name;

    pr_info("fw_path: %s\n", panel_data->manufacture_info.fw_path);
    pr_info("[TP]vendor:%s fw:%s limit:%s\n",
            vendor, panel_data->fw_name,
            panel_data->test_limit_name == NULL ? "NO Limit" : panel_data->test_limit_name);

    return 0;

}
