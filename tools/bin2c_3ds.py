#!/usr/bin/env python3
"""Emit byte arrays without depending on host hexdump implementations."""
import pathlib
import sys
source, destination = map(pathlib.Path, sys.argv[1:])
data = source.read_bytes()
destination.write_text("\n".join(",".join(f"0x{b:02x}" for b in data[i:i + 16]) + ","
                                  for i in range(0, len(data), 16)) + "\n")
