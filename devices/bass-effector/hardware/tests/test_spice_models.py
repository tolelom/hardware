import contextlib
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import gen_spice  # noqa: E402
import ngspice_run  # noqa: E402
from gen_schematic import BLOCKS, Part  # noqa: E402


class GeneratedFreshTest(unittest.TestCase):
    def test_generated_files_match_blocks(self) -> None:
        expected = gen_spice.blocks_hash()
        files = sorted(gen_spice.GENERATED.glob("*.cir"))
        self.assertTrue(files, "spice/generated/가 비어 있다. python3 devices/bass-effector/hardware/gen_spice.py 실행")
        for path in files:
            with self.subTest(path=path.name):
                header = path.read_text(encoding="utf-8").splitlines()[2]
                self.assertEqual(header, f"* BLOCKS {expected}",
                                 "BLOCKS가 바뀌었다. python3 devices/bass-effector/hardware/gen_spice.py로 다시 생성")

    def test_generated_files_match_regeneration(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            fresh = {path.name: path.read_text(encoding="utf-8") for path in gen_spice.generate(Path(directory))}
        committed = {path.name: path.read_text(encoding="utf-8") for path in gen_spice.GENERATED.glob("*.cir")}
        self.assertEqual(sorted(committed), sorted(fresh))
        for name, text in fresh.items():
            with self.subTest(name=name):
                self.assertEqual(committed[name], text, "생성물이 BLOCKS·gen_spice.py와 다르다. gen_spice.py로 다시 생성")

    def test_net_preservation(self) -> None:
        power = gen_spice.opamp_power(BLOCKS)
        for block in BLOCKS:
            expected = set()
            for p in block.parts:
                if (p.lib, p.name) in gen_spice.SKIPPED or (p.name == "TL072" and p.unit == 3):
                    continue
                expected |= {gen_spice.node(n) for n in p.nets.values() if n != "NC"}
            emitted = set()
            for p in block.parts:
                for line in gen_spice.part_lines(p, power):
                    fields = line.split()
                    count = {"X": len(fields) - 2, "Q": 3}.get(line[0], 2)
                    emitted |= set(fields[1:1 + count])
            if any(p.name == "TL072" and p.unit != 3 for p in block.parts):
                emitted -= {"vrail"} - expected
            with self.subTest(block=block.title[0]):
                self.assertEqual(emitted, expected)


class ErrorLinesTest(unittest.TestCase):
    def test_flags_errors_and_aborts(self) -> None:
        for line in ("Error on line 3 : X1 a b nosuch", "Error: measure  pad_db  find(AT) : out of interval",
                     "doAnalyses: TRAN:  Timestep too small; time = 1e-9", "tran simulation(s) aborted",
                     "Warning from checkvalid: vector v(nosuch) is not available or has zero length.",
                     "meas tran t_90 failed!"):
            with self.subTest(line=line):
                self.assertEqual(ngspice_run.error_lines(line + "\n"), [line])

    def test_ignores_titles_and_recovered_warnings(self) -> None:
        output = "Circuit: * failed bench title\nWarning: gmin stepping failed\nNote: source stepping succeeded\n"
        self.assertEqual(ngspice_run.error_lines(output), [])


@ngspice_run.requires_ngspice
class NgspiceTest(unittest.TestCase):
    def test_vbias_divider(self) -> None:
        text = ".include generated/vbias.cir\nVrail vrail 0 16.9\n.op\n.control\nrun\nprint v(vbias)\n.endc\n.end\n"
        result = ngspice_run.run_text(text, expect=["v(vbias)"])
        self.assertAlmostEqual(result["v(vbias)"], 8.45, places=6)

    def test_rc_cutoff(self) -> None:
        power: dict[str, tuple[str, str]] = {}
        r = gen_spice.part_lines(Part("R1", "Device", "R", "1k", {"1": "IN", "2": "OUT"}), power)
        c = gen_spice.part_lines(Part("C1", "Device", "C", "159.155n", {"1": "OUT", "2": "GND"}), power)
        text = "\n".join(["* rc", "Vin in 0 AC 1", *r, *c, ".ac dec 200 100 10k", ".control", "run",
                          "meas ac f3 WHEN vdb(out)=-3.0103", ".endc", ".end"]) + "\n"
        result = ngspice_run.run_text(text, expect=["f3"])
        self.assertAlmostEqual(result["f3"], 1000.0, delta=2.0)

    def relay_text(self, k1_on: str) -> str:
        return "\n".join([
            ".include models.inc", ".include generated/block_F.cir", k1_on,
            "Vt in_tip 0 1", "Rl in_a 0 1k", "Vv9 v9 0 9", "Vdrv seed_d0_k1drv 0 0",
            ".op", ".control", "run", "print v(in_a)", ".endc", ".end"]) + "\n"

    def test_relay_parameter(self) -> None:
        on = ngspice_run.run_text(self.relay_text(".param k1_on=1"), expect=["v(in_a)"])
        off = ngspice_run.run_text(self.relay_text(".param k1_on=0"), expect=["v(in_a)"])
        self.assertAlmostEqual(on["v(in_a)"], 1000 / 1000.05, places=6)
        self.assertLess(abs(off["v(in_a)"]), 1e-6)

    def test_missing_parameter_raises(self) -> None:
        with self.assertRaises(ngspice_run.NgspiceError):
            ngspice_run.run_text(self.relay_text("* k1_on 없음"), expect=["v(in_a)"])

    def test_error_and_missing_result_raise(self) -> None:
        with self.assertRaises(ngspice_run.NgspiceError):
            ngspice_run.run_text("* bad\nX1 a b nosuch\n.op\n.end\n")
        with self.assertRaises(ngspice_run.NgspiceError):
            ngspice_run.run_text("* ok\nR1 a 0 1k\nV1 a 0 1\n.op\n.control\nrun\n.endc\n.end\n", expect=["v(a)"])

    def test_led_forward_voltage(self) -> None:
        text = ".include models.inc\nI1 0 a 3m\nD1 a 0 DLEDR\n.op\n.control\nrun\nprint v(a)\n.endc\n.end\n"
        result = ngspice_run.run_text(text, expect=["v(a)"])
        self.assertTrue(1.8 < result["v(a)"] < 2.1)


@unittest.skipUnless(os.environ.get("KICAD_SYMBOL_DIR"), "KICAD_SYMBOL_DIR 없음(KiCad 9.0.7 심볼 11개 이상)")
@unittest.skipUnless(os.environ.get("KICAD_SYMBOL_DIR"), "KICAD_SYMBOL_DIR 없음(KiCad 9.0.7 심볼 11개 이상)")
class SchematicRegenerationTest(unittest.TestCase):
    def test_default_blocks_reproduce_committed_files(self) -> None:
        import gen_schematic as gs
        with tempfile.TemporaryDirectory() as directory:
            with contextlib.redirect_stdout(io.StringIO()):
                gs.render(gs.Library(Path(os.environ["KICAD_SYMBOL_DIR"])), Path(directory))
            for name in ("bass-compressor.kicad_sch", "netlist.md"):
                with self.subTest(name=name):
                    self.assertEqual((Path(directory) / name).read_bytes(), (gs.HERE / name).read_bytes())

    def test_custom_name_and_blocks(self) -> None:
        import gen_schematic as gs
        with tempfile.TemporaryDirectory() as directory:
            with contextlib.redirect_stdout(io.StringIO()):
                gs.render(gs.Library(Path(os.environ["KICAD_SYMBOL_DIR"])), Path(directory),
                          blocks=gs.BLOCKS[:2], name="part", title="부분")
            self.assertTrue((Path(directory) / "part.kicad_sch").exists())
            self.assertTrue((Path(directory) / "part-netlist.md").exists())


if __name__ == "__main__":
    unittest.main()
