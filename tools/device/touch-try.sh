#!/bin/sh
# Выполняется НА ПЛАНШЕТЕ под root, запускать через setsid - иначе процесс
# умирает вместе с ssh-сессией.
# Подключает модуль тача, даёт 25 секунд на касания, пишет отчёт в /tmp/touch.log.
LOG=/tmp/touch.log
: > "$LOG"
say() { echo "$@" >> "$LOG"; }
run() { echo "--- $*" >> "$LOG"; "$@" >> "$LOG" 2>&1; }

say "=== uptime: $(cat /proc/uptime) ==="
say "=== прошивка на месте? ==="
ls -l /lib/firmware/tsp_focaltech/ >> "$LOG" 2>&1

dmesg -C
# insmod может зависнуть навсегда, если мьютекс модулей занят упавшей загрузкой
timeout 30 insmod /tmp/focaltech-ft8203.ko dyndbg=+p >> "$LOG" 2>&1 &
INS=$!
sleep 5

if kill -0 $INS 2>/dev/null; then
	say "=== insmod ВИСИТ - мьютекс модулей занят, нужна перезагрузка ==="
else
	say "=== insmod вернулся ==="
fi

say "=== привязка к spi0.0 ==="
ls -l /sys/bus/spi/devices/spi0.0/driver >> "$LOG" 2>&1
run grep spi0.0 /proc/interrupts
say "=== dmesg после загрузки ==="
dmesg >> "$LOG" 2>&1

say ""
say ">>> ТРОГАЙ ЭКРАН <<<"
dmesg -C
sleep 25

say "=== IRQ после касаний ==="
run grep spi0.0 /proc/interrupts
say "=== input ==="
grep -i -A4 focal /proc/bus/input/devices >> "$LOG" 2>&1
say "=== dmesg за время касаний ==="
dmesg >> "$LOG" 2>&1
say "=== ГОТОВО ==="
