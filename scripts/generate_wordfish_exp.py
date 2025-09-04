#!/usr/bin/env python3
"""Generate an empty SugaR/BrainLearn v2 experience file.

The generated file contains the full 256-byte "SugaR Experience version 2"
header followed by a zero-filled table of 65,536 entries in the v2 layout (34
bytes per entry).  This matches the format used by Revolution and allows
external tools such as HypnoS to recognise the file even before any experience
data is stored.
"""

import struct

def main(path: str = "wordfish.exp") -> None:
    entry_size = 34
    table_size = 1 << 16
    table_bytes = entry_size * table_size

    header = bytearray(256)
    header[:26] = b"SugaR Experience version 2"
    struct.pack_into('<H', header, 26, 2)  # version
    struct.pack_into('<Q', header, 28, 0x06103380A463E280)  # seed/uuid
    struct.pack_into('<H', header, 36, 256)  # header size
    struct.pack_into('<I', header, 38, table_bytes)

    with open(path, 'wb') as f:
        f.write(header)
        f.write(b"\x00" * table_bytes)
    print(f"Wrote {path} ({256 + table_bytes} bytes)")

if __name__ == '__main__':
    main()
