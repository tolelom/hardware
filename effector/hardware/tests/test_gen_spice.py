import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import gen_spice  # noqa: E402
from gen_schematic import BLOCKS, Part  # noqa: E402


def part(ref: str, lib: str, name: str, value: str, nets: dict[str, str], unit: int = 1) -> Part:
    return Part(ref, lib, name, value, nets, unit)


class ParseValueTest(unittest.TestCase):
    def test_suffixes(self) -> None:
        cases = {"4.7k": 4.7e3, "10R": 10.0, "3.3R": 3.3, "2.2M": 2.2e6, "1000u 25V": 1e-3,
                 "100n 필름": 1e-7, "4.7n": 4.7e-9, "820R": 820.0, "1.8M": 1.8e6}
        for text, expected in cases.items():
            with self.subTest(text=text):
                self.assertAlmostEqual(gen_spice.parse_value(text), expected, delta=expected * 1e-12)

    def test_unparsable(self) -> None:
        for text in ("abc", "", "4k7", "ohm 10"):
            with self.subTest(text=text):
                with self.assertRaises(gen_spice.SpiceError):
                    gen_spice.parse_value(text)


class PartLinesTest(unittest.TestCase):
    power = {"U1": ("vrail", "0")}

    def lines(self, p: Part, scale: float = 1.0) -> list[str]:
        return gen_spice.part_lines(p, self.power, scale)

    def test_resistor_and_scale(self) -> None:
        r = part("R7", "Device", "R", "47k", {"1": "U1B_OUT", "2": "FB"})
        self.assertEqual(self.lines(r), ["R7 u1b_out fb 47000"])
        self.assertEqual(self.lines(r, 1.01), ["R7 u1b_out fb 47470"])

    def test_capacitor_first_token(self) -> None:
        c = part("C7", "Device", "C_Polarized", "1000u 25V", {"1": "V9", "2": "GND"})
        self.assertEqual(self.lines(c), ["C7 v9 0 0.001"])

    def test_diode_anode_first(self) -> None:
        d = part("D1", "Device", "D_Schottky", "1N5819", {"1": "PUMPX", "2": "V9"})
        self.assertEqual(self.lines(d), ["D1 v9 pumpx D1N5819"])
        z = part("D3", "Device", "D_Zener", "1N4742A 12V", {"1": "ZK", "2": "GND"})
        self.assertEqual(self.lines(z), ["D3 0 zk DZ12"])
        led = part("D5", "Device", "LED", "3mm 미터 1", {"1": "GND", "2": "LED1_A"})
        self.assertEqual(self.lines(led), ["D5 led1_a 0 DLEDR"])

    def test_bjt_collector_base_emitter(self) -> None:
        q = part("Q3", "Transistor_BJT", "2N3906", "2N3906", {"1": "VS", "2": "OV_B", "3": "VG"})
        self.assertEqual(self.lines(q), ["Q3 vg ov_b vs Q2N3906"])

    def test_mosfet_subcircuit(self) -> None:
        m = part("Q1", "Transistor_FET", "IRF9540N", "IRF9540N", {"1": "VG", "2": "VIN_RAW", "3": "VS"})
        self.assertEqual(self.lines(m), ["XQ1 vin_raw vg vs IRF9540N"])

    def test_opamp_units(self) -> None:
        a = part("U1", "Amplifier_Operational", "TL072", "OPA2992", {"3": "IN_C", "2": "BUF_OUT", "1": "BUF_OUT"}, 1)
        b = part("U1", "Amplifier_Operational", "TL072", "OPA2992", {"5": "OUT_IN", "6": "FB", "7": "U1B_OUT"}, 2)
        p = part("U1", "Amplifier_Operational", "TL072", "OPA2992", {"8": "VRAIL", "4": "GND"}, 3)
        self.assertEqual(self.lines(a), ["XU1A in_c buf_out buf_out vrail 0 opamp_model"])
        self.assertEqual(self.lines(b), ["XU1B out_in fb u1b_out vrail 0 opamp_model"])
        self.assertEqual(self.lines(p), [])

    def test_lt1054(self) -> None:
        u = part("U2", "Regulator_SwitchedCapacitor", "LT1054", "LT1054CP",
                 {"8": "V9", "3": "GND", "5": "GND", "2": "PUMP", "1": "NC", "4": "NC", "6": "NC", "7": "NC"})
        self.assertEqual(self.lines(u), ["XU2 v9 pump 0 lt1054"])

    def test_relay(self) -> None:
        k = part("K1", "Relay", "Relay_DPDT", "TQ2-9V",
                 {"A1": "V9", "A2": "K1_COIL", "11": "IN_TIP", "12": "BYPASS", "14": "IN_A",
                  "21": "OUT_TIP", "22": "BYPASS", "24": "OUT_C"})
        self.assertEqual(self.lines(k), [
            "RK1_COIL v9 k1_coil 579",
            "RK1_11_12 in_tip bypass {k1_on > 0.5 ? 1e+12 : 0.05}",
            "RK1_11_14 in_tip in_a {k1_on > 0.5 ? 0.05 : 1e+12}",
            "RK1_21_22 out_tip bypass {k1_on > 0.5 ? 1e+12 : 0.05}",
            "RK1_21_24 out_tip out_c {k1_on > 0.5 ? 0.05 : 1e+12}",
        ])

    def test_potentiometer(self) -> None:
        rv = part("RV1", "Device", "R_Potentiometer", "B10K Input",
                  {"1": "SEED_3V3A", "2": "SEED_A0_INPUT", "3": "GND"})
        self.assertEqual(self.lines(rv), [
            "RRV1_A seed_3v3a seed_a0_input {max(1, 10000 * (1 - pos_rv1))}",
            "RRV1_B seed_a0_input 0 {max(1, 10000 * pos_rv1)}",
        ])

    def test_skipped_symbols(self) -> None:
        for lib, name in gen_spice.SKIPPED:
            with self.subTest(name=name):
                self.assertEqual(self.lines(part("J9", lib, name, "x", {"1": "A", "2": "B"})), [])

    def test_unknown_symbol_fails(self) -> None:
        with self.assertRaises(gen_spice.SpiceError):
            self.lines(part("U9", "Amplifier_Operational", "NE5532", "NE5532", {"1": "A"}))

    def test_unknown_diode_value_fails(self) -> None:
        with self.assertRaises(gen_spice.SpiceError):
            self.lines(part("D9", "Device", "D", "1N4007", {"1": "A", "2": "B"}))


class BlocksTest(unittest.TestCase):
    def test_every_blocks_part_maps(self) -> None:
        power = gen_spice.opamp_power(BLOCKS)
        self.assertEqual(power, {"U1": ("vrail", "0")})
        for block in BLOCKS:
            for p in block.parts:
                gen_spice.part_lines(p, power)


if __name__ == "__main__":
    unittest.main()
