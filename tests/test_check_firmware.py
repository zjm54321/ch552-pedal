"""Mutation regressions against the linked descriptor audit (no USB access)."""

import re
import unittest
from pathlib import Path

from scripts import check_firmware


class DescriptorAuditTests(unittest.TestCase):
    """The audit must reject changes beyond the descriptor prefix."""

    VARIANTS = (("test", "pedal.json"), ("is0", "is0.json"))

    @staticmethod
    def device_descriptor_start(map_path: Path) -> int:
        """Locate the linked device descriptor without consulting its source."""
        match = re.search(
            r"\b([0-9A-Fa-f]{8})\s+_DeviceDescriptor\b",
            map_path.read_text(encoding="ascii"),
        )
        assert match is not None
        return int(match[1], 16)

    def test_keyboard_report_count_mutation(self) -> None:
        """Changing six keyboard array bytes to five must fail the real audit."""
        for variant, definition in self.VARIANTS:
            with self.subTest(variant=variant):
                build = check_firmware.ROOT / "build/ch552"
                map_path = build / variant / "pedal.map"
                definition_path = check_firmware.ROOT / "via" / definition
                image = check_firmware.read_hex(build / f"pedal-{variant}.hex")
                check_firmware.check_descriptors(image, map_path, definition_path)
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
                    check_firmware.check_descriptors(image, map_path, definition_path)

    def test_linked_id_mutations(self) -> None:
        """Every image must match its own definition, including both USB IDs."""
        build = check_firmware.ROOT / "build/ch552"
        for variant, definition in self.VARIANTS:
            map_path = build / variant / "pedal.map"
            definition_path = check_firmware.ROOT / "via" / definition
            self.assertIn("matrix 1x1", check_firmware.check_identity(definition_path))
            for offset, field in ((8, "VID"), (10, "PID")):
                with self.subTest(variant=variant, field=field):
                    image = check_firmware.read_hex(build / f"pedal-{variant}.hex")
                    start = self.device_descriptor_start(map_path)
                    image[start + offset] ^= 1
                    with self.assertRaisesRegex(ValueError, f"linked {field} differs from VIA"):
                        check_firmware.check_descriptors(image, map_path, definition_path)

    def test_wrong_variant_definition(self) -> None:
        """A test image cannot pass an is0 audit, or vice versa."""
        build = check_firmware.ROOT / "build/ch552"
        for variant, definition in (("test", "is0.json"), ("is0", "pedal.json")):
            with self.subTest(variant=variant):
                image = check_firmware.read_hex(build / f"pedal-{variant}.hex")
                with self.assertRaisesRegex(ValueError, "linked VID differs from VIA"):
                    check_firmware.check_descriptors(
                        image, build / variant / "pedal.map", check_firmware.ROOT / "via" / definition
                    )


if __name__ == "__main__":
    unittest.main()
