#!/bin/sh
# Выполняется НА ПЛАНШЕТЕ под root.
# Проверяет, доходит ли содержимое фреймбуфера до панели: заливает его
# сплошными цветами. Если экран послушно меняет цвет - тракт DSI/DSC исправен,
# и проблема лишь в том, что в буфер никто не рисует.
FB=/dev/fb0
[ -e "$FB" ] || { echo "нет $FB"; exit 1; }

W=$(cut -d, -f1 /sys/class/graphics/fb0/virtual_size)
H=$(cut -d, -f2 /sys/class/graphics/fb0/virtual_size)
BPP=$(cat /sys/class/graphics/fb0/bits_per_pixel)
STRIDE=$(cat /sys/class/graphics/fb0/stride 2>/dev/null)
SIZE=$((STRIDE * H))
echo "fb0: ${W}x${H}, ${BPP} бит, stride ${STRIDE}, всего ${SIZE} байт"

echo "--- БЕЛЫЙ (5 секунд) ---"
tr '\0' '\377' < /dev/zero | dd of=$FB bs=64k count=$((SIZE / 65536 + 1)) 2>/dev/null
sleep 5

echo "--- ЧЁРНЫЙ (5 секунд) ---"
dd if=/dev/zero of=$FB bs=64k count=$((SIZE / 65536 + 1)) 2>/dev/null
sleep 5

echo "--- БЕЛЫЙ (остаётся) ---"
tr '\0' '\377' < /dev/zero | dd of=$FB bs=64k count=$((SIZE / 65536 + 1)) 2>/dev/null
echo "готово"
