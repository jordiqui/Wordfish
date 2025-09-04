#!/usr/bin/env python3
"""Generate an empty SugaR/BrainLearn v2 experience file.

The generated file contains the 42-byte "SugaR Experience version 2" header
followed by a zero-filled table of 65,536 entries in the v2 layout (34 bytes
per entry).  This matches the format used by Revolution and allows external
tools such as HypnoS to recognise the file even before any experience data is
stored.
"""

import struct

def main(path: str = "wordfish.exp") -> None:
    # 26-byte ASCII string + required 16-byte binary block
    header = b"SugaR Experience version 2" + bytes(
        [0x02, 0x00, 0x80, 0xE2,
         0x63, 0xA4, 0x80, 0x33,
         0x10, 0x06, 0x00, 0x00,
         0x22, 0x00, 0x00, 0x00]
    )

    entry_size = 34
    table_size = 1 << 16

    with open(path, 'wb') as f:
        f.write(header)
        f.write(b"\x00" * (entry_size * table_size))
    print(f"Wrote {path} ({len(header) + entry_size * table_size} bytes)")

if __name__ == '__main__':
    main()
