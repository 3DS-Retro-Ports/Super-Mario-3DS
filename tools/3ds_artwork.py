#!/usr/bin/env python3
"""Generate original geometric homebrew artwork and a silent banner sound."""
from pathlib import Path
import struct
import sys
import wave
import zlib

root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
def png(path, width, height):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            # Two nested screens, with a bright bottom screen.
            xn, yn = x / width, y / height
            color = (24, 32, 52)
            if .18 < xn < .82 and .10 < yn < .45: color = (64, 80, 105)
            if .18 < xn < .82 and .55 < yn < .90: color = (225, 55, 60)
            rows.extend(color)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
png(root / 'icon.png', 48, 48)
png(root / 'banner.png', 256, 128)
with wave.open(str(root / 'banner.wav'), 'wb') as sound:
    sound.setparams((1, 2, 32000, 0, 'NONE', 'not compressed'))
    sound.writeframes(b'\0' * 6400)
