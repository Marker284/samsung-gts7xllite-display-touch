# Прошивка SM-T735 (gts7xllite)

Статус: **процедура ещё НЕ проверена на железе.** Всё ниже помечено как план,
подтверждённые куски отмечены отдельно.

## Инструменты
- **odin4** — `/usr/local/bin/odin4`, собран из `tools/odin4/`
  (`Llucs/odin4`, открытая реализация Thor-протокола, версия 7.3.0-a46321b).
  Сборка: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`.
  На Arch нужен `PKG_CONFIG_PATH` с symlink `libcrypto++.pc -> libcryptopp.pc`.
- **samloader-rs** — `tools/samloader/samloader`. Умеет `flash`, `dump-pit`,
  `print-pit`, `reboot-download`, `fix-usb` (ставит udev-правила).
- **heimdall** 2.2.2 есть в системе, но на родственном SM-T733 он падал с
  "Failed to confirm end of file transfer sequence" → по умолчанию не используем.

## Вход в Download mode (физические действия Mark)
1. Полностью выключить планшет.
2. Зажать **Vol− и Vol+** одновременно.
3. Не отпуская — подключить USB-C к ноутбуку.
4. На экране появится предупреждение → **Vol+** для подтверждения входа в Download mode.

Если устройство в бутлупе: **Vol− + Power** ~7 с до перезагрузки, затем сразу **Vol− + Vol+**.

Проверка с ноутбука:
```
odin4 -l
# или
tools/samloader/samloader detect
```

## Откат на сток
Полный набор лежит в `stock/SM-T735_4_20250620193611_2nmxo129bn_fac.zip`:
```
BL_T735XXS9DYF1_..._MULTI_CERT.tar.md5
AP_T735XXS9DYF1_..._meta_OS14.tar.md5
CP_T735XXS9DYF1_CP30639088_..._MULTI_CERT.tar.md5
HOME_CSC_OXM_T735OXM9DYF1_..._MULTI_CERT.tar.md5   ← сохраняет данные
CSC_OXM_T735OXM9DYF1_..._MULTI_CERT.tar.md5        ← стирает данные
```
Это **ровно тот билд, что стоит на устройстве** (`T735XXS9DYF1`), значит
понижения версии загрузчика не будет.

Откат:
```
odin4 -b BL_*.tar.md5 -a AP_*.tar.md5 -c CP_*.tar.md5 -s HOME_CSC_*.tar.md5 --reboot
```

## Бэкапы (есть)
`stock/backup/`: `boot.img`, `recovery.img`, `dtbo.img`, `vbmeta.img`,
`vbmeta_system.img`, `persist.img`, `misc.bin` — распакованы из AP, оригиналы `.lz4` рядом.
Распакованный стоковый boot: `stock/backup/unpacked/{kernel,ramdisk,dtb}`.

**НЕ УДАЛЯТЬ.**

## Сборка нашего boot.img
Параметры взяты из стокового boot.img (см. notes/hardware.md, подтверждено):
```sh
mkbootimg \
  --header_version 2 \
  --kernel      out/vmlinuz \
  --ramdisk     out/initramfs \
  --dtb         out/sm7225-samsung-gts7xllite.dtb \
  --pagesize    4096 \
  --base        0x00000000 \
  --kernel_offset  0x00008000 \
  --ramdisk_offset 0x02000000 \
  --tags_offset    0x01e00000 \
  --dtb_offset     0x01f00000 \
  --os_version     11.0.0 \
  --os_patch_level 2025-06 \
  --cmdline "console=tty0 earlycon ignore_loglevel clk_ignore_unused pd_ignore_unused" \
  --output out/boot.img

# Samsung ABL ожидает этот хвост — в стоковом boot.img он есть (offset 54263808)
printf 'SEANDROIDENFORCE' >> out/boot.img
```

Упаковка под odin4:
```sh
cd out && tar -H ustar -cf boot.tar boot.img
# md5 по формату odin (tar.md5 = tar + md5sum, дописанный в конец):
# md5sum -t boot.tar >> boot.tar && mv boot.tar boot.tar.md5
```

Прошивка **только boot** (разрешено правилами проекта, бэкап есть):
```sh
odin4 --check-only -a out/boot.tar    # сначала dry-run: проверит PIT и архив
odin4 -a out/boot.tar
odin4 --reboot
```

## vbmeta — TODO(verify)
Нужно ли отключать verified boot, выяснится по поведению ABL при первой прошивке.
Если ABL отвергнет неподписанный boot:
```sh
avbtool make_vbmeta_image --flags 2 --padding_size 4096 --output out/vbmeta.img
cd out && tar -H ustar -cf vbmeta.tar vbmeta.img
odin4 -a out/vbmeta.tar
```

## ОТКРЫТЫЙ ВОПРОС — спросить Mark
**Разблокирован ли OEM?** Без «OEM unlocking» в параметрах разработчика Samsung ABL
не примет неподписанный boot.img, и прошивка просто не пройдёт.
Проверить не удалось: устройство отключилось от USB до того, как я прочитал
`ro.boot.verifiedbootstate` / `ro.boot.flash.locked` / `ro.boot.warranty_bit`.

Важно: **разблокировка OEM на Samsung стирает userdata** (factory reset) и выставляет
KNOX warranty bit необратимо.

## Чего НИКОГДА не писать
`efs`, `sec_efs`, `persist`, `modem`/`apnhlos`, `param`, `xbl`, `abl`, `tz`, `hyp`,
`aop`, RPMB.
