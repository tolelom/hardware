import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import enclosure as en  # noqa: E402

M = en.Dimension


def case() -> en.Case:
    return en.Case(M(95, "t"), M(120, "t"), M(56, "t"), M(3, "t"), M(5, "t"))


def spec(name: str, outside: float, body: float = 10, depth: float = 10) -> en.PartSpec:
    return en.PartSpec(name, M(7, "t"), M(outside, "t"), M(body, "t"), M(body, "t"), M(depth, "t"))


def values_of(c: en.Case, parts: dict[str, en.PartSpec], internal: list) -> dict[str, float]:
    values, missing = en.resolve(c, parts, internal, draft=False)
    assert not missing, missing
    return values


class GeometryTest(unittest.TestCase):
    def test_face_transform(self) -> None:
        c = case()
        v = values_of(c, {}, [])
        self.assertEqual(en.to_case(c, en.Placement("a", "p", "top", 10, -20), v), (10, -20, 56))
        self.assertEqual(en.to_case(c, en.Placement("a", "p", "in", 30, 5), v), (47.5, 30, 33))
        self.assertEqual(en.to_case(c, en.Placement("a", "p", "back", -15, 0), v), (15, 60, 28))
        self.assertEqual(en.to_case(c, en.Placement("a", "p", "out", 30, 5), v), (-47.5, -30, 33))

    def test_face_x_is_viewer_right_from_outside(self) -> None:
        # 면 밖에서 면을 바라볼 때 오른쪽 방향(위 = +Z). 바깥 법선 n과 위 u에서 오른쪽 = u x n
        c = case()
        v = values_of(c, {}, [])
        normals = {"top": (0, 0, 1), "in": (1, 0, 0), "out": (-1, 0, 0), "back": (0, 1, 0)}
        for face, normal in normals.items():
            with self.subTest(face=face):
                origin = en.to_case(c, en.Placement("o", "p", face, 0, 0), v)
                moved = en.to_case(c, en.Placement("o", "p", face, 1, 0), v)
                step = tuple(m - o for m, o in zip(moved, origin))
                up = (0, 1, 0) if face == "top" else (0, 0, 1)
                right = (up[1] * normal[2] - up[2] * normal[1], up[2] * normal[0] - up[0] * normal[2],
                         up[0] * normal[1] - up[1] * normal[0])
                self.assertEqual(step, right)

    def test_body_box_goes_inward(self) -> None:
        c = case()
        parts = {"p": spec("p", 10, body=16, depth=12)}
        v = values_of(c, parts, [])
        box = en.body_box(c, en.Placement("k", "p", "top", 0, 0), parts["p"], v)
        self.assertEqual((box.x0, box.x1, box.z0, box.z1), (-8, 8, 56 - 3 - 12, 56 - 3))
        side = en.body_box(c, en.Placement("j", "p", "in", 0, 0), parts["p"], v)
        self.assertEqual((side.x0, side.x1), (47.5 - 3 - 12, 47.5 - 3))


class CheckTest(unittest.TestCase):
    def run_check(self, placements: list[en.Placement], parts: dict[str, en.PartSpec], internal=()) -> list[en.Finding]:
        c = case()
        return en.check(c, parts, placements, list(internal), values_of(c, parts, list(internal)))

    def failures(self, findings: list[en.Finding]) -> list[str]:
        return [f.check for f in findings if not f.ok]

    def test_outside_overlap(self) -> None:
        parts = {"knob": spec("knob", 18.9)}
        close = [en.Placement("a", "knob", "top", 0, 0), en.Placement("b", "knob", "top", 20, 0)]
        far = [en.Placement("a", "knob", "top", 0, 0), en.Placement("b", "knob", "top", 25, 0)]
        self.assertIn("패널 바깥 간섭", self.failures(self.run_check(close, parts)))
        self.assertNotIn("패널 바깥 간섭", self.failures(self.run_check(far, parts)))

    def test_knob_gap_rule(self) -> None:
        parts = {"knob": spec("knob", 18.9)}
        tight = [en.Placement("a", "knob", "top", 0, 0), en.Placement("b", "knob", "top", 24.9, 0)]
        self.assertIn("패널 바깥 간섭", self.failures(self.run_check(tight, parts)))

    def test_edge_margin_and_out_of_face(self) -> None:
        parts = {"led": spec("led", 3)}
        inside = [en.Placement("a", "led", "top", 47.5 - 5 - 1.5, 0)]
        outside = [en.Placement("a", "led", "top", 47.5 - 5 - 1.4, 0)]
        negative = [en.Placement("a", "led", "top", -60, 0)]
        self.assertNotIn("가장자리 여유", self.failures(self.run_check(inside, parts)))
        self.assertIn("가장자리 여유", self.failures(self.run_check(outside, parts)))
        self.assertIn("가장자리 여유", self.failures(self.run_check(negative, parts)))

    def test_inside_collision_across_faces(self) -> None:
        parts = {"pot": spec("pot", 10, body=16, depth=30), "jack": spec("jack", 10, body=16, depth=30)}
        placements = [en.Placement("pot", "pot", "top", 35, 0), en.Placement("jack", "jack", "in", 0, 15)]
        self.assertIn("패널 안쪽 간섭", self.failures(self.run_check(placements, parts)))

    def test_floor_stack(self) -> None:
        parts = {"pot": spec("pot", 10, body=16, depth=40)}
        stack = [en.Box("seed3", -10, 10, -10, 10, 3, 3 + 20)]
        self.assertIn("패널 안쪽 간섭", self.failures(
            self.run_check([en.Placement("pot", "pot", "top", 0, 0)], parts, stack)))
        too_deep = {"pot": spec("pot", 10, body=16, depth=60)}
        self.assertIn("높이 스택", self.failures(self.run_check([en.Placement("pot", "pot", "top", 30, 30)], too_deep)))


class ResolveTest(unittest.TestCase):
    def test_unmeasured_blocks_final(self) -> None:
        c = en.Case(M(95, "t"), M(120, "t"), M(56, "t"), M(None, "미측정"), M(5, "t"))
        values, missing = en.resolve(c, {}, [], draft=False)
        self.assertEqual(missing, ["케이스 벽 두께"])
        values, missing = en.resolve(c, {}, [], draft=True)
        self.assertEqual(missing, ["케이스 벽 두께"])
        self.assertEqual(values["case.wall"], en.DRAFT_DEFAULTS["case.wall"])

    def test_project_data_resolves_in_draft(self) -> None:
        values, missing = en.resolve(en.CASE, en.PARTS, [], draft=True)
        self.assertTrue(missing)
        en.check(en.CASE, en.PARTS, en.PLACEMENTS, en.internal_boxes(values), values)


class OutputTest(unittest.TestCase):
    def test_svg_contains_holes_and_draft_mark(self) -> None:
        values, _ = en.resolve(en.CASE, en.PARTS, [], draft=True)
        svg = en.face_svg(en.CASE, "top", en.PLACEMENTS, values, draft=True)
        self.assertTrue(svg.startswith("<svg"))
        self.assertIn("초안", svg)
        self.assertEqual(svg.count("<circle"), sum(1 for p in en.PLACEMENTS if p.face == "top"))
        self.assertIn("Input", svg)

    def test_final_refuses_with_unmeasured(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = en.main(["--final", "--out", directory])
            self.assertEqual(code, 2)
            self.assertIn("미측정", out.getvalue())
            self.assertEqual(list(Path(directory).glob("*.svg")), [])

    def test_draft_writes_outputs(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with contextlib.redirect_stdout(io.StringIO()):
                en.main(["--draft", "--out", directory])
            names = sorted(p.name for p in Path(directory).iterdir())
        self.assertEqual(names, ["back.svg", "board.svg", "in.svg", "out.svg", "report.md", "top.svg"])

    def test_report_keeps_only_smallest_margin_per_passing_check(self) -> None:
        findings = [en.Finding("가장자리 여유", True, "a 여유 9.0mm", 9.0),
                    en.Finding("가장자리 여유", True, "b 여유 2.0mm", 2.0),
                    en.Finding("패널 안쪽 간섭", True, "c-d", None)]
        text = en.report(findings, [], draft=False)
        self.assertIn("b 여유 2.0mm", text)
        self.assertNotIn("a 여유 9.0mm", text)
        self.assertIn("c-d", text)

    def test_report_lists_failures_first(self) -> None:
        findings = [en.Finding("가장자리 여유", True, "a"), en.Finding("패널 바깥 간섭", False, "b-c")]
        text = en.report(findings, ["케이스 벽 두께"], draft=True)
        self.assertLess(text.index("b-c"), text.index("가장자리 여유"))
        self.assertIn("케이스 벽 두께", text)


class RectTest(unittest.TestCase):
    def rect_spec(self, name: str, w: float, h: float) -> en.PartSpec:
        return en.PartSpec(name, M(1, "t"), M(1, "t"), M(w, "t"), M(h, "t"), M(5, "t"),
                           cutout=(M(w, "t"), M(h, "t")), outside_rect=(M(w, "t"), M(h, "t")))

    def test_rect_circle_gap(self) -> None:
        c = case()
        parts = {"scr": self.rect_spec("scr", 40, 20), "knob": spec("knob", 10)}
        near = [en.Placement("s", "scr", "top", 0, 0), en.Placement("k", "knob", "top", 24, 0)]
        far = [en.Placement("s", "scr", "top", 0, 0), en.Placement("k", "knob", "top", 26, 0)]
        values = values_of(c, parts, [])
        bad = [f for f in en.check(c, parts, near, [], values) if f.check == "패널 바깥 간섭" and not f.ok]
        good = [f for f in en.check(c, parts, far, [], values) if f.check == "패널 바깥 간섭" and not f.ok]
        self.assertTrue(bad)
        self.assertFalse(good)

    def test_rect_edge_margin(self) -> None:
        c = case()
        parts = {"scr": self.rect_spec("scr", 40, 20)}
        values = values_of(c, parts, [])
        inside = en.check(c, parts, [en.Placement("s", "scr", "top", 47.5 - 5 - 20, 0)], [], values)
        outside = en.check(c, parts, [en.Placement("s", "scr", "top", 47.5 - 5 - 19.9, 0)], [], values)
        self.assertTrue(all(f.ok for f in inside if f.check == "가장자리 여유"))
        self.assertFalse(all(f.ok for f in outside if f.check == "가장자리 여유"))


class DesignTest(unittest.TestCase):
    def test_platform_design_draft_has_no_failures(self) -> None:
        from designs import platform_1590r1 as d
        values, missing = en.resolve(d.DESIGN.case, d.DESIGN.parts, [], draft=True)
        findings = en.check(d.DESIGN.case, d.DESIGN.parts, d.DESIGN.placements, d.DESIGN.internal(values), values)
        failed = [f for f in findings if not f.ok]
        self.assertEqual(failed, [], failed)

    def test_back_jack_body_collides_with_tall_board_part(self) -> None:
        from designs import platform_1590r1 as d
        values, _ = en.resolve(d.DESIGN.case, d.DESIGN.parts, [], draft=True)
        tall = en.Box("시험용 키 큰 부품", -10, 10, 30, 50, 2, 40)
        findings = en.check(d.DESIGN.case, d.DESIGN.parts, d.DESIGN.placements, [tall], values)
        self.assertTrue(any(not f.ok and "시험용" in f.detail for f in findings if f.check == "패널 안쪽 간섭"))

    def test_compressor_design_output_unchanged(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with contextlib.redirect_stdout(io.StringIO()):
                en.main(["--draft", "--out", directory])
            for name in ("top.svg", "report.md"):
                with self.subTest(name=name):
                    self.assertEqual((Path(directory) / name).read_text(encoding="utf-8"),
                                     (Path(en.__file__).parent / "enclosure" / name).read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
