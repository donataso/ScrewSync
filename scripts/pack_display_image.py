#!/usr/bin/env python
"""Pack a display .tft for the "display" flash partition.

Layout must match main/lib/Display/DisplayImageStore.cpp: one 4096-byte
header sector (magic, length, crc32, reserved; little-endian, 0xFF padded),
then the image.
"""
import struct
import sys
import zlib
from pathlib import Path

MAGIC = 0x31544644
SECTOR = 4096


def main() -> None:
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    image = src.read_bytes()
    header = struct.pack("<IIII", MAGIC, len(image), zlib.crc32(image), 0xFFFFFFFF)
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(header.ljust(SECTOR, b"\xff") + image)


if __name__ == "__main__":
    main()
