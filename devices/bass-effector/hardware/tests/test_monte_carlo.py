import random
import sys
import tempfile
import unittest
from pathlib import Path

HARDWARE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HARDWARE))
sys.path.insert(0, str(HARDWARE / "spice"))

import monte_carlo as mc  # noqa: E402
import ngspice_run  # noqa: E402
from gen_schematic import BLOCKS  # noqa: E402


class ToleranceTest(unittest.TestCase):
    def test_every_r_and_c_has_tolerance(self) -> None:
        for block in BLOCKS:
            for p in block.parts:
                if p.lib == "Device" and p.name in ("R", "C", "C_Polarized"):
                    with self.subTest(ref=p.ref):
                        self.assertIn(p.ref, mc.TOLERANCES)
        self.assertEqual(mc.TOLERANCES["R32"], 0.05)
        self.assertEqual(mc.TOLERANCES["C23"], 0.05)
        self.assertEqual(mc.TOLERANCES["C7"], 0.20)
        self.assertEqual(mc.TOLERANCES["R7"], 0.01)

    def test_seed_reproducible_and_bounded(self) -> None:
        a = mc.draw(random.Random(7))
        b = mc.draw(random.Random(7))
        self.assertEqual(a, b)
        for ref, scale in a.scales.items():
            self.assertLessEqual(abs(scale - 1), mc.TOLERANCES[ref] + 1e-12)
        self.assertTrue(8.7 <= a.adapter_v <= 9.6)
        self.assertTrue(4000 <= a.codec_ohm <= 6000)

    def test_zero_sample_is_nominal(self) -> None:
        zero = mc.draw(random.Random(1), zero=True)
        self.assertTrue(all(scale == 1.0 for scale in zero.scales.values()))
        self.assertEqual(zero.lt1054_ron, 25.0)
        self.assertEqual(zero.codec_ohm, 5000.0)

    def test_lt1054_range(self) -> None:
        self.assertEqual(mc.LT1054_RON_DATASHEET, (7.9, 12.7))
        for _ in range(50):
            sample = mc.draw(random.Random(_))
            self.assertTrue(7.9 <= sample.lt1054_ron <= 12.7)
        fixed = mc.draw(random.Random(3), ron_range=(25.0, 25.0))
        self.assertEqual(fixed.lt1054_ron, 25.0)

    def test_schottky_is(self) -> None:
        self.assertAlmostEqual(mc.schottky_is(0.3125), 1e-8, delta=1e-9)


@ngspice_run.requires_ngspice
class SimulateTest(unittest.TestCase):
    def test_nominal_matches_benches(self) -> None:
        with tempfile.TemporaryDirectory(dir=ngspice_run.SPICE_DIR) as directory:
            result = mc.simulate(mc.draw(random.Random(1), zero=True), Path(directory))
        self.assertTrue(16.0 < result["rail_min"] < 16.7, result)
        self.assertAlmostEqual(result["pad_db"], -15.0, delta=0.6)
        self.assertGreater(result["out_dbu"], 16.5)
        self.assertAlmostEqual(result["vbias_ratio"], 0.5, delta=0.002)


if __name__ == "__main__":
    unittest.main()
