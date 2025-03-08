/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

// This demo UI is adapted from LVGL official example: https://docs.lvgl.io/master/widgets/extra/meter.html#simple-meter

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "lvgl.h"

static lv_obj_t *info_label = NULL;


void example_lvgl_demo_ui(lv_disp_t *disp)
{

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);

    // 创建标签
    info_label = lv_label_create(lv_scr_act());
    // 设置标签样式
    lv_obj_set_style_text_color(info_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, 0);
    // 设置初始文本
    lv_label_set_text(info_label, "Waiting for GPS data...");
    // 居中显示
    lv_obj_align(info_label, LV_ALIGN_TOP_MID, 0, 10);
}

// 添加更新显示的函数
void update_gps_info(const char *lat, char lat_dir, const char *lon, char lon_dir, const char *time)
{
    if (info_label) {
        static char buf[128];
        // 格式化显示GPS信息
        snprintf(buf, sizeof(buf), 
                "N:%s %c\n"
                "E:%s %c\n"
                "Time:%c%c:%c%c:%c%c",
                lat, lat_dir,
                lon, lon_dir,
                time[0], time[1],    // 时
                time[2], time[3],    // 分
                time[4], time[5]);   // 秒
        
        lv_label_set_text(info_label, buf);
        // 确保标签始终居中显示
        lv_obj_align(info_label, LV_ALIGN_TOP_MID, 0, 10);
    }
}