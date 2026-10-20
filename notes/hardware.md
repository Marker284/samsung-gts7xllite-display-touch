# Железо SM-T735 — подтверждённые факты

Источник, если не указан иначе: `adb shell getprop` на стоковом Android 14, 2026-10-02.

## Идентификация
| Параметр | Значение | Источник |
|---|---|---|
| Модель | SM-T735 | `ro.product.model` |
| **Codename** | **`gts7xllite`** ✅ ПОДТВЕРЖДЁН | `ro.product.device`, `ro.product.device_for_attestation` |
| product name | `gts7xllitexx` | `ro.product.name` |
| Платформа | **`lito`** ✅ | `ro.board.platform` |
| Чип | **SM7225** ✅ | `ro.hardware.chipname` |
| GPU | Adreno (619 — TODO(verify) по модели) | `ro.hardware.egl`, `ro.hardware.vulkan` = adreno |
| Serial | R52RB00DB4H | `adb devices` |
| HW revision | 7 | `ro.revision` |

## Прошивка (для скачивания стока и откатa)
| Параметр | Значение |
|---|---|
| **PDA / build** | **`T735XXS9DYF1`** |
| Bootloader | `T735XXS9DYF1` (bootloader rev **9**) |
| CSC | **SKZ** (Kazakhstan, multi-CSC OXM) |
| CSC build | `T735OXM9DYF1`, SAOMC `SAOMC_SM-T735_OXM_SKZ_14_0006` |
| Android | 14, OneUI 6.1 (`ro.build.version.oneui`=60100), SDK 34 |
| Security patch | 2025-06-01 |
| base_os | `samsung/gts7xllitexx/gts7xllite:14/UP1A.231005.007/T735XXU5DXE1:user/release-keys` |

Для Frija/SamFW: модель `SM-T735`, регион `SKZ`.

## Память
- MemTotal 3 518 292 kB → **RAM 4 ГБ** (вариант 4/64 или 4/128).

## Схема разделов
- **Не A/B**: `ro.build.ab_update` пуст, `ro.boot.slot_suffix` пуст, нет `_a`/`_b` суффиксов.
- `super` присутствует (dynamic partitions), `vbmeta` + `vbmeta_system` раздельно.
- `ro.boot.dtb_idx` = 0, `ro.boot.dtbo_idx` = 2 → ABL выбирает dtb #0 из boot.img и dtbo #2 из раздела dtbo.

Ключевые разделы (`/dev/block/by-name/`):

| Раздел | Блок |
|---|---|
| boot | sda25 |
| recovery | sda26 |
| dtbo | sda33 |
| vbmeta | sde21 |
| vbmeta_system | sda22 |
| super | sda27 |
| userdata | sda34 |
| modem | sda24 |
| apnhlos (NON-HLOS) | sda19 |
| **efs** (не трогать) | sda6 |
| **sec_efs** (не трогать) | sda9 |
| **persist** (не трогать) | sda5 |
| **param** (не трогать) | sda7 |
| xbl / abl / tz / hyp / aop (не трогать) | sdb1 / sde3 / sde1 / sde32 / sde2 |

Полный список снят в сессии 1, есть в LOG.

## TODO(verify)
- Версия заголовка boot.img — нужен стоковый AP, `unpack_bootimg`.
- `qcom,msm-id` / `qcom,board-id` — из стокового dtb (внутри boot.img или dtbo).
- Адрес/формат continuous splash framebuffer, регион ramoops — из стокового dtb.
- Компатиблы и GPIO: панель, sec_touchscreen, Wacom, PMIC (pm6350 / pm7250b?), кодек, зарядка.
- Точная модель Adreno (ожидается 619 для SM7225).
- Панель: 12.4" 2560x1600 — модель контроллера неизвестна.

---

# Разведка стокового boot.img / dtbo (сессия 1)

Источник: стоковая прошивка `SM-T735_4_20250620193611_2nmxo129bn_fac.zip`
(`T735XXS9DYF1` / CSC `T735OXM9DYF1`, SKZ), скачана через samloader-rs 2.2.0.
Файлы бэкапа: `stock/backup/{boot,recovery,dtbo,vbmeta,vbmeta_system,persist,misc}.img`.

## boot.img — ПОДТВЕРЖДЕНО
`unpack_bootimg --boot_img stock/backup/boot.img`

| Параметр | Значение |
|---|---|
| **boot image header version** | **2** |
| magic | `ANDROID!` |
| page size | **4096** |
| kernel load addr | `0x00008000` (base `0x00000000`, kernel_offset `0x00008000`) |
| ramdisk load addr | `0x02000000` |
| tags load addr | `0x01e00000` |
| dtb addr | `0x01f00000` |
| second (dt) size | 0 |
| recovery dtbo size | 0 |
| **dtb внутри boot.img** | да, 401068 байт, DTB v17 |
| os version | `11.0.0` |
| os patch level | `2025-06` |
| product name | `SRPUB23B009` |
| boot header size | 1660 |
| vendor_boot | **отсутствует** |
| **хвост `SEANDROIDENFORCE`** | **есть**, на offset 54263808 |

Стоковый cmdline:
```
console=null androidboot.hardware=qcom androidboot.memcg=1 lpm_levels.sleep_disabled=1
video=vfb:640x400,bpp=32,memsize=3072000 msm_rtb.filter=0x237 service_locator.enable=1
androidboot.usbcontroller=a600000.dwc3 swiotlb=2048 cgroup.memory=nokmem,nosocket
firmware_class.path=/vendor/firmware_mnt/image loop.max_part=7
```

### Готовые параметры для mkbootimg
```
--header_version 2 --pagesize 4096 --base 0x00000000
--kernel_offset 0x00008000 --ramdisk_offset 0x02000000
--tags_offset 0x01e00000 --dtb_offset 0x01f00000
--os_version 11.0.0 --os_patch_level 2025-06
```
плюс дописать `SEANDROIDENFORCE` в конец готового boot.img.

## SoC id из стокового DTB — ПОДТВЕРЖДЕНО
`downstream/stock-boot.dts` (dtc -I dtb -O dts из boot.img):
```
model = "Qualcomm Technologies, Inc. Lagoon SoC";
qcom,msm-id   = <0x1b2 0x10000>, <0x1cb 0x10000>;   // 434 (SM6350) и 459 (SM7225), rev 1.0
qcom,board-id = <0x00 0x00>;
```

## dtbo.img
6 оверлеев, Samsung board-id варианты. `ro.boot.dtbo_idx` = **2**:

| idx | model |
|---|---|
| 0 | (пустой, 294 байта) |
| 1 | Samsung GTS7XLLITE PROJECT (board-id,00) |
| **2** | **Samsung GTS7XLLITE PROJECT (board-id,05)** ← наш, `ro.revision`=7 |
| 3 | (board-id,03) |
| 4 | (board-id,02) |
| 5 | (board-id,04) |

Распакованы в `downstream/dtbo/*.dtb`, наш декомпилирован в
`downstream/stock-dtbo2-board05.dts`.

## Панель — ПОДТВЕРЖДЕНО
- Дефолтная: `qcom,dsi-default-panel = <&ss_dsi_panel_HX83121_PPC357DB11_WQXGA>`
- Контроллер **Himax HX83121A**, панель **PPC357DB11**, WQXGA (2560x1600).
- Второй вариант в дереве: `ss_dsi_panel_FT8203_TS124QDM_WQXGA` (FocalTech FT8203) —
  альтернативный поставщик панели, TODO(verify) какой стоит физически.
- Регуляторы панели (из overlay board-05): `panel_buck_en`, `panel_buck_en2`,
  `panel_boost_en`, `panel_reset`.
- В mainline драйвера панели HX83121 **нет** → надо писать.

## Тачскрин — ПОДТВЕРЖДЕНО
- **Himax HX83121A**, тач интегрирован в тот же контроллер, что и панель.
- Прошивка тача: `tsp_himax/hx83121a_gts7xllite.bin`.
- В mainline есть `drivers/input/touchscreen/himax_hx83112b.c`; в ветке sm6350-mainline
  добавлена поддержка HX83112A (для FP4). Для HX83121A нужно расширение драйвера.
- В overlay также присутствуют `synaptics,tcm-i2c` и пины `ft820x_tsp_*` —
  это заготовки QC/альтернативных панелей, не наш активный тач. TODO(verify).

## S Pen — ПОДТВЕРЖДЕНО (из порта)
- Wacom, `compatible = "wacom,w90xx"`, на `i2c8`, адрес `0x56`.
- Драйвер `Wacom WEZ01` добавлен в ветку `spen`.

## Hall sensor
- `hall_irq`, `hall_wacom_irq` — датчик Холла (чехол) + детект пера.

## Панель и тачскрин (источник: downstream/stock-dtbo2-board05.dts, board-id 05)

### ВАЖНО: у модели ДВЕ возможные панели
В стоковом dtbo описаны обе, выбор по GPIO `vendor_check` = **tlmm 23**:
- `ss_dsi_panel_HX83121_PPC357DB11_WQXGA` + тач Himax (`vendor_check_enable_value = 0`)
- `ss_dsi_panel_FT8203_TS124QDM_WQXGA` + тач Focaltech (`vendor_check_enable_value = 1`)
TODO(verify): прочитать tlmm 23 на устройстве и подтвердить, что у Mark именно Himax/CSOT.

### Панель CSOT PPC357DB11 (HX83121A DDIC, TDDI)
- `samsung,panel-vendor = "CSOT"`, `samsung,disp-model = "PPC357DB11"`
- 1600x2560, bpp 24, **video mode**, 60 Гц, 4 линии DSI
- **одиночный DSI**: `qcom,dsi-ctrl-num = <0>`, `qcom,dsi-phy-num = <0>`
  (у SM6350/SM7225 в mainline есть только `mdss_dsi0`, узла `dsi1` нет)
- тайминги: h-pulse 20, h-back 40, h-front 118; v-pulse 4, v-back 18, v-front 60
- `qcom,mdss-dsi-panel-clockrate = <0x284af100>` = **676 МГц**
- phy-timings: `<0x180606 0x21200606 0x7020400>`
- **DSC включён**: version 1.1, slice 1600x40, 8 bpc, **8 bpp** (сжатие 3:1),
  block prediction on, `samsung,no_qcom_pps`
- физический размер: 166 x 266 мм (`0xa6` x `0x10a`)
- подсветка: DCS (`bl_ctrl_dcs`), max level **425** (`0x1a9`), default 255
- reset-последовательность: `<1 2 0 2 1 37>` (high 2 мс, low 2 мс, high 55 мс)

### Питание панели (порядок включения из qcom,panel-supply-entry)
| № | имя | источник | напряжение | задержки |
|---|---|---|---|---|
| 0 | `panel_ldo_en` | **pm6350_l14** (PMIC LDO) | 1,9 В | pre-on 2 мс |
| 1 | `panel_buck_en` | tlmm GPIO **88** (fixed-regulator) | 1,8 В ном. | — |
| 2 | `panel_buck_en2` | tlmm GPIO **89** (fixed-regulator) | 1,8 В ном. | post-on 1 мс |
| 3 | `panel_boost_en` | tlmm GPIO **72** (fixed-regulator) | 1,8 В ном. | post-on 1 мс |
| 5 | `panel_reset` | tlmm GPIO **73** (fixed-regulator, active-high) | — | pre-on 1 мс |

В mainline-драйвере регуляторы называются `vddi`, `avdd`, `avee` →
соответствие: vddi = pm6350_l14, avdd/avee = panel_buck_en / panel_buck_en2.

### Тачскрин Himax HX83121A (TDDI, один кристалл с панелью)
- шина **`qupv3_se6_spi`**, `spi-max-frequency` = **7 МГц** (`0x6acfc0`), `num-cs = <1>`
- **CS** tlmm **16**, **IRQ** tlmm **22**, **RESET** tlmm **18**, vendor_check tlmm **23**
- downstream compatible `himax,hxcommon`, firmware `tsp_himax/hx83121a_gts7xllite.bin`
- координаты 1600x2560, `himax,select_lcdid = <0x87f230 0x13d230>`
- питание — общее с панелью (panel_buck_en/buck_en2/ldo_en/panel_reset)
