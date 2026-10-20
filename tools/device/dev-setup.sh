#!/bin/sh
# Выполняется НА ПЛАНШЕТЕ под root.
# Порядок важен: сначала делаем USB-сеть постоянной (иначе связь снова пропадёт
# через пару минут, когда NetworkManager заберёт интерфейс у initramfs),
# и только потом ставим пакет ядра.
APK=/root/linux-samsung-gts7xllite-7.2.0-r1.apk

echo "===== 1. USB-СЕТЬ НАВСЕГДА ====="
# Какой интерфейс создал гаджет
IFACE=$(ls /sys/class/net | grep -E '^(usb[0-9]+|rndis[0-9]+)$' | head -1)
echo "iface: ${IFACE:-НЕ НАЙДЕН}"
ip -br addr show "$IFACE" 2>&1

# NetworkManager не должен трогать интерфейс гаджета
mkdir -p /etc/NetworkManager/conf.d
cat > /etc/NetworkManager/conf.d/99-usb-gadget.conf <<'CONF'
# USB gadget network is configured by the pmOS initramfs. Let NetworkManager
# leave it alone, otherwise it drops the 172.16.42.1 address a couple of
# minutes into the session and SSH over the cable dies.
[keyfile]
unmanaged-devices=interface-name:usb0;interface-name:rndis0;interface-name:usb1
CONF
echo "конфиг NetworkManager записан"

# Служба, которая поднимает адрес при каждой загрузке — не зависит от NM
cat > /etc/systemd/system/usb-gadget-net.service <<'UNIT'
[Unit]
Description=Static address on the USB gadget network (debug link)
After=sys-subsystem-net-devices-usb0.device
Wants=network.target
Before=network.target

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/sh -c 'for i in $(seq 1 30); do I=$(ls /sys/class/net | grep -E "^(usb[0-9]+|rndis[0-9]+)$" | head -1); [ -n "$I" ] && break; sleep 1; done; [ -n "$I" ] || exit 0; ip link set "$I" up; ip addr replace 172.16.42.1/16 dev "$I"; ip route replace default via 172.16.42.2 dev "$I" metric 1000 || true'

[Install]
WantedBy=multi-user.target
UNIT
systemctl daemon-reload
systemctl enable usb-gadget-net.service 2>&1 | tail -2
systemctl restart usb-gadget-net.service 2>&1 | tail -2
echo "служба usb-gadget-net включена"

# DNS, чтобы apk работал
grep -q 1.1.1.1 /etc/resolv.conf 2>/dev/null || echo nameserver 1.1.1.1 >> /etc/resolv.conf
if ping -c1 -W3 1.1.1.1 >/dev/null 2>&1; then echo "internet: ok"; else echo "internet: no"; fi

echo "===== 2. /boot ИЗ FSTAB ====="
# Ядро живёт в Android-образе внутри recovery, раздела /boot не существует,
# systemd ждал его и не доходил до сети. Сейчас это обойдено
# systemd.mask=boot.mount в cmdline; убираем причину.
if grep -q '[[:space:]]/boot[[:space:]]' /etc/fstab; then
	cp /etc/fstab /etc/fstab.bak
	sed -i '/[[:space:]]\/boot[[:space:]]/s/^/#/' /etc/fstab
	echo "строка /boot закомментирована (бэкап /etc/fstab.bak)"
else
	echo "строки /boot в fstab нет"
fi

echo "===== 3. ПАКЕТ ЯДРА ====="
if [ -f "$APK" ]; then
	apk add --allow-untrusted "$APK" 2>&1 | tail -20
	echo "--- установлено ---"
	apk info -v 2>/dev/null | grep linux-samsung
	echo "--- modules ---"
	ls /usr/lib/modules/
else
	echo "ПАКЕТА НЕТ: $APK"
fi

echo "===== 4. СОСТОЯНИЕ ====="
uptime
echo "--- модули загружены? ---"
lsmod | head -10
echo "всего модулей: $(lsmod | tail -n +2 | wc -l)"
echo "===== DONE ====="
