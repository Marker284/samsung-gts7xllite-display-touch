#!/bin/sh
# Выполняется НА ПЛАНШЕТЕ под root.
# Подключает драйвер тача FT8203 и показывает, ожил ли он.

MOD=/root/focaltech-ft8203.ko
FW=/lib/firmware/tsp_focaltech/ft8203_gts7xllite.bin

[ -f "$FW" ] || { echo "нет прошивки $FW"; exit 1; }
[ -f "$MOD" ] || { echo "нет модуля $MOD"; exit 1; }

echo "===== SPI ====="
ls /sys/bus/spi/devices/ 2>&1

echo "===== ЗАГРУЗКА МОДУЛЯ ====="
dmesg -c > /dev/null 2>&1
insmod "$MOD" 2>&1 || echo "insmod вернул ошибку"

sleep 2

echo "===== DMESG ====="
dmesg | tail -40

echo "===== INPUT-УСТРОЙСТВА ====="
cat /proc/bus/input/devices 2>&1 | grep -i -A2 -B2 focal

echo "===== ПРЕРЫВАНИЯ ====="
grep -i -E "spi|focal" /proc/interrupts 2>&1
