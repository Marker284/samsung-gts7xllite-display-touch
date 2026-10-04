// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <drm/display/drm_dsc.h>
#include <drm/display/drm_dsc_helper.h>

/*
 * DSC in DSI video mode is the least exercised path in msm; allow turning it off
 * from the kernel command line so the uncompressed link (1.67 Gbps per lane,
 * within the 10nm PHY's limits) can be tried without a rebuild.
 */
static bool no_dsc;
module_param(no_dsc, bool, 0644);
MODULE_PARM_DESC(no_dsc, "disable DSC and drive the panel uncompressed");

/*
 * Traffic mode. Burst only makes sense when the link runs faster than the
 * pixel stream needs: with DSC it does (676 vs ~557 Mbps per lane), but
 * uncompressed the link rate equals the pixel rate exactly, so there is no
 * headroom to burst into and the picture breaks up.
 */
static int traffic = 2;		/* 0 = sync event, 1 = sync pulse, 2 = burst */
module_param(traffic, int, 0644);
MODULE_PARM_DESC(traffic, "DSI traffic mode: 0=sync event, 1=sync pulse, 2=burst");

/*
 * The stock device tree sets qcom,mdss-dsc-slice-per-pkt = <2>: both slices of
 * a line travel in one DSI packet. msm only grew support for that recently, so
 * keep it switchable while we confirm which the DDIC accepts.
 */
static int slice_per_pkt = 2;
module_param(slice_per_pkt, int, 0644);
MODULE_PARM_DESC(slice_per_pkt, "DSC slices per DSI packet (stock uses 2)");

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct ft8203_ts124qdm_wqxga {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator_bulk_data supplies[3];
	struct drm_dsc_config dsc;
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline
struct ft8203_ts124qdm_wqxga *to_ft8203_ts124qdm_wqxga(struct drm_panel *panel)
{
	return container_of(panel, struct ft8203_ts124qdm_wqxga, panel);
}

#define dsi_dcs_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_dcs_write_buffer(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

static void ft8203_ts124qdm_wqxga_reset(struct ft8203_ts124qdm_wqxga *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(4000, 5000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(12000, 13000);
}

static int ft8203_ts124qdm_wqxga_on(struct ft8203_ts124qdm_wqxga *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	struct drm_dsc_picture_parameter_set pps;
	int ret;

	dsi_dcs_write_seq(dsi, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0xff, 0x82, 0x01, 0x01);
	dsi_dcs_write_seq(dsi, 0x00, 0x80);
	dsi_dcs_write_seq(dsi, 0xff, 0x82, 0x01);
	dsi_dcs_write_seq(dsi, 0x00, 0x93);
	dsi_dcs_write_seq(dsi, 0xc5, 0x16);
	dsi_dcs_write_seq(dsi, 0x00, 0x97);
	dsi_dcs_write_seq(dsi, 0xc5, 0x16);
	dsi_dcs_write_seq(dsi, 0x00, 0x9e);
	dsi_dcs_write_seq(dsi, 0xc5, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0x9a);
	dsi_dcs_write_seq(dsi, 0xc5, 0x25);
	dsi_dcs_write_seq(dsi, 0x00, 0x9c);
	dsi_dcs_write_seq(dsi, 0xc5, 0x25);
	dsi_dcs_write_seq(dsi, 0x00, 0xb6);
	dsi_dcs_write_seq(dsi, 0xc5, 0x07, 0x07);
	dsi_dcs_write_seq(dsi, 0x00, 0xb8);
	dsi_dcs_write_seq(dsi, 0xc5, 0x1b, 0x1b);
	dsi_dcs_write_seq(dsi, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0xd8, 0xaa, 0xaa);
	dsi_dcs_write_seq(dsi, 0x00, 0x82);
	dsi_dcs_write_seq(dsi, 0xc5, 0x95);
	dsi_dcs_write_seq(dsi, 0x00, 0x83);
	dsi_dcs_write_seq(dsi, 0xc5, 0x07);
	dsi_dcs_write_seq(dsi, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0xe1,
			  0x05, 0x0e, 0x23, 0x37, 0x42, 0x4f, 0x62, 0x70, 0x73,
			  0x81, 0x83, 0x98, 0x6c, 0x59, 0x5a, 0x50);
	dsi_dcs_write_seq(dsi, 0x00, 0x10);
	dsi_dcs_write_seq(dsi, 0xe1,
			  0x49, 0x3f, 0x32, 0x29, 0x22, 0x14, 0x06, 0x02);
	dsi_dcs_write_seq(dsi, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0xe2,
			  0x05, 0x0e, 0x23, 0x37, 0x42, 0x4f, 0x62, 0x70, 0x73,
			  0x81, 0x83, 0x98, 0x6c, 0x59, 0x5a, 0x50);
	dsi_dcs_write_seq(dsi, 0x00, 0x10);
	dsi_dcs_write_seq(dsi, 0xe2,
			  0x49, 0x3f, 0x32, 0x29, 0x22, 0x14, 0x06, 0x02);
	dsi_dcs_write_seq(dsi, 0x00, 0x80);
	dsi_dcs_write_seq(dsi, 0xa4, 0x8c);
	dsi_dcs_write_seq(dsi, 0x00, 0xa0);
	dsi_dcs_write_seq(dsi, 0xf3, 0x10);
	dsi_dcs_write_seq(dsi, 0x00, 0xa1);
	dsi_dcs_write_seq(dsi, 0xb3, 0x06, 0x40);
	dsi_dcs_write_seq(dsi, 0x00, 0xa3);
	dsi_dcs_write_seq(dsi, 0xb3, 0x0a, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0xa5);
	dsi_dcs_write_seq(dsi, 0xb3, 0x00, 0x13);
	dsi_dcs_write_seq(dsi, 0x00, 0xd0);
	dsi_dcs_write_seq(dsi, 0xc1, 0xb0);
	dsi_dcs_write_seq(dsi, 0x00, 0x80);
	dsi_dcs_write_seq(dsi, 0xcb, 0x3f, 0x33, 0x30, 0x3f, 0x30, 0x33, 0x30);
	dsi_dcs_write_seq(dsi, 0x00, 0x87);
	dsi_dcs_write_seq(dsi, 0xcb, 0x3f);
	dsi_dcs_write_seq(dsi, 0x00, 0x88);
	dsi_dcs_write_seq(dsi, 0xcb,
			  0x00, 0x3f, 0x33, 0x33, 0x33, 0x30, 0x3f, 0x3f);
	dsi_dcs_write_seq(dsi, 0x00, 0x90);
	dsi_dcs_write_seq(dsi, 0xcb, 0x00, 0x33, 0x33, 0x33, 0x30, 0x30, 0x3f);
	dsi_dcs_write_seq(dsi, 0x00, 0x97);
	dsi_dcs_write_seq(dsi, 0xcb, 0x33);
	dsi_dcs_write_seq(dsi, 0x00, 0x98);
	dsi_dcs_write_seq(dsi, 0xcb,
			  0xd7, 0x14, 0x14, 0xd4, 0x14, 0x14, 0x14, 0xd7);
	dsi_dcs_write_seq(dsi, 0x00, 0xa0);
	dsi_dcs_write_seq(dsi, 0xcb,
			  0x00, 0xfc, 0x14, 0x14, 0x14, 0x14, 0xeb, 0xd4);
	dsi_dcs_write_seq(dsi, 0x00, 0xa8);
	dsi_dcs_write_seq(dsi, 0xcb,
			  0x28, 0x14, 0x14, 0x14, 0x14, 0x14, 0xc0, 0x14);
	dsi_dcs_write_seq(dsi, 0x00, 0xb0);
	dsi_dcs_write_seq(dsi, 0xcb, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0xb7);
	dsi_dcs_write_seq(dsi, 0xcb, 0xff);
	dsi_dcs_write_seq(dsi, 0x00, 0xb8);
	dsi_dcs_write_seq(dsi, 0xcb,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0xc0);
	dsi_dcs_write_seq(dsi, 0xcb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0xc7);
	dsi_dcs_write_seq(dsi, 0xcb, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0xd0);
	dsi_dcs_write_seq(dsi, 0xcb,
			  0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
			  0x03);
	dsi_dcs_write_seq(dsi, 0x00, 0x80);
	dsi_dcs_write_seq(dsi, 0xcc,
			  0x38, 0x2d, 0x2d, 0x2d, 0x13, 0x13, 0x13, 0x07);
	dsi_dcs_write_seq(dsi, 0x00, 0x88);
	dsi_dcs_write_seq(dsi, 0xcc,
			  0x08, 0x2c, 0x17, 0x2b, 0x2b, 0x01, 0x23, 0x23);
	dsi_dcs_write_seq(dsi, 0x00, 0x90);
	dsi_dcs_write_seq(dsi, 0xcc, 0x11, 0x12, 0x0f, 0x10, 0x2c, 0x2c);
	dsi_dcs_write_seq(dsi, 0x00, 0x80);
	dsi_dcs_write_seq(dsi, 0xcd,
			  0x38, 0x2d, 0x2d, 0x2d, 0x13, 0x13, 0x13, 0x07);
	dsi_dcs_write_seq(dsi, 0x00, 0x88);
	dsi_dcs_write_seq(dsi, 0xcd,
			  0x08, 0x2c, 0x17, 0x2b, 0x2b, 0x01, 0x23, 0x23);
	dsi_dcs_write_seq(dsi, 0x00, 0x90);
	dsi_dcs_write_seq(dsi, 0xcd, 0x11, 0x12, 0x0f, 0x10, 0x2c, 0x2c);
	dsi_dcs_write_seq(dsi, 0x00, 0xa0);
	dsi_dcs_write_seq(dsi, 0xcc,
			  0x38, 0x2d, 0x2d, 0x2d, 0x13, 0x13, 0x13, 0x08);
	dsi_dcs_write_seq(dsi, 0x00, 0xa8);
	dsi_dcs_write_seq(dsi, 0xcc,
			  0x07, 0x17, 0x2c, 0x2b, 0x2b, 0x01, 0x23, 0x23);
	dsi_dcs_write_seq(dsi, 0x00, 0xb0);
	dsi_dcs_write_seq(dsi, 0xcc, 0x10, 0x0f, 0x12, 0x11, 0x2c, 0x2c);
	dsi_dcs_write_seq(dsi, 0x00, 0xa0);
	dsi_dcs_write_seq(dsi, 0xcd,
			  0x38, 0x2d, 0x2d, 0x2d, 0x13, 0x13, 0x13, 0x08);
	dsi_dcs_write_seq(dsi, 0x00, 0xa8);
	dsi_dcs_write_seq(dsi, 0xcd,
			  0x07, 0x17, 0x2c, 0x2b, 0x2b, 0x01, 0x23, 0x23);
	dsi_dcs_write_seq(dsi, 0x00, 0xb0);
	dsi_dcs_write_seq(dsi, 0xcd, 0x10, 0x0f, 0x12, 0x11, 0x2c, 0x2c);
	dsi_dcs_write_seq(dsi, 0x00, 0x81);
	dsi_dcs_write_seq(dsi, 0xc2, 0x40);
	dsi_dcs_write_seq(dsi, 0x00, 0x90);
	dsi_dcs_write_seq(dsi, 0xc2, 0x84, 0x02, 0x58, 0x93);
	dsi_dcs_write_seq(dsi, 0x00, 0x94);
	dsi_dcs_write_seq(dsi, 0xc2, 0x83, 0x02, 0x58, 0x93);
	dsi_dcs_write_seq(dsi, 0x00, 0xe0);
	dsi_dcs_write_seq(dsi, 0xc2,
			  0x8b, 0x09, 0x01, 0x61, 0x93, 0x00, 0x00, 0x03);
	dsi_dcs_write_seq(dsi, 0x00, 0xe8);
	dsi_dcs_write_seq(dsi, 0xc2,
			  0x8a, 0x0a, 0x01, 0x61, 0x93, 0x00, 0x00, 0x03);
	dsi_dcs_write_seq(dsi, 0x00, 0xf0);
	dsi_dcs_write_seq(dsi, 0xc2,
			  0x89, 0x07, 0x01, 0x61, 0x93, 0x00, 0x00, 0x03);
	dsi_dcs_write_seq(dsi, 0x00, 0xf8);
	dsi_dcs_write_seq(dsi, 0xc2,
			  0x88, 0x08, 0x01, 0x61, 0x93, 0x00, 0x00, 0x03);
	dsi_dcs_write_seq(dsi, 0x00, 0xe0);
	dsi_dcs_write_seq(dsi, 0xc3, 0x36, 0x24, 0x00, 0xc2);
	dsi_dcs_write_seq(dsi, 0x00, 0xe4);
	dsi_dcs_write_seq(dsi, 0xc3, 0x35, 0x24, 0x00, 0x76);
	dsi_dcs_write_seq(dsi, 0x00, 0xe8);
	dsi_dcs_write_seq(dsi, 0xc3, 0x35, 0x24, 0x00, 0x76);
	dsi_dcs_write_seq(dsi, 0x00, 0xa0);
	dsi_dcs_write_seq(dsi, 0xc3, 0x01, 0xaa, 0x0a, 0x0f);
	dsi_dcs_write_seq(dsi, 0x00, 0xfd);
	dsi_dcs_write_seq(dsi, 0xcb, 0x82);
	dsi_dcs_write_seq(dsi, 0x00, 0x80);
	dsi_dcs_write_seq(dsi, 0xc0, 0x00, 0x79, 0x00, 0x10, 0x00, 0x10);
	dsi_dcs_write_seq(dsi, 0x00, 0x90);
	dsi_dcs_write_seq(dsi, 0xc0, 0x00, 0x79, 0x00, 0x10, 0x00, 0x10);
	dsi_dcs_write_seq(dsi, 0x00, 0xa0);
	dsi_dcs_write_seq(dsi, 0xc0, 0x00, 0xf0, 0x00, 0x10, 0x00, 0x10);
	dsi_dcs_write_seq(dsi, 0x00, 0xb0);
	dsi_dcs_write_seq(dsi, 0xc0, 0x00, 0x79, 0x00, 0x10, 0x10);
	dsi_dcs_write_seq(dsi, 0x00, 0xa3);
	dsi_dcs_write_seq(dsi, 0xc1, 0x2d, 0x21, 0x04);
	dsi_dcs_write_seq(dsi, 0x00, 0x80);
	dsi_dcs_write_seq(dsi, 0xce,
			  0x01, 0x81, 0xff, 0xff, 0x00, 0x88, 0x00, 0xd0, 0x00,
			  0x54, 0x00, 0x68);
	dsi_dcs_write_seq(dsi, 0x00, 0x90);
	dsi_dcs_write_seq(dsi, 0xce,
			  0x00, 0x87, 0x0e, 0x00, 0x00, 0x87, 0x80, 0xff, 0xff,
			  0x00, 0x06, 0x00, 0x17, 0x0f);
	dsi_dcs_write_seq(dsi, 0x00, 0xa0);
	dsi_dcs_write_seq(dsi, 0xce, 0x00, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0xb0);
	dsi_dcs_write_seq(dsi, 0xce, 0x20, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0xd1);
	dsi_dcs_write_seq(dsi, 0xce, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0xe1);
	dsi_dcs_write_seq(dsi, 0xce, 0x09, 0x02, 0x30, 0x02, 0x30, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0xf0);
	dsi_dcs_write_seq(dsi, 0xce,
			  0x80, 0x17, 0x0b, 0x01, 0x10, 0x01, 0xa0, 0x00, 0x20,
			  0x25);
	dsi_dcs_write_seq(dsi, 0x00, 0xb0);
	dsi_dcs_write_seq(dsi, 0xcf, 0x00, 0x00, 0x46, 0x4a);
	dsi_dcs_write_seq(dsi, 0x00, 0xb5);
	dsi_dcs_write_seq(dsi, 0xcf, 0x05, 0x05, 0x00, 0x04);
	dsi_dcs_write_seq(dsi, 0x00, 0xc0);
	dsi_dcs_write_seq(dsi, 0xcf, 0x09, 0x09, 0xec, 0xf0);
	dsi_dcs_write_seq(dsi, 0x00, 0xc5);
	dsi_dcs_write_seq(dsi, 0xcf, 0x00, 0x0a, 0x08, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0x90);
	dsi_dcs_write_seq(dsi, 0xc4, 0x88);
	dsi_dcs_write_seq(dsi, 0x00, 0x92);
	dsi_dcs_write_seq(dsi, 0xc4, 0xc0);
	dsi_dcs_write_seq(dsi, 0x00, 0xc5);
	dsi_dcs_write_seq(dsi, 0xc3, 0x00, 0x00, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0xd6);
	dsi_dcs_write_seq(dsi, 0xc1, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0xd0);
	dsi_dcs_write_seq(dsi, 0xc1, 0x90);
	dsi_dcs_write_seq(dsi, 0x00, 0xbf);
	dsi_dcs_write_seq(dsi, 0xc0, 0x04);
	dsi_dcs_write_seq(dsi, 0x00, 0xd5);
	dsi_dcs_write_seq(dsi, 0xc0, 0xf1);
	dsi_dcs_write_seq(dsi, 0x00, 0x9f);
	dsi_dcs_write_seq(dsi, 0xc5, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0x91);
	dsi_dcs_write_seq(dsi, 0xc5, 0x4c);
	dsi_dcs_write_seq(dsi, 0x00, 0xd7);
	dsi_dcs_write_seq(dsi, 0xce, 0x01);
	dsi_dcs_write_seq(dsi, 0x00, 0x94);
	dsi_dcs_write_seq(dsi, 0xc5, 0x46);
	dsi_dcs_write_seq(dsi, 0x00, 0x98);
	dsi_dcs_write_seq(dsi, 0xc5, 0x64);
	dsi_dcs_write_seq(dsi, 0x00, 0x9b);
	dsi_dcs_write_seq(dsi, 0xc5, 0x65);
	dsi_dcs_write_seq(dsi, 0x00, 0x9d);
	dsi_dcs_write_seq(dsi, 0xc5, 0x65);
	dsi_dcs_write_seq(dsi, 0x00, 0x9a);
	dsi_dcs_write_seq(dsi, 0xcf, 0xff);
	dsi_dcs_write_seq(dsi, 0x00, 0x82);
	dsi_dcs_write_seq(dsi, 0xa5, 0x01);
	dsi_dcs_write_seq(dsi, 0x00, 0x8c);
	dsi_dcs_write_seq(dsi, 0xcf, 0x40, 0x40);
	dsi_dcs_write_seq(dsi, 0x00, 0xa2);
	dsi_dcs_write_seq(dsi, 0xf5, 0x1f);
	dsi_dcs_write_seq(dsi, 0x00, 0xc1);
	dsi_dcs_write_seq(dsi, 0xc0, 0x11);
	dsi_dcs_write_seq(dsi, 0x00, 0x9a);
	dsi_dcs_write_seq(dsi, 0xf5, 0x3f);
	dsi_dcs_write_seq(dsi, 0x00, 0x9c);
	dsi_dcs_write_seq(dsi, 0xf5, 0x1e);
	dsi_dcs_write_seq(dsi, 0x00, 0xb6);
	dsi_dcs_write_seq(dsi, 0xc0, 0x02);
	dsi_dcs_write_seq(dsi, 0x00, 0xb0);
	dsi_dcs_write_seq(dsi, 0xca, 0x09, 0x09, 0x04);
	dsi_dcs_write_seq(dsi, 0x00, 0xb4);
	dsi_dcs_write_seq(dsi, 0xca, 0x03);
	dsi_dcs_write_seq(dsi, 0x00, 0x80);
	dsi_dcs_write_seq(dsi, 0xca,
			  0xf0, 0xd9, 0xc8, 0xba, 0xaf, 0xa6, 0x9e, 0x98, 0x92,
			  0x8d, 0x88, 0x84);
	dsi_dcs_write_seq(dsi, 0x00, 0x90);
	dsi_dcs_write_seq(dsi, 0xca, 0xfb, 0xff, 0x33);
	dsi_dcs_write_seq(dsi, 0x00, 0xa0);
	dsi_dcs_write_seq(dsi, 0xca, 0x06);
	dsi_dcs_write_seq(dsi, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0xec,
			  0x00, 0x04, 0x08, 0x0c, 0x00, 0x10, 0x14, 0x18, 0x1c,
			  0x40, 0x20, 0x28, 0x30, 0x38, 0x01);
	dsi_dcs_write_seq(dsi, 0x00, 0x10);
	dsi_dcs_write_seq(dsi, 0xec,
			  0x40, 0x48, 0x4f, 0x57, 0xf0, 0x5f, 0x67, 0x6f, 0x77,
			  0xff, 0x7f, 0x87, 0x8f, 0x97, 0xff);
	dsi_dcs_write_seq(dsi, 0x00, 0x20);
	dsi_dcs_write_seq(dsi, 0xec,
			  0x9f, 0xa7, 0xaf, 0xb7, 0xff, 0xbf, 0xc7, 0xcf, 0xd7,
			  0xaf, 0xdf, 0xe7, 0xee, 0xf6, 0xfa);
	dsi_dcs_write_seq(dsi, 0x00, 0x30);
	dsi_dcs_write_seq(dsi, 0xec, 0xfa, 0xfc, 0xfd, 0x3f);
	dsi_dcs_write_seq(dsi, 0x00, 0x40);
	dsi_dcs_write_seq(dsi, 0xec,
			  0x00, 0x04, 0x08, 0x0c, 0x00, 0x10, 0x14, 0x18, 0x1c,
			  0x00, 0x20, 0x28, 0x30, 0x38, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0x50);
	dsi_dcs_write_seq(dsi, 0xec,
			  0x40, 0x48, 0x50, 0x58, 0x00, 0x60, 0x68, 0x70, 0x78,
			  0x00, 0x80, 0x88, 0x90, 0x98, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0x60);
	dsi_dcs_write_seq(dsi, 0xec,
			  0xa0, 0xa8, 0xb0, 0xb8, 0x00, 0xc0, 0xc8, 0xd0, 0xd8,
			  0x00, 0xe0, 0xe8, 0xf0, 0xf8, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0x70);
	dsi_dcs_write_seq(dsi, 0xec, 0xfc, 0xfe, 0xff, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0x80);
	dsi_dcs_write_seq(dsi, 0xec,
			  0x00, 0x03, 0x07, 0x0b, 0x6c, 0x0f, 0x13, 0x16, 0x1a,
			  0xb1, 0x1e, 0x26, 0x2e, 0x35, 0xc5);
	dsi_dcs_write_seq(dsi, 0x00, 0x90);
	dsi_dcs_write_seq(dsi, 0xec,
			  0x3d, 0x45, 0x4d, 0x54, 0x86, 0x5c, 0x64, 0x6b, 0x73,
			  0x61, 0x7b, 0x82, 0x8a, 0x91, 0xd8);
	dsi_dcs_write_seq(dsi, 0x00, 0xa0);
	dsi_dcs_write_seq(dsi, 0xec,
			  0x99, 0xa1, 0xa8, 0xb0, 0x72, 0xb8, 0xbf, 0xc7, 0xcf,
			  0x6c, 0xd7, 0xde, 0xe7, 0xee, 0xcc);
	dsi_dcs_write_seq(dsi, 0x00, 0xb0);
	dsi_dcs_write_seq(dsi, 0xec, 0xf2, 0xf4, 0xf5, 0x2b);
	dsi_dcs_write_seq(dsi, 0x00, 0xb0);
	dsi_dcs_write_seq(dsi, 0xb4,
			  0x00, 0x28, 0x02, 0x00, 0x04, 0x9f, 0x00, 0x0b, 0x02,
			  0x77, 0x01, 0xb1, 0x10, 0xd0);
	dsi_dcs_write_seq(dsi, 0x00, 0xbe);
	dsi_dcs_write_seq(dsi, 0xb4, 0x00, 0x80);
	dsi_dcs_write_seq(dsi, 0x00, 0xd5);
	dsi_dcs_write_seq(dsi, 0xc1, 0x80);
	dsi_dcs_write_seq(dsi, 0x00, 0x80);
	dsi_dcs_write_seq(dsi, 0xb0, 0x92);
	dsi_dcs_write_seq(dsi, 0x1c, 0x01);
	dsi_dcs_write_seq(dsi, 0x00, 0xa4);
	dsi_dcs_write_seq(dsi, 0xf3, 0x0b);
	dsi_dcs_write_seq(dsi, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0xfa, 0x02);
	dsi_dcs_write_seq(dsi, 0x00, 0xa8);
	dsi_dcs_write_seq(dsi, 0xc5, 0x09);
	dsi_dcs_write_seq(dsi, 0x00, 0xcb);
	dsi_dcs_write_seq(dsi, 0xc5, 0x01);
	dsi_dcs_write_seq(dsi, 0x00, 0xb6);
	dsi_dcs_write_seq(dsi, 0xc5, 0x07, 0x07);
	dsi_dcs_write_seq(dsi, 0x00, 0xb8);
	dsi_dcs_write_seq(dsi, 0xc5, 0x1b, 0x1b);
	dsi_dcs_write_seq(dsi, 0x00, 0x91);
	dsi_dcs_write_seq(dsi, 0xa5, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0xb2);
	dsi_dcs_write_seq(dsi, 0xce, 0x79);
	dsi_dcs_write_seq(dsi, 0x00, 0xa4);
	dsi_dcs_write_seq(dsi, 0xf3, 0x0b);
	dsi_dcs_write_seq(dsi, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0xfa, 0x5a);
	dsi_dcs_write_seq(dsi, 0x00, 0xa4);
	dsi_dcs_write_seq(dsi, 0xf3, 0x0b);
	dsi_dcs_write_seq(dsi, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0xfa, 0x01);
	dsi_dcs_write_seq(dsi, 0x00, 0xa8);
	dsi_dcs_write_seq(dsi, 0xc5, 0x09);
	dsi_dcs_write_seq(dsi, 0x00, 0xcb);
	dsi_dcs_write_seq(dsi, 0xc5, 0x09);
	dsi_dcs_write_seq(dsi, 0x00, 0xb6);
	dsi_dcs_write_seq(dsi, 0xc5, 0x05, 0x05);
	dsi_dcs_write_seq(dsi, 0x00, 0xb8);
	dsi_dcs_write_seq(dsi, 0xc5, 0x19, 0x19);
	dsi_dcs_write_seq(dsi, 0x00, 0x91);
	dsi_dcs_write_seq(dsi, 0xa5, 0x40);
	dsi_dcs_write_seq(dsi, 0x00, 0xb2);
	dsi_dcs_write_seq(dsi, 0xce, 0x7a);
	dsi_dcs_write_seq(dsi, 0x00, 0xa4);
	dsi_dcs_write_seq(dsi, 0xf3, 0x0b);
	dsi_dcs_write_seq(dsi, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0xfa, 0x5a);
	dsi_dcs_write_seq(dsi, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0xff, 0x00, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0x00, 0x80);
	dsi_dcs_write_seq(dsi, 0xff, 0x00, 0x00);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(128);

	/*
	 * The DDIC needs the DSC picture parameter set from us: the stock device
	 * tree sets samsung,no_qcom_pps, so the controller does not generate it.
	 * msm fills in the derived fields of ctx->dsc while bringing the link
	 * up, so the config is complete by the time we get here.
	 */
	if (no_dsc) {
		/*
		 * The init sequence above comes from the stock device tree,
		 * which runs this panel compressed, so the DDIC is left
		 * expecting a compressed stream. Tell it otherwise, or it
		 * decodes our uncompressed pixels as garbage.
		 */
		ret = mipi_dsi_compression_mode(dsi, false);
		if (ret < 0) {
			dev_err(dev, "Failed to disable compression: %d\n", ret);
			return ret;
		}
		goto skip_dsc;
	}

	drm_dsc_pps_payload_pack(&pps, &ctx->dsc);

	ret = mipi_dsi_picture_parameter_set(dsi, &pps);
	if (ret < 0) {
		dev_err(dev, "Failed to send PPS: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_compression_mode(dsi, true);
	if (ret < 0) {
		dev_err(dev, "Failed to enable compression: %d\n", ret);
		return ret;
	}

skip_dsc:
	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}

	dsi_dcs_write_seq(dsi, 0x35, 0x00);
	msleep(32);

	return 0;
}

static int ft8203_ts124qdm_wqxga_off(struct ft8203_ts124qdm_wqxga *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(32);

	return 0;
}

static int ft8203_ts124qdm_wqxga_prepare(struct drm_panel *panel)
{
	struct ft8203_ts124qdm_wqxga *ctx = to_ft8203_ts124qdm_wqxga(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators: %d\n", ret);
		return ret;
	}

	ft8203_ts124qdm_wqxga_reset(ctx);

	ret = ft8203_ts124qdm_wqxga_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int ft8203_ts124qdm_wqxga_unprepare(struct drm_panel *panel)
{
	struct ft8203_ts124qdm_wqxga *ctx = to_ft8203_ts124qdm_wqxga(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = ft8203_ts124qdm_wqxga_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode ft8203_ts124qdm_wqxga_mode = {
	.clock = (1600 + 90 + 8 + 89) * (2560 + 16 + 8 + 13) * 60 / 1000,
	.hdisplay = 1600,
	.hsync_start = 1600 + 90,
	.hsync_end = 1600 + 90 + 8,
	.htotal = 1600 + 90 + 8 + 89,
	.vdisplay = 2560,
	.vsync_start = 2560 + 16,
	.vsync_end = 2560 + 16 + 8,
	.vtotal = 2560 + 16 + 8 + 13,
	.width_mm = 166,
	.height_mm = 266,
};

static int ft8203_ts124qdm_wqxga_get_modes(struct drm_panel *panel,
					   struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &ft8203_ts124qdm_wqxga_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs ft8203_ts124qdm_wqxga_panel_funcs = {
	.prepare = ft8203_ts124qdm_wqxga_prepare,
	.unprepare = ft8203_ts124qdm_wqxga_unprepare,
	.get_modes = ft8203_ts124qdm_wqxga_get_modes,
};

static int ft8203_ts124qdm_wqxga_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness = backlight_get_brightness(bl);
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness(dsi, brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return 0;
}

// TODO: Check if /sys/class/backlight/.../actual_brightness actually returns
// correct values. If not, remove this function.
static int ft8203_ts124qdm_wqxga_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_get_display_brightness(dsi, &brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return brightness;
}

static const struct backlight_ops ft8203_ts124qdm_wqxga_bl_ops = {
	.update_status = ft8203_ts124qdm_wqxga_bl_update_status,
	.get_brightness = ft8203_ts124qdm_wqxga_bl_get_brightness,
};

static struct backlight_device *
ft8203_ts124qdm_wqxga_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 425,
		.max_brightness = 425,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &ft8203_ts124qdm_wqxga_bl_ops, &props);
}

static int ft8203_ts124qdm_wqxga_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct ft8203_ts124qdm_wqxga *ctx;
	int ret;

	ctx = devm_drm_panel_alloc(dev, struct ft8203_ts124qdm_wqxga, panel,
				   &ft8203_ts124qdm_wqxga_panel_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	ctx->supplies[0].supply = "vddi";
	ctx->supplies[1].supply = "avdd";
	ctx->supplies[2].supply = "avee";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ctx->supplies),
				      ctx->supplies);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to get regulators\n");

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	/*
	 * The init sequence is sent over DSI, so the host has to be powered up
	 * and clocking before the panel is prepared - otherwise every DCS write
	 * fails and prepare() bails out with -EINVAL.
	 */
	ctx->panel.prepare_prev_first = true;

	/*
	 * The stock device tree runs this panel compressed: DSC 1.1 with two
	 * 800x40 slices per line at 8 bits per pixel. The msm DSI host packs
	 * and sends the PPS itself once dsi->dsc is set.
	 */
	ctx->dsc.dsc_version_major = 1;
	ctx->dsc.dsc_version_minor = 1;
	ctx->dsc.slice_height = 40;
	ctx->dsc.slice_width = 800;
	ctx->dsc.slice_count = 2;
	ctx->dsc.bits_per_component = 8;
	ctx->dsc.bits_per_pixel = 8 << 4;
	ctx->dsc.block_pred_enable = true;
	if (!no_dsc) {
		dsi->dsc = &ctx->dsc;
		dsi->dsc_slice_per_pkt = slice_per_pkt;
	}

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	/*
	 * Burst mode: the stock panel node sets bllp-power-mode (and not the
	 * hfp/hbp/hsa ones), and runs the link at 676 MHz per lane while the
	 * compressed stream only needs ~557 Mbps - that headroom is what burst
	 * mode is for. Driving it as a non-burst mode gives DLN0 PHY errors and
	 * a lit but blank panel.
	 */
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS |
			  MIPI_DSI_MODE_LPM;
	if (traffic == 1)
		dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
	else if (traffic == 2)
		dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST;

	ctx->panel.backlight = ft8203_ts124qdm_wqxga_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void ft8203_ts124qdm_wqxga_remove(struct mipi_dsi_device *dsi)
{
	struct ft8203_ts124qdm_wqxga *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id ft8203_ts124qdm_wqxga_of_match[] = {
	/* TODO(verify): panel vendor is unknown, named after the FocalTech
	 * FT8203 driver IC for now. Revisit before upstreaming. */
	{ .compatible = "focaltech,ft8203-ts124qdm" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ft8203_ts124qdm_wqxga_of_match);

static struct mipi_dsi_driver ft8203_ts124qdm_wqxga_driver = {
	.probe = ft8203_ts124qdm_wqxga_probe,
	.remove = ft8203_ts124qdm_wqxga_remove,
	.driver = {
		.name = "panel-ft8203-ts124qdm-wqxga",
		.of_match_table = ft8203_ts124qdm_wqxga_of_match,
	},
};
module_mipi_dsi_driver(ft8203_ts124qdm_wqxga_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for ss_dsi_panel_FT8203_TS124QDM_WQXGA");
MODULE_LICENSE("GPL v2");
