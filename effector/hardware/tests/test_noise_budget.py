import math
import sys
import tempfile
import unittest
from pathlib import Path

HARDWARE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HARDWARE))
sys.path.insert(0, str(HARDWARE / "spice"))

import noise_budget as nb  # noqa: E402


def log_grid(lo: float, hi: float, per_decade: int) -> list[float]:
    count = int(round(math.log10(hi / lo) * per_decade))
    return [lo * 10 ** (i / per_decade) for i in range(count + 1)]


class WeightingTest(unittest.TestCase):
    def test_a_weight_reference_points(self) -> None:
        self.assertAlmostEqual(10 * math.log10(nb.a_weight(1000)), 0.0, delta=0.01)
        self.assertAlmostEqual(10 * math.log10(nb.a_weight(100)), -19.1, delta=0.1)
        self.assertAlmostEqual(10 * math.log10(nb.a_weight(10000)), -2.5, delta=0.1)

    def test_white_ratio_a_vs_band(self) -> None:
        ratio = nb.white_ratio(nb.WEIGHTINGS["A"], nb.WEIGHTINGS["125-8000"])
        self.assertAlmostEqual(20 * math.log10(ratio), 2.12, delta=0.02)


class IntegrateTest(unittest.TestCase):
    def test_white_noise_band_on_log_grid(self) -> None:
        frequencies = log_grid(20, 24000, 50)
        density = [10e-9] * len(frequencies)
        vrms = nb.integrate(frequencies, density, nb.WEIGHTINGS["20-20k"])
        expected = 10e-9 * math.sqrt(20000 - 20)
        self.assertAlmostEqual(20 * math.log10(vrms / expected), 0.0, delta=0.1)

    def test_sparse_grid_stays_within_0_1_db(self) -> None:
        frequencies = log_grid(20, 24000, 10)
        density = [10e-9] * len(frequencies)
        exact = 10e-9 * math.sqrt(nb.white_power(nb.WEIGHTINGS["A"]))
        vrms = nb.integrate(frequencies, density, nb.WEIGHTINGS["A"])
        self.assertAlmostEqual(20 * math.log10(vrms / exact), 0.0, delta=0.1)


class ZoneTest(unittest.TestCase):
    def test_boundaries(self) -> None:
        self.assertEqual(nb.t7_zone(105.0), "통과")
        self.assertEqual(nb.t7_zone(104.99), "회색")
        self.assertEqual(nb.t7_zone(98.0), "회색")
        self.assertEqual(nb.t7_zone(97.99), "미달")


class ReadSpectrumTest(unittest.TestCase):
    def test_reads_named_columns(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "s.txt"
            path.write_text(" frequency onoise_spectrum onoise_r7\n 1.0e+03 2.0e-08 1.0e-08\n"
                            " 2.0e+03 3.0e-08 1.5e-08\n", encoding="utf-8")
            spectrum = nb.read_spectrum(path)
        self.assertEqual(spectrum.frequencies, [1000.0, 2000.0])
        self.assertEqual(spectrum.vectors["onoise_r7"], [1e-8, 1.5e-8])


if __name__ == "__main__":
    unittest.main()
