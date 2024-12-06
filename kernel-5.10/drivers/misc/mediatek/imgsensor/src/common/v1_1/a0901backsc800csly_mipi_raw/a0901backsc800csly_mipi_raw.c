/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 SC800cs_mipi_raw_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/


#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/types.h>
#include "a0901backsc800csly_mipi_raw.h"

#define PFX "a0901backsc800csly_mipi_raw"
#define printk(format, args...)    printk(PFX "[%s] " format, __FUNCTION__, ##args)

#define SC800CS_LY_SENSOR_GAIN_MAX_VALID_INDEX  6
#define SC800CS_LY_SENSOR_GAIN_MAP_SIZE         6

#define SC800CS_LY_SENSOR_BASE_GAIN           0x400
#define SC800CS_LY_SENSOR_MAX_GAIN            (635 * SC800CS_LY_SENSOR_BASE_GAIN /10)
#define SC800CS_LY_EEPROM_IC_ADDR             0xA0
#define SC800CS_LY_MODULE_ID                  0x01

static DEFINE_SPINLOCK(imgsensor_drv_lock);
/*Tab A9 code for SR-AX6739A-01-224 by hudongdong at 20230519 start */
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = A0901_BACK_SC800CSLY_SENSOR_ID,

	.checksum_value = 0xf7375923,        //checksum value for Camera Auto Test

	.pre = {
		.pclk = 132000000,
		.linelength = 1760,
		.framelength = 2500,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 275200000,//1376*2/10
	},
	.cap = {
		.pclk = 132000000,
		.linelength = 1760,
		.framelength = 2500,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 275200000,//1376*2/10
	},
	.cap1 = {
		.pclk = 132000000,
		.linelength = 1760,
		.framelength = 2500,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 275200000,//1376*2/10
	},
	.normal_video = {
		.pclk = 132000000,
		.linelength = 1760,
		.framelength = 2500,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 275200000,//1376*2/10
	},
	.hs_video = {
		.pclk = 132000000,
		.linelength = 1760,
		.framelength = 2500,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 275200000,//1376*2/10
	},
	.slim_video = {
		.pclk = 132000000,
		.linelength = 1760,
		.framelength = 2500,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 275200000,//1376*2/10
	},

	.margin = 5,
	.min_shutter = 2,
    .min_gain = 64,
    .max_gain = 1024,
    .min_gain_iso = 100,
    .gain_step = 1,
    .gain_type = 3,
	.max_frame_length = 0x3fff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 5,	  //support sensor mode num

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
    .hs_video_delay_frame = 2,    /* enter high_speed_video delay frame num */
    .slim_video_delay_frame = 2,    /* enter slim_video delay frame num */
    .frame_time_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.i2c_addr_table = {0x6c, 0x20, 0xff},
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x4de,
	.gain = 0x40,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
    .test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
    .ihdr_en = 0,
	.i2c_write_id = 0x6c,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] =
{
	{ 3264, 2448, 0, 0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448},	/* Preview */
	{ 3264, 2448, 0, 0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448},	/* capture */
	{ 3264, 2448, 0, 0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448},	/* video */
	{ 3264, 2448, 0, 0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448},
	{ 3264, 2448, 0, 0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}};



static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
	char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	printk("frame length = %d\n", imgsensor.frame_length);
	write_cmos_sensor(0x320e, (imgsensor.frame_length >> 8) & 0xff);
	write_cmos_sensor(0x320f, imgsensor.frame_length & 0xff);
}

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x3107) << 8) | read_cmos_sensor(0x3108)); //0xd126
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
    kal_uint32 frame_length = imgsensor.frame_length;

    printk("framerate = %d, min framelength should enable? \n", framerate,min_framelength_en);

    frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
    spin_lock(&imgsensor_drv_lock);
    imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
	 frame_length : imgsensor.min_frame_length;
    imgsensor.dummy_line = imgsensor.frame_length -
	 imgsensor.min_frame_length;

    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
    {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
        imgsensor.dummy_line = imgsensor.frame_length -
		 imgsensor.min_frame_length;
    }
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}

/*************************************************************************
 * FUNCTION
 *    set_shutter
 *
 * DESCRIPTION
 *    This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *    iShutter : exposured lines
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/

static void write_shutter(kal_uint16 shutter)
{
    kal_uint16 realtime_fps = 0;

	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */

	// if shutter bigger than frame_length, should extend frame length first
	//printk("pangfei shutter %d line %d\n",shutter,__LINE__);
	spin_lock(&imgsensor_drv_lock);

	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

    if (imgsensor.autoflicker_en) {
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if(realtime_fps >= 297 && realtime_fps <= 305)
            set_max_framerate(296,0);
        else if(realtime_fps >= 147 && realtime_fps <= 150)
            set_max_framerate(146,0);
        else {
        // Extend frame length
	    write_cmos_sensor(0x326d, (imgsensor.frame_length >> 16) & 0x7f);
		write_cmos_sensor(0x320e, (imgsensor.frame_length >> 8) & 0xff);
		write_cmos_sensor(0x320f, imgsensor.frame_length & 0xFF);
            }
    } else {
        // Extend frame length
		write_cmos_sensor(0x326d, (imgsensor.frame_length >> 16) & 0x7f);
		write_cmos_sensor(0x320e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x320f, imgsensor.frame_length & 0xFF);
    }
    // Update Shutter
    shutter = shutter *2;
	write_cmos_sensor(0x3e20, (shutter >> 20) & 0x0F);
	write_cmos_sensor(0x3e00, (shutter >> 12) & 0xFF);
	write_cmos_sensor(0x3e01, (shutter >> 4)&0xFF);
	write_cmos_sensor(0x3e02, (shutter<<4) & 0xF0);
	printk("Exit! shutter = %d, framelength = %d\n", shutter, imgsensor.frame_length);
}

static void set_shutter(kal_uint16 shutter)
{
    unsigned long flags;
    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	write_shutter(shutter);

}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = gain << 4;

	if (reg_gain < SC800CS_LY_SENSOR_BASE_GAIN)
		reg_gain = SC800CS_LY_SENSOR_BASE_GAIN;
	else if (reg_gain > SC800CS_LY_SENSOR_MAX_GAIN)
		reg_gain = SC800CS_LY_SENSOR_MAX_GAIN;

	return (kal_uint16)reg_gain;
}
/*************************************************************************
* FUNCTION
*	set_gain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*	iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;
	kal_uint32 temp_gain;
	kal_int16 gain_index;
	kal_uint16 SC800CS_LY_AGC_Param[SC800CS_LY_SENSOR_GAIN_MAP_SIZE][2] = {
		{  1024,  0x00 },
		{  2048,  0x01 },
		{  4096,  0x03 },
		{  8192,  0x07 },
		{ 16384,  0x0f },
		{ 32768,  0x1f },
	};

	reg_gain = gain2reg(gain);

	for (gain_index = SC800CS_LY_SENSOR_GAIN_MAX_VALID_INDEX - 1; gain_index >= 0; gain_index--)
		if (reg_gain >= SC800CS_LY_AGC_Param[gain_index][0])
			break;

	write_cmos_sensor(0x3e09, SC800CS_LY_AGC_Param[gain_index][1]);
	temp_gain = reg_gain * SC800CS_LY_SENSOR_BASE_GAIN / SC800CS_LY_AGC_Param[gain_index][0];
	write_cmos_sensor(0x3e07, (temp_gain >> 3) & 0xff);
	printk("SC800CS_LY_AGC_Param[gain_index][1] = 0x%x, temp_gain = 0x%x, reg_gain = %d, gain = %d\n",
		SC800CS_LY_AGC_Param[gain_index][1], temp_gain, reg_gain, gain);

	return reg_gain;
}

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	printk("le: 0x%x, se: 0x%x, gain: 0x%x\n", le, se, gain);
}


#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	printk("image_mirror = %d\n", image_mirror);
}
#endif
/*************************************************************************
* FUNCTION
*	night_mode
*
* DESCRIPTION
*	This function night mode of sensor.
*
* PARAMETERS
*	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
	/* No Need to implement this function */
}

static void sensor_init(void)
{
	printk(">> %s()\n", __func__);
	/*	
	Resolution: 3264(H)x2444(V)_30fps
	MCLK=24M
	MIPI速率：1376Mbps
	2lane no-continue, RAW 10
	*/
	write_cmos_sensor(0x0103,0x01);
	write_cmos_sensor(0x0100,0x00);
	write_cmos_sensor(0x36e9,0x80);
	write_cmos_sensor(0x36f9,0x80);
	write_cmos_sensor(0x36ea,0x08);
	write_cmos_sensor(0x36eb,0x0c);
	write_cmos_sensor(0x36ec,0x0b);
	write_cmos_sensor(0x36ed,0x14);
	write_cmos_sensor(0x36e9,0x04);
	write_cmos_sensor(0x36f9,0x24);
	write_cmos_sensor(0x301f,0x50);
	write_cmos_sensor(0x302d,0x20);
	write_cmos_sensor(0x3106,0x01);
	write_cmos_sensor(0x320c,0x06);
	write_cmos_sensor(0x320d,0xe0);
	write_cmos_sensor(0x3250,0x40);
	write_cmos_sensor(0x325f,0x24);
	write_cmos_sensor(0x3301,0x08);
	write_cmos_sensor(0x3306,0x40);
	write_cmos_sensor(0x3309,0x80);
	write_cmos_sensor(0x330b,0xc6);
	write_cmos_sensor(0x331e,0x49);
	write_cmos_sensor(0x331f,0x71);
	write_cmos_sensor(0x3333,0x10);
	write_cmos_sensor(0x335d,0x60);
	write_cmos_sensor(0x3364,0x56);
	write_cmos_sensor(0x3390,0x01);
	write_cmos_sensor(0x3391,0x03);
	write_cmos_sensor(0x3392,0x07);
	write_cmos_sensor(0x3393,0x08);
	write_cmos_sensor(0x3394,0x10);
	write_cmos_sensor(0x3395,0x34);
	write_cmos_sensor(0x33b1,0x80);
	write_cmos_sensor(0x341e,0x00);
	write_cmos_sensor(0x34a9,0x18);
	write_cmos_sensor(0x34ab,0xc6);
	write_cmos_sensor(0x34ad,0xc6);
	write_cmos_sensor(0x3621,0x68);
	write_cmos_sensor(0x3622,0x83);
	write_cmos_sensor(0x3627,0x14);
	write_cmos_sensor(0x3635,0x26);
	write_cmos_sensor(0x3637,0x23);
	write_cmos_sensor(0x3638,0xc7);
	write_cmos_sensor(0x3639,0xf4);
	write_cmos_sensor(0x3670,0x4b);
	write_cmos_sensor(0x3674,0xc0);
	write_cmos_sensor(0x3675,0xa6);
	write_cmos_sensor(0x3676,0xaa);
	write_cmos_sensor(0x367c,0x03);
	write_cmos_sensor(0x367d,0x07);
	write_cmos_sensor(0x3690,0x63);
	write_cmos_sensor(0x3691,0x53);
	write_cmos_sensor(0x3692,0x64);
	write_cmos_sensor(0x3699,0x8a);
	write_cmos_sensor(0x369a,0x9d);
	write_cmos_sensor(0x369b,0xbc);
	write_cmos_sensor(0x369c,0x03);
	write_cmos_sensor(0x369d,0x07);
	write_cmos_sensor(0x36a2,0x03);
	write_cmos_sensor(0x36a3,0x07);
	write_cmos_sensor(0x36a6,0x01);
	write_cmos_sensor(0x36a7,0x07);
	write_cmos_sensor(0x36ab,0xc1);
	write_cmos_sensor(0x36ac,0xc1);
	write_cmos_sensor(0x36ad,0xc1);
	write_cmos_sensor(0x3900,0x1d);
	write_cmos_sensor(0x3902,0xc5);
	write_cmos_sensor(0x3905,0xd9);
	write_cmos_sensor(0x3907,0x00);
	write_cmos_sensor(0x3908,0x41);
	write_cmos_sensor(0x391b,0x80);
	write_cmos_sensor(0x391c,0x38);
	write_cmos_sensor(0x391d,0x19);
	write_cmos_sensor(0x391f,0x00);
	write_cmos_sensor(0x3954,0x86);
	write_cmos_sensor(0x3e00,0x01);
	write_cmos_sensor(0x3e01,0x37);
	write_cmos_sensor(0x3e02,0xe0);
	write_cmos_sensor(0x4000,0x00);
	write_cmos_sensor(0x4001,0x06);
	write_cmos_sensor(0x4002,0x23);
	write_cmos_sensor(0x4003,0xb8);
	write_cmos_sensor(0x4004,0x8e);
	write_cmos_sensor(0x4005,0x00);
	write_cmos_sensor(0x4006,0x0b);
	write_cmos_sensor(0x4007,0x3d);
	write_cmos_sensor(0x4008,0x00);
	write_cmos_sensor(0x4009,0xb3);
	write_cmos_sensor(0x440e,0x02);
	write_cmos_sensor(0x4509,0x30);
	write_cmos_sensor(0x4800,0x24);
	write_cmos_sensor(0x4816,0x21);
	write_cmos_sensor(0x4837,0x17);
	write_cmos_sensor(0x5000,0x0e);
	write_cmos_sensor(0x5799,0x06);
	write_cmos_sensor(0x5900,0xf1);
	write_cmos_sensor(0x5901,0x04);
	write_cmos_sensor(0x59e0,0xfe);
	write_cmos_sensor(0x59e1,0x40);
	write_cmos_sensor(0x59e2,0x38);
	write_cmos_sensor(0x59e3,0x30);
	write_cmos_sensor(0x59e4,0x20);
	write_cmos_sensor(0x59e5,0x38);
	write_cmos_sensor(0x59e6,0x30);
	write_cmos_sensor(0x59e7,0x20);
	write_cmos_sensor(0x59e8,0x3f);
	write_cmos_sensor(0x59e9,0x38);
	write_cmos_sensor(0x59ea,0x30);
	write_cmos_sensor(0x59eb,0x3f);
	write_cmos_sensor(0x59ec,0x38);
	write_cmos_sensor(0x59ed,0x30);
	write_cmos_sensor(0x59ee,0xfe);
	write_cmos_sensor(0x59ef,0x40);
	write_cmos_sensor(0x59f4,0x38);
	write_cmos_sensor(0x59f5,0x30);
	write_cmos_sensor(0x59f6,0x20);
	write_cmos_sensor(0x59f7,0x38);
	write_cmos_sensor(0x59f8,0x30);
	write_cmos_sensor(0x59f9,0x20);
	write_cmos_sensor(0x59fa,0x3f);
	write_cmos_sensor(0x59fb,0x38);
	write_cmos_sensor(0x59fc,0x30);
	write_cmos_sensor(0x59fd,0x3f);
	write_cmos_sensor(0x59fe,0x38);
	write_cmos_sensor(0x59ff,0x30);
	write_cmos_sensor(0x3650,0x61);
	write_cmos_sensor(0x3651,0x9f);
	printk("<< %s()\n", __func__);
}

static void preview_setting(void)
{
	//Resolution: 3264(H)x2444(V)_30fps
	//MCLK=24M
	//MIPI速率：1376Mbps
	//2lane no-continue, RAW 10
	write_cmos_sensor(0x0103,0x01);
	write_cmos_sensor(0x0100,0x00);
	write_cmos_sensor(0x36e9,0x80);
	write_cmos_sensor(0x36f9,0x80);
	write_cmos_sensor(0x36ea,0x08);
	write_cmos_sensor(0x36eb,0x0c);
	write_cmos_sensor(0x36ec,0x0b);
	write_cmos_sensor(0x36ed,0x14);
	write_cmos_sensor(0x36e9,0x04);
	write_cmos_sensor(0x36f9,0x24);
	write_cmos_sensor(0x301f,0x50);
	write_cmos_sensor(0x302d,0x20);
	write_cmos_sensor(0x3106,0x01);
	write_cmos_sensor(0x320c,0x06);
	write_cmos_sensor(0x320d,0xe0);
	write_cmos_sensor(0x3250,0x40);
	write_cmos_sensor(0x325f,0x24);
	write_cmos_sensor(0x3301,0x08);
	write_cmos_sensor(0x3306,0x40);
	write_cmos_sensor(0x3309,0x80);
	write_cmos_sensor(0x330b,0xc6);
	write_cmos_sensor(0x331e,0x49);
	write_cmos_sensor(0x331f,0x71);
	write_cmos_sensor(0x3333,0x10);
	write_cmos_sensor(0x335d,0x60);
	write_cmos_sensor(0x3364,0x56);
	write_cmos_sensor(0x3390,0x01);
	write_cmos_sensor(0x3391,0x03);
	write_cmos_sensor(0x3392,0x07);
	write_cmos_sensor(0x3393,0x08);
	write_cmos_sensor(0x3394,0x10);
	write_cmos_sensor(0x3395,0x34);
	write_cmos_sensor(0x33b1,0x80);
	write_cmos_sensor(0x341e,0x00);
	write_cmos_sensor(0x34a9,0x18);
	write_cmos_sensor(0x34ab,0xc6);
	write_cmos_sensor(0x34ad,0xc6);
	write_cmos_sensor(0x3621,0x68);
	write_cmos_sensor(0x3622,0x83);
	write_cmos_sensor(0x3627,0x14);
	write_cmos_sensor(0x3635,0x26);
	write_cmos_sensor(0x3637,0x23);
	write_cmos_sensor(0x3638,0xc7);
	write_cmos_sensor(0x3639,0xf4);
	write_cmos_sensor(0x3670,0x4b);
	write_cmos_sensor(0x3674,0xc0);
	write_cmos_sensor(0x3675,0xa6);
	write_cmos_sensor(0x3676,0xaa);
	write_cmos_sensor(0x367c,0x03);
	write_cmos_sensor(0x367d,0x07);
	write_cmos_sensor(0x3690,0x63);
	write_cmos_sensor(0x3691,0x53);
	write_cmos_sensor(0x3692,0x64);
	write_cmos_sensor(0x3699,0x8a);
	write_cmos_sensor(0x369a,0x9d);
	write_cmos_sensor(0x369b,0xbc);
	write_cmos_sensor(0x369c,0x03);
	write_cmos_sensor(0x369d,0x07);
	write_cmos_sensor(0x36a2,0x03);
	write_cmos_sensor(0x36a3,0x07);
	write_cmos_sensor(0x36a6,0x01);
	write_cmos_sensor(0x36a7,0x07);
	write_cmos_sensor(0x36ab,0xc1);
	write_cmos_sensor(0x36ac,0xc1);
	write_cmos_sensor(0x36ad,0xc1);
	write_cmos_sensor(0x3900,0x1d);
	write_cmos_sensor(0x3902,0xc5);
	write_cmos_sensor(0x3905,0xd9);
	write_cmos_sensor(0x3907,0x00);
	write_cmos_sensor(0x3908,0x41);
	write_cmos_sensor(0x391b,0x80);
	write_cmos_sensor(0x391c,0x38);
	write_cmos_sensor(0x391d,0x19);
	write_cmos_sensor(0x391f,0x00);
	write_cmos_sensor(0x3954,0x86);
	write_cmos_sensor(0x3e00,0x01);
	write_cmos_sensor(0x3e01,0x37);
	write_cmos_sensor(0x3e02,0xe0);
	write_cmos_sensor(0x4000,0x00);
	write_cmos_sensor(0x4001,0x06);
	write_cmos_sensor(0x4002,0x23);
	write_cmos_sensor(0x4003,0xb8);
	write_cmos_sensor(0x4004,0x8e);
	write_cmos_sensor(0x4005,0x00);
	write_cmos_sensor(0x4006,0x0b);
	write_cmos_sensor(0x4007,0x3d);
	write_cmos_sensor(0x4008,0x00);
	write_cmos_sensor(0x4009,0xb3);
	write_cmos_sensor(0x440e,0x02);
	write_cmos_sensor(0x4509,0x30);
	write_cmos_sensor(0x4800,0x24);
	write_cmos_sensor(0x4816,0x21);
	write_cmos_sensor(0x4837,0x17);
	write_cmos_sensor(0x5000,0x0e);
	write_cmos_sensor(0x5799,0x06);
	write_cmos_sensor(0x59e0,0xfe);
	write_cmos_sensor(0x59e1,0x40);
	write_cmos_sensor(0x59e2,0x38);
	write_cmos_sensor(0x59e3,0x30);
	write_cmos_sensor(0x59e4,0x20);
	write_cmos_sensor(0x59e5,0x38);
	write_cmos_sensor(0x59e6,0x30);
	write_cmos_sensor(0x59e7,0x20);
	write_cmos_sensor(0x59e8,0x3f);
	write_cmos_sensor(0x59e9,0x38);
	write_cmos_sensor(0x59ea,0x30);
	write_cmos_sensor(0x59eb,0x3f);
	write_cmos_sensor(0x59ec,0x38);
	write_cmos_sensor(0x59ed,0x30);
	write_cmos_sensor(0x59ee,0xfe);
	write_cmos_sensor(0x59ef,0x40);
	write_cmos_sensor(0x59f4,0x38);
	write_cmos_sensor(0x59f5,0x30);
	write_cmos_sensor(0x59f6,0x20);
	write_cmos_sensor(0x59f7,0x38);
	write_cmos_sensor(0x59f8,0x30);
	write_cmos_sensor(0x59f9,0x20);
	write_cmos_sensor(0x59fa,0x3f);
	write_cmos_sensor(0x59fb,0x38);
	write_cmos_sensor(0x59fc,0x30);
	write_cmos_sensor(0x59fd,0x3f);
	write_cmos_sensor(0x59fe,0x38);
	write_cmos_sensor(0x59ff,0x30);
	write_cmos_sensor(0x3650,0x61);
	write_cmos_sensor(0x3651,0x9f);
}

static void capture_setting(void)
{
	preview_setting();
}

static void normal_video_setting(void)
{
   //printk("E! currefps:%d\n",currefps);
	preview_setting();
}

static void hs_video_setting(void)
{
    //printk("E! currefps:%d\n",currefps);
	preview_setting();
}

static void slim_video_setting(void)
{
    //printk("E! currefps:%d\n",currefps);
	preview_setting();
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	printk("enable: %d\n", enable);

	if (enable)
		write_cmos_sensor(0x4501, 0xbc);
	else
		write_cmos_sensor(0x4501, 0xb4);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 module_id = 0;
	printk("[get_imgsensor_id] ");
	imgsensor.i2c_write_id = SC800CS_LY_EEPROM_IC_ADDR;
	module_id = read_cmos_sensor(0x0002);
	if(module_id == SC800CS_LY_MODULE_ID || module_id == 0xff)
	{
		printk("SC800CS_LY read module id success! Current module id is 0x%x", module_id);
		while (imgsensor_info.i2c_addr_table[i] != 0xff)
		{
			spin_lock(&imgsensor_drv_lock);
			imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
			spin_unlock(&imgsensor_drv_lock);
			do {
				*sensor_id =return_sensor_id();
				if (*sensor_id == imgsensor_info.sensor_id) {
					printk("i2c write id  : 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
					return ERROR_NONE;
				}
				printk("get_imgsensor_id Read sensor id fail, i2c write id: 0x%x,sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
				retry--;
			} while(retry > 0);
			i++;
			retry = 2;
		}
		printk("SC800CS_LY i2c write id  : 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
		if (*sensor_id != imgsensor_info.sensor_id) {
			*sensor_id = 0xFFFFFFFF;
			return ERROR_SENSOR_CONNECT_FAIL;
		}
	}
	else
	{
		printk("SC800CS_LY read module id fail! Current module id is 0x%x", module_id);
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*	open
*
* DESCRIPTION
*	This function initialize the registers of CMOS sensor
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	 kal_uint8 retry = 2;
	 kal_uint32 sensor_id = 0;
	printk("[open]: PLATFORM:MT6789,MIPI 24LANE\n");
	printk("preview 1632*1224@30fps,360Mbps/lane; capture 3264*2448@30fps,880Mbps/lane\n");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				printk("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
				break;
			}
			printk("open:Read sensor id fail open i2c write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
			retry--;
		} while(retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;
	/* initail sequence write in  */
    sensor_init();

    spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en= KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = KAL_FALSE;
    imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}	/*	open  */
static kal_uint32 close(void)
{
	printk("close E");
	/*No Need to implement this function*/
	return ERROR_NONE;
}	/*	close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*	This function start the sensor preview.
*
* PARAMETERS
*	*image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	printk("E");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}	/*	preview   */

/*************************************************************************
* FUNCTION
*	capture
*
* DESCRIPTION
*	This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	printk("E");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
    if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {//PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
        imgsensor.pclk = imgsensor_info.cap1.pclk;
        imgsensor.line_length = imgsensor_info.cap1.linelength;
        imgsensor.frame_length = imgsensor_info.cap1.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    } else {
        if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
            printk("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",imgsensor.current_fps,imgsensor_info.cap.max_framerate/10);
        imgsensor.pclk = imgsensor_info.cap.pclk;
        imgsensor.line_length = imgsensor_info.cap.linelength;
        imgsensor.frame_length = imgsensor_info.cap.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    spin_unlock(&imgsensor_drv_lock);
    capture_setting();
	//set_mirror_flip(sensor_config_data->SensorImageMirror);
    return ERROR_NONE;

}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	printk("E");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    normal_video_setting();
	//set_mirror_flip(sensor_config_data->SensorImageMirror);
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    printk("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.pclk = imgsensor_info.hs_video.pclk;
    //imgsensor.video_mode = KAL_TRUE;
    imgsensor.line_length = imgsensor_info.hs_video.linelength;
    imgsensor.frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    hs_video_setting();
//	slim_video_setting();
	//set_mirror_flip(sensor_config_data->SensorImageMirror);
    return ERROR_NONE;
}    /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    printk("E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.pclk = imgsensor_info.slim_video.pclk;
    imgsensor.line_length = imgsensor_info.slim_video.linelength;
    imgsensor.frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    slim_video_setting();
//	hs_video_setting();
	//set_mirror_flip(sensor_config_data->SensorImageMirror);

    return ERROR_NONE;
}    /*    slim_video     */






static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    printk("E sc800cs wuwenjie get_resolution\n");
    sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
    sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

    sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
    sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

    sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
    sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


    sensor_resolution->SensorHighSpeedVideoWidth     = imgsensor_info.hs_video.grabwindow_width;
    sensor_resolution->SensorHighSpeedVideoHeight     = imgsensor_info.hs_video.grabwindow_height;

    sensor_resolution->SensorSlimVideoWidth     = imgsensor_info.slim_video.grabwindow_width;
    sensor_resolution->SensorSlimVideoHeight     = imgsensor_info.slim_video.grabwindow_height;
    return ERROR_NONE;
}    /*    get_resolution    */


static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                      MSDK_SENSOR_INFO_STRUCT *sensor_info,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    printk("scenario_id = %d\n", scenario_id);


    sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
    sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
    sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 4; /* not use */
    sensor_info->SensorResetActiveHigh = FALSE; /* not use */
    sensor_info->SensorResetDelayCount = 5; /* not use */

    sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
    sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
    sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
    sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

    sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
    sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
    sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
    sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
    sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

    sensor_info->SensorMasterClockSwitch = 0; /* not use */
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;          /* The frame of setting shutter default 0 for TG int */
    sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;    /* The frame of setting sensor gain */
    sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
    sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
    sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
    sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

    sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
    sensor_info->SensorClockFreq = imgsensor_info.mclk;
    sensor_info->SensorClockDividCount = 3; /* not use */
    sensor_info->SensorClockRisingCount = 0;
    sensor_info->SensorClockFallingCount = 2; /* not use */
    sensor_info->SensorPixelClockCount = 3; /* not use */
    sensor_info->SensorDataLatchCount = 2; /* not use */

    sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
    sensor_info->SensorHightSampling = 0;    // 0 is default 1x
    sensor_info->SensorPacketECCOrder = 1;

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

            sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

            break;
        default:
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
            break;
    }

    return ERROR_NONE;
}    /*    get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    printk("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:

			printk("preview\n");
			preview(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			//case MSDK_SCENARIO_ID_CAMERA_ZSD:
			capture(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			normal_video(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            hs_video(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            slim_video(image_window, sensor_config_data);
            break;
		default:
			printk("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	printk("framerate = %d ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		// Dynamic frame rate
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);

	if ((framerate == 30) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 15) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = 10 * framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps,1);
	set_dummy();
	return ERROR_NONE;
}


static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	printk("enable = %d, framerate = %d ", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
    kal_uint32 frame_length;

    printk("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            if(framerate == 0)
                return ERROR_NONE;
            frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        	  if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
                frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
                spin_lock(&imgsensor_drv_lock);
		            imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ? (frame_length - imgsensor_info.cap1.framelength) : 0;
		            imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
		            imgsensor.min_frame_length = imgsensor.frame_length;
		            spin_unlock(&imgsensor_drv_lock);
            } else {
        		    if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
                    printk("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",framerate,imgsensor_info.cap.max_framerate/10);
                frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
                spin_lock(&imgsensor_drv_lock);
		            imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
		            imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		            imgsensor.min_frame_length = imgsensor.frame_length;
		            spin_unlock(&imgsensor_drv_lock);
            }
			if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
            imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
            break;
        default:  //coding with  preview scenario by default
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
			if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
            printk("error scenario_id = %d, we use preview scenario \n", scenario_id);
            break;
    }
    return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
    printk("scenario_id = %d\n", scenario_id);

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            *framerate = imgsensor_info.pre.max_framerate;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *framerate = imgsensor_info.normal_video.max_framerate;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *framerate = imgsensor_info.cap.max_framerate;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *framerate = imgsensor_info.hs_video.max_framerate;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *framerate = imgsensor_info.slim_video.max_framerate;
            break;
        default:
            break;
    }

    return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	printk("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
	{
		write_cmos_sensor(0x0100,0x01);
		mdelay(10);//delay  10ms
		write_cmos_sensor(0x302d,0x00);
	}
	else
	{
		write_cmos_sensor(0x0100, 0x00);
	}
	mdelay(10);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
                             UINT8 *feature_para,UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16=(UINT16 *) feature_para;
    UINT16 *feature_data_16=(UINT16 *) feature_para;
    UINT32 *feature_return_para_32=(UINT32 *) feature_para;
    UINT32 *feature_data_32=(UINT32 *) feature_para;
    unsigned long long *feature_data=(unsigned long long *) feature_para;
//    unsigned long long *feature_return_para=(unsigned long long *) feature_para;

    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

    printk("feature_id = %d\n", feature_id);
    switch (feature_id) {
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
    *(feature_data + 1) = imgsensor_info.min_shutter;
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(feature_data + 2) = 2;
            break;
        default:
            *(feature_data + 2) = 1;
            break;
        }
        break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.pclk;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.pclk;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.pclk;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.pclk;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
	    *feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
			kal_uint32 rate;

			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				rate = imgsensor_info.cap.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				rate =
				    imgsensor_info.normal_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				rate = imgsensor_info.hs_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				rate =
				    imgsensor_info.slim_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				rate = imgsensor_info.pre.mipi_pixel_rate;
				break;
			}
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = rate;
	}
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
		printk("adb_i2c_read 0x%x = 0x%x\n", sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16 + 1));
            break;
        case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
            set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
            break;
        case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
            break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		printk("current fps: %d\n", (UINT32)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		printk("ihdr enable: %d\n", (BOOL)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL)*feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		printk("SENSOR_FEATURE_GET_CROP_INFO scenarioId: %d\n", (UINT32)*feature_data);
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		printk("SENSOR_SET_SENSOR_IHDR LE = %d, SE = %d, Gain = %d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data + 1),
			(UINT16)*(feature_data + 2));
		ihdr_write_shutter_gain((UINT16)*feature_data,
			(UINT16)*(feature_data + 1), (UINT16)*(feature_data + 2));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		printk("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		printk("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n", *feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	default:
		break;
	}
    return ERROR_NONE;
}   /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 A0901BACKSC800CSLY_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;
	return ERROR_NONE;
}	/*	SC800CS_MIPI_RAW_SensorInit	*/
/*Tab A9 code for SR-AX6739A-01-224 by hudongdong at 20230519 end */