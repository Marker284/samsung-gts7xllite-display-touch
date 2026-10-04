# Samsung Galaxy Tab S7 FE (SM-T735 / gts7xllite) — Display Panel & Touchscreen Drivers

This repository provides working Linux mainline drivers, kernel patches, device tree definitions, firmware, and documentation for the **12.4" WQXGA (1600x2560) Display Panel** and **FocalTech FT8203 TDDI Touchscreen** on the **Samsung Galaxy Tab S7 FE LTE (SM-T735 / gts7xllite)** (Qualcomm Snapdragon 750G / SM7225).

Both the display (with DSC 1.1 compression via KMS) and multi-touch (10 touch points) are fully tested and functional on Linux 7.2+ (tested under Nura / postmarketOS with KDE Plasma Desktop).

---

## Table of Contents
1. [Hardware Overview & Panel Detection](#hardware-overview--panel-detection)
2. [Display Panel (FT8203 / TS124QDM) & The Upstream DPU Bug](#display-panel-ft8203--ts124qdm--the-upstream-dpu-bug)
3. [Touchscreen Controller (FT8203 SPI TDDI)](#touchscreen-controller-ft8203-spi-tddi)
4. [Power Sequencing & RPMh PLDO Stepping Trap](#power-sequencing--rpmh-pldo-stepping-trap)
5. [Repository Structure](#repository-structure)
6. [How to Apply and Build](#how-to-apply-and-build)
7. [Kernel Command Line & Booting](#kernel-command-line--booting)

---

## Hardware Overview & Panel Detection

### Device Specs
- **SoC:** Qualcomm Snapdragon 750G (**SM7225**, lagoon / lito family)
- **Codename:** `gts7xllite` (LTE: SM-T735)
- **Display:** 12.4" TFT LCD, 1600x2560 (portrait native mounting), 60 Hz
- **Display Driver IC (DDIC):** FocalTech **FT8203** (TS124QDM)
- **Touch Controller:** FocalTech **FT8203** (TDDI on SPI)

### Dual-Sourcing Note (FocalTech vs Himax)
Samsung dual-sourced the panels for the Tab S7 FE:
1. **FocalTech FT8203 / TS124QDM** (covered by this repo)
2. **CSOT / Himax HX83121A / PPC357DB11**

To verify which panel your device has:
- **GPIO 23 hardware strap:**
  ```sh
  gpioget --bias=disabled --chip gpiochip2 23
  ```
  - Reads `1` &rarr; **FocalTech FT8203** (this driver)
  - Reads `0` &rarr; Himax HX83121A
- **ABL cmdline argument:** Check `/proc/cmdline`:
  ```
  msm_drm.dsi_display0=ss_dsi_panel_FT8203_TS124QDM_WQXGA: lcd_id=0x876270
  ```

---

## Display Panel (FT8203 / TS124QDM) & The Upstream DPU Bug

The display driver is located at `drivers/gpu/drm/panel/panel-ft8203-ts124qdm-wqxga.c`.

### Panel Specifications
- **Resolution:** 1600 &times; 2560 (portrait)
- **DSI Interface:** 4 data lanes, single DSI controller (`mdss_dsi0`)
- **Mode:** DSI Video Mode with **Burst traffic** (`MIPI_DSI_MODE_VIDEO_BURST`)
- **Compression:** VESA **DSC 1.1**
  - Slice size: 800 &times; 40
  - Slice count per line: 2
  - Slices per DSI packet: 2 (`slice_per_pkt = 2`)
  - Color depth: 8 bpc (24 bpp uncompressed), target compressed rate: 8 bpp (3:1 compression ratio)
  - Pixel clock: ~278.45 MHz; DSI link bit clock: ~676 MHz (557 Mbps/lane compressed minimum, 676 Mbps burst headroom)

### 1. The Critical Upstream DPU Bug (`DIV_ROUND_UP`)
> **Patch:** `patches/0001-drm-msm-dpu-round-up-line-width-in-video-mode-with-DSC.patch`

In upstream `drivers/gpu/drm/msm/disp/dpu1/dpu_encoder_phys_vid.c`, the compressed line width was computed as:
```c
timing->width = timing->width * drm_dsc_get_bpp_int(dsc) /
                (dsc->bits_per_component * 3);
```
On this panel:
$$\frac{1600 \times 8}{24} = 533.333\dots$$
Integer truncation calculated **533** pixel slots per line.

However, `drivers/gpu/drm/msm/dsi/dsi_host.c` already computes `hdisplay` using `DIV_ROUND_UP`:
$$\lceil 533.333\rceil = 534$$
and tracks the single remaining byte in `eol_byte_num = total_bytes_per_intf % 3 = 1600 % 3 = 1`.

**The Bug:**
The DPU sent 533 slots per line, but DSI interface was waiting for 534 slots before closing the line. Because the last slot was never completed:
- `vblank wait timed out on crtc 0` triggered continuously
- Hardware `video done` interrupt never fired
- The screen remained completely black

**The Fix:**
Change DPU line width calculation to `DIV_ROUND_UP`:
```c
timing->width = DIV_ROUND_UP(timing->width * drm_dsc_get_bpp_int(dsc),
                             dsc->bits_per_component * 3);
```
With this fix, DPU and DSI match at 534 slots, vblank timeouts drop to 0, and frames render cleanly.

### 2. PPS (Picture Parameter Set) Packet Transmission
The stock Samsung vendor tree sets `samsung,no_qcom_pps`. The Qualcomm DSI controller does not generate the PPS packet automatically.
The panel driver explicitly formats and sends the PPS packet during `prepare()`:
```c
drm_dsc_pps_payload_pack(&pps, dsi->dsc);
mipi_dsi_picture_parameter_set(dsi, &pps);
mipi_dsi_compression_mode(dsi, true);
```

### 3. Multiple Slices Per Packet (`slice_per_pkt > 1`)
> **Patch:** `patches/0002-drm-msm-dsi-support-DSC-configurations-with-slice_per_pkt-greater-than-1.patch`

Stock DT specifies `qcom,mdss-dsc-slice-per-pkt = <2>`, meaning both 800px slices for a line are packaged into a single DSI transmission. We backport `dsc_slice_per_pkt` support to `dsi_host.c` and `struct mipi_dsi_device`.

---

## Touchscreen Controller (FT8203 SPI TDDI)

The touchscreen driver is located at `drivers/input/touchscreen/focaltech-ft8203.c`.

### Architecture & Connection
The FT8203 is a **TDDI (Touch and Display Driver Integration)** IC. Display and touch reside on the same silicon die:
- **Bus:** SPI via Qualcomm QUPv3 SE6 (`qupv3_se6_spi` / `&spi6`)
- **Clock:** 7 MHz (`spi-max-frequency = <7000000>`)
- **Chip Select:** TLMM GPIO 16
- **IRQ:** TLMM GPIO 22 (`IRQ_TYPE_EDGE_FALLING`)
- **Reset:** TLMM GPIO 18 (shared / co-dependent with panel reset)
- **Resolution:** 1600 &times; 2560, up to 10 multi-touch contacts

### drm_panel Follower
Because power and reset are shared with the display, the touch driver implements `devm_drm_panel_add_follower()`. It does not attempt to power regulators or talk to the chip until the display DRM panel has been powered and prepared.

### Crucial Firmware Upload & Vendor ECC Checksum
The chip contains no internal flash memory for touch firmware. On reset, it boots into a ROM bootloader and replies `0xef` to register reads.

1. **Firmware File:** `/lib/firmware/tsp_focaltech/ft8203_gts7xllite.bin` (82,032 bytes, included in `firmware/`).
2. **PRAM Upload:** The driver writes the firmware into the chip's PRAM via SPI chunked transfers.
3. **The Mandatory ECC Checksum Step:**
   Merely writing PRAM and sending command `0x08` (run) leaves the chip inert (`flow_work_cnt` in register `0x91` stays at 0).
   The host **must** compute a reflected CRC-16 with polynomial $(1 \ll 15) \mid (1 \ll 10) \mid (1 \ll 3)$:
   ```c
   /* Host calculates CRC matching fts_ecc_cal_host */
   u16 host_crc = ft8203_crc16(fw->data, fw->size); /* 0xdbe3 for ft8203_gts7xllite.bin */
   ```
   and query the chip's hardware CRC via commands `0xCC`, `0xCE` (wait for ready `0xA5`), and `0xCD` (read 2-byte CRC).
   Only after the hardware verifies this checksum (`0xdbe3`) does command `0x08` start the internal capacitive scanning loop!

### Coordinate Offsets
In vendor Samsung drivers, the SPI packet structure includes the register command in byte 0. In our driver's buffer layout, point data starts at offset 2 (not offset 3).

---

## Power Sequencing & RPMh PLDO Stepping Trap

### Power Sequence
The panel supplies must be enabled in the following sequence:
1. `panel_ldo_en` &rarr; pm6350 L14 (`vreg_l14a`, 1.896 V)
2. `panel_buck_en` &rarr; TLMM GPIO 88 (`panel_avdd`)
3. `panel_buck_en2` &rarr; TLMM GPIO 89 (`panel_avee`)
4. `panel_boost_en` &rarr; TLMM GPIO 72 (`panel_boost`, keep always-on)
5. `panel_reset` &rarr; TLMM GPIO 75

### The RPMh Regulator Stepping Trap
The downstream Samsung DTS specifies 1.9 V for `vreg_l14a`.
However, Qualcomm RPMh PLDO regulators (`pmic5_pldo` in `drivers/regulator/qcom-rpmh-regulator.c`) use an 8 mV step grid starting from 1.504 V:
$$\frac{1\,900\,000 - 1\,504\,000}{8\,000} = 49.5 \quad (\text{not an integer step!})$$

**The Trap:**
If you set:
```dts
vreg_l14a: ldo14 {
    regulator-min-microvolt = <1900000>;
    regulator-max-microvolt = <1900000>;
};
```
The regulator core cannot resolve 1.900 V onto the hardware grid. The entire `pm6350` regulator driver fails probing, which prevents UFS storage, USB, PHYs, and MDSS from ever probing (`deferred probe pending` indefinitely).

**The Solution:**
Constrain `vreg_l14a` to valid grid steps:
```dts
vreg_l14a: ldo14 {
    regulator-min-microvolt = <1896000>;
    regulator-max-microvolt = <1904000>;
    regulator-initial-mode = <RPMH_REGULATOR_MODE_HPM>;
};
```

---

## Repository Structure

```
├── drivers/
│   ├── gpu/drm/panel/
│   │   └── panel-ft8203-ts124qdm-wqxga.c    # Panel DRM KMS driver
│   └── input/touchscreen/
│       └── focaltech-ft8203.c              # SPI TDDI touchscreen driver
├── patches/
│   ├── 0001-drm-msm-dpu-round-up-line-width-in-video-mode-with-DSC.patch
│   ├── 0002-drm-msm-dsi-support-DSC-configurations-with-slice_per_pkt-greater-than-1.patch
│   ├── 0003-drivers-add-FT8203-panel-and-touchscreen-Kconfig-and-Makefile.patch
│   └── 0004-arm64-dts-qcom-sm7225-samsung-gts7xllite-add-display-and-touch.patch
├── dts/
│   └── sm7225-samsung-gts7xllite.dts       # Complete updated device tree
├── firmware/
│   └── tsp_focaltech/
│       └── ft8203_gts7xllite.bin           # Touchscreen PRAM firmware (82 KB)
└── README.md
```

---

## How to Apply and Build

### 1. Apply Patches
In your Linux kernel tree (based on SM6350/SM7225 mainline):
```sh
git am patches/0001-drm-msm-dpu-round-up-line-width-in-video-mode-with-DSC.patch
git am patches/0002-drm-msm-dsi-support-DSC-configurations-with-slice_per_pkt-greater-than-1.patch
git am patches/0003-drivers-add-FT8203-panel-and-touchscreen-Kconfig-and-Makefile.patch
git am patches/0004-arm64-dts-qcom-sm7225-samsung-gts7xllite-add-display-and-touch.patch

# Copy driver source files into place
cp drivers/gpu/drm/panel/panel-ft8203-ts124qdm-wqxga.c drivers/gpu/drm/panel/
cp drivers/input/touchscreen/focaltech-ft8203.c drivers/input/touchscreen/
```

### 2. Enable Kernel Options
Add to your `defconfig` or `.config`:
```kconfig
CONFIG_DRM_PANEL_FOCALTECH_FT8203_TS124QDM=y
CONFIG_TOUCHSCREEN_FOCALTECH_FT8203=m
CONFIG_DRM_DISPLAY_DSC_HELPER=y
CONFIG_SPI_QCOM_GENI=y
```

### 3. Build Notes (Clang / CFI)
If building an Android/postmarketOS mainline kernel with Clang and `CONFIG_CFI=y`:
Always build with Clang (`make LLVM=1 ARCH=arm64`). Building out-of-tree modules with GCC against a Clang-CFI kernel will cause CFI oopses on module load.

### 4. Install Firmware
Copy the touchscreen firmware file to your root filesystem:
```sh
mkdir -p /lib/firmware/tsp_focaltech/
cp firmware/tsp_focaltech/ft8203_gts7xllite.bin /lib/firmware/tsp_focaltech/
```

---

## Kernel Command Line & Booting

### Working Cmdline Parameters
```
console=tty0 ignore_loglevel loglevel=8 fbcon=nodefer pmos.nosplash panel_ft8203_ts124qdm_wqxga.slice_per_pkt=2 panel_ft8203_ts124qdm_wqxga.traffic=2
```

### Booting on SM-T735 (Samsung ABL)
- **Important:** On SM-T735, the Samsung ABL bootloader rejects custom kernels placed in the `boot` partition (`Set warranty bit: kernel` &rarr; reboot to Odin/Download mode).
- Custom Linux kernels **must be flashed to the `recovery` partition**!
- Boot into Linux by holding **Volume Up + Power** until the Samsung splash appears, then release Power and continue holding Volume Up.

---

## Authors & Credits
- **Mark (Marker284)** — Kernel debugging, DPU DSC line width fix, FT8203 touchscreen reverse-engineering & ECC implementation, DTS integration.
- Driver baseline generated with `linux-mdss-dsi-panel-driver-generator` by z3ntu and ported from Samsung downstream sources (`daviddean-x/android_kernel-gts7xllite`).
