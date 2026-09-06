#!/usr/bin/env python3
"""Empacota pu.7z em pu.dat (LLQTPKG1 + XOR). Nao entra no binario do launcher.

Uso:
    python pack_pu.py pu.7z pu.dat
    python pack_pu.py                  # padrao: pu.7z -> pu.dat
"""
import argparse
import struct
import sys
from pathlib import Path

XOR_KEY = bytes([0x4C, 0x4C, 0x51, 0x54, 0x21, 0x9F, 0x3D, 0x7B])
MAGIC = b"LLQTPKG1"


def pack(src_7z_path: Path, out_dat_path: Path) -> None:
    data = src_7z_path.read_bytes()
    obfuscated = bytes(b ^ XOR_KEY[i % len(XOR_KEY)] for i, b in enumerate(data))
    out_dat_path.write_bytes(MAGIC + struct.pack("<I", len(obfuscated)) + obfuscated)
    print(f"ok  {src_7z_path}  ({len(data)} bytes)")
    print(f" -> {out_dat_path}  ({12 + len(obfuscated)} bytes)")
    print("publique em https://github.com/MestreTM/CLL-CodLanLauncher/releases/download/v0.1/pu.dat")


def main() -> int:
    p = argparse.ArgumentParser(description="Gera pu.dat ofuscado a partir de pu.7z")
    p.add_argument("src", nargs="?", default="pu.7z", help="arquivo 7z de origem")
    p.add_argument("dst", nargs="?", default="pu.dat", help="arquivo .dat de saida")
    args = p.parse_args()
    src = Path(args.src)
    if not src.is_file():
        print(f"nao achei {src}", file=sys.stderr)
        return 1
    pack(src, Path(args.dst))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
