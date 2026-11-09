#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""Audit SDCC output addresses and descriptor/VIA identity without USB access."""

import argparse
import json
import re
from pathlib import Path

FLASH_LIMIT = 0x3800
XRAM_START = 266
XRAM_LIMIT = 1024
ROOT = Path(__file__).resolve().parent.parent
KEYBOARD_DESCRIPTOR = bytes.fromhex(
    "05 01 09 06 A1 01 05 07 19 E0 29 E7 15 00 25 01 95 08 75 01 81 02 "
    "95 01 75 08 81 03 95 06 75 08 15 00 26 FF 00 05 07 19 00 29 E7 81 00 "
    "05 08 19 01 29 05 15 00 25 01 95 05 75 01 91 02 95 01 75 03 91 03 C0"
)
RAW_DESCRIPTOR = bytes.fromhex(
    "06 60 FF 09 61 A1 01 09 62 15 00 26 FF 00 95 20 75 08 81 02 "
    "09 63 91 02 C0"
)


def read_hex(path: Path) -> dict[int, int]:
    """Read Intel HEX, rejecting bad checksums and conflicting/addressed data."""
    image: dict[int, int] = {}
    base = 0
    ended = False
    for number, line in enumerate(path.read_text(encoding="ascii").splitlines(), 1):
        if ended or not line.startswith(":"):
            raise ValueError(f"{path}:{number}: data after EOF or missing colon")
        record = bytes.fromhex(line[1:])
        if len(record) < 5 or len(record) != record[0] + 5 or sum(record) & 255:
            raise ValueError(f"{path}:{number}: invalid record size/checksum")
        size = record[0]
        address = int.from_bytes(record[1:3], "big")
        kind = record[3]
        data = record[4:-1]
        if kind == 0:
            for offset, value in enumerate(data):
                absolute = base + address + offset
                if absolute >= FLASH_LIMIT:
                    raise ValueError(f"{path}:{number}: protected address 0x{absolute:04X}")
                if absolute in image and image[absolute] != value:
                    raise ValueError(f"{path}:{number}: conflicting address 0x{absolute:04X}")
                image[absolute] = value
        elif kind == 1 and size == 0:
            ended = True
        elif kind in (2, 4) and size == 2:
            base = int.from_bytes(data, "big") << (4 if kind == 2 else 16)
        else:
            raise ValueError(f"{path}:{number}: unexpected HEX record type {kind}")
    if not ended or not image:
        raise ValueError(f"{path}: missing EOF or empty image")
    return image


def check_map(path: Path) -> list[str]:
    """Verify SDCC map's relocatable XRAM and absolute DMA buffer placement."""
    text = path.read_text(encoding="ascii")
    lines = []
    for symbol, expected in (("Ep0Buffer", 0), ("Ep1Buffer", 10), ("Ep2Buffer", 138)):
        match = re.search(rf"\b([0-9A-Fa-f]{{8}})\s+_{symbol}\b", text)
        if not match or int(match[1], 16) != expected:
            raise ValueError(f"{path}: incorrect or missing {symbol} at {expected}")
    areas = re.findall(
        r"^\s*(XSEG|XISEG)\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\b", text, re.MULTILINE
    )
    if not areas:
        raise ValueError(f"{path}: no SDCC XRAM areas found")
    maximum = XRAM_START
    for name, start_text, size_text in areas:
        start, size = int(start_text, 16), int(size_text, 16)
        if size and (start < XRAM_START or start + size > XRAM_LIMIT):
            raise ValueError(f"{path}: {name} collides with DMA or exceeds XRAM")
        maximum = max(maximum, start + size)
        lines.append(f"{name}: 0x{start:04X}..0x{start + size:04X} ({size} bytes)")
    lines.append(f"DMA: [0,8), [10,138), [138,266); XRAM end 0x{maximum:04X}/0x0400")
    return lines


def check_identity() -> str:
    """Require VIA's development IDs/matrix to match the firmware constants."""
    config = (ROOT / "firmware/ch552/board_config.h").read_text(encoding="utf-8")
    definition = json.loads((ROOT / "via/pedal.json").read_text(encoding="utf-8"))
    for constant, field in (("USB_VID", "vendorId"), ("USB_PID", "productId")):
        match = re.search(rf"^#define {constant}\s+(0x[0-9A-Fa-f]+)\b", config, re.MULTILINE)
        if not match or int(match[1], 16) != int(definition[field], 16):
            raise ValueError(f"VIA {field} differs from {constant}")
    if definition["matrix"] != {"rows": 1, "cols": 1}:
        raise ValueError("VIA matrix must be 1x1")
    if definition["layouts"]["keymap"] != [["0,0"]]:
        raise ValueError("VIA pedal must occupy coordinate 0,0")
    return f"VIA: {definition['vendorId']}:{definition['productId']}, matrix 1x1"


def check_descriptors(image: dict[int, int], map_path: Path) -> str:
    """Verify the linked descriptor bytes, not just source-level declarations."""
    text = map_path.read_text(encoding="ascii")

    def descriptor(symbol: str, length: int) -> bytes:
        match = re.search(rf"\b([0-9A-Fa-f]{{8}})\s+_{symbol}\b", text)
        if not match:
            raise ValueError(f"{map_path}: missing {symbol}")
        start = int(match[1], 16)
        return bytes(image[start + offset] for offset in range(length))

    device = descriptor("DeviceDescriptor", 18)
    definition = json.loads((ROOT / "via/pedal.json").read_text(encoding="utf-8"))
    if device[:2] != b"\x12\x01" or device[7] != 8:
        raise ValueError("linked device descriptor has wrong type/EP0 size")
    if int.from_bytes(device[8:10], "little") != int(definition["vendorId"], 16):
        raise ValueError("linked VID differs from VIA")
    if int.from_bytes(device[10:12], "little") != int(definition["productId"], 16):
        raise ValueError("linked PID differs from VIA")
    header = descriptor("ConfigurationDescriptor", 9)
    total = int.from_bytes(header[2:4], "little")
    config = descriptor("ConfigurationDescriptor", total)
    if header[:2] != b"\x09\x02" or header[4] != 2 or total != 73:
        raise ValueError("linked configuration must contain two HID interfaces")
    interfaces: list[bytes] = []
    endpoints: dict[int, int] = {}
    hid_lengths: list[int] = []
    offset = 9
    while offset < total:
        size, kind = config[offset : offset + 2]
        if size < 2 or offset + size > total:
            raise ValueError("malformed linked configuration descriptor")
        item = config[offset : offset + size]
        if kind == 4:
            interfaces.append(item)
        elif kind == 5:
            endpoints[item[2]] = int.from_bytes(item[4:6], "little")
        elif kind == 0x21:
            hid_lengths.append(int.from_bytes(item[7:9], "little"))
        offset += size
    if [item[2:8] for item in interfaces] != [b"\x00\x00\x02\x03\x01\x01", b"\x01\x00\x02\x03\x00\x00"]:
        raise ValueError("linked interfaces are not boot keyboard + Raw HID")
    if endpoints != {0x81: 8, 0x01: 8, 0x82: 32, 0x02: 32}:
        raise ValueError("linked endpoint addresses/sizes differ from DMA contract")
    if len(hid_lengths) != 2:
        raise ValueError("missing linked HID descriptors")
    keyboard = descriptor("ReportDescriptor", hid_lengths[0])
    raw = descriptor("RawHIDReportDescriptor", hid_lengths[1])
    if keyboard != KEYBOARD_DESCRIPTOR:
        raise ValueError("keyboard descriptor differs from the complete 8-byte boot report contract")
    if raw != RAW_DESCRIPTOR:
        raise ValueError("Raw HID descriptor differs from the complete FF60/61 32-byte contract")
    return "Linked descriptors: boot keyboard 8 bytes, Raw HID FF60/61 32 bytes"


def stack_placement(path: Path) -> str:
    """Report linker-available stack space, without claiming dynamic high-water."""
    text = path.read_text(encoding="ascii")
    match = re.search(r"Stack starts at: (0x[0-9a-f]+).*with (\d+) bytes available", text)
    if not match:
        raise ValueError(f"{path}: missing stack placement")
    return f"Stack: {match[1]}, {match[2]} linker-available bytes (not a runtime high-water)"


def main() -> None:
    """Check image, interrupt vectors, memory regions, and VIA IDs; emit evidence."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/ch552")
    args = parser.parse_args()
    build = args.build_dir
    image = read_hex(build / "pedal.ihx")
    if read_hex(build / "pedal.hex") != image:
        raise ValueError("packed HEX and linked IHX differ")
    map_text = (build / "pedal.map").read_text(encoding="ascii")
    for name, vector in (("reset", 0), ("Timer2", 0x2B), ("USB", 0x43)):
        if image.get(vector) != 0x02:
            raise ValueError(f"missing LJMP at {name} vector 0x{vector:02X}")
        destination = (image[vector + 1] << 8) | image[vector + 2]
        if destination not in image:
            raise ValueError(f"{name} vector does not target linked code")
        if name != "reset":
            symbol = "timer2_interrupt" if name == "Timer2" else "device_usb_interrupt"
            match = re.search(rf"\b([0-9A-Fa-f]{{8}})\s+_{symbol}\b", map_text)
            if not match or destination != int(match[1], 16):
                raise ValueError(f"{name} vector does not target {symbol}")
    evidence = [
        f"Flash: {len(image)} emitted bytes; span {max(image) + 1} bytes; "
        f"highest 0x{max(image):04X}; limit 0x3800",
        "Linked vectors: reset 0x00, Timer2 0x2B, USB 0x43",
        *check_map(build / "pedal.map"),
        stack_placement(build / "pedal.mem"),
        check_identity(),
        check_descriptors(image, build / "pedal.map"),
    ]
    report = "\n".join(evidence) + "\n"
    (build / "bounds.txt").write_text(report, encoding="utf-8")
    print(report, end="")


if __name__ == "__main__":
    main()
