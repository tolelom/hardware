import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import ngspice_run  # noqa: E402

SPICE = ngspice_run.SPICE_DIR


@ngspice_run.requires_ngspice
class BenchTest(unittest.TestCase):
    def test_protection(self) -> None:
        r = ngspice_run.run(SPICE / "protection.cir",
                            ["v9_rev18", "v9_at9", "v9_at12", "v9_at15"])
        self.assertLess(abs(r["v9_rev18"]), 0.01)
        self.assertAlmostEqual(r["v9_at9"], 8.94, delta=0.03)
        self.assertGreater(r["v9_at12"], 11.9)
        self.assertLess(abs(r["v9_at15"]), 0.01)

    def test_doubler(self) -> None:
        r = ngspice_run.run(SPICE / "doubler.cir",
                            ["rail_avg", "rail_pp", "raw_pp", "t_90", "c8_avg", "c9_avg", "vbias_ratio"])
        self.assertTrue(16.40 < r["rail_avg"] < 16.65, r["rail_avg"])
        self.assertLess(r["rail_pp"], 50e-6)
        self.assertLess(r["raw_pp"], 3e-3)
        self.assertTrue(0.030 < r["t_90"] < 0.045, r["t_90"])
        self.assertGreater(r["c8_avg"], 1.0)
        self.assertGreater(r["c9_avg"], 1.0)
        self.assertAlmostEqual(r["vbias_ratio"], 0.5, delta=0.001)

    def test_output_stage(self) -> None:
        r = ngspice_run.run(SPICE / "output_stage.cir", ["out_pp", "out_pp_161"])
        self.assertAlmostEqual(r["out_pp"], 15.94, delta=0.1)
        self.assertAlmostEqual(r["out_pp_161"], 15.35, delta=0.1)

    def test_boot(self) -> None:
        r = ngspice_run.run(SPICE / "boot.cir", ["outc_max", "t_60dbu"])
        self.assertAlmostEqual(r["outc_max"], 5.91, delta=0.2)
        self.assertTrue(9.0 < r["t_60dbu"] < 9.8, r["t_60dbu"])

    def test_dc_polarity(self) -> None:
        r = ngspice_run.run(SPICE / "dc_polarity.cir", ["c2", "c4", "c5", "c6"])
        for name in ("c2", "c4", "c5", "c6"):
            with self.subTest(capacitor=name):
                self.assertGreater(r[name], 8.0)

    @unittest.skipUnless((SPICE / "vendor" / "opa2992.lib").exists(), "TI 모델 없음(spice/README.md)")
    def test_cable_ti(self) -> None:
        r = ngspice_run.run(SPICE / "cable_ti.cir", ["step_final", "step_max", "step47n_final", "step47n_max"])
        for final, peak in (("step_final", "step_max"), ("step47n_final", "step47n_max")):
            with self.subTest(case=final):
                overshoot = (r[peak] - r[final]) / abs(r[final] - 8.3)
                self.assertLess(overshoot, 0.02)


if __name__ == "__main__":
    unittest.main()
