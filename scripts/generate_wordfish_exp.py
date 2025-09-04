#!/usr/bin/env python3
"""Generate a minimal Hypnos-compatible .exp file for Wordfish.

The output file contains the 42-byte "SugaR Experience version 2" header,
followed by a single 56-byte record with placeholder values. This avoids
shipping binary data in the repository while still allowing users to create
the required file.
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

    # Single placeholder record; slots 2/extra fields left empty
    record = struct.pack(
        '<14I',
        0x0000070C,  # move1
        1,           # visits1
        0, 0,        # key1_lo, key1_hi
        20,          # score1 (centipawns)
        10,          # depth1 (plies)
        0, 0, 0, 0,  # move2, visits2, key2_lo, key2_hi
        0, 0,        # score2, depth2
        0, 0         # extraA, extraB
    )

    with open(path, 'wb') as f:
        f.write(header)
        f.write(record)
    print(f"Wrote {path} ({len(header) + len(record)} bytes)")

if __name__ == '__main__':
    main()
