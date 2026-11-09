"""Mutation regressions against the linked descriptor audit (no USB access)."""

import re
import unittest

from scripts import check_firmware


class DescriptorAuditTests(unittest.TestCase):
    """The audit must reject changes beyond the descriptor prefix."""

    def test_keyboard_report_count_mutation(self) -> None:
        """Changing six keyboard array bytes to five must fail the real audit."""
        build = check_firmware.ROOT / "build/ch552"
        map_path = build / "pedal.map"
        image = check_firmware.read_hex(build / "pedal.hex")
        check_firmware.check_descriptors(image, map_path)
        match = re.search(
            r"\b([0-9A-Fa-f]{8})\s+_ReportDescriptor\b",
            map_path.read_text(encoding="ascii"),
        )
        self.assertIsNotNone(match)
        assert match is not None
        start = int(match[1], 16)
        offset = check_firmware.KEYBOARD_DESCRIPTOR.index(b"\x95\x06") + 1
        self.assertEqual(image[start + offset], 6)
        image[start + offset] = 5
        with self.assertRaisesRegex(ValueError, "complete 8-byte boot report"):
            check_firmware.check_descriptors(image, map_path)


if __name__ == "__main__":
    unittest.main()
