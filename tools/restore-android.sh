#!/bin/bash
# Откат устройства на стоковый Android (SM-T735, gts7xllite).
#
# Два режима:
#   quick  - вернуть только то, что мы меняли: RECOVERY, BOOT, DTBO, VBMETA
#            из stock/backup. Быстро, но /data остаётся с rootfs pmOS, поэтому
#            Android почти наверняка попросит сброс настроек при первом старте.
#   full   - полная прошивка BL+AP+CP+CSC из стокового zip. Это и есть
#            настоящий возврат к заводскому состоянию: CSC (не HOME_CSC)
#            форматирует userdata, то есть rootfs pmOS будет стёрт.
#
# По умолчанию скрипт НИЧЕГО не пишет: показывает план и выходит.
# Запись только с флагом --yes.
#
# Устройство должно быть в Download mode: выключить, зажать Vol- и Vol+,
# подключить USB.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT=$PWD
SAMLOADER=$ROOT/tools/samloader/samloader
BACKUP=$ROOT/stock/backup
ZIP=$(ls "$ROOT"/stock/SM-T735_*.zip 2>/dev/null | head -1 || true)
EXTRACT=$ROOT/stock/extracted

MODE=${1:-}
CONFIRM=${2:-}

die() { echo "ОШИБКА: $*" >&2; exit 1; }

# Разделы, в которые этому скрипту разрешено писать. Всё остальное -- efs,
# sec_efs, persist, modem/NON-HLOS, param, загрузчики -- не трогаем никогда,
# кроме режима full, где их пишет сам стоковый BL/CP пакет целиком.
QUICK_PARTS=(RECOVERY BOOT DTBO VBMETA)

usage() {
	cat <<USAGE
Использование: tools/restore-android.sh <quick|full> [--yes]

  quick        вернуть RECOVERY, BOOT, DTBO, VBMETA из stock/backup
  full         прошить BL+AP+CP+CSC из стокового zip (стирает userdata)
  --yes        реально писать; без него только показывается план

Перед запуском устройство должно быть в Download mode.
USAGE
}

plan_quick() {
	echo "РЕЖИМ: quick -- восстановление из stock/backup"
	echo
	printf '%-10s %s\n' РАЗДЕЛ ФАЙЛ
	for p in "${QUICK_PARTS[@]}"; do
		local f
		f=$(quick_file "$p")
		printf '%-10s %s\n' "$p" "$f"
	done
	echo
	echo "userdata НЕ затрагивается: rootfs pmOS останется на месте,"
	echo "и Android при первом старте, скорее всего, предложит сброс."
}

quick_file() {
	case $1 in
	RECOVERY) echo "$BACKUP/recovery.img" ;;
	BOOT)     echo "$BACKUP/boot.img" ;;
	DTBO)     echo "$BACKUP/dtbo.img" ;;
	VBMETA)   echo "$BACKUP/vbmeta.img" ;;
	*)        die "неизвестный раздел $1" ;;
	esac
}

do_quick() {
	local args=()
	for p in "${QUICK_PARTS[@]}"; do
		local f
		f=$(quick_file "$p")
		[ -f "$f" ] || die "нет файла $f"
		args+=(-p "$p" "$f")
	done
	# script -qefc нужен потому, что samloader требует pty для прогресс-бара
	script -qefc "$SAMLOADER flash --no-reboot ${args[*]}" /dev/null
	echo
	echo "Готово. Перезагрузить: $SAMLOADER reboot"
}

plan_full() {
	[ -n "$ZIP" ] || die "не найден стоковый zip в stock/"
	echo "РЕЖИМ: full -- полная прошивка из $(basename "$ZIP")"
	echo
	echo "Слоты: BL, AP, CP, CSC"
	unzip -l "$ZIP" | awk '/\.tar\.md5/ {print "  " $4}'
	echo
	echo "ВНИМАНИЕ: берётся CSC, а не HOME_CSC -- userdata будет отформатирован,"
	echo "rootfs pmOS со всем содержимым будет стёрт. Это заводской сброс."
	echo "Распаковка займёт около 8 ГБ в stock/extracted."
}

do_full() {
	[ -n "$ZIP" ] || die "не найден стоковый zip в stock/"
	mkdir -p "$EXTRACT"
	echo "распаковываю стоковые пакеты..."
	unzip -n "$ZIP" -d "$EXTRACT" >/dev/null
	local bl ap cp csc
	bl=$(ls "$EXTRACT"/BL_*.tar.md5 | head -1)
	ap=$(ls "$EXTRACT"/AP_*.tar.md5 | head -1)
	cp=$(ls "$EXTRACT"/CP_*.tar.md5 | head -1)
	csc=$(ls "$EXTRACT"/CSC_*.tar.md5 | head -1)
	for f in "$bl" "$ap" "$cp" "$csc"; do
		[ -f "$f" ] || die "нет пакета $f"
	done
	script -qefc "$SAMLOADER flash --no-reboot -b $bl -a $ap -c $cp -s $csc" /dev/null
	echo
	echo "Готово. Перезагрузить: $SAMLOADER reboot"
}

case $MODE in
quick) plan_quick ;;
full)  plan_full ;;
*)     usage; exit 1 ;;
esac

if [ "$CONFIRM" != "--yes" ]; then
	echo
	echo "Это был только план. Чтобы прошить, добавь --yes."
	exit 0
fi

echo
read -r -p "Писать на устройство? Введи ДА, чтобы продолжить: " answer
[ "$answer" = "ДА" ] || die "отменено"

case $MODE in
quick) do_quick ;;
full)  do_full ;;
esac
