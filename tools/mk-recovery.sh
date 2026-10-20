#!/bin/bash
# Сборка загрузочного образа для раздела RECOVERY (Samsung ABL не грузит ядро из boot).
# Параметры заголовка восстановлены из out/boot-noboot.img (unpack_bootimg).
# Использование: tools/mk-recovery.sh <имя> ["доп. cmdline"]
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT=$PWD
NAME=${1:-test}
EXTRA=${2:-}
KDIR=$ROOT/linux-pmos
DTB=$KDIR/arch/arm64/boot/dts/qcom/sm7225-samsung-gts7xllite.dtb
IMAGE=$KDIR/arch/arm64/boot/Image.gz
RAMDISK=${RAMDISK:-$ROOT/out/ramdisk-r1.cpio.gz}   # initramfs с устройства (пакет r1)
OUT=$ROOT/out/$NAME.img
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# systemd.mask=boot.mount больше не нужен: строка /boot убрана из fstab на устройстве
# pmos_root задаёт раздел напрямую: rootfs лежит прямо на userdata. Поиск по UUID
# работает, но у initramfs на него лимит 10 секунд, и при живой панели (длинная
# init-последовательность DSI) он в него не укладывался -> "failed to mount subpartitions".
CMDLINE="console=tty0 ignore_loglevel loglevel=8 fbcon=nodefer pmos.nosplash pmos_root=/dev/disk/by-partlabel/userdata pmos_root_uuid=c1aca5ea-57e1-43a3-9709-1305c9102c54 pmos_rootfsopts=defaults${EXTRA:+ $EXTRA}"

# DTB приклеивается к ядру И дублируется в поле заголовка — так загрузился рабочий образ
cat "$IMAGE" "$DTB" > "$WORK/kernel"

mkbootimg \
  --kernel "$WORK/kernel" --ramdisk "$RAMDISK" --dtb "$DTB" \
  --base 0x80000000 --kernel_offset 0x8000 --ramdisk_offset 0x02000000 \
  --tags_offset 0x01e00000 --dtb_offset 0x01f00000 \
  --pagesize 4096 --header_version 2 \
  --os_version 11.0.0 --os_patch_level 2025-06 \
  --board SRPUB23B009 --cmdline "$CMDLINE" --output "$OUT"

printf 'SEANDROIDENFORCE' >> "$OUT"        # Samsung ABL проверяет этот хвост

# tar для samloader: имя файла обязано совпадать с Flash Filename из PIT
mkdir -p "$ROOT/out/flash-$NAME"
cp "$OUT" "$ROOT/out/flash-$NAME/recovery.img"
tar -H ustar -cf "$ROOT/out/$NAME.tar" -C "$ROOT/out/flash-$NAME" recovery.img

echo "образ:  $OUT"
echo "tar:    $ROOT/out/$NAME.tar"
echo "cmdline: $CMDLINE"
