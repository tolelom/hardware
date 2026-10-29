import contextlib
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import gen_spice  # noqa: E402
import pinmap  # noqa: E402
import platform_blocks as pb  # noqa: E402


class PlatformBlocksTest(unittest.TestCase):
    def all_parts(self):
        return [p for b in pb.PLATFORM_BLOCKS for p in b.parts]

    def test_every_block_names_its_board(self) -> None:
        for block in pb.PLATFORM_BLOCKS:
            with self.subTest(block=block.title):
                self.assertTrue(block.title.startswith(("[메인]", "[조작부]")))

    def test_inherits_analog_blocks_unchanged(self) -> None:
        from gen_schematic import BLOCKS
        inherited = {p.ref: p for b in BLOCKS[:6] for p in b.parts if p.ref not in pb.REPLACED}
        platform = {(p.ref, p.unit): p for p in self.all_parts()}
        for part in inherited.values():
            with self.subTest(ref=part.ref):
                moved = platform[(part.ref, part.unit)]
                self.assertEqual(moved.value, part.value)
                self.assertEqual(moved.nets, {pin: pb.SEED_RENAMES.get(net, net) for pin, net in part.nets.items()})

    def test_seed_socket_matches_pinmap(self) -> None:
        socket = next(p for p in self.all_parts() if p.ref == "J4")
        for function, pin in pinmap.PLATFORM.items():
            physical = str(pinmap.SEED3_PINS[pin].physical)
            with self.subTest(function=function):
                self.assertEqual(socket.nets[physical], pinmap.net(function))

    def test_board_link_nets_exist_on_both_boards(self) -> None:
        nets_by_board = {"[메인]": set(), "[조작부]": set()}
        for block in pb.PLATFORM_BLOCKS:
            board = "[메인]" if block.title.startswith("[메인]") else "[조작부]"
            for p in block.parts:
                if p.ref not in ("J20", "J21"):
                    nets_by_board[board] |= set(p.nets.values())
        for net in set(pb.BOARD_LINK) - {"GND"}:
            with self.subTest(net=net):
                self.assertIn(net, nets_by_board["[메인]"])
                self.assertIn(net, nets_by_board["[조작부]"])

    def test_every_seed_signal_reaches_a_part(self) -> None:
        users: dict[str, int] = {}
        for p in self.all_parts():
            if p.ref not in ("J4", "J20", "J21"):
                for net in p.nets.values():
                    users[net] = users.get(net, 0) + 1
        for function in pinmap.PLATFORM:
            with self.subTest(function=function):
                self.assertGreater(users.get(pinmap.net(function), 0), 0)

    def test_encoder_inputs_avoid_output_only_pins(self) -> None:
        for p in self.all_parts():
            if p.name.startswith("MCP23017"):
                with self.subTest(ref=p.ref):
                    self.assertEqual(p.nets["8"], "NC")
                    self.assertEqual(p.nets["28"], "NC")

    def part(self, ref: str):
        return next(p for p in self.all_parts() if p.ref == ref)

    def test_buck_runs_continuous_and_filters_its_input(self) -> None:
        buck = self.part("U10")
        self.assertEqual(buck.value, "TPS562208")
        vin = buck.nets["3"]
        self.assertNotEqual(vin, "V9")
        filters = [p for p in self.all_parts() if p.name == "FerriteBead" and set(p.nets.values()) == {"V9", vin}]
        self.assertEqual(len(filters), 1)
        bulk = [p for p in self.all_parts() if p.name.startswith("C") and vin in p.nets.values()
                and gen_spice.parse_value(p.value) >= 10e-6 * 0.999]
        self.assertTrue(bulk)

    def test_midi_out_short_does_not_load_seed_rail(self) -> None:
        source = self.part("R42")
        self.assertNotIn("SEED_3V3D", source.nets.values())

    def test_exp_detects_unplugged_for_pedals_up_to_250k(self) -> None:
        series = gen_spice.parse_value(self.part("R50").value)
        for pedal in (10e3, 25e3, 50e3, 100e3, 250e3):
            with self.subTest(pedal=pedal):
                self.assertLess(pedal / (pedal + series), pb.EXP_UNPLUGGED_THRESHOLD - 0.005)

    def test_spice_mapping_covers_platform(self) -> None:
        power = gen_spice.opamp_power(pb.PLATFORM_BLOCKS)
        for p in self.all_parts():
            gen_spice.part_lines(p, power)

    def test_platform_spice_files_are_fresh(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            fresh = {path.name: path.read_text(encoding="utf-8")
                     for path in gen_spice.generate(Path(directory), blocks=pb.PLATFORM_BLOCKS, prefix="platform")}
        committed = {path.name: path.read_text(encoding="utf-8") for path in pb.SPICE_DIR.glob("*.cir")}
        self.assertEqual(sorted(committed), sorted(fresh))
        for name, text in fresh.items():
            with self.subTest(name=name):
                self.assertEqual(committed[name], text, "python3 effector/hardware/platform_blocks.py로 다시 생성")


@unittest.skipUnless(os.environ.get("KICAD_SYMBOL_DIR"), "KICAD_SYMBOL_DIR 없음(KiCad 9.0.7 심볼)")
class PlatformSchematicTest(unittest.TestCase):
    def test_every_power_input_net_is_driven(self) -> None:
        import gen_schematic as gs
        library = gs.Library(Path(os.environ["KICAD_SYMBOL_DIR"]))
        driven, inputs = set(), {}
        for block in pb.PLATFORM_BLOCKS:
            for part in block.parts:
                if part.name == "PWR_FLAG":
                    driven.add(part.nets["1"])
                    continue
                for pin in library.pins(part.lib, part.name):
                    net = part.nets.get(pin.number)
                    if pin.unit not in (0, part.unit) or net in (None, "NC"):
                        continue
                    if pin.electrical == "power_out":
                        driven.add(net)
                    elif pin.electrical == "power_in":
                        inputs.setdefault(net, f"{part.ref}.{pin.number}")
        for net, pin in sorted(inputs.items()):
            with self.subTest(net=net, pin=pin):
                self.assertIn(net, driven)

    def test_committed_schematic_matches_regeneration(self) -> None:
        import gen_schematic as gs
        with tempfile.TemporaryDirectory() as directory:
            with contextlib.redirect_stdout(io.StringIO()):
                pb.render_schematic(gs.Library(Path(os.environ["KICAD_SYMBOL_DIR"])), Path(directory))
            for name in ("platform.kicad_sch", "platform-netlist.md"):
                with self.subTest(name=name):
                    self.assertEqual((Path(directory) / name).read_bytes(), (gs.HERE / name).read_bytes())


if __name__ == "__main__":
    unittest.main()
