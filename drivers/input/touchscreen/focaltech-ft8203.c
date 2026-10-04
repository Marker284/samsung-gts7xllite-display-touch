// SPDX-License-Identifier: GPL-2.0-only
/*
 * FocalTech FT8203 touchscreen driver (SPI).
 *
 * The FT8203 is a TDDI controller: touch and display live on the same die and
 * share their supplies with the panel, so this driver does not manage any
 * regulators of its own. It attaches as a drm_panel follower instead and only
 * talks to the chip while the panel is powered.
 *
 * The part has no flash of its own. After every reset it comes up in its ROM
 * bootloader and answers 0xef to every application register until the host has
 * written the touch firmware into its PRAM and told it to run.
 *
 * The SPI framing, the download sequence and the touch report layout were taken
 * from the vendor driver in the Samsung Galaxy Tab S7 FE kernel sources,
 * drivers/input/touchscreen/focaltech/ft820x/.
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio/consumer.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#include <drm/drm_panel.h>

/* Command byte: bit 7 selects a read, bit 5 asks for a CRC on the payload. */
#define FT8203_CMD_WRITE	0x00
#define FT8203_CMD_READ		0xa0

/* Bits set in the status byte when the controller did not accept the frame. */
#define FT8203_STATUS_ERR	0xa0

#define FT8203_HEADER_LEN	4	/* addr, cmd, len_hi, len_lo */
#define FT8203_DUMMY_LEN	3
#define FT8203_CRC_LEN		2
#define FT8203_RETRIES		3
#define FT8203_CS_DELAY_US	150

/* Application registers. */
#define FT8203_REG_TOUCHDATA	0x01
#define FT8203_REG_WORKMODE	0x00
#define FT8203_REG_CHIP_ID	0xa3
/*
 * Firmware diagnostics. INT_CNT counts the interrupts the firmware believes it
 * has raised, FLOW_WORK_CNT ticks on every pass of its scan loop: if the latter
 * stands still the firmware is loaded but not scanning.
 */
#define FT8203_REG_FLOW_CNT	0x91
#define FT8203_REG_POWER_MODE	0xa5
/*
 * A one-byte read of CHIP_ID cannot tell the ROM loader from the running
 * application: the ROM answers 0x82 0x01 and the application 0x82 0x03, so the
 * first byte is 0x82 either way. CHIP_ID2 holds the part that actually differs.
 */
#define FT8203_REG_FW_VER	0xa6

/* ROM bootloader commands. */
#define FT8203_CMD_START1	0x55
#define FT8203_CMD_READ_ID	0x90
#define FT8203_CMD_SET_ADDR	0xad
#define FT8203_CMD_PRAM_WRITE	0xae
#define FT8203_CMD_START_APP	0x08
/*
 * The ROM loader can checksum a region of PRAM for us, which is the only way to
 * find out whether the image really landed: every write is acknowledged whether
 * or not it took effect.
 */
#define FT8203_CMD_ECC		0xcc
#define FT8203_CMD_ECC_READ	0xcd
#define FT8203_CMD_ECC_FINISH	0xce
#define FT8203_ECC_OK		0xa5
#define FT8203_ECC_TRIES	100
/* The loader's checksum is a reflected CRC with this polynomial. */
#define FT8203_ECC_POLY		((1 << 15) | (1 << 10) | (1 << 3))

/* ROM boot reports 0x8201; the running firmware reports 0x8203. */
#define FT8203_ROM_ID_H		0x82
#define FT8203_ROM_ID_L		0x01
#define FT8203_CHIP_ID		0x82

#define FT8203_BOOT_RETRIES	30
#define FT8203_BOOT_DELAY_MS	8
#define FT8203_APP_START_MS	100
#define FT8203_ID_POLL_MS	50
#define FT8203_ID_TIMEOUT_MS	1000

/* Length of the firmware image, and its sanity check, live at this offset. */
#define FT8203_APP_INFO_OFFSET	0x100
#define FT8203_FW_MIN_LEN	0x120

#define FT8203_PRAM_PACKET	4092

#define FT8203_MAX_POINTS	10
#define FT8203_POINT_LEN	6
/* Two leading bytes, then the point count, then the points themselves. */
#define FT8203_TOUCHDATA_LEN	(FT8203_MAX_POINTS * FT8203_POINT_LEN + 3)
/*
 * Offsets inside the payload. The vendor driver counts from its own buffer,
 * whose byte 0 holds the register address it then overwrites with the reply,
 * so every vendor offset is one larger than ours: its point area starts at 3,
 * the point count sits at its 2.
 */
#define FT8203_POINTS_OFFSET	2
#define FT8203_POINT_NUM_OFFSET	1

#define FT8203_EVENT_DOWN	0
#define FT8203_EVENT_UP		1
#define FT8203_EVENT_CONTACT	2

#define FT8203_XFER_LEN		(FT8203_TOUCHDATA_LEN + FT8203_HEADER_LEN + \
				 FT8203_DUMMY_LEN + FT8203_CRC_LEN)

struct ft8203_ts {
	struct spi_device *spi;
	struct input_dev *input;
	struct gpio_desc *reset_gpio;
	struct touchscreen_properties props;
	struct drm_panel_follower panel_follower;
	const char *fw_name;
	bool running;

	u8 tx[FT8203_XFER_LEN];
	u8 rx[FT8203_XFER_LEN];
};

/* Reflected CCITT CRC-16, as used by the controller for read payloads. */
static u16 ft8203_crc16(const u8 *data, size_t len)
{
	u16 crc = 0xffff;
	size_t i;
	int bit;

	for (i = 0; i < len; i++) {
		crc ^= data[i];
		for (bit = 0; bit < 8; bit++) {
			if (crc & 0x01)
				crc = (crc >> 1) ^ 0x8408;
			else
				crc >>= 1;
		}
	}

	return crc;
}

static int ft8203_xfer(struct ft8203_ts *ts, void *tx, void *rx, size_t len)
{
	struct spi_transfer xfer = {
		.tx_buf = tx,
		.rx_buf = rx,
		.len = len,
	};

	return spi_sync_transfer(ts->spi, &xfer, 1);
}

static int ft8203_read(struct ft8203_ts *ts, u8 addr, u8 *data, u16 len)
{
	size_t payload = FT8203_HEADER_LEN + FT8203_DUMMY_LEN;
	size_t total = payload + len + FT8203_CRC_LEN;
	int ret = -EIO;
	int i;

	if (total > sizeof(ts->tx))
		return -EINVAL;

	for (i = 0; i < FT8203_RETRIES; i++) {
		memset(ts->tx, 0, total);
		ts->tx[0] = addr;
		ts->tx[1] = FT8203_CMD_READ;
		ts->tx[2] = len >> 8;
		ts->tx[3] = len & 0xff;

		ret = ft8203_xfer(ts, ts->tx, ts->rx, total);
		if (ret || (ts->rx[3] & FT8203_STATUS_ERR)) {
			ret = -EIO;
		} else {
			const u8 *buf = &ts->rx[payload];
			u16 crc_read = buf[len] | (buf[len + 1] << 8);

			if (ft8203_crc16(buf, len) != crc_read) {
				ret = -EBADMSG;
			} else {
				memcpy(data, buf, len);
				return 0;
			}
		}

		usleep_range(FT8203_CS_DELAY_US, FT8203_CS_DELAY_US + 50);
	}

	return ret;
}

/*
 * Writes carry the command byte first, then the payload; @len counts only the
 * payload. Large PRAM packets do not fit the inline buffers, so those get a
 * temporary allocation.
 */
static int ft8203_write(struct ft8203_ts *ts, u8 cmd, const u8 *data, u16 len)
{
	size_t total = FT8203_HEADER_LEN + (len ? FT8203_DUMMY_LEN + len : 0);
	u8 *tx = ts->tx, *rx = ts->rx;
	bool allocated = false;
	int ret = -EIO;
	int i;

	if (total > sizeof(ts->tx)) {
		tx = kzalloc(total, GFP_KERNEL);
		rx = kzalloc(total, GFP_KERNEL);
		if (!tx || !rx) {
			kfree(tx);
			kfree(rx);
			return -ENOMEM;
		}
		allocated = true;
	}

	for (i = 0; i < FT8203_RETRIES; i++) {
		memset(tx, 0, total);
		tx[0] = cmd;
		tx[1] = FT8203_CMD_WRITE;
		tx[2] = len >> 8;
		tx[3] = len & 0xff;
		if (len)
			memcpy(&tx[FT8203_HEADER_LEN + FT8203_DUMMY_LEN],
			       data, len);

		ret = ft8203_xfer(ts, tx, rx, total);
		if (!ret && !(rx[3] & FT8203_STATUS_ERR))
			break;

		ret = -EIO;
		usleep_range(FT8203_CS_DELAY_US, FT8203_CS_DELAY_US + 50);
	}

	if (allocated) {
		kfree(tx);
		kfree(rx);
	}

	return ret;
}

static void ft8203_reset(struct ft8203_ts *ts)
{
	gpiod_set_value_cansleep(ts->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ts->reset_gpio, 0);
}

/* Reset the part and catch it in its ROM bootloader before the app would run. */
static int ft8203_enter_boot(struct ft8203_ts *ts)
{
	u8 id[2];
	int i, j;

	for (i = 0; i < FT8203_BOOT_RETRIES; i++) {
		ft8203_reset(ts);
		msleep(FT8203_BOOT_DELAY_MS);

		for (j = 0; j < 30; j++) {
			if (!ft8203_write(ts, FT8203_CMD_START1, NULL, 0)) {
				msleep(FT8203_BOOT_DELAY_MS);
				if (!ft8203_read(ts, FT8203_CMD_READ_ID, id,
						 sizeof(id)) &&
				    id[0] == FT8203_ROM_ID_H &&
				    id[1] == FT8203_ROM_ID_L) {
					dev_dbg(&ts->spi->dev,
						"in rom boot after %d tries\n",
						i);
					return 0;
				}
			}
			usleep_range(1000, 2000);
		}
	}

	return -ENODEV;
}

static u16 ft8203_ecc_host(const u8 *data, u32 len)
{
	u16 ecc = 0;
	u32 i, j;

	for (i = 0; i + 1 < len; i += 2) {
		ecc ^= (data[i] << 8) | data[i + 1];
		for (j = 0; j < 16; j++) {
			if (ecc & 0x01)
				ecc = (ecc >> 1) ^ FT8203_ECC_POLY;
			else
				ecc >>= 1;
		}
	}

	return ecc;
}

static int ft8203_ecc_chip(struct ft8203_ts *ts, u32 addr, u32 len, u16 *ecc)
{
	u8 cmd[6], val[2];
	int ret, i;

	cmd[0] = (addr >> 16) & 0xff;
	cmd[1] = (addr >> 8) & 0xff;
	cmd[2] = addr & 0xff;
	cmd[3] = (len >> 16) & 0xff;
	cmd[4] = (len >> 8) & 0xff;
	cmd[5] = len & 0xff;

	ret = ft8203_write(ts, FT8203_CMD_ECC, cmd, sizeof(cmd));
	if (ret)
		return ret;

	usleep_range(2000, 3000);

	for (i = 0; i < FT8203_ECC_TRIES; i++) {
		val[0] = 0;
		ret = ft8203_read(ts, FT8203_CMD_ECC_FINISH, val, 1);
		if (ret)
			return ret;
		if (val[0] == FT8203_ECC_OK)
			break;
		usleep_range(1000, 2000);
	}
	if (i == FT8203_ECC_TRIES) {
		dev_err(&ts->spi->dev, "checksum did not finish (0x%02x)\n",
			val[0]);
		return -ETIMEDOUT;
	}

	ret = ft8203_read(ts, FT8203_CMD_ECC_READ, val, sizeof(val));
	if (ret)
		return ret;

	*ecc = (val[0] << 8) | val[1];

	return 0;
}

static int ft8203_write_pram(struct ft8203_ts *ts, const u8 *buf, u32 len)
{
	u8 addr_cmd[3];
	u32 offset;
	int ret;

	for (offset = 0; offset < len; offset += FT8203_PRAM_PACKET) {
		u32 chunk = min_t(u32, FT8203_PRAM_PACKET, len - offset);

		addr_cmd[0] = (offset >> 16) & 0xff;
		addr_cmd[1] = (offset >> 8) & 0xff;
		addr_cmd[2] = offset & 0xff;

		ret = ft8203_write(ts, FT8203_CMD_SET_ADDR, addr_cmd,
				   sizeof(addr_cmd));
		if (ret)
			return ret;

		ret = ft8203_write(ts, FT8203_CMD_PRAM_WRITE, buf + offset,
				   chunk);
		if (ret)
			return ret;
	}

	return 0;
}

static int ft8203_load_firmware(struct ft8203_ts *ts)
{
	u16 ecc_host, ecc_chip;
	struct device *dev = &ts->spi->dev;
	const struct firmware *fw;
	u32 app_size;
	u16 len, len_n;
	int ret;

	ret = request_firmware(&fw, ts->fw_name, dev);
	if (ret) {
		dev_err(dev, "cannot load %s: %d\n", ts->fw_name, ret);
		return ret;
	}

	if (fw->size < FT8203_FW_MIN_LEN) {
		ret = -EINVAL;
		goto out;
	}

	/* The image carries its length twice, the second time inverted. */
	len = (fw->data[FT8203_APP_INFO_OFFSET] << 8) |
	       fw->data[FT8203_APP_INFO_OFFSET + 1];
	len_n = (fw->data[FT8203_APP_INFO_OFFSET + 2] << 8) |
		 fw->data[FT8203_APP_INFO_OFFSET + 3];
	if (len + len_n != 0xffff) {
		dev_err(dev, "bad firmware header (%04x %04x)\n", len, len_n);
		ret = -EINVAL;
		goto out;
	}

	/* This part stores the length halved. */
	app_size = (u32)len * 2;
	if (app_size > fw->size) {
		dev_err(dev, "firmware shorter than its header claims\n");
		ret = -EINVAL;
		goto out;
	}

	ret = ft8203_enter_boot(ts);
	if (ret) {
		dev_err(dev, "controller did not enter rom boot\n");
		goto out;
	}

	ret = ft8203_write_pram(ts, fw->data, app_size);
	if (ret) {
		dev_err(dev, "writing firmware to pram failed: %d\n", ret);
		goto out;
	}

	/*
	 * Verify before starting it. A PRAM write that never reached the chip
	 * is acknowledged exactly like one that did, so without this the only
	 * symptom of a lost image is a firmware that answers registers but
	 * never scans.
	 */
	ecc_host = ft8203_ecc_host(fw->data, app_size);
	ret = ft8203_ecc_chip(ts, 0, app_size, &ecc_chip);
	if (ret) {
		dev_err(dev, "cannot read back firmware checksum: %d\n", ret);
		goto out;
	}
	if (ecc_host != ecc_chip) {
		dev_err(dev, "firmware checksum mismatch: chip 0x%04x host 0x%04x\n",
			ecc_chip, ecc_host);
		ret = -EIO;
		goto out;
	}
	dev_info(dev, "firmware checksum 0x%04x verified\n", ecc_chip);

	ret = ft8203_write(ts, FT8203_CMD_START_APP, NULL, 0);
	if (ret) {
		dev_err(dev, "cannot start firmware: %d\n", ret);
		goto out;
	}

	msleep(FT8203_APP_START_MS);
	dev_info(dev, "firmware %s downloaded (%u bytes)\n", ts->fw_name,
		 app_size);

out:
	release_firmware(fw);
	return ret;
}

static void ft8203_report(struct ft8203_ts *ts, const u8 *buf)
{
	unsigned int i;

	for (i = 0; i < FT8203_MAX_POINTS; i++) {
		const u8 *p = buf + FT8203_POINTS_OFFSET + i * FT8203_POINT_LEN;
		unsigned int id = p[2] >> 4;
		unsigned int event = p[0] >> 6;
		unsigned int x, y;
		bool down;

		if (id >= FT8203_MAX_POINTS)
			break;

		down = event == FT8203_EVENT_DOWN ||
		       event == FT8203_EVENT_CONTACT;

		input_mt_slot(ts->input, id);
		if (!input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, down))
			continue;

		x = ((p[0] & 0x0f) << 8) | p[1];
		y = ((p[2] & 0x0f) << 8) | p[3];

		touchscreen_report_pos(ts->input, &ts->props, x, y, true);
		input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, p[5]);
		input_report_abs(ts->input, ABS_MT_TOUCH_MINOR, p[4]);
	}

	input_mt_sync_frame(ts->input);
	input_sync(ts->input);
}

static irqreturn_t ft8203_irq(int irq, void *dev_id)
{
	struct ft8203_ts *ts = dev_id;
	u8 buf[FT8203_TOUCHDATA_LEN];
	int ret;

	ret = ft8203_read(ts, FT8203_REG_TOUCHDATA, buf, sizeof(buf));
	if (ret) {
		dev_err_ratelimited(&ts->spi->dev, "touch read failed: %d\n",
				    ret);
		return IRQ_NONE;
	}

	/* The buffer reads back as 0xef while the firmware is not running. */
	if (buf[1] == 0xef || buf[1] == 0xff)
		return IRQ_HANDLED;

	dev_dbg(&ts->spi->dev, "touch frame: %*ph\n", 14, buf);

	ft8203_report(ts, buf);

	return IRQ_HANDLED;
}

/*
 * Report whether the firmware is actually scanning. A standing flow counter
 * means the image was downloaded and answers registers, but its sampling loop
 * never got going, which looks exactly like a touchscreen that ignores fingers.
 */
static void ft8203_diag(struct ft8203_ts *ts)
{
	struct device *dev = &ts->spi->dev;
	u8 mode = 0, power = 0, flow1 = 0, flow2 = 0;

	ft8203_read(ts, FT8203_REG_WORKMODE, &mode, 1);
	ft8203_read(ts, FT8203_REG_POWER_MODE, &power, 1);
	ft8203_read(ts, FT8203_REG_FLOW_CNT, &flow1, 1);
	/* The counter advances roughly every 30ms, so sample well beyond that. */
	msleep(150);
	ft8203_read(ts, FT8203_REG_FLOW_CNT, &flow2, 1);

	dev_dbg(dev, "workmode 0x%02x power 0x%02x flow %u->%u\n",
		mode, power, flow1, flow2);
	if (flow1 == flow2)
		dev_warn(dev, "firmware is not scanning\n");
}

static int ft8203_start(struct ft8203_ts *ts)
{
	struct device *dev = &ts->spi->dev;
	unsigned int elapsed = 0;
	u8 id = 0, fw = 0;
	int ret;

	if (ts->running)
		return 0;

	ret = ft8203_load_firmware(ts);
	if (ret)
		return ret;

	/* Wait for the freshly started firmware to answer on its own registers. */
	do {
		ret = ft8203_read(ts, FT8203_REG_CHIP_ID, &id, sizeof(id));
		if (!ret && id == FT8203_CHIP_ID)
			break;

		msleep(FT8203_ID_POLL_MS);
		elapsed += FT8203_ID_POLL_MS;
	} while (elapsed < FT8203_ID_TIMEOUT_MS);

	if (ret || id != FT8203_CHIP_ID) {
		dev_err(dev, "firmware not running (id 0x%02x, ret %d)\n",
			id, ret);
		return -ENODEV;
	}

	if (ft8203_read(ts, FT8203_REG_FW_VER, &fw, sizeof(fw)))
		fw = 0;
	dev_info(dev, "FT8203 ready, firmware version 0x%02x\n", fw);

	ft8203_diag(ts);

	enable_irq(ts->spi->irq);
	ts->running = true;

	return 0;
}

static void ft8203_stop(struct ft8203_ts *ts)
{
	if (!ts->running)
		return;

	disable_irq(ts->spi->irq);
	gpiod_set_value_cansleep(ts->reset_gpio, 1);
	ts->running = false;

	input_mt_sync_frame(ts->input);
	input_sync(ts->input);
}

static int ft8203_panel_prepared(struct drm_panel_follower *follower)
{
	struct ft8203_ts *ts = container_of(follower, struct ft8203_ts,
					    panel_follower);

	return ft8203_start(ts);
}

static int ft8203_panel_unpreparing(struct drm_panel_follower *follower)
{
	struct ft8203_ts *ts = container_of(follower, struct ft8203_ts,
					    panel_follower);

	ft8203_stop(ts);

	return 0;
}

static const struct drm_panel_follower_funcs ft8203_follower_funcs = {
	.panel_prepared = ft8203_panel_prepared,
	.panel_unpreparing = ft8203_panel_unpreparing,
};

static int ft8203_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct ft8203_ts *ts;
	int ret;

	ts = devm_kzalloc(dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	ts->spi = spi;
	spi_set_drvdata(spi, ts);

	if (device_property_read_string(dev, "firmware-name", &ts->fw_name))
		ts->fw_name = "tsp_focaltech/ft8203_gts7xllite.bin";

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	ret = spi_setup(spi);
	if (ret)
		return dev_err_probe(dev, ret, "failed to set up SPI\n");

	ts->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ts->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ts->reset_gpio),
				     "failed to get reset gpio\n");

	ts->input = devm_input_allocate_device(dev);
	if (!ts->input)
		return -ENOMEM;

	ts->input->name = "FocalTech FT8203 Touchscreen";
	ts->input->phys = "input/ts";
	ts->input->id.bustype = BUS_SPI;

	input_set_abs_params(ts->input, ABS_MT_POSITION_X, 0, 0, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_Y, 0, 0, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_TOUCH_MINOR, 0, 255, 0, 0);

	touchscreen_parse_properties(ts->input, true, &ts->props);
	if (!ts->props.max_x || !ts->props.max_y)
		return dev_err_probe(dev, -EINVAL,
				     "touchscreen-size-x/y are required\n");

	ret = input_mt_init_slots(ts->input, FT8203_MAX_POINTS,
				  INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);
	if (ret)
		return dev_err_probe(dev, ret, "failed to init MT slots\n");

	ret = input_register_device(ts->input);
	if (ret)
		return dev_err_probe(dev, ret, "failed to register input\n");

	ret = devm_request_threaded_irq(dev, spi->irq, NULL, ft8203_irq,
					IRQF_ONESHOT | IRQF_NO_AUTOEN,
					dev_name(dev), ts);
	if (ret)
		return dev_err_probe(dev, ret, "failed to request irq\n");

	/*
	 * Touch shares its supplies and reset with the panel, so it can only be
	 * brought up once the panel is powered.
	 */
	ts->panel_follower.funcs = &ft8203_follower_funcs;
	ret = devm_drm_panel_add_follower(dev, &ts->panel_follower);
	if (ret)
		return dev_err_probe(dev, ret, "failed to add panel follower\n");

	return 0;
}

static void ft8203_remove(struct spi_device *spi)
{
	struct ft8203_ts *ts = spi_get_drvdata(spi);

	ft8203_stop(ts);
}

static const struct of_device_id ft8203_of_match[] = {
	{ .compatible = "focaltech,ft8203-ts" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ft8203_of_match);

static const struct spi_device_id ft8203_spi_ids[] = {
	{ "ft8203-ts" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(spi, ft8203_spi_ids);

static struct spi_driver ft8203_driver = {
	.probe = ft8203_probe,
	.remove = ft8203_remove,
	.id_table = ft8203_spi_ids,
	.driver = {
		.name = "focaltech-ft8203",
		.of_match_table = ft8203_of_match,
	},
};
module_spi_driver(ft8203_driver);

MODULE_FIRMWARE("tsp_focaltech/ft8203_gts7xllite.bin");
MODULE_DESCRIPTION("FocalTech FT8203 touchscreen driver");
MODULE_LICENSE("GPL");
