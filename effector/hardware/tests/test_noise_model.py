import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import ngspice_run  # noqa: E402

BOLTZMANN = 1.380649e-23
KELVIN = 300.15


def thermal(ohm: float) -> float:
    return math.sqrt(4 * BOLTZMANN * KELVIN * ohm)


def noise_at_1k(body: str) -> float:
    text = "\n".join([".include opamp_rr_noise.inc", body, ".noise v(out) Vin dec 10 1k 1k",
                      ".control", "run", "setplot noise1", "print onoise_spectrum", ".endc", ".end"]) + "\n"
    return ngspice_run.run_text(text, expect=["onoise_spectrum"])["onoise_spectrum"]


@ngspice_run.requires_ngspice
class NoiseModelTest(unittest.TestCase):
    def test_single_resistor(self) -> None:
        value = noise_at_1k("Vin in 0 DC 0 AC 1\nRs in out 10k\nC1 out 0 1f")
        self.assertAlmostEqual(value, thermal(10e3), delta=thermal(10e3) * 0.01)

    def test_noninverting_gain_5_7(self) -> None:
        body = "\n".join(["Vcc vcc 0 16.9", "Vin inp 0 DC 8.45 AC 1",
                          "X1 inp fb out vcc 0 opamp_model en=7n", "R7 out fb 47k", "R8 fb fbc 10k",
                          "Vb fbc 0 8.45"])
        expected = math.sqrt((7e-9 * 5.7) ** 2 + (thermal(10e3) * 4.7) ** 2 + thermal(47e3) ** 2)
        self.assertAlmostEqual(noise_at_1k(body), expected, delta=expected * 0.01)

    def test_current_noise_into_source_resistance(self) -> None:
        body = "\n".join(["Vcc vcc 0 16.9", "Vin src 0 DC 8.45 AC 1", "Rs src inp 1meg noisy=0",
                          "X1 inp out out vcc 0 opamp_model en=1e-15 in=60f"])
        self.assertAlmostEqual(noise_at_1k(body), 60e-15 * 1e6, delta=60e-15 * 1e6 * 0.02)


if __name__ == "__main__":
    unittest.main()
