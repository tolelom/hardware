import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import pinmap  # noqa: E402


class PinmapTest(unittest.TestCase):
    def test_table_shape(self) -> None:
        self.assertEqual(len(pinmap.SEED3_PINS), 31)
        self.assertEqual(pinmap.SEED3_PINS["D15"].adc, "A0")
        self.assertEqual(pinmap.SEED3_PINS["D28"].adc, "A11")
        self.assertIsNotNone(pinmap.SEED3_PINS["D29"].reserved)

    def test_platform_assignment_is_clean(self) -> None:
        self.assertEqual(pinmap.check(pinmap.PLATFORM), [])

    def test_detects_duplicate_and_wrong_capability(self) -> None:
        bad = dict(pinmap.PLATFORM)
        bad["FS1"] = bad["K1_DRIVE"]
        bad["EXP_ADC"] = "D0"
        bad["SPI1_SCK"] = "D16"
        bad["USB_SPARE"] = "D29"
        problems = " ".join(pinmap.check(bad))
        for word in ("중복", "ADC", "SPI1_SCK", "D29"):
            with self.subTest(word=word):
                self.assertIn(word, problems)

    def test_physical_pins_match_compressor_socket(self) -> None:
        from gen_schematic import BLOCKS
        socket = next(p for b in BLOCKS for p in b.parts if p.ref == "J4")
        for name in ("D0", "D1", "D10", "D15", "D20"):
            with self.subTest(pin=name):
                net = socket.nets[str(pinmap.SEED3_PINS[name].physical)]
                self.assertTrue(net.startswith(f"SEED_{name}_") or net.startswith(f"SEED_A"), net)

    def test_backlight_pwm_pin_has_timer(self) -> None:
        pin = pinmap.SEED3_PINS[pinmap.PLATFORM["DISPLAY_BL_PWM"]]
        self.assertTrue(any(f.startswith("TIM") for f in pin.functions))

    def test_net_name(self) -> None:
        self.assertEqual(pinmap.net("EXP_ADC"), "SEED_D15_EXP_ADC")
