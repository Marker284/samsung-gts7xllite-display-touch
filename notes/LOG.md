# Журнал сессий

## Сессия 1 — 2026-10-02

### Цель, уточнённая Mark
Рабочий порт **Nura** (бывший postmarketOS, переименован 2026-09-27) с **Plasma Desktop**.

### Что сделано

**Разведка устройства (adb, сток Android 14)** — этап 0 закрыт по идентификации:
- codename **`gts7xllite`** подтверждён, платформа **`lito`**, чип **SM7225**;
- билд **T735XXS9DYF1**, CSC **SKZ**, Android 14 / OneUI 6.1, патч 2025-06;
- RAM 4 ГБ, **не A/B**, dynamic partitions (`super`), `dtbo_idx`=2, `dtb_idx`=0;
- полный список разделов снят → notes/hardware.md.

**Тулинг установлен:**
- `pmbootstrap` 3.11.1, `aarch64-linux-gnu-gcc` 16.1.0 — из repo Arch;
- **AUR недоступен**: TLS-handshake к `aur.archlinux.org` обрывается (DPI, не sandbox).
  Всё, что нужно из AUR, берём с GitHub;
- **odin4** — собран из исходников `Llucs/odin4` (открытая реализация Thor-протокола,
  не слитый бинарник), лежит в `tools/odin4/`, установлен в `/usr/local/bin/odin4`.
  При сборке: Arch зовёт pkg-config файл `libcryptopp.pc`, а CMake ищет `libcrypto++`
  → нужен symlink через `PKG_CONFIG_PATH`;
- **samloader-rs** 2.2.0 (topjohnwu, активен) — `tools/samloader/samloader`.
  Умеет и качать, и прошивать, и дампить PIT. `samloader/samloader` (Python) архивирован.

**Стоковая прошивка скачана** (6.4 ГБ, `stock/*.zip`) — ровно тот билд, что стоит
на устройстве, значит откат через Odin возможен. Бэкапы распакованы в `stock/backup/`:
boot, recovery, dtbo, vbmeta, vbmeta_system, persist, misc.

**boot.img разобран** — header v2, все offsets и SEANDROIDENFORCE выписаны в hardware.md.
**DTB и dtbo декомпилированы** → `downstream/stock-boot.dts`,
`downstream/stock-dtbo2-board05.dts`.

### ГЛАВНАЯ НАХОДКА: порт уже существует

**`github.com/vishalbala-nps/linux`, ветка `spen`** — fork `sm6350-mainline/linux`,
активен (последний коммит 2026-09-20), устройство именно наше: SM-T735 LTE gts7xllite.
DTS: `arch/arm64/boot/dts/qcom/sm7225-samsung-gts7xllite.dts` (606 строк).

Компаньон: **`github.com/vishalbala-nps/firmware-samsung-gts7xllite`** — adsp, cdsp,
modem, lagoon_ipa_fws, wlanmdsp, hexagonfs (acdb, sensors, socinfo).
`a615_zap.mbn` там **нет** — потому что GPU отключён.

#### Что в том DTS уже есть
simple-framebuffer, gpio-keys, ramoops, rmtfs-mem, регуляторы pm6350 + pm6150l,
UFS (`ufs_mem_hc` + phy), SD (`sdhc_2`), USB3 (hsphy + qmpphy, режим otg),
Wi-Fi (`&wifi`), BT (`qcom,wcn3991-bt` на uart1), modem (`mpss`), adsp, cdsp, ipa,
RTC (pmk8350), `pm6350_resin`, Wacom S Pen (`wacom,w90xx` @ i2c8 0x56).

#### Чего НЕТ
- `&dispcc { status = "disabled"; }` — **нет DSI/панели**
- `&gpu { status = "disabled"; }` — **нет Adreno 619**
- **нет тачскрина**
- нет звука

Автор отмечает: "Works on portrait mode", USB переведён в otg для ручного переключения.

### Вывод по Plasma Desktop
Сейчас картинка возможна только через `simple-framebuffer` — это не KMS, значит
Plasma пойдёт на llvmpipe без KMS, на 2560x1600 это практически неюзабельно.
**Для рабочего Plasma Desktop обязательны: dispcc + драйвер панели HX83121 + GPU.**
Это и есть основная работа порта, а не загрузка.

Панель — Himax **HX83121A** / PPC357DB11 WQXGA; mainline-драйвера нет.
Тач — тот же HX83121A; в mainline есть `himax_hx83112b.c`, в sm6350-mainline добавлен
HX83112A для FP4 → расширять до HX83121A.

### Следующие шаги
1. Клонировать `vishalbala-nps/linux` ветку `spen` в `linux/` (клон рвётся — сеть
   нестабильна, HTTP/2 reset; помогает `-c http.version=HTTP/1.1`).
2. Собрать ядро конфигом `config-postmarketos-qcom-sm6350.aarch64` из
   `pmaports/device/community/linux-postmarketos-qcom-sm6350`.
3. Создать `pmaports-local/device-samsung-gts7xllite` + пакет firmware
   (в upstream pmaports устройства нет, в pmaports-fork автора — тоже).
4. Собрать boot.img по параметрам из hardware.md, прошить **только boot** (odin4 -a),
   получить USB-сеть и SSH.
5. Дальше по приоритету Mark: GPU и панель.

### Прошивок НЕ делали
Ничего не записывалось на устройство.

### Решения Mark (сессия 1)
- Цель: **Nura + `postmarketos-ui-plasma-desktop`** (пакет в pmaports есть).
- **OEM unlock**: тумблер в параметрах разработчика уже включён, но фактическая
  разблокировка требует входа в загрузчик и подтверждения. Mark хочет сделать это
  одним заходом: сначала собираем образ, потом разблокировка + сразу прошивка.
  Разблокировка сотрёт userdata — это принято.
- **rootfs: внутренняя память (userdata)**, не microSD. Раз unlock всё равно стирает
  данные, Android не сохраняем. Откат на сток возможен — полная прошивка скачана.

### Подготовлено в pmaports-local (ещё не собиралось)
- `device-samsung-gts7xllite/{APKBUILD,deviceinfo,modules-initfs}` —
  header v2, offsets из стокового boot.img, chassis=tablet, dtb
  `qcom/sm7225-samsung-gts7xllite`, экран 1600x2560 (панель физически 2560x1600,
  но смонтирована портретом — так же задан framebuffer в DTS).
- `linux-samsung-gts7xllite/APKBUILD` — ядро 7.2.0 из форка `spen`,
  commit запинен на `92c1b7792348314475c9bf993e29abc1e1a3bd95`.
- `firmware-samsung-gts7xllite/APKBUILD` — по образцу `firmware-fairphone-fp4`,
  commit `98d77cd6e66dbe7667f5a5849656f3c8c42f8f0e`, subpackages
  adsp/cdsp/hexagonfs/ipa/modem/wlan. **Без -adreno**: GPU в DTS отключён и
  `a615_zap.mbn` в репозитории firmware отсутствует.
  Пути firmware из DTS: `qcom/sm7225/Samsung/gts7xllite/`.

### Факты из DTS порта (подтверждено, DTS прочитан)
- `qcom,msm-id = <QCOM_ID_SM7225 0x10000>`, `qcom,board-id = <8 5>`
  (board-id 5 совпадает с нашим dtbo_idx=2 → board-id,05 ✅).
- framebuffer: `0xa0000000`, 1600x2560, stride 1600*4, `a8r8g8b8`,
  `stdout-path = "framebuffer0"`.
- ramoops: `0xb1b00000`, 1 МБ, record/console/ftrace/pmsg по 256 КБ.
- rmtfs: `0xefe01000`, 6 МБ, client-id 1.
- gpio-keys: только **volume-up** (`pm6350_gpios 2`). Power и volume-down идут
  через `pm6350_resin`/qpnp_pon — на родственном T733 они приходили без scancode.
- `serial0 = &uart9`, `serial1 = &uart1` (на uart1 висит BT wcn3991).

### Сетевые проблемы (для будущих сессий)
- `aur.archlinux.org` — TLS-handshake обрывается, AUR недоступен.
- `codeload.github.com` отдаёт tar.gz **без Content-Length и без поддержки Range**,
  скорость падает до ~100 КБ/с и соединение подвисает. Параллелить нельзя.
  Надёжнее `https://github.com/<owner>/<repo>/archive/<sha>.tar.gz`.
- `git clone` рвётся с "HTTP/2 stream reset by server"; помогает
  `-c http.version=HTTP/1.1`, но скорость всё равно низкая.

### План заливки (черновик, не проверен)
rootfs в userdata: `pmbootstrap install --split` → `rootfs.img` упаковать в tar и
писать через `odin4 -a`. Проверить по PIT (`samloader dump-pit` / `print-pit`),
какое имя файла ожидает раздел USERDATA. Альтернатива — TWRP
(`JuliusGudni/twrp_gts7xllite`) и `pmbootstrap install --android-recovery-zip`.

---

## Сессия 1, часть 2 — образ собран (2026-10-02, вечер)

### Загрузчик разблокирован ✅
Mark разблокировал через Download mode, прошёл setup с Wi-Fi. Проверено через adb:
```
ro.boot.verifiedbootstate   = orange
ro.boot.flash.locked        = 0
ro.boot.vbmeta.device_state = unlocked
sys.oem_unlock_allowed      = 1
ro.boot.warranty_bit        = 0   (KNOX ещё не сожжён — сгорит при первой заливке)
ro.boot.veritymode          = enforcing  (не мешает при device_state=unlocked)
```
Пункт «OEM unlocking» после разблокировки исчезает из настроек — это норма, не ошибка.

### Собранные пакеты (все в ~/.local/var/pmbootstrap/packages/edge/aarch64/)
- `linux-samsung-gts7xllite-7.2.0-r0.apk` (34 МБ) — 4013 объектников, clang, 57 мин
- `device-samsung-gts7xllite-0-r0.apk`
- `firmware-samsung-gts7xllite` + adsp, cdsp, hexagonfs, ipa, modem, wlan

### Образ
`pmbootstrap -y install --split --password 147147` → 1268 пакетов, rootfs 5,6 ГБ.
Канал **systemd-edge** (pmbootstrap выбрал сам: plasma-desktop не поддерживает openrc).
Включились `plasmalogin.service` (display-manager), `rmtfs.service`, `tqftpserv.service`.

UUID из сборки (нужны в cmdline, иначе initramfs не найдёт rootfs):
```
pmos_boot_uuid = 70c666eb-7f7c-43ca-96b7-5f81b535e84a
pmos_root_uuid = c1aca5ea-57e1-43a3-9709-1305c9102c54
```

### Что пришлось доделать руками
boot.img от pmbootstrap собран верно (header v2, pagesize 4096, offsets из deviceinfo,
наш DTB 68963 Б внутри), **но**:
1. нет хвоста `SEANDROIDENFORCE`;
2. `os_version` / `os_patch_level` пустые.

Samsung ABL смотрит на оба → пересобрал через `mkbootimg` из распакованных
kernel/ramdisk/dtb, добавив `--os_version 11.0.0 --os_patch_level 2025-06`
и дописав `SEANDROIDENFORCE`.

Также заменил cmdline: вместо `quiet splash plymouth.*` поставил
`console=tty0 earlycon ignore_loglevel` — чтобы на первой загрузке видеть лог ядра
на экране через simple-framebuffer, а не чёрный экран под плимутом. UUID сохранены.

### Артефакты в out/
| файл | размер | куда |
|---|---|---|
| `boot.img` / `boot.tar` | 26,7 МБ | раздел **boot** |
| `userdata.img` / `userdata.tar` | 4,87 ГБ | раздел **userdata** |
| `boot-pmos.img` | 26,7 МБ | оригинал от pmbootstrap, для сравнения |
| `Image.gz`, `*.dtb` | | локальная gcc-сборка, для быстрого цикла |

### Инфраструктурные находки (важно для будущих сессий)
1. **Мёртвый IPv6** — корень всех «обрывов сети» этой сессии. `/etc/gai.conf` был пуст,
   glibc отдавал AAAA первым. curl выживает (Happy Eyeballs), а `urllib` в pmbootstrap
   ждёт без таймаута → вечное зависание. Прописал `precedence ::ffff:0:0/96 100`.
   Тот же запрос: было бесконечно → стало 0,97 с.
2. **Зеркало pmOS**: официальное `mirror.postmarketos.org` то 3,7 с, то висит.
   Замерено с этой сети: `ftp.halifax.rwth-aachen.de` **1,0 с** (выбрано),
   sakamoto.pl 1,4 с, aliyun 1,4 с, `distrohub.kyiv.ua` таймаут.
   Прописано в `~/.config/pmbootstrap_v3.cfg` (mirrors.pmaports + mirrors.systemd).
3. **pmbootstrap настроен без интерактивного `init`**: формат `~/.config/pmbootstrap_v3.cfg`
   (INI, секции `[pmbootstrap]`/`[providers]`/`[mirrors]`), плюс вручную созданы
   `$WORK` и файл `$WORK/version` с содержимым `8` (`pmb.config.work_version`).
4. **sudo в фоне**: pmbootstrap падал на `sudo losetup` — нет tty. Решено через
   `SUDO_ASKPASS` + хелпер, читающий пароль из переменной окружения (на диск не пишется),
   плюс `sudo_timer = True` в конфиге.
5. Не хватало `bc` для сборки ядра (генерация `timeconst.h`).

### Следующий шаг
Mark вводит планшет в Download mode → `samloader dump-pit` + `odin4 --check-only`
(ничего не пишет) → показать Mark разделы → после «да» прошить boot и userdata.

**TODO(verify) после первой загрузки:**
- Расширить rootfs на весь раздел userdata (образ 4,87 ГБ, раздел много больше).
- `/boot` как раздел не прошивается (ядро внутри boot.img) → обновления ядра
  через boot-deploy, вероятно, не заработают. Разобраться.
- Прочитать `/sys/fs/pstore/` если загрузка упадёт.

---

## Сессия 1, часть 3 — прошивка освоена, ядро не стартует (2026-10-03, ночь)

### Что теперь РАБОТАЕТ (процедура прошивки полностью отлажена)
- **samloader-rs**, НЕ odin4. odin4 (Llucs) не проходит USB handshake на этом устройстве.
- **Только первое обращение после подключения проходит handshake.** После закрытия сессии
  следующая попытка падает — нужно переподключать кабель. Поэтому все разделы писать
  ОДНОЙ командой через общий tar.
- **Запуск через `script -qefc "..." /dev/null`** — обязательно! `indicatif` в samloader
  в не-tty режиме прячет и прогресс-бар, и сообщения об успехе. Без pty кажется, что
  ничего не произошло, хотя запись прошла.
- **Большие образы только через `img2simg`** (Android sparse). Без него userdata
  отваливается с `PIT and GPT do not match` — у USERDATA в PIT `Block Count = 6`,
  размер не описан, раздел динамический.
- `sudo systemctl stop ModemManager` перед прошивкой.
- Имена файлов в tar обязаны совпадать с `Flash Filename` из PIT:
  `boot.img`, `vbmeta.img`, `userdata.img`, `dtbo.img`, `recovery.img`.

Успешно записаны: BOOT, VBMETA, USERDATA (3,57 ГиБ за 2:43), DTBO, RECOVERY (TWRP).

### Состояние устройства сейчас
| раздел | содержимое |
|---|---|
| boot | `out/boot-pmos.img` — нетронутый boot.img от pmbootstrap |
| vbmeta | наш, `avbtool --flags 2` (верификация отключена) |
| dtbo | **стоковый** (пустой записать не удалось, см. ниже) |
| recovery | **TWRP 3.6.1** (daviddean-x/gts7xllite-twrp) — работает, есть терминал |
| userdata | наш rootfs с Plasma |
| super | стоковый Android (не тронут) |

`OEM LOCK: OFF`, `WARRANTY VOID: 0x1`, `KG STATUS: CHECKING(00)`.
Android не загрузится — userdata занят нашим rootfs. TWRP загружается нормально.

### ГЛАВНАЯ ПРОБЛЕМА: ядро не стартует
При попытке загрузки экран показывает Samsung splash + диагностику, жёлтым
**`Set warranty bit: vbmeta`**, затем устройство уходит в download mode.

**`/sys/fs/pstore/console-ramoops-0` содержит лог DOWNSTREAM-ядра**
(`focaltech_ts`, `sec_input`, `sm5440-charger`, `GRIP_WIFI`, `sec_debug`,
`Restarting system with command 'recovery'`), а записей нашего ядра **нет вообще**.
→ Наше ядро не доживает до инициализации pstore. Падает в первые миллисекунды
или не запускается совсем.

### Что проверено и НЕ помогло
Симптом не менялся ни разу:
1. `SEANDROIDENFORCE` в конце boot.img (в стоке он есть, offset 54263808).
2. `os_version 11.0.0` / `os_patch_level 2025-06`, `product name SRPUB23B009`.
3. `qcom,msm-id` — добавил оба ида как в стоке: `<0x1b2 0x10000>, <0x1cb 0x10000>`.
4. `qcom,board-id = <8 7>` (свободный) — **ЭТО ОШИБКА, НЕ ПОВТОРЯТЬ**: board-id это
   *селектор*, ABL по нему ищет подходящий DTB и при отсутствии совпадения выдаёт
   "No match found for Soc Dtb type" и уходит в Odin. Реальное значение — `<8 5>`.
5. Сырой `Image` вместо gzip `vmlinuz` (у стока ядро несжатое).
6. Appended DTB (`cat vmlinuz dtb`) + дубль в поле заголовка.
7. `__symbols__` в DTB (`DTC_FLAGS=-@`).
8. Узел `memory` вместо `memory@80000000` (у стока `memory` с `reg = <0 0 0 0>`).
9. vbmeta с отключённой верификацией вместе с нетронутым boot от pmbootstrap.

**Заголовки boot.img сверены со стоковым побайтово — все структурные поля идентичны**
(magic, адреса, page_size, header_version, header_size, dtb_addr). Отличаются только
размеры, cmdline и id. Формат контейнера точно правильный.

**Разделяющий тест:** стоковый boot.img загрузился, Android пошёл и ушёл в recovery
с требованием сброса (userdata занят). → **ABL исправен и ядро грузить умеет.
Проблема именно в нашем ядре/DTB.**

### Тупик с dtbo
`board-id = <8 5>` совпадает со стоковым оверлеем №2 → ABL накладывает downstream-оверлей
(Samsung 4.19) на наш mainline DTB. Убрать его пустым dtbo **не удаётся**:
`FAIL! Auth` даже при vbmeta с отключённой верификацией → Samsung проверяет подпись dtbo
отдельно и всегда. А сменить board-id нельзя (п.4 выше).

### Куда копать дальше
- Автор порта (`vishalbala-nps`) писал в PR #9 sm6350-mainline, что **на pmOS у него
  работали** framebuffer, UFS, SD, кнопки, USB-сеть и OTG, Wi-Fi, BT, модем.
  Значит ядро живое и грузится — **надо выяснить, чем его boot.img отличается от нашего**.
  Стоит написать ему напрямую (PR #10 открыт, он активен).
- Проверить, не ломает ли наше ядро что-то из добавленного мной в конфиг
  (`CONFIG_PSTORE_RAM`, `CONFIG_TOUCHSCREEN_WACOM_WEZ01`, nftables) — собрать строго
  апстримным `config-postmarketos-qcom-sm6350` без правок.
- Заметки по Samsung Tab S9 Ultra (`agcarbajo/postmarketos-galaxy-tab-s9-ultra`,
  `docs/development-notes.md`) — там разобраны отказы ABL и требования к carveouts;
  возможно, нужны все reserved-memory регионы, совпадающие с живым FDT.
- TWRP на месте → можно читать ramoops и ставить эксперименты без риска.

### Полезное про инфраструктуру
- **Мёртвый IPv6** ломал всё: `/etc/gai.conf` был пуст, добавлено
  `precedence ::ffff:0:0/96 100`. Без этого `urllib` в pmbootstrap висит вечно.
- Зеркало: `ftp.halifax.rwth-aachen.de` (1,0 с) вместо официального (висит).
- `sudo` в фоне: `SUDO_ASKPASS` + хелпер, читающий пароль из переменной окружения.
- AUR недоступен (TLS режется), всё ставим с GitHub.

---

## Сессия 1, часть 4 — систематическое исключение причин (2026-10-03, утро)

### Прорыв в понимании: TWRP пишет туда, куда Odin отказывается
Odin отвергает неподписанные `dtbo`/`vbmeta` с `FAIL! Auth`, **но TWRP пишет их напрямую
через `dd`**. Это снимает тупик предыдущей сессии.

Рабочий путь — flashable zip + `adb sideload` (TWRP на этом устройстве поднимает adb
ТОЛЬКО в режиме sideload; в обычном меню USB не появляется вообще):
```
out/zip/META-INF/com/google/android/update-binary   # sh-скрипт, dd в /dev/block/by-name/<part>
out/zip/<образ>.img
zip -r flash.zip . && adb sideload flash.zip
```
Готовый пример: `out/dtbo-noop-twrp.zip` (делает бэкап, чистит раздел, пишет, проверяет magic).

### Что ещё исключено прямыми тестами
| проверено | результат |
|---|---|
| initramfs убран полностью (пустой ramdisk, `panic=10`) | мгновенный Download Mode → умирает ДО userspace |
| noop-dtbo (5 оверлеев с селекторами Samsung, записан из TWRP, magic подтверждён) | без изменений → downstream-оверлей ни при чём |
| carveout'ы выровнены по стоку (ADSP 0x88d00000/40 МБ, camera/npu/video/cdsp, + gpu_region) | без изменений |
| узел `memory` вместо `memory@80000000` | без изменений |
| cmdline `clk_ignore_unused pd_ignore_unused regulator_ignore_unused ignore_console_null panic=10` | без изменений |
| `pmbootstrap bootimg_analyze` на стоковом boot.img | все offsets совпали с моими; выдал `append_dtb="false"` (у меня стояло true) |

### ЕДИНСТВЕННОЕ, что изменило поведение: CONFIG_EFI_ZBOOT
Конфиг pmaports `config-postmarketos-qcom-sm6350` содержит `CONFIG_EFI_ZBOOT=y`.
После сборки с `fp4_defconfig` (EFI zboot нет) ABL **впервые начал печатать
`Set warranty bit: kernel`** — раньше было только `: vbmeta`.
→ С EFI-ядром ABL до стадии проверки ядра вообще не доходил. Направление верное.

### Полезное из порта Samsung Galaxy Tab S9 Ultra
`github.com/agcarbajo/postmarketos-galaxy-tab-s9-ultra` — `docs/boot-strategy.md`,
`docs/development-notes.md`, `scripts/`, `configs/dtbo/*-noop.dts`. Оттуда:
- ABL **дописывает `console=null`** в cmdline → нужен `ignore_console_null`;
- стоковый dtbo не применяется на mainline DTB, нужны **no-op оверлеи с сохранением
  селекторов Samsung** (их `configs/dtbo/gts9uwifi-board0*-noop.dts` — образец);
- их рабочий cmdline: `console=tty0 console=ttyMSM0,115200n8 ignore_console_null earlycon
  loglevel=7 log_buf_len=4M panic=10 clk_ignore_unused pd_ignore_unused
  regulator_ignore_unused rootwait pmos.nosplash`;
- режимы отказа ABL: нет селекторов → "No match found for Soc Dtb type" → Odin;
  нет `__symbols__` → "ufdt apply overlay failed" → Odin;
  не те carveouts → ядро стартует, но TZ даёт `TZBSP_ERR_FATAL_NOC_ERROR` → reset до userspace.

### Отправлен вопрос автору порта
`gh pr comment 10 --repo sm6350-mainline/linux` →
https://github.com/sm6350-mainline/linux/pull/10#issuecomment-5966248409
Спрошено: чем прошивает, какой defconfig, gzip или raw Image, нужны ли правки carveouts.
Отмечено расхождение: его DTS резервирует ADSP `0x84800000`/64 МБ, стоковый DTB
нашего SM-T735 — `0x88d00000`/40 МБ.

### Текущая гипотеза в работе
Ядро собиралось системным **gcc 16.1**, апстрим собирает **clang (`LLVM=1`)**.
Собирается `linux-clang/` тем же тулчейном. На Arch понадобилось доставить
`clang llvm` и починить рассинхрон: `llvm`/`clang` 23.1.1 против `llvm-libs` 22.1.8
(`libLLVM.so.23.1 not found`) → `pacman -S llvm-libs lld`.

---

## ПРОРЫВ: ядро грузится ТОЛЬКО из раздела recovery (2026-10-03, 12:16)

### Главный вывод
**Samsung ABL на gts7xllite отказывается запускать кастомное ядро из `boot`,
но запускает его из `recovery`.**

Идея Mark'а. Признак, который стоило заметить сразу: TWRP — это тоже неподписанное
кастомное ядро, и он всё это время нормально грузился именно из `recovery`.

Это объясняет ВСЁ поведение предыдущих суток: девять итераций boot.img были обречены
независимо от содержимого. Формат, компилятор, конфиг, адреса, DTB, dtbo, carveouts —
ни один из факторов не был причиной. `Set warranty bit: kernel` означал лишь, что ABL
видел наше ядро в `boot` и помечал устройство, но запускать отказывался.

### Как грузиться
1. Записать boot.img в раздел **recovery** (не boot!).
2. Включение: **Vol+ и Power**, после логотипа отпустить Power, Vol+ держать.
3. `boot` можно оставить стоковым.

При повторной прошивке recovery через Odin первый раз может прийти `FAIL! Auth` —
повторить, со второго раза проходит чисто.

### Образ, который загрузился
`out/boot-final.img` (он же `out/flashrec/recovery.img`):
- ядро `linux-pmos/arch/arm64/boot/Image.gz` — конфиг pmaports
  `config-postmarketos-qcom-sm6350` с **`# CONFIG_EFI_ZBOOT is not set`**, clang 23.1.1
- DTB приклеен к ядру (`cat Image.gz dtb`) И продублирован в поле заголовка
- `--base 0x80000000` (как в портах Tab S9 Ultra и Tab A2 XL), kernel_offset 0x8000,
  ramdisk 0x02000000, tags 0x01e00000, dtb 0x01f00000, pagesize 4096, header v2
- cmdline: `console=tty0 ignore_loglevel loglevel=8 fbcon=nodefer pmos_boot_uuid=... pmos_root_uuid=... pmos_rootfsopts=defaults`
- хвост `SEANDROIDENFORCE`, os_version 11.0.0, patch 2025-06, board SRPUB23B009
- DTS авторский, без правок
- в dtbo — noop-оверлеи (записаны из TWRP)
- vbmeta — `avbtool --flags 2`

TODO(verify): какие из этих параметров реально обязательны, а какие остались от
перебора. Проверять по одному теперь дёшево — прошивка recovery идёт за секунды.

### ВАЖНО: TWRP затёрт
В разделе recovery теперь наше ядро. Восстановление TWRP:
```
script -qefc "tools/samloader/samloader flash --no-reboot -a tools/twrp/recovery.tar.md5" /dev/null
```

---

## ЯДРО ЗАГРУЗИЛОСЬ, systemd стартовал (2026-10-03, 12:22)

### Что видно в логе на экране (фото)
Загрузка дошла далеко:
- `Reached target Local Encrypted Volumes`
- `Started Show Plymouth Boot Screen`
- `Mounted Temporary Directory /tmp`
- **`Reached target Hardware activated USB gadget`** ← гаджет поднялся
- `Listening on Disk Image Download Service Socket`
- `Finished Load Kernel Module fuse`, `Finished Virtual Console Setup`
- живые `qcom-sm6350` clock-controller, `qcom-rpmhpd`, interconnect sync_state

→ **UFS работает, rootfs найден по UUID и смонтирован, SoC инициализирован,
systemd работает.**

### ЧТО ДЕРЖИТ ЗАГРУЗКУ
```
A start job is running for /dev/disk/by-uuid/70c666eb-7f7c-43ca-96b7-5f81b535e84a
```
Это UUID **boot-раздела**. `pmbootstrap install --split` прописал его в `/etc/fstab`
rootfs, но физически такого раздела нет: ядро лежит в Android-образе внутри `recovery`,
а ext2-образ `/boot` никуда не писался. systemd ждёт его и до сети не доходит.

**Обход:** `systemd.mask=boot.mount` в cmdline.
Долгосрочно — либо убрать строку `/boot` из `/etc/fstab` в rootfs, либо писать
ext2 boot-образ в отдельный раздел.

### Вторая проблема (не блокирующая)
Много красных `BPF: Invalid name_offset ... failed to validate module BTF: -22`.
Причина: модули в rootfs от пакета `linux-samsung-gts7xllite` (собран с EFI_ZBOOT),
а ядро собрано вручную в `linux-pmos/` — BTF не совпадает, модули не грузятся.
Исправляется пересборкой пакета ядра с текущим конфигом и переустановкой rootfs.

### USB
`dr_mode = "otg"` в авторском DTS → автоопределение роли не работает
(автор прямо писал об этом в PR #9: "no autodetect though, requires dts change").
Заменено на `dr_mode = "peripheral"` в `linux-pmos/.../sm7225-samsung-gts7xllite.dts`.
UDC устройства: `a600000.dwc3` (из стокового cmdline).

### Текущий рабочий cmdline
```
console=tty0 ignore_loglevel loglevel=8 fbcon=nodefer systemd.mask=boot.mount
pmos.nosplash pmos_root_uuid=c1aca5ea-57e1-43a3-9709-1305c9102c54 pmos_rootfsopts=defaults
```
`pmos_boot_uuid` убран намеренно.

### Следующие шаги
1. Прошить `out/boot-noboot.tar` в **recovery** → ожидаем доход до сети 172.16.42.1 и SSH
   (пользователь `mark`, пароль `147147`).
2. Пересобрать пакет ядра `linux-samsung-gts7xllite` с конфигом без EFI_ZBOOT +
   `dr_mode=peripheral` + правкой deviceinfo (`append_dtb="false"`), переустановить rootfs
   → уйдут ошибки BTF, заработают модули.
3. Убрать `/boot` из fstab вместо маскировки.
4. Затем по приоритетам: GPU (Adreno 619 + a615_zap из apnhlos), панель HX83121
   (генератор драйверов `z3ntu/linux-mdss-dsi-panel-driver-generator`), тач.

---

## ✅ PLASMA DESKTOP ЗАПУСТИЛСЯ (2026-10-03, ~12:35)

**Nura (postmarketOS) с Plasma Desktop работает на SM-T735.**

Что подтверждено рабочим:
- mainline-ядро 7.2.0 (sm6350-mainline, ветка `spen`), собрано clang 23.1.1
- UFS, rootfs в userdata найден по UUID и смонтирован
- systemd, графический вход Plasma
- экран через `simple-framebuffer` (1600x2560, портрет)
- SoC: клоки, RPMh, interconnect, power domains

Не работает:
- **тачскрин** — узла в DTS нет вообще, автор порта его не добавлял
- USB-сеть — в момент запуска Plasma устройство на шине не появилось, выясняется

### Рецепт загрузки (ИТОГОВЫЙ)
1. Образ пишется в раздел **recovery**, НЕ в boot.
2. Включение: **Vol+ и Power**, после логотипа отпустить Power, Vol+ держать.
3. `boot` остаётся стоковым, `vbmeta` — с `--flags 2`, `dtbo` — noop-оверлеи.

Файл образа: `out/boot-noboot.img` (= `out/flashnb/recovery.img`).
Сборка: `linux-pmos/arch/arm64/boot/Image.gz` + DTB приклеен и продублирован в заголовке,
`--base 0x80000000`, header v2, pagesize 4096, `SEANDROIDENFORCE`.

cmdline:
```
console=tty0 ignore_loglevel loglevel=8 fbcon=nodefer systemd.mask=boot.mount
pmos.nosplash pmos_root_uuid=c1aca5ea-57e1-43a3-9709-1305c9102c54 pmos_rootfsopts=defaults
```

### Следующие задачи по приоритету Mark
1. USB-сеть и SSH — без них всё остальное отлаживать тяжело.
2. Пересборка пакета ядра (без EFI_ZBOOT, `dr_mode=peripheral`) → уйдут ошибки BTF,
   заработают модули.
3. **Тачскрин**: Himax HX83121A. В ядре есть `drivers/input/touchscreen/himax_hx83112b.c`
   с поддержкой HX83112A (добавлена для FP4) — расширить до HX83121A.
   Параметры брать из `downstream/stock-dtbo2-board05.dts`
   (fw `tsp_himax/hx83121a_gts7xllite.bin`).
4. **GPU** Adreno 619: снять `status = "disabled"` с `&gpu`, достать `a615_zap.mbn`
   из раздела `apnhlos` самого устройства (как это делает порт T733 для a660_zap).
5. **Панель** HX83121/PPC357DB11: драйвер генерируется из стокового DTB утилитой
   `z3ntu/linux-mdss-dsi-panel-driver-generator` — писать с нуля не нужно.

---

## Сессия 2 — причина отсутствия USB найдена (2026-10-03, 16:00)

### Диагноз подтверждён прямой проверкой
`CONFIG_USB_CONFIGFS=m`, `USB_LIBCOMPOSITE=m`, `USB_F_RNDIS=m`, `USB_F_NCM=m` —
весь стек гаджета был **модульным**. Модули лежат в initramfs
(`usr/lib/modules/7.2.0/kernel/drivers/usb/gadget/*.ko.zst`, всего 14 модулей) и собраны
пакетом `linux-samsung-gts7xllite` с `CONFIG_EFI_ZBOOT=y`, а работает ядро из `linux-pmos/`,
собранное вручную → vermagic/BTF не сходятся, `modprobe` молча падает
(в `init_functions.sh` стоит `modprobe libcomposite 2>/dev/null || true`).

Отсюда и противоречие в логе: `Reached target Hardware activated USB gadget` есть,
а устройства на шине нет. Цепочка: нет `libcomposite` → нет `/sys/kernel/config/usb_gadget`
→ `setup_usb_network_configfs` выходит с "usb_gadget not found in configfs, skipping".

Подтверждено также, что `dr_mode = "peripheral"` реально попал в прошитый
`out/boot-noboot.img` (DTB извлечён из образа, свойство на месте) — то есть дело было
не в DTS.

### ВТОРАЯ находка, объясняет неработающий ввод целиком
`CONFIG_INPUT_EVDEV=m` → evdev тоже модуль и тоже не грузится.
**У Plasma нет ни одного устройства ввода вообще**, не только тача. То есть
«тач не работает» — это не только отсутствие узла в DTS, но и отсутствие evdev.

### Решение (быстрый путь вместо пересборки пакета)
Вместо пересборки пакета ядра и перезаливки userdata (3,5 ГБ) — собрать нужное
**внутрь ядра**, тогда модули из rootfs не нужны, а раздел recovery пишется за секунды:
```
USB_CONFIGFS=y  USB_CONFIGFS_{ECM,ECM_SUBSET,EEM,SERIAL,RNDIS,NCM,ACM}=y
INPUT_EVDEV=y   INPUT_UINPUT=y
→ USB_LIBCOMPOSITE=y USB_F_RNDIS=y USB_F_NCM=y USB_F_ECM=y USB_U_ETHER=y USB_U_SERIAL=y
```
Уже встроены и потому работали: `SCSI_UFS_QCOM`, `MMC_SDHCI_MSM`, `DRM_SIMPLEDRM`, `EXT4_FS`.

Долгосрочно пакет всё равно пересобрать (конфиг пакета содержит `CONFIG_EFI_ZBOOT=y`) —
иначе модули rootfs останутся мёртвыми (Wi-Fi, BT, звук — тоже модули).

### Инфраструктура
- **`tools/mk-recovery.sh`** — сборка образа теперь скриптом, параметры заголовка
  восстановлены из рабочего образа через `unpack_bootimg`:
  base `0x80000000`, kernel_offset `0x8000`, ramdisk `0x02000000`, tags `0x01e00000`,
  dtb `0x01f00000`, pagesize 4096, header v2, os_version 11.0.0, patch 2025-06,
  board `SRPUB23B009`, хвост `SEANDROIDENFORCE`. Ramdisk сохранён в `out/ramdisk.cpio.gz`.
- `pmbootstrap bootimg_analyze` требует sudo и падает без него — использовать
  `unpack_bootimg` напрямую, он sudo не требует.
- У Mark **нет OTG-адаптера** → путь через USB-host (клавиатура в планшет) закрыт,
  работаем только через гаджет.
- `dmesg` требует sudo (`kernel.dmesg_restrict=1`).

### ✅ РЕЗУЛЬТАТ: USB-сеть и SSH РАБОТАЮТ (2026-10-03, 16:14) — этап 2 закрыт
Прошит `out/usbfix.img` в recovery (`RECOVERY flash successful`, 25,57 МБ за 1 с,
без `FAIL! Auth`). **ModemManager останавливать не потребовалось** — прошивка прошла
при работающем сервисе (Mark попросил его не трогать).

Ядро `7.2.0 #2 SMP PREEMPT Sat Oct 3 16:08:23`.

- USB-гаджет поднялся: на ноутбуке интерфейс `enp0s20f0u5`, адрес `172.16.42.2/24`,
  планшет `172.16.42.1`, ping 2,7 мс.
- **Гаджет представляется как `18d1:d001` (Google Nexus 4), НЕ Samsung `04e8`** —
  искать на шине по `04e8` бессмысленно, это важно для будущих наблюдателей.
- SSH: `mark` / `147147`, порт 22. **Ключ `~/.ssh/id_ed25519.pub` установлен**,
  дальше вход без пароля. telnet 23 закрыт (debug-shell не активен).
- UDC: `a600000.usb`, гаджет `g1` в configfs.
- `/dev/input/event0..2`: `pm8941_pwrkey`, `pm8941_resin`, `gpio-keys` (Vol+).
  Тача нет — узла в DTS не существует.
- DRM: `card0` (simpledrm), `/dev/fb0`.

Диагноз подтверждён логом ядра: `failed to validate module [evdev] BTF: -22` —
модуль из rootfs отвергнут, но ввод работает, т.к. evdev теперь встроен.
`lsmod` пуст: **ни один модуль rootfs не грузится**.

### Прочие ошибки из dmesg (на потом, не блокируют)
```
psci: [Firmware Bug]: failed to set PC mode: -3
spmi-0: failed to xlate sid=0x0 periph=0x62 irq=1 rc=-19
rtc-pm8xxx ...rtc@6100: error -ENXIO: IRQ index 0 not found
qcom-pon ...pon@1300: kobject_add_internal failed, -EEXIST → probe -17
qcom-spmi-adc5 ...adc@3100: no channels defined → -22   (нужны каналы из стокового DTB)
qcom,spmi-sdam nvram@{7c00,7d00,8400,8500,8600,9800,9d00}: SDAM_SIZE rc=-19
aux_bridge.0: error -ENODEV: failed to acquire drm_bridge
```

### Следующий шаг (план)
Теперь, когда есть SSH, пакет ядра можно не вшивать в rootfs заново: пересобрать
`linux-samsung-gts7xllite` с нашим конфигом и **поставить .apk прямо на устройство
по scp + apk add** — перезаливка userdata (3,5 ГБ) не нужна. Тогда модули и BTF
совпадут и оживут Wi-Fi, BT, звук.

## Сессия 2, часть 2 — ПРОРЫВ по панели и тачу (2026-10-03, 16:30)

### Тач: HX83121A на SPI, НЕ на I2C
Из стокового dtbo (`downstream/stock-dtbo2-board05.dts`, fragment@58):
- шина **`qupv3_se6_spi`**, `spi-max-frequency = <0x6acfc0>` = **7 МГц**, `num-cs = <1>`
- **CS**: `tlmm` GPIO **16**
- **IRQ**: `tlmm` GPIO **22** (`himax,irq-gpio`)
- **RESET**: `tlmm` GPIO **18** (`himax,rst-gpio`)
- vendor_check: `tlmm` GPIO **23**, `vendor_check_enable_value = <0>`
- `compatible = "himax,hxcommon"`, `report_type = <1>`
- `himax,panel-coords`/`display-coords` = `<0x640 0xa00 0x4b0 0x7d0>` → **1600 x 2560**
- firmware: `tsp_himax/hx83121a_gts7xllite.bin`
- питание по именам: `panel_buck_en`, `panel_buck_en2`, `panel_ldo_en`, `panel_reset`
  → общее с панелью (TDDI)
- `himax,select_lcdid = <0x87f230 0x13d230>`

Mainline `drivers/input/touchscreen/himax_hx83112b.c` — **только I2C**
(`module_i2c_driver`), чипы hx83100a/112a/112b. Для нас не годится напрямую,
хотя регистры там уже через regmap.

### НАЙДЕН готовый SPI-драйвер HX83121A
`github.com/chiyuki0325/EGoTouchRev-Linux` → `touchscreen-hx83121a-dkms/`:
`himax-spi-core.c` (1478 строк) + `hx-algo.c` (1133) + `hx-algo.h` (191),
автор Kirikaze Chiyuki. Форки: `KawaiiHachimi/linux-gaokun-buildbot`
(`drivers/touchscreen-hx83121a/`), патчи в `vahiru/gaokun-android/patches/004*`.
- `compatible = "himax,hx83121a-ts"` и `"himax,hx83121a"`
- `module_spi_driver`, `spi->mode = SPI_MODE_3`
- GPIO только один: `devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH)`
- устройство-источник: Huawei Matebook E Go, SPI6 @ 12 МГц, IRQ GPIO175, rst GPIO99

### КРИТИЧНО: тач НЕВОЗМОЖЕН до панели
Драйвер — **drm panel follower**: в probe вызывается
`devm_drm_panel_add_follower()`, которая через `find_panel_by_dev()` ищет phandle
`panel` в DT; без зарегистрированной панели probe возвращает ошибку и выходит.
Включение/питание/ресет тача живут в коллбэках `himax_panel_prepared` /
`himax_panel_enabled`. Это и есть TDDI: тач и дисплей — один кристалл.
→ **Порядок работ: dispcc + DSI + панель, затем тач.** Обойти нельзя.

### Панель: драйвер УЖЕ В MAINLINE и уже в нашем дереве
`drivers/gpu/drm/panel/panel-himax-hx83121a.c` (`CONFIG_DRM_PANEL_HIMAX_HX83121A`),
автор Pengyu Luo, добавлен для Huawei Matebook E Go:
- compatible: **`boe,ppc357db1-4`**, **`csot,ppc357db1-4`**
- наша стоковая панель: `ss_dsi_panel_HX83121_PPC357DB11_WQXGA` → то же семейство PPC357DB1
- **dual-DSI** (`is_dual_dsi`), 1600x2560 (`hdisplay = 800*2`), 266x166 мм (12,4")
- режимы 60 и 120 Гц, опциональный DSC (параметр модуля `enable_dsc`)
- регуляторы: `vddi`, `avdd`, `avee`
TODO(verify): сверить init-последовательность драйвера со стоковой из
`ss_dsi_panel_HX83121_PPC357DB11_WQXGA` в dtbo.

### GPU: zap-шейдер найден на устройстве
Раздел `apnhlos` смонтирован по SSH только для чтения:
`/mnt/apn/image/a615_zap.{mdt,b00,b01,b02}` — собирается в `.mbn` конкатенацией
mdt+b0x (как это делает порт T733 для a660_zap).

## ❗ ПОВОРОТ: у Mark панель FT8203, а НЕ Himax (2026-10-03, 17:00)

### Подтверждено двумя независимыми способами
1. **GPIO 23 (`vendor_check`) = 1.** Прочитан на живом устройстве через libgpiod
   (`gpioget --bias=disabled --chip gpiochip2 23`, три чтения подряд).
   В стоке: `himax,vendor_check_enable_value = <0>`,
   `focaltech,vendor_check_enable_value = <1>` → совпадает с Focaltech.
   У обоих вариантов pinctrl идентичен (`input-enable; bias-disable;`),
   значит уровень задаёт перемычка на плате.
2. **Сам ABL пишет это в cmdline** (`/proc/cmdline` на устройстве):
   ```
   msm_drm.dsi_display0=ss_dsi_panel_FT8203_TS124QDM_WQXGA:
   lcd_id=0x876270
   ```
   `lcd_id=0x876270` НЕ совпадает с `himax,select_lcdid = <0x87f230 0x13d230>`.

→ Работа по CSOT PPC357DB1-1 (дескриптор в `panel-himax-hx83121a.c` + узел в DTS)
**откачена**, она для другого экземпляра устройства.
Драйвер `panel-himax-hx83121a.c` и его комментарий «CSOT PPC357DB1-1: on SAMSUNG
Galaxy Tab S7 FE» — про ревизии с панелью Himax; такие экземпляры существуют,
но у Mark не такой.

### Параметры НАСТОЯЩЕЙ панели: FT8203 / TS124QDM WQXGA
Источник: `downstream/stock-dtbo2-board05.dts`, узел
`ss_dsi_panel_FT8203_TS124QDM_WQXGA` (строка 7004).
- 1600x2560, bpp 24, **video mode**, 60 Гц, 4 линии, одиночный DSI
- тайминги: h-pulse **8**, h-back **89**, h-front **90** → htotal **1787**
            v-pulse **8**, v-back **13**, v-front **16** → vtotal **2597**
- пиксельклок ≈ **278,45 МГц**; `panel-clockrate` 676 МГц;
  phy-timings `<0x180606 0x21200606 0x7020400>`
- **DSC** v1.1: slice **800x40**, slice-per-pkt **2**, 8 bpc, **8 bpp**
- физический размер 166 x 266 мм; подсветка DCS, max **425**
- reset-последовательность: `<1 5 0 4 1 12>` (high 5 мс, low 4 мс, high 12 мс)
- `qcom,mdss-dsi-on-command` — длинная init-последовательность, есть в DTS целиком
- off: `0x28` (display off) + `0x10` (sleep in), состояние команд `dsi_lp_mode`
- reset GPIO: **tlmm 75** (`qcom,platform-reset-gpio = <&tlmm 0x4b>`, строка 5779)
- питание — те же рейлы, что у Himax-варианта (см. notes/hardware.md)

**Mainline-драйвера для FT8203 нет.** Делать через
`z3ntu/linux-mdss-dsi-panel-driver-generator`: он принимает ровно такой
Qualcomm-MDSS-узел и генерирует драйвер панели.

### Тач на этом экземпляре — тоже Focaltech, не Himax
Узел `focaltech@1` (fragment@43, строка ~5931), шина SPI (`cs-gpios`):
- IRQ **tlmm 22** (`<0x16 0x02>`), RESET **tlmm 18** (`<0x12 0x01>`)
- `focaltech,max-touch-number = <10>`, координаты 1600x2560
- firmware `tsp_focaltech/ft8203_gts7xllite.bin`
- питание общее с панелью → снова TDDI, снова панель первой
В mainline есть `edt-ft5x06` (другой чип, I2C). Для FT8203 драйвера нет.

### Инфраструктура
- **Планшету дан интернет** через NAT на ноутбуке:
  `sysctl net.ipv4.ip_forward=1` + `iptables -t nat -A POSTROUTING -s 172.16.42.0/24
  -o wlp0s20f3 -j MASQUERADE` + правила FORWARD; на устройстве
  `ip route add default via 172.16.42.2` и `nameserver 1.1.1.1`.
  Проверено: ping 1.1.1.1 — 29 мс. `apk add` работает, поставлен `libgpiod`.
  TODO: сделать это постоянным (на устройстве маршрут и resolv.conf не переживут ребут).
- `gpiochip2` = `f100000.pinctrl` = tlmm (157 линий). GPIO-выходы панели (72/73/88/89)
  **не читать** через gpioget — запрос переключит пин во вход и снимет питание.
- Связь по USB один раз пропадала (ARP INCOMPLETE), вернулась после переподключения
  устройства — номер на шине сменился 006 → 007.

## План починки ошибок загрузки (составлен 2026-10-03, 17:20)

Источник — dmesg с живого устройства (ядро 7.2.0 #2). Отсортировано по важности.

### A. СРОЧНО: устройство самопроизвольно переподключается/перезагружается
Дважды за сессию связь по USB пропадала (`ARP INCOMPLETE`) и возвращалась с
**новым MAC гаджета** и новым номером на шине (006 → 007 → …). Новый MAC означает
переинициализацию гаджета, т.е. перезагрузку или пересоздание configfs.
Гипотезы: уход в suspend (на родственном T733 suspend ломал загрузку),
watchdog, либо падение.
Проверка: после восстановления связи сразу читать `/sys/fs/pstore/` (ramoops
настроен на `0xb1b00000`) и `journalctl -b -1`. Зафиксировать `uptime` в момент связи.
Это блокирует отладку панели — без стабильного SSH прошивать KMS-ядро рискованно.

### B. Батарея и зарядка — ADC не стартует
```
qcom-spmi-adc5 c440000.spmi:pmic@0:adc@3100: error -EINVAL: no channels defined
qcom-spmi-adc5 ...: probe with driver qcom-spmi-adc5 failed with error -22
```
Узел ADC включён, но без описания каналов. Нужны каналы из стокового DTB
(`downstream/stock-boot.dts`, узел `adc@3100` у pmic@0) — без ADC не будет ни
напряжения батареи, ни температур. **Это предпосылка для этапа «батарея».**

### C. Конфликт имён PON
```
kobject: kobject_add_internal failed for qcom-pon with -EEXIST
qcom-pon c440000.spmi:pmic@0:pon@1300: probe with driver qcom-pon failed with error -17
```
У pm6350 два узла PON: `pon@800` (от него работают pwrkey и resin) и `pon@1300`.
Второй регистрируется под тем же именем и падает. Починка: отключить `pon@1300`
в нашем DTS (`status = "disabled"`), т.к. используется `pon@800`.
TODO(verify): сверить по pm6350.dtsi, какой из них отвечает за что.

### D. RTC без прерывания
```
spmi spmi-0: failed to xlate sid = 0x0, periph = 0x62, irq = 1 rc = -19
rtc-pm8xxx c440000.spmi:pmic@0:rtc@6100: error -ENXIO: IRQ index 0 not found
```
Прерывание RTC у pm6350 недоступно (на Qualcomm этот периферийный блок часто
принадлежит TZ/ADSP). Следствие — alarm не работает. Починка: либо отключить
`rtc@6100` у pm6350 (мы используем RTC из pmk8350), либо убрать у него
`interrupts`/`interrupt-names`, оставив только чтение времени.

### E. SDAM недоступен (7 узлов)
```
qcom,spmi-sdam nvram@{7c00,7d00,8400,8500,8600,9800,9d00}: Failed to read SDAM_SIZE rc=-19
```
Те же права доступа: регионы SDAM принадлежат другому исполнителю.
Починка: отключить неиспользуемые узлы SDAM в DTS. Косметика, но лог чище и
загрузка быстрее.

### F. aux_bridge
```
aux_bridge.aux_bridge.0: error -ENODEV: failed to acquire drm_bridge
```
Относится к DP-alt-mode по USB-C. Сейчас не нужен; станет актуален, если захотим
вывод на внешний монитор. Отложить.

### G. psci (не починить, не мешает)
```
psci: [Firmware Bug]: failed to set PC mode: -3
```
Прошивка не поддерживает PSCI OSI; ядро откатывается на PC mode. Косметика.

### H. Модули и BTF — уйдёт само
```
libcomposite: module verification failed: signature and/or required key missing
failed to validate module [evdev] BTF: -22
```
Закроется установкой пересобранного пакета ядра (шаг 3 плана): сейчас модули в
rootfs от старой сборки. Функциональных последствий уже нет — и гаджет, и evdev
встроены в ядро.

## Сессия 2, часть 3 — настройка устройства по SSH (2026-10-03, 17:30)

### Корень обрывов USB-связи НАЙДЕН
Связь пропадала не случайно: адрес `172.16.42.1` выставляет initramfs, а затем
**NetworkManager в Plasma забирает интерфейс `usb0` себе и снимает адрес**.
Отсюда схема «сразу после загрузки SSH есть, через пару минут нет», а также
пустой ARP при живом устройстве на шине.
Планшет при этом был полностью жив — на экране висел логин Plasma.

Починка (применена на устройстве):
- `/etc/NetworkManager/conf.d/99-usb-gadget.conf` →
  `[keyfile] unmanaged-devices=interface-name:usb0;interface-name:rndis0`
- служба `/etc/systemd/system/usb-gadget-net.service` (enabled), поднимающая
  адрес при каждой загрузке независимо от NM.
Скрипты сохранены в **`tools/device/dev-setup.sh`** и **`tools/device/run-on-device.sh`**.

### ВАЖНО: маска подсети /16, а не /24
Initramfs назначает `usb0` адрес **`172.16.42.1/16`**. Я прописал в службе `/24`,
и `ip addr replace` с тем же адресом но другой маской заменил адрес и снёс маршруты
→ **сессия SSH порвалась ровно в момент `apk add`** (`Connection reset by peer`).
Это и было причиной «зависаний sshd», а не нехватка памяти (ошибочная гипотеза:
ICMP шёл, новые подключения не принимались — я решил, что OOM).
В `tools/device/dev-setup.sh` исправлено на `/16`.

### Сделано на устройстве
- NetworkManager больше не трогает `usb0`, служба адреса включена.
- `/boot` закомментирован в `/etc/fstab` (бэкап `/etc/fstab.bak`) — можно убирать
  костыль `systemd.mask=boot.mount` из cmdline при следующей сборке образа.
- Пакет `linux-samsung-gts7xllite-7.2.0-r1.apk` лежит в **`/root`** (переживает
  перезагрузку, в отличие от `/tmp` — tmpfs). Установка оборвалась, Mark доделывает
  вручную.

### Ramoops пуст — падений не было
`/sys/fs/pstore` пуст, все перезагрузки за сессию делал Mark.
Гипотезу «устройство самопроизвольно перезагружается» снять.

### Грабли инструментов (для будущих сессий)
- **Фоновый запуск Bash схлопывает переводы строк** в команде: heredoc внутри
  `run_in_background` превращается в одну строку, а вложенные кавычки ломаются так,
  что `ssh host 'cmd'` выполняется как `ssh host` + локальный пайп. Дважды из-за
  этого `apk add` запускался НА НОУТБУКЕ (безвредно: на Arch нет apk).
  → Писать скрипт файлом (Write), запускать одной строкой.
- Вывод через `| grep` в SSH буферизуется: прогресса не видно до конца команды.
- У планшета сильно отстают часы — RTC не работает (см. план починки, пункт D).

## ✅ ПАНЕЛЬ ПОДНЯЛАСЬ ЧЕРЕЗ KMS (2026-10-03, 17:55)

Ядро `7.2.0 #4`, образ `out/panel4.img`:
```
/sys/class/drm/card1/card1-DSI-1: connected / enabled / 1600x2560
/dev/fb0 -> msmdrmfb          (настоящий KMS, не simple-framebuffer)
vreg_l14a: enabled 1896000    panel_buck_en/buck_en2/boost_en: enabled
```
Драйвер `panel-ft8203-ts124qdm-wqxga.c`, сгенерированный из стокового DTB
генератором z3ntu, с дописанным вручную DSC и `prepare_prev_first`.

**Состояние: панель светится, подсветка работает, картинки нет.**

### ГРАБЛИ: напряжение RPMh должно попадать в сетку
Я зафиксировал `vreg_l14a` на `1900000` (сток требует 1,9 В) — и **убил загрузку целиком**.
У `pmic5_pldo` в `drivers/regulator/qcom-rpmh-regulator.c`:
```
REGULATOR_LINEAR_RANGE(1504000, 0, 255, 8000)   // база 1,504 В, шаг 8 мВ
(1900000 - 1504000) / 8000 = 49,5  → НЕ в сетке
```
Регистрация одного регулятора падает → **не поднимается весь узел pm6350** →
`1d84000.ufshc`, `88e8000.phy`, `1d87000.phy`, `88e3000.phy`, `a600000.usb`,
`ae00000.display-subsystem` навсегда повисают в `deferred probe pending:
wait for supplier /soc@0/rsc@18200000/regulators-0/ldoN`.
Симптом на экране — `[pmOS-rd]: ERROR: failed to mount subpartitions!`, потому что
диска физически нет. Я сначала ошибочно решил, что это таймаут поиска корня.
Исправлено: `1896000`–`1904000` (49 и 50 шагов).
**Правило: любое напряжение RPMh сверять с сеткой нужного типа LDO.**

### Диагностика по тактированию (всё сошлось со стоком)
```
dsi0_pll_bit_clk            674 076 416 Гц  (сток: 676 000 000)
dsi0_phy_pll_out_byteclk     84 259 552 Гц
disp_cc_mdss_pclk0_clk      112 346 070 Гц  = bit_clk * 4 линии / 24 bpp
```

### Текущая гипотеза: нужен BURST-режим
`dsi_err_worker: status=4` и `5` — маска ошибок msm: бит `0x4` = DLN0 PHY error,
`0x1` = ACK error. То есть сигнализация на линиях данных неверная.
Доводы за burst:
- в стоковом узле заданы `qcom,mdss-dsi-bllp-power-mode` и `bllp-eof-power-mode`,
  но НЕ заданы `hfp/hbp/hsa-power-mode` — это подпись burst;
- минимум для потока с DSC 8 bpp: 278,45 Мпикс × 8 / 4 линии = **557 Мбит/с на линию**,
  а линк работает на **676** — в не-burst режимах тактовая равна ровно пиксельной
  скорости, избыток бывает только в burst.
`qcom,mdss-dsi-traffic-mode` в Samsung-узле отсутствует, моё прежнее значение
`non_burst_sync_event` было помечено TODO(verify) и оказалось неверным.
Правка: `MIPI_DSI_MODE_VIDEO_BURST` в `mode_flags`.

### Также замечено
- WARN из `msm_atomic_commit_tail` при первом `drm_fb_helper_set_par` (не фатально).
- `card1-Writeback-1` присутствует и disabled — норма.

## Отладка панели FT8203 — где остановились (2026-10-03, 19:02)

### ✅ Достигнуто
- **Панель даёт картинку.** KMS работает: `card1-DSI-1 connected/enabled 1600x2560`,
  `/dev/fb0 = msmdrmfb`. Драйвер `panel-ft8203-ts124qdm-wqxga.c` (из генератора z3ntu
  + ручные правки) управляет панелью.
- Система грузится, UFS, rootfs, USB-сеть и SSH работают.
- Пакет ядра `7.2.0-r1` установлен на устройство (лежит в `/root`).
- USB-сеть сделана постоянной (NM не трогает `usb0` + служба `usb-gadget-net`).
- `/boot` убран из fstab.

### Матрица экспериментов (ядро одно, различия только в cmdline)
Параметры модуля: `panel_ft8203_ts124qdm_wqxga.no_dsc=0|1` и `.traffic=0|1|2`
(0 = sync event, 1 = sync pulse, 2 = burst).

| образ | DSC | traffic | результат |
|---|---|---|---|
| `panel4` | да | event | панель светится, картинки нет; `wait for video done timed out`, `dsi_err_worker status=4/5` |
| `panel5`/`panel6` | да | burst | светится ярче, картинки нет; те же ошибки; PPS добавлен — не помогло |
| `panel7nodsc` | нет | burst | **картинка есть**, но сильное мерцание и цветной шум; **ошибок DSI ноль** |
| `nodsc-event` | нет | event | картинка с тяжёлыми артефактами; **ошибок DSI ноль** |
| `nodsc2-*` | нет | burst/event | собраны, **не проверены** — добавлено явное `mipi_dsi_compression_mode(dsi, false)` |

### Ключевые наблюдения
- **С DSC кадры не идут вообще**: `wait for video done` не срабатывает ни разу.
  Значит проблема на стороне DPU/DSI, а не в параметрах линка.
  Частоты при этом верные: bit_clk 674 МГц (сток 676), pclk 112,346 МГц,
  `new_htotal = 721` — msm считает всё правильно.
- **Без DSC линк абсолютно чистый**: ноль ошибок, ноль underrun, кадры завершаются,
  bit_clk = 1 670 700 000 (1,67 Гбит/с на линию, PHY 10нм держит до ~2,5),
  pclk = 278 450 000 — ровно наш режим. Но картинка искажена.
  → Вывод: DDIC настроен стоковой init-последовательностью на приём **сжатого**
  потока и декодирует несжатые пиксели как мусор. Отсюда последняя правка —
  явно слать `mipi_dsi_compression_mode(dsi, false)` в режиме `no_dsc=1`.

### Что сделано в драйвере сверх генератора
1. `prepare_prev_first = true` — иначе init-последовательность уходит до поднятия хоста.
2. DSC-конфиг (1.1, slice 800x40, slice_count 2, 8 bpc, 8 bpp, block_pred) + `dsi->dsc`.
3. **Отправка PPS**: `drm_dsc_pps_payload_pack` + `mipi_dsi_picture_parameter_set` +
   `mipi_dsi_compression_mode(true)`. Нужна, т.к. в стоке `samsung,no_qcom_pps`
   (контроллер PPS не генерирует). Генератор этого не умеет —
   см. PR #25 к `linux-mdss-dsi-panel-driver-generator`.
4. Модульные параметры `no_dsc` и `traffic` для перебора без пересборки.
5. API осовременено: `devm_drm_panel_alloc`, `remove` → void.

### Проверено и исключено
- карта линий: сток `lane_map_0123` = наш `data-lanes = <0 1 2 3>`;
- видео-режим DSC в нашем ядре поддержан (`VIDEO_COMPRESSION_MODE_CTRL_WC`,
  пересчёт `hdisplay`), и тестировалось именно 3:1 — у нас 3:1;
- `DSI PLL(0) lock failed` на 1,0 с — из `dsi_phy_driver_probe`, безвредно;
- `Zero divisor and CLK_DIVIDER_ALLOW_ZERO` — известное косметическое предупреждение;
- DPU sm6350 имеет блок DSC (`sm6350_dsc` в каталоге).

### Следующие шаги
1. Прошить **`out/nodsc2-event.tar`** и `out/nodsc2-burst.tar` — проверить, убирает ли
   явное отключение сжатия в DDIC артефакты.
2. Если нет — вернуться к DSC и копать, почему DPU не отдаёт кадры:
   смотреть `dpu_encoder` и топологию DSC для video mode на sm6350,
   сверить `slice_per_pkt` (сток = 2, msm в `dsi_host.c:941` считает 1 —
   умножение на slice_per_pkt закомментировано).
3. Затем тач (Focaltech FT8203 по SPI, IRQ tlmm 22, reset tlmm 18) — он panel follower,
   поэтому только после стабильной панели.

## Отладка панели, круг 2: найдена ошибка в mainline (2026-10-03, 21:20)

### Исходники ядра Samsung НАКОНЕЦ СКАЧАНЫ
**`daviddean-x/android_kernel-gts7xllite`** (ветка master) — ядро Galaxy Tab S7 FE.
Точечно забран драйвер нашей панели в `downstream/samsung-panel/`:
`techpack/display/msm/samsung/FT8203_TS124QDM/ss_dsi_panel_FT8203_TS124QDM.{c,h}`
Там же лежат драйверы тача (focaltech) — брать оттуда же.

### Питание панели: микросхемы по I2C (проверено, НЕ причина)
Downstream перед включением панели программирует по I2C на шине `qupv3_se8_i2c` (= `i2c8`):
- **ISL98608** @ **0x29** (`display_buck@29`): `0x06=0x0D`, `0x08=0x04` (VP 5.6 В),
  `0x09=0x04` (VN 5.6 В), `0x05=0x27`
- **MAX77816** @ **0x18** (`display_boost@18`): `0x04=0x78` (5 В для BLIC)
- BLIC: LP8558
**Проверено на живом устройстве** (`tools/device/`, python через `/dev/i2c-0`):
все регистры УЖЕ стояли в этих значениях до нашей записи — их выставляет загрузчик.
Гипотеза закрыта.
Полезно: `&i2c8` уже включена в DTS (ради Wacom), `CONFIG_I2C_CHARDEV=y`,
на устройстве шина одна — `/dev/i2c-0` (Geni-I2C). Python3 есть, i2c-tools нет.

### ❗ ГЛАВНОЕ: несоответствие ширины между DPU и DSI при DSC
```
DPU  (dpu_encoder_phys_vid.c): width = 1600 * 8 / (8*3)        = 533  (обрезание)
DSI  (dsi_host.c):             hdisplay = DIV_ROUND_UP(1600*8, 24) = 534  (вверх)
```
`1600 * 8 / 24 = 533,33` — не целое. DPU отдаёт 533 слота на строку, DSI ждёт 534,
строка не закрывается → **нет vblank, нет video done, экран чёрный**.
Подтверждение в логе: `msm_dpu ae01000.display-controller: [drm] vblank wait timed out on crtc 0`.
Что правильно именно 534, видно из `eol_byte_num = total_bytes_per_intf % 3 = 1600 % 3 = 1`
— драйвер сам учитывает лишний байт, которому нужен ещё один слот.
**Исправлено**: в `dpu_encoder_phys_vid.c` поставлен `DIV_ROUND_UP`.

### Перенесён апстрим-патч slice_per_pkt > 1
В нашем ядре msm умел только 1 слайс DSC на пакет (в коде стоял `TODO`), а панель по
стоку требует 2 (`qcom,mdss-dsc-slice-per-pkt = <2>`). Перенесена серия
[drm/msm/dsi: support DSC configurations with slice_per_pkt > 1]:
поле `dsc_slice_per_pkt` в `struct mipi_dsi_device` и `msm_dsi_host`,
пересчёт `bytes_per_pkt`, `pkt_per_line`, word count.
Сам по себе картинку не вернул, но нужен.

### Проверено и ОТБРОШЕНО
- **Сброс ядра MDSS** (как в серии для SM6115/SM6125, симптомы совпадали):
  у sm6350 `resets = <&dispcc DISP_CC_MDSS_CORE_BCR>` **уже есть** и в dispcc, и в dtsi.
- **Напряжения ISL98608/MAX77816** — уже выставлены загрузчиком (см. выше).
- **Карта линий** — сток `lane_map_0123`, совпадает.
- **Патч hdisplay calculation** (2026-02) — уже в ядре, влияет только при bpc=10.
- **Топология DPU** — `num_lm: 1 num_dsc: 1 num_intf: 1`, `Binding dsc:0 to pp:0`, верно.

### ⚠ ИСПРАВЛЕНИЕ МОИХ ПРЕЖНИХ ОШИБОК В ЖУРНАЛЕ
1. `dsi_err_worker: status=4` — это **НЕ** DLN0 PHY, а `DSI_ERR_STATE_FIFO`.
   Биты в нашем ядре: TIMEOUT 0x1, DLN0_PHY 0x2, **FIFO 0x4**, MDP_UNDERFLOW 0x8.
2. Этот поток ошибок **безобиден**: в регистре `FIFO_STATUS` есть биты
   `DLN0_LP_FIFO_EMPTY` (12), `DLN0_HS_FIFO_EMPTY` (16), `DLN1_HS_FIFO_EMPTY` (20) —
   нормальные состояния, а `dsi_fifo_status()` считает ошибкой ЛЮБОЕ ненулевое значение.
   Отсюда ~30 000 сообщений в секунду. Диагностировать по ним нельзя.
3. Ранее записанный вывод «панель даёт картинку» был неточен: без DSC картинка есть,
   но это мусор (DDIC ждёт сжатый поток), с DSC — чистый чёрный.

### Полезные приёмы
- `drm.debug=0x6 log_buf_len=8M` в cmdline — решающий лог (vblank timeout, топология DSC).
  Образ с другим cmdline собирается за секунды, ядро пересобирать не нужно.
- Заливка `/dev/fb0` сплошным цветом (`tools/device/fbtest.sh`) — быстрый способ
  отличить «нет вывода» от «нечего выводить».
- Модульные параметры драйвера панели: `no_dsc`, `traffic` (0/1/2), `slice_per_pkt`.
- `tools/askpass.sh` — хелпер sudo в проекте (scratchpad периодически вычищается).

## ✅✅ ПАНЕЛЬ РАБОТАЕТ, ВИДНА PLASMA (2026-10-03, 21:30)

Образ **`out/width-burst.tar`**. Подтверждение: `vblank wait timed out` — **0 раз**
(до исправления шло непрерывно). Mark видит Plasma на экране 1600x2560.

### Что оказалось причиной — ошибка в mainline
`drivers/gpu/drm/msm/disp/dpu1/dpu_encoder_phys_vid.c`, расчёт ширины строки при DSC:
```c
/* было */ timing->width = timing->width * drm_dsc_get_bpp_int(dsc) /
                           (dsc->bits_per_component * 3);          /* 1600*8/24 = 533 */
/* стало */ timing->width = DIV_ROUND_UP(timing->width * drm_dsc_get_bpp_int(dsc),
                                         dsc->bits_per_component * 3);  /* = 534 */
```
`dsi_host.c` уже использовал `DIV_ROUND_UP` и ждал **534** слота на строку, а DPU
отдавал **533**. Строка не закрывалась → не было ни vblank, ни video done → чёрный экран.
Правильность 534 следует из `eol_byte_num = 1600 % 3 = 1`: один байт не влезает
в целый пиксельный слот, значит нужен ещё один.

Проявляется, когда `hdisplay * bpp / (bpc * 3)` не целое. У нас 1600*8/24 = 533,33.
**Это кандидат на отправку в апстрим.**

### Полный набор правок, которыми живёт панель
1. `panel-ft8203-ts124qdm-wqxga.c` — драйвер из генератора z3ntu по стоковому DTB, плюс:
   `prepare_prev_first = true`; конфиг DSC (1.1, slice 800x40, slice_count 2, 8 bpc,
   8 bpp, block_pred); отправка PPS (`drm_dsc_pps_payload_pack` +
   `mipi_dsi_picture_parameter_set` + `mipi_dsi_compression_mode`), т.к. в стоке
   `samsung,no_qcom_pps`; `MIPI_DSI_MODE_VIDEO_BURST`; API `devm_drm_panel_alloc`,
   `remove` → void. Параметры модуля: `no_dsc`, `traffic`, `slice_per_pkt`.
2. **Перенос апстрим-серии** `drm/msm/dsi: support DSC configurations with slice_per_pkt > 1`
   (поле `dsc_slice_per_pkt`, пересчёт `bytes_per_pkt`/`pkt_per_line`/word count).
3. **Исправление округления ширины в DPU** (см. выше).
4. DTS: `dispcc`/`mdss`/`mdss_dsi0`/`mdss_dsi0_phy` включены, узел панели
   `focaltech,ft8203-ts124qdm`, reset `tlmm 75`, `vddi` = `vreg_l14a`,
   `avdd`/`avee` = fixed-regulator на tlmm 88/89, boost на tlmm 72 (always-on);
   `vreg_l14a` = 1 896 000–1 904 000 (сетка RPMh!).

### Рабочий cmdline
```
console=tty0 ignore_loglevel loglevel=8 fbcon=nodefer pmos.nosplash
pmos_root=/dev/disk/by-partlabel/userdata pmos_root_uuid=c1aca5ea-57e1-43a3-9709-1305c9102c54
pmos_rootfsopts=defaults
panel_ft8203_ts124qdm_wqxga.slice_per_pkt=2 panel_ft8203_ts124qdm_wqxga.traffic=2
```

### Следующее
1. **Тач** — Focaltech FT8203 по SPI (`qupv3_se6_spi`, CS tlmm 16, IRQ tlmm 22,
   reset tlmm 18, 7 МГц, firmware `tsp_focaltech/ft8203_gts7xllite.bin`).
   Драйвер брать из скачанных исходников Samsung. Он panel follower — теперь можно.
2. Проверить, не гаснет ли панель от DPMS (на родственном T733 это было фатально).
3. Перенести все правки в пакет `linux-samsung-gts7xllite` (патчами), чтобы работали модули.
4. GPU: `a615_zap` из `apnhlos`, снять `status = "disabled"` с `&gpu`.

## ⚠ ВАЖНО: в Linux планшет НЕ ЗАРЯЖАЕТСЯ (2026-10-03, 22:20)

Симптом: после нескольких часов работы устройство ушло в цикл — подсветка
вспыхивает на секунду, затем ресет, и так по кругу. **Заведомо рабочий образ
`width-burst` ведёт себя точно так же**, то есть дело не в ядре и не в драйвере тача.

Причина: драйверы в конфиге есть (`CONFIG_CHARGER_QCOM_SMB2=y`,
`CONFIG_BATTERY_QCOM_QG=y`), но **в DTS нет ни одного узла зарядки/батареи**,
поэтому они ни к чему не привязываются. Планшет работает только от батареи.
Включение панели даёт скачок потребления → просадка → защита → ресет.

В Download mode устройство живёт стабильно: потребление мизерное, и **загрузчик
Samsung батарею заряжает**. Это же и способ восстановления: оставить в Download
mode или выключенным на зарядке.

TODO (важно, иначе каждая сессия будет упираться в это):
- добавить в DTS узлы заряда/топливомера (pm7250b: `pmic-charger`/`qcom,smb2`,
  `pm6150l`/`pmk8350` для QG), параметры брать из стокового DTB;
- до этого — не работать долго без Download-mode пауз на зарядку.

## Сессия 3 — ТАЧ ЗАРАБОТАЛ (2026-10-04)

### ✅ РЕЗУЛЬТАТ: FocalTech FT8203 работает, этап «тач» закрыт
Проверка объективная: счётчик прерываний `msmgpio 22` за 25 секунд касаний
вырос с 4 до 155. `/dev/input/event3` = «FocalTech FT8203 Touchscreen»,
multitouch 10 точек. Плазма реагирует.

### Что было причиной (главное)
Прошивка заливалась в PRAM корректно, чип отвечал на регистры и сообщал
версию — но **не сканировал матрицу**. Диагностика по вендорским регистрам:
- `0x91` (FTS_REG_FLOW_WORK_CNT) — счётчик рабочего цикла прошивки,
- `0x8F` (FTS_REG_INT_CNT) — счётчик прерываний со стороны чипа.

`flow 0->0` = прошивка загружена, но цикл опроса не запущен.

**Лечится вендорской ECC-проверкой перед командой запуска `0x08`:**
```
0xCC + addr[3] + len[3]   -> чип считает контрольную сумму по PRAM
0xCE                      -> опрос готовности, ждём 0xA5 (до 100 раз по 1 мс)
0xCD                      -> чтение 2 байт суммы
```
Хостовая сумма: реflected CRC, полином `(1<<15)|(1<<10)|(1<<3)`, по 2 байта
big-endian за итерацию (`fts_ecc_cal_host` в `focaltech_flash.c`).
Совпало: `firmware checksum 0xdbe3 verified`.

Это не только верификация: **без этой последовательности прошивка не
начинает сканировать**. В предыдущей сборке диагностика уже была и `flow`
стоял на нуле; ожило ровно после добавления ECC-шага.

### Разбор кадра касания — исправлено смещение
Вендорские индексы считаются от его буфера, где байт 0 — адрес регистра,
затираемый ответом. В нашем буфере адреса нет, поэтому **все вендорские
смещения на единицу больше наших**: область точек у вендора с 3, у нас с 2.
Было `FT8203_POINTS_OFFSET 3` — ID, X и Y читались на байт правее.

### Про DRAM — ложный след, проверено
У вендора есть `fts_dram_write_ecc`, но для FT8203 `drwr_support = 0`, а в
образе `const_len = 0` по смещению `0x108` → вендор DRAM пропускает.
`code_len = 41016` по `0x100`, при `half_length = 1` это ×2 = 82032 байта,
то есть весь файл. Наша заливка совпадает байт-в-байт.

### ⚠️ ЯДРО СОБИРАЕТСЯ ТОЛЬКО CLANG: `make LLVM=1 ARCH=arm64`
Установленное ядро — clang 23.1.1 с `CONFIG_CFI=y`. Модуль, собранный gcc,
загрузиться не может: CFI убивает `insmod` на первом косвенном вызове:
```
CFI failure at do_one_initcall (target: ft8203_driver_init; expected type: 0x6fbb3035)
Internal error: Oops - CFI
```
Это не безобидно: `insmod` умирает **внутри `do_one_initcall` с выключенными
прерываниями и ненулевым `preempt_count`**, не отпустив мьютекс загрузчика
модулей. После этого любой `insmod`/`rmmod` висит в состоянии `S` навсегда,
модуль виден в `lsmod`, но ни к устройству не привязан, ни прерывание не
запрашивает. Восстанавливается только перезагрузкой.

Хуже того, сборка gcc **переписывает `.config`** под gcc и выключает
`CONFIG_CFI`. Восстановление: дописать `CONFIG_CFI=y` и
`CONFIG_CFI_ICALL_NORMALIZE_INTEGERS=y`, затем `make LLVM=1 olddefconfig`
и обязательно `make LLVM=1 modules_prepare` — иначе `include/config/auto.conf`
остаётся от gcc и подсовывает clang несовместимые флаги
(`-fmin-function-alignment`, `-fconserve-stack`).

### Быстрый цикл отладки (секунды вместо 10 минут)
Драйвер собран модулем, поэтому:
```
make LLVM=1 ARCH=arm64 -j M=drivers/input/touchscreen   # ~8 секунд
scp .../focaltech-ft8203.ko mark@172.16.42.1:/tmp/mod.ko
ssh mark@172.16.42.1 'echo 147147 | sudo -S sh /tmp/inst.sh'
```
Прошивать ядро для правки драйвера больше не нужно.

### Тач теперь постоянный
- `/lib/modules/7.2.0/extra/focaltech-ft8203.ko` + `depmod -a`
- `/etc/modules-load.d/focaltech-ft8203.conf` → автозагрузка при старте
- прошивка: `/lib/firmware/tsp_focaltech/ft8203_gts7xllite.bin` (82032 байта)

### Автологин в Плазму — сделан
`/etc/plasmalogin.conf.d/10-autologin.conf`:
```
[Autologin]
User=mark
Session=plasma.desktop
Relogin=false
```
Менеджер входа здесь **не SDDM, а `plasmalogin`** (пакет
`plasma-login-manager-6.7.5`, юнит `plasmalogin.service`); `/etc/sddm.conf`
ему не указ. В логе: `Authentication for user "mark" successful`,
`Session started true`.

### Грабли по работе через SSH (чтобы не терять время снова)
- `nohup ... &` внутри `sudo sh -c` не выживает: `sudo` уходит и забирает
  фоновый процесс. Нужен `setsid`.
- `sudo -S` читает пароль из stdin, поэтому `< /dev/null` на внешней команде
  ломает авторизацию молча — скрипт просто не запускается, а читается
  прошлый лог. Перенаправлять stdin только у внутренней команды.
- `/tmp` на устройстве очищается при перезагрузке — рабочие файлы в `/root`.

### Следующие шаги
1. Перенести драйвер тача, патчи панели и DTS в пакет `linux-samsung-gts7xllite`
   в pmaports — сейчас всё держится на модуле, подложенном руками.
2. Зарядка/батарея в DTS — её до сих пор нет, планшет разряжается за сессию.
3. S Pen (Wacom).
4. GPU: `a615_zap` из `apnhlos`, снять `status = "disabled"` с `&gpu`.
5. Проверить, что гашение экрана и suspend не убивают панель (прецедент T733).

---

## Сессии 4–5 — зависания: найден механизм (2026-10-04 … 10-09)

### Итог одной строкой
Зависает **дисплейный тракт**, а не DE, GPU, idle или батарея. Детерминированный
триггер — перезапуск греетера (`systemctl restart plasmalogin`): смена компози́тора
выключает и включает панель и DSI-хост. В драйвере панели найдено и исправлено три
дефекта; последний фикс (`out/work4.tar`) прошит, но **не проверен** — Mark вернул
Android до теста.

### Что исключено (с доказательствами)
| версия | как опровергнута |
|---|---|
| Плазма | GNOME виснет так же |
| GPU / рендеринг | на llvmpipe виснет так же; клины были до включения GPU |
| глубокий сон CPU | DTB без `domain-idle-states` (cpuidle отсутствует совсем) — виснет |
| `idle=poll` | на arm64 **игнорируется**: `state1 usage` растёт и с ним |
| ADSP | при выключенном ADSP виснет; но сам ADSP — отдельный дефект, см. ниже |
| тач | автологин с тачем в blacklist — виснет |
| UFS / питание UFS | супплаи `L7E/L12A/L18A/L22A` совпадают со стоком (dtbo), все LDO в HPM |
| батарея | Mark против; проверить нечем (драйвера заряда нет) — отложено |

### Механизм и найденные дефекты (`panel-ft8203-ts124qdm-wqxga.c`)
1. **`unprepare()` обесточивает панель** (ресет + `regulator_bulk_disable`). На
   загрузке рельсы уже подняты ABL, и первый `prepare()` работает; повторный
   холодный старт даёт `dsi_err_worker: status=5` (TIMEOUT 0x1 | FIFO 0x4) →
   `dpu_crtc_frame_event_cb ... overflow` → смерть SoC.
   Обход: параметр `keep_powered` (по умолчанию `Y`), `unprepare()` ничего не делает.
   После него перезапуск греетера ×5 (и ×3 с тачем) — жив. Но экран после смены
   компози́тора **чёрный**: панель под питанием не синхронизируется с новым потоком.
   TODO: нормальная последовательность питания по downstream (порядок vddi→avdd→avee,
   задержки), тогда `keep_powered` не нужен.
2. **Яркость отправлялась в HS** посреди видеопотока (драйвер снимал
   `MIPI_DSI_MODE_LPM`). Сток шлёт в LP (`pwm_cmds->state = DSI_CMD_SET_STATE_LP`,
   `ss_dsi_panel_FT8203_TS124QDM.c:235`). Воспроизведено: 3 круга яркости 425→50→425
   шагом 5 с SSH → `dsi_err_worker: status=5` ×4. Плавное затемнение Плазмы на простое
   (~2,5 мин) — это оно. Также при ошибке записи флаг LPM не восстанавливался.
3. **`get_brightness()` делал DCS-чтение** (BTA во время видео → TIMEOUT), а его
   дёргают powerdevil/UPower через `actual_brightness`. Удалён.
Фиксы 2–3 — в `work4`. Проверка, которую надо прогнать первой: `dim2.sh`
(яркость + чтения `actual_brightness`) и `dmesg | grep -c dsi_err` → должно быть 0.

### Прочие находки
- **Модули из пакета pmOS на нашем ядре не грузятся** (`BTF: -22`) — без них нет
  remoteproc, Wi-Fi, qrtr. После каждой пересборки ядра ставить свои:
  `make modules_install INSTALL_MOD_PATH=…` → tar → `/lib/modules/7.2.0` → `depmod`.
  Пересобирается **всё** BTF модулей даже при правке одного встроенного файла.
- **ADSP в цикле падений**: `fatal error: EF:sensor_process:...SNS_REG_INIT...
  sns_registry_sensor.c:95`, к краху №67 за загрузку. Нет реестра датчиков Samsung.
  Каждый перезапуск — длинные вызовы в TrustZone. Обход: `blacklist qcom_q6v5_pas`.
- **Зарядки нет**: контроллер **SM5714** (`siliconmitus,sm5714mfd`: заряд, датчик,
  USB-PD, MUIC) + SM5440. В mainline драйвера нет. `/sys/class/power_supply` пуст.
- **IPA**: `error -22` на прошивке, как и у автора DTS (PR #9) — не вешает.
- Автор DTS (vishalbala-nps, [PR #9](https://github.com/sm6350-mainline/linux/pull/9))
  гонял только simplefb, без панели/GPU/тача — зависаний не упоминает.

### Инструменты отладки без UART — что работает, что нет
| способ | итог |
|---|---|
| journald | ловит `hard LOCKUP`, но трассировку не успевает |
| синхронный логгер `/root/kmsglog.sh` (cat /dev/kmsg + sync на строку) | **работает**, именно он поймал `dsi_err_worker` |
| `drm.debug=0x16` + dynamic_debug msm на лету | работает, показал шаги до смерти |
| netconsole | не работает: `u_ether` не отдаёт пакеты из netpoll |
| ramoops | DDR через сброс не сохраняется (проверено контрольной паникой, оба адреса) |
| DRM_PANIC | msm/DPU не поддерживает, экран пустой |
| buddy hardlockup detector | ловит, печатает `hard LOCKUP on cpu N` |
- В `/dev/kmsg` кириллица экранируется (`\xd0\x9c...`) — маркеры писать латиницей.
- `pkill -f 'шаблон'` убивает собственный шелл, если шаблон есть в командной строке.
- `sysrq` в ядре выключен.

### Прошивка
- Возврат Linux после Android: `out/userdata-sparse.img` (образ 2 октября) →
  `-p USERDATA … -p RECOVERY …` одной сессией; затем заново ключ SSH, модули,
  прошивки GPU/тача, `modules-load.d`, blacklist ADSP. Нужен `systemd.mask=boot.mount`
  в cmdline (в старом fstab строка `/boot`).
- **Полная стоковая прошивка samloader'ом (BL+AP+CP+CSC) падает** на
  `Dynamic partition pre-flight check failed` — не помогают ни `-e`, ни
  `--skip-size-check`. Рабочий откат: `-p RECOVERY/BOOT/DTBO/VBMETA` из
  `stock/backup/` при уже стоковом `super` (+ `-e` стирает userdata).
- После неудачной сессии samloader кабель надо переподключать.
- Только после явной команды Mark на прошивку; Mark шлёт команды сам через `!`.

### Состояние на конец
Android возвращён (RECOVERY/BOOT/DTBO/VBMETA из бэкапа, userdata стёрт).
Наработки целы: ядро с фиксами в `linux-pmos/` (`work4`), модули
`out/modules-7.2.0.tar.gz`, образ rootfs `out/userdata-sparse.img`.

### Следующий шаг, когда вернёмся в Linux
1. Прошить `work4` + наши модули, прогнать `dim2.sh` и перезапуск греетера.
2. Если чисто — разбираться с чёрным экраном после смены компози́тора (правильная
   последовательность питания панели вместо `keep_powered`).
3. `DSI PLL(0) lock failed` и WARN нулевого делителя при загрузке.
