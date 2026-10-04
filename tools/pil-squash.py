#!/usr/bin/env python3
"""Склеивает Qualcomm-прошивку из <name>.mdt и сегментов <name>.bNN в один .mbn.

Повторяет pil-squasher: сначала ELF-заголовок, затем таблица программ по
смещению e_phoff, затем каждый непустой сегмент по своему p_offset.
"""
import struct
import sys
from pathlib import Path


def squash(mdt: Path, out: Path) -> None:
    data = mdt.read_bytes()
    if data[:4] != b"\x7fELF":
        raise SystemExit("%s не ELF" % mdt)
    if data[4] != 1:
        raise SystemExit("ожидался ELF32, в файле class=%d" % data[4])

    e_phoff, = struct.unpack_from("<I", data, 0x1C)
    e_ehsize, e_phentsize, e_phnum = struct.unpack_from("<HHH", data, 0x28)
    if e_phentsize != 32:
        raise SystemExit("неожидаемый размер записи таблицы программ: %d" % e_phentsize)

    blob = bytearray()

    def put(offset: int, chunk: bytes) -> None:
        end = offset + len(chunk)
        if len(blob) < end:
            blob.extend(b"\0" * (end - len(blob)))
        blob[offset:end] = chunk

    put(0, data[:e_ehsize])
    put(e_phoff, data[e_phoff:e_phoff + e_phnum * e_phentsize])

    base = mdt.with_suffix("")
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_offset, _p_vaddr, _p_paddr, p_filesz = struct.unpack_from("<4xIIII", data, off)
        if not p_filesz:
            continue
        seg = base.with_suffix(".b%02d" % i)
        if not seg.exists():
            raise SystemExit("нет сегмента %s (p_filesz=%d)" % (seg, p_filesz))
        payload = seg.read_bytes()
        if len(payload) != p_filesz:
            print("  предупреждение: %s %d байт, а в таблице %d"
                  % (seg.name, len(payload), p_filesz))
        put(p_offset, payload)
        print("  сегмент %2d -> смещение 0x%06x, %d байт" % (i, p_offset, len(payload)))

    out.write_bytes(bytes(blob))
    print("%s: %d байт" % (out, len(blob)))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("использование: pil-squash.py <вход.mdt> <выход.mbn>")
    squash(Path(sys.argv[1]), Path(sys.argv[2]))
