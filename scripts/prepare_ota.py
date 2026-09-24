"""
Prepare ZMK firmware binary for Telink OTA flashing.

Patches:
  1. Offset 0x06: CRC type field = 0x5D 0x02 (required by Telink boot ROM)
  2. Offset 0x18: OTA size field (total size including CRC32 trailer)
  3. Body 16-byte alignment (pads with 0xFF so body is a multiple of 16)
  4. Appends Telink CRC32 trailer (no final XOR)
"""
import sys
import struct
import argparse
import zlib
import os

TLNK_MAGIC = b'\x4b\x4e\x4c\x54'  # "KNLT" at offset 0x20
OTA_SIZE_OFFSET = 0x18
TLNK_OFFSET = 0x20
CRC_TYPE_OFFSET = 0x06  # Boot ROM "CRC type" field — must match stock value
CRC_TYPE_VALUE = b'\x5d\x02'  # Value from stock Evision firmware
CALIBRATION_START = 0xFE000  # Must not overwrite calibration/MAC


def telink_crc32(data: bytes) -> int:
    """CRC32 as used by Telink OTA: standard poly, init=0xFFFFFFFF, no final XOR."""
    return (zlib.crc32(data) & 0xFFFFFFFF) ^ 0xFFFFFFFF


def show_info(data: bytes, filename: str):
    """Display Telink boot header information."""
    print(f"File: {filename}")
    print(f"  Size: {len(data)} bytes (0x{len(data):X})")

    # Check TLNK magic
    if len(data) > TLNK_OFFSET + 4:
        magic = data[TLNK_OFFSET:TLNK_OFFSET + 4]
        has_tlnk = magic == TLNK_MAGIC
        print(f"  TLNK magic at 0x20: {magic.hex()} ({'OK' if has_tlnk else 'MISSING'})")
    else:
        print(f"  TLNK magic: file too small")
        has_tlnk = False

    # OTA size field
    if len(data) > OTA_SIZE_OFFSET + 4:
        ota_size = struct.unpack_from('<I', data, OTA_SIZE_OFFSET)[0]
        print(f"  OTA size at 0x18: {ota_size} bytes (0x{ota_size:X})")
        if ota_size == 0:
            print(f"    -> Unset (needs patching for OTA)")
        elif ota_size == len(data):
            print(f"    -> Matches file size")
        elif ota_size != len(data):
            print(f"    -> Does NOT match file size ({len(data)})")

    # CRC32 at end (if OTA size is set and valid)
    if has_tlnk and ota_size > 4 and ota_size <= len(data):
        crc_stored = struct.unpack_from('<I', data, ota_size - 4)[0]
        print(f"  CRC32 at 0x{ota_size - 4:X}: 0x{crc_stored:08X}")
        if crc_stored in (0, 0xFFFFFFFF):
            print(f"    -> Looks invalid (placeholder)")
        else:
            # Verify CRC32 (Telink variant: no final XOR)
            crc_computed = telink_crc32(data[:ota_size - 4])
            if crc_computed == crc_stored:
                print(f"    -> CRC32 VALID (matches computed)")
            else:
                print(f"    -> CRC32 mismatch (computed: 0x{crc_computed:08X})")

    # Boot vector
    if len(data) >= 4:
        boot_word = struct.unpack_from('<I', data, 0)[0]
        print(f"  Boot vector at 0x00: 0x{boot_word:08X}")

    # Offset 0x26 marker
    if len(data) > 0x28:
        marker = struct.unpack_from('<H', data, 0x26)[0]
        print(f"  Marker at 0x26: 0x{marker:04X}")

    # OTA segment count and alignment
    body_size = ota_size - 4 if has_tlnk and ota_size > 4 else len(data)
    aligned = body_size % 16 == 0
    total_segments = (len(data) + 15) // 16
    print(f"  OTA segments: {total_segments} (u16 max: 65535)")
    print(f"  Body 16-byte aligned: {'Yes' if aligned else 'NO — OTA will fail with 0x0B'}")
    if total_segments > 65535:
        print(f"    -> WARNING: exceeds u16 index range!")

    print()


def prepare_ota(input_path: str, output_path: str, no_crc: bool = False):
    """Prepare a firmware image for OTA flashing."""
    with open(input_path, 'rb') as f:
        data = bytearray(f.read())

    print(f"Input: {input_path} ({len(data)} bytes)")

    # Verify TLNK magic
    if len(data) < TLNK_OFFSET + 4:
        print("ERROR: File too small to contain Telink boot header")
        sys.exit(1)

    magic = bytes(data[TLNK_OFFSET:TLNK_OFFSET + 4])
    if magic != TLNK_MAGIC:
        print(f"ERROR: TLNK magic not found at offset 0x20")
        print(f"  Expected: {TLNK_MAGIC.hex()}")
        print(f"  Found:    {magic.hex()}")
        print(f"  This doesn't look like a Telink/Zephyr firmware binary.")
        sys.exit(1)
    print(f"  TLNK magic at 0x20: OK")

    # Patch CRC type field at offset 0x06 to match stock firmware.
    if data[CRC_TYPE_OFFSET:CRC_TYPE_OFFSET + 2] != CRC_TYPE_VALUE:
        data[CRC_TYPE_OFFSET:CRC_TYPE_OFFSET + 2] = CRC_TYPE_VALUE
        print(f"  CRC type at 0x06: patched to {CRC_TYPE_VALUE.hex()}")
    else:
        print(f"  CRC type at 0x06: already correct")

    # Safety check: image must not overwrite calibration
    if len(data) >= CALIBRATION_START:
        print(f"ERROR: Image too large ({len(data)} bytes)")
        print(f"  Would overwrite calibration at 0x{CALIBRATION_START:X}")
        sys.exit(1)

    # Pad firmware body to 16-byte alignment before CRC trailer.
    pad_needed = (16 - (len(data) % 16)) % 16
    if pad_needed:
        data.extend(bytes([0xFF] * pad_needed))
        print(f"  Padded {pad_needed} bytes to 16-byte align body ({len(data)} bytes)")

    if no_crc:
        total_size = len(data)
        struct.pack_into('<I', data, OTA_SIZE_OFFSET, total_size)
        print(f"  OTA size at 0x18: {total_size} (0x{total_size:X})")
        print(f"  CRC32: skipped (--no-crc)")
    else:
        total_size = len(data) + 4
        struct.pack_into('<I', data, OTA_SIZE_OFFSET, total_size)
        print(f"  OTA size at 0x18: {total_size} (0x{total_size:X})")

        crc = telink_crc32(bytes(data))
        data.extend(struct.pack('<I', crc))
        print(f"  CRC32 appended: 0x{crc:08X}")

    total_segments = (len(data) + 15) // 16
    print(f"  Total size: {len(data)} bytes, {total_segments} OTA segments")

    with open(output_path, 'wb') as f:
        f.write(data)
    print(f"\nOutput: {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Prepare ZMK image for OTA flashing')
    parser.add_argument('input', help='Input firmware binary')
    parser.add_argument('--info', action='store_true',
                        help='Show header info only, do not modify')
    parser.add_argument('--no-crc', action='store_true',
                        help='Skip CRC32 trailer')
    parser.add_argument('-o', '--output', default=None,
                        help='Output file path')
    args = parser.parse_args()

    if not os.path.isfile(args.input):
        print(f"ERROR: File not found: {args.input}")
        sys.exit(1)

    with open(args.input, 'rb') as f:
        data = f.read()

    if args.info:
        show_info(data, args.input)
        return

    output_path = args.output or args.input
    prepare_ota(args.input, output_path, no_crc=args.no_crc)


if __name__ == '__main__':
    main()
