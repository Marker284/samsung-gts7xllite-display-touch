#!/bin/bash
# Выполняется НА НОУТБУКЕ. Ждёт появления SSH на планшете, затем заливает и
# запускает dev-setup.sh. Нужен, потому что sshd на планшете поднимается не сразу
# после загрузки и иногда обрывается на обмене баннером.
set -u
SCRATCH="$(dirname "$0")"
HOST=mark@172.16.42.1
O=(-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o BatchMode=yes -o ConnectTimeout=10)
PASS=147147

echo "--- ожидание SSH ---"
ok=0
for i in $(seq 1 30); do
	if timeout 15 ssh "${O[@]}" "$HOST" true 2>/dev/null; then
		echo "SSH доступен (попытка $i)"
		ok=1
		break
	fi
	sleep 10
done
if [ "$ok" -ne 1 ]; then
	echo "SSH не поднялся — планшет недоступен"
	exit 1
fi

echo "--- заливаю скрипт ---"
scp "${O[@]}" "$SCRATCH/dev-setup.sh" "$HOST":/tmp/dev-setup.sh || exit 1

echo "--- проверяю наличие пакета на устройстве ---"
if ! ssh "${O[@]}" "$HOST" "test -f /root/linux-samsung-gts7xllite-7.2.0-r1.apk" 2>/dev/null; then
	echo "пакета нет, копирую (34 МБ)"
	ssh "${O[@]}" "$HOST" "echo $PASS | sudo -S chmod 777 /root" >/dev/null 2>&1
	scp "${O[@]}" /home/mark/.local/var/pmbootstrap/packages/edge/aarch64/linux-samsung-gts7xllite-7.2.0-r1.apk "$HOST":/root/ || exit 1
fi

echo "--- запускаю на планшете ---"
ssh "${O[@]}" "$HOST" "chmod +x /tmp/dev-setup.sh; echo $PASS | sudo -S /tmp/dev-setup.sh" 2>&1 | grep -v '^\[sudo'
