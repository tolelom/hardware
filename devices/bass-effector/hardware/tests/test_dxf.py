import contextlib
import io
import re
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import dxf  # noqa: E402


class DxfTest(unittest.TestCase):
    def test_round_trip(self) -> None:
        text = dxf.document([("circle", 10, -5, 3.75, "CUT"), ("rect", 0, 0, 20, 10, "OUTLINE"),
                             ("text", 1, 2, 2.5, "E1", "NOTE")])
        self.assertTrue(text.startswith("0\nSECTION\n2\nENTITIES\n"))
        self.assertTrue(text.rstrip().endswith("0\nEOF"))
        entities = dxf.parse(text)
        self.assertIn(("CIRCLE", "CUT", (10.0, -5.0, 3.75)), entities)
        self.assertIn(("TEXT", "NOTE", (1.0, 2.0, 2.5, "E1")), entities)
        lines = sorted(e[2] for e in entities if e[0] == "LINE")
        self.assertEqual(lines, sorted([(-10.0, -5.0, 10.0, -5.0), (10.0, -5.0, 10.0, 5.0),
                                        (10.0, 5.0, -10.0, 5.0), (-10.0, 5.0, -10.0, -5.0)]))

    def test_numbers_are_fixed_point(self) -> None:
        text = dxf.document([("circle", 0.00001, 123456.5, 0.5, "CUT")])
        values = text.splitlines()[1::2]
        numeric = [v for v in values if re.fullmatch(r"[-+0-9.eE]+", v) and not v.isdigit()]
        self.assertTrue(numeric)
        for v in numeric:
            with self.subTest(v=v):
                self.assertNotIn("e", v.lower())



class EnclosureDxfTest(unittest.TestCase):
    def test_platform_faces(self) -> None:
        import enclosure as en
        with tempfile.TemporaryDirectory() as directory:
            with contextlib.redirect_stdout(io.StringIO()):
                en.main(["--draft", "--design", "platform_1590r1", "--dxf", "--out", directory])
            top = dxf.parse((Path(directory) / "top.dxf").read_text(encoding="utf-8"))
            back = dxf.parse((Path(directory) / "back.dxf").read_text(encoding="utf-8"))
        cut = lambda es, kind: [e for e in es if e[0] == kind and e[1] == "CUT"]
        self.assertEqual(len(cut(top, "CIRCLE")), 13)
        self.assertEqual(len(cut(top, "LINE")), 4)
        self.assertEqual(len(cut(back, "CIRCLE")), 6)
        self.assertEqual(len(cut(back, "LINE")), 4)
        self.assertIn(("CIRCLE", "CUT", (-54.0, 34.63, 3.75)), top)
        for entity in top + back:
            if entity[0] == "TEXT":
                with self.subTest(text=entity[2][3]):
                    self.assertTrue(entity[2][3].isascii())
        outline = [e for e in top if e[1] == "OUTLINE"]
        self.assertEqual(len(outline), 4)

if __name__ == "__main__":
    unittest.main()
