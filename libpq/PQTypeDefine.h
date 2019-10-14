/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: header file
 */

#ifndef __PQ_TYPE_DEFINE_H
#define __PQ_TYPE_DEFINE_H

#include "cm.h"
#include "ve.h"
#include "ldim.h"
#include "amstream.h"
#include "amvecm.h"
#include "common.h"
// ***************************************************************************
// *** enum definitions *********************************************
// ***************************************************************************
typedef union tag_suc {
    short s;
    unsigned char c[2];
} SUC;

typedef union tag_usuc {
    unsigned short s;
    unsigned char c[2];
} USUC;

typedef enum is_3d_type_e {
    INDEX_3D_INVALID = -1,
    INDEX_2D = 0,
    INDEX_3D = 1,
} is_3d_type_t;

typedef enum vpp_deblock_mode_e {
    VPP_DEBLOCK_MODE_OFF,
    VPP_DEBLOCK_MODE_LOW,
    VPP_DEBLOCK_MODE_MIDDLE,
    VPP_DEBLOCK_MODE_HIGH,
    VPP_DEBLOCK_MODE_AUTO,
} vpp_deblock_mode_t;

typedef enum vpp_color_space_type_e {
    VPP_COLOR_SPACE_AUTO,
    VPP_COLOR_SPACE_YUV,
    VPP_COLOR_SPACE_RGB,
} vpp_color_space_type_t;

typedef enum vpp_display_mode_e {
    VPP_DISPLAY_MODE_169,
    VPP_DISPLAY_MODE_PERSON,
    VPP_DISPLAY_MODE_MOVIE,
    VPP_DISPLAY_MODE_CAPTION,
    VPP_DISPLAY_MODE_MODE43,
    VPP_DISPLAY_MODE_FULL,
    VPP_DISPLAY_MODE_NORMAL,
    VPP_DISPLAY_MODE_NOSCALEUP,
    VPP_DISPLAY_MODE_CROP_FULL,
    VPP_DISPLAY_MODE_CROP,
    VPP_DISPLAY_MODE_ZOOM,
    VPP_DISPLAY_MODE_MAX,
} vpp_display_mode_t;

typedef enum vpp_color_management2_e {
    VPP_COLOR_MANAGEMENT2_MODE_OFF,
    VPP_COLOR_MANAGEMENT2_MODE_OPTIMIZE,
    VPP_COLOR_MANAGEMENT2_MODE_ENHANCE,
    VPP_COLOR_MANAGEMENT2_MODE_DEMO,
    VPP_COLOR_MANAGEMENT2_MODE_MAX,
} vpp_color_management2_t;

typedef enum vpp_noise_reduction_mode_e {
    VPP_NOISE_REDUCTION_MODE_OFF,
    VPP_NOISE_REDUCTION_MODE_LOW,
    VPP_NOISE_REDUCTION_MODE_MID,
    VPP_NOISE_REDUCTION_MODE_HIGH,
    VPP_NOISE_REDUCTION_MODE_AUTO,
    VPP_NOISE_REDUCTION_MODE_MAX,
} vpp_noise_reduction_mode_t;

typedef enum vpp_xvycc_mode_e {
    VPP_XVYCC_MODE_OFF,
    VPP_XVYCC_MODE_STANDARD,
    VPP_XVYCC_MODE_ENHANCE,
    VPP_XVYCC_MODE_MAX,
} vpp_xvycc_mode_t;

typedef enum vpp_mcdi_mode_e {
    VPP_MCDI_MODE_OFF,
    VPP_MCDI_MODE_STANDARD,
    VPP_MCDI_MODE_ENHANCE,
    VPP_MCDI_MODE_MAX,
} vpp_mcdi_mode_t;

typedef enum vpp_color_temperature_mode_e {
    VPP_COLOR_TEMPERATURE_MODE_STANDARD,
    VPP_COLOR_TEMPERATURE_MODE_WARM,
    VPP_COLOR_TEMPERATURE_MODE_COLD,
    VPP_COLOR_TEMPERATURE_MODE_USER,
    VPP_COLOR_TEMPERATURE_MODE_MAX,
} vpp_color_temperature_mode_t;

typedef struct vpp_pq_para_s {
    int brightness;
    int contrast;
    int saturation;
    int hue;
    int sharpness;
    int backlight;
    int nr;
} vpp_pq_para_t;

typedef enum vpp_gamma_curve_e {
    VPP_GAMMA_CURVE_DEFAULT,//choose gamma table by value has been saved.
    VPP_GAMMA_CURVE_1,
    VPP_GAMMA_CURVE_2,
    VPP_GAMMA_CURVE_3,
    VPP_GAMMA_CURVE_4,
    VPP_GAMMA_CURVE_5,
    VPP_GAMMA_CURVE_6,
    VPP_GAMMA_CURVE_7,
    VPP_GAMMA_CURVE_8,
    VPP_GAMMA_CURVE_9,
    VPP_GAMMA_CURVE_10,
    VPP_GAMMA_CURVE_11,
    VPP_GAMMA_CURVE_MAX,
} vpp_gamma_curve_t;

typedef enum vpp_picture_mode_e {
    VPP_PICTURE_MODE_STANDARD,
    VPP_PICTURE_MODE_BRIGHT,
    VPP_PICTURE_MODE_SOFT,
    VPP_PICTURE_MODE_USER,
    VPP_PICTURE_MODE_MOVIE,
    VPP_PICTURE_MODE_COLORFUL,
    VPP_PICTURE_MODE_MONITOR,
    VPP_PICTURE_MODE_GAME,
    VPP_PICTURE_MODE_SPORTS,
    VPP_PICTURE_MODE_SONY,
    VPP_PICTURE_MODE_SAMSUNG,
    VPP_PICTURE_MODE_SHARP,
    VPP_PICTURE_MODE_MAX,
} vpp_picture_mode_t;

typedef enum tvpq_data_type_e {
    TVPQ_DATA_BRIGHTNESS,
    TVPQ_DATA_CONTRAST,
    TVPQ_DATA_SATURATION,
    TVPQ_DATA_HUE,
    TVPQ_DATA_SHARPNESS,
    TVPQ_DATA_VOLUME,

    TVPQ_DATA_MAX,
} tvpq_data_type_t;

typedef enum vpp_test_pattern_e {
    VPP_TEST_PATTERN_NONE,
    VPP_TEST_PATTERN_RED,
    VPP_TEST_PATTERN_GREEN,
    VPP_TEST_PATTERN_BLUE,
    VPP_TEST_PATTERN_WHITE,
    VPP_TEST_PATTERN_BLACK,
    VPP_TEST_PATTERN_MAX,
} vpp_test_pattern_t;

typedef enum vpp_color_demomode_e {
    VPP_COLOR_DEMO_MODE_ALLON,
    VPP_COLOR_DEMO_MODE_YOFF,
    VPP_COLOR_DEMO_MODE_COFF,
    VPP_COLOR_DEMO_MODE_GOFF,
    VPP_COLOR_DEMO_MODE_MOFF,
    VPP_COLOR_DEMO_MODE_ROFF,
    VPP_COLOR_DEMO_MODE_BOFF,
    VPP_COLOR_DEMO_MODE_RGBOFF,
    VPP_COLOR_DEMO_MODE_YMCOFF,
    VPP_COLOR_DEMO_MODE_ALLOFF,
    VPP_COLOR_DEMO_MODE_MAX,
} vpp_color_demomode_t;

typedef enum vpp_color_basemode_e {
    VPP_COLOR_BASE_MODE_OFF,
    VPP_COLOR_BASE_MODE_OPTIMIZE,
    VPP_COLOR_BASE_MODE_ENHANCE,
    VPP_COLOR_BASE_MODE_DEMO,
    VPP_COLOR_BASE_MODE_MAX,
} vpp_color_basemode_t;

typedef enum noline_params_type_e {
    NOLINE_PARAMS_TYPE_BRIGHTNESS,
    NOLINE_PARAMS_TYPE_CONTRAST,
    NOLINE_PARAMS_TYPE_SATURATION,
    NOLINE_PARAMS_TYPE_HUE,
    NOLINE_PARAMS_TYPE_SHARPNESS,
    NOLINE_PARAMS_TYPE_VOLUME,
    NOLINE_PARAMS_TYPE_BACKLIGHT,
    NOLINE_PARAMS_TYPE_MAX,
} noline_params_type_t;

typedef enum SSM_status_e
{
    SSM_HEADER_INVALID = 0,
    SSM_HEADER_VALID = 1,
    SSM_HEADER_STRUCT_CHANGE = 2,
} SSM_status_t;

typedef enum Dynamic_contrst_status_e
{
    DYNAMIC_CONTRAST_OFF,
    DYNAMIC_CONTRAST_LOW,
    DYNAMIC_CONTRAST_MID,
    DYNAMIC_CONTRAST_HIGH,
} Dynamic_contrst_status_t;

typedef enum Dynamic_backlight_status_e
{
    DYNAMIC_BACKLIGHT_OFF = 0,
    DYNAMIC_BACKLIGHT_LOW = 1,
    DYNAMIC_BACKLIGHT_HIGH = 2,
} Dynamic_backlight_status_t;

typedef enum tvin_aspect_ratio_e {
    TVIN_ASPECT_NULL = 0,
    TVIN_ASPECT_1x1,
    TVIN_ASPECT_4x3,
    TVIN_ASPECT_16x9,
    TVIN_ASPECT_14x9,
    TVIN_ASPECT_MAX,
} tvin_aspect_ratio_t;

typedef enum color_fmt_e {
    RGB444 = 0,
    YUV422, // 1
    YUV444, // 2
    YUYV422,// 3
    YVYU422,// 4
    UYVY422,// 5
    VYUY422,// 6
    NV12,   // 7
    NV21,   // 8
    BGGR,   // 9  raw data
    RGGB,   // 10 raw data
    GBRG,   // 11 raw data
    GRBG,   // 12 raw data
    COLOR_FMT_MAX,
} color_fmt_t;

typedef enum local_contrast_mode_e
{
    LOCAL_CONTRAST_MODE_OFF = 0,
    LOCAL_CONTRAST_MODE_LOW,
    LOCAL_CONTRAST_MODE_MID,
    LOCAL_CONTRAST_MODE_HIGH,
    LOCAL_CONTRAST_MODE_MAX,
} local_contrast_mode_t;

typedef struct noline_params_s {
    int osd0;
    int osd25;
    int osd50;
    int osd75;
    int osd100;
} noline_params_t;

// ***************************************************************************
// *** struct definitions *********************************************
// ***************************************************************************
typedef struct source_input_param_s {
    tv_source_input_t source_input;
    tvin_sig_fmt_t sig_fmt;
    tvin_trans_fmt_t trans_fmt;
} source_input_param_t;

typedef struct tvin_cutwin_s {
    unsigned short hs;
    unsigned short he;
    unsigned short vs;
    unsigned short ve;
} tvin_cutwin_t;

typedef struct tvpq_data_s {
    int TotalNode;
    int NodeValue;
    int IndexValue;
    int RegValue;
    double step;
} tvpq_data_t;

typedef struct tvpq_sharpness_reg_s {
    int TotalNode;
    am_reg_t Value;
    int NodeValue;
    int IndexValue;
    double step;
} tvpq_sharpness_reg_t;

typedef struct tvpq_sharpness_regs_s {
    int length;
    tvpq_sharpness_reg_t reg_data[50];
} tvpq_sharpness_regs_t;

#define CC_PROJECT_INFO_ITEM_MAX_LEN  (64)

typedef struct tvpq_nonlinear_s {
    int osd0;
    int osd25;
    int osd50;
    int osd75;
    int osd100;
} tvpq_nonlinear_t;

typedef struct tvpq_databaseinfo_s {
    char ToolVersion[32];
    char ProjectVersion[32];
    char GenerateTime[32];
}tvpq_databaseinfo_t;
#endif
