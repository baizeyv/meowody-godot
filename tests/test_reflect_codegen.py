"""Run with python -m unittest discover -s tests -p test_reflect_codegen.py."""
import copy
import shutil
import unittest
from pathlib import Path
from reflect_codegen import reflect_generator as gen

ROOT = Path(__file__).resolve().parents[1]


def method(name, result="void", args=None, attributes=None):
    return dict(name=name, return_=result, args=args or [], attributes=attributes or [],
                const=False, static=False, definition=False, access="public", file=Path("Probe.h"), line=12)


def raw_class():
    getter = method("get_data", "godot::Dictionary")
    setter = method("set_data", args=[dict(name="value", type="const godot::Dictionary &")])
    for item in [getter, setter]:
        item["return"] = item.pop("return_")
    return dict(name="Probe", qualified="sample::Probe", base="Resource", file=Path("Probe.h"),
                include="Probe.h", line=1, methods=[getter, setter],
                fields=[dict(name="data", type="godot::Dictionary", file=Path("Probe.h"), line=5,
                             attributes=[("gdproperty", "getter=get_data|setter=set_data|usage=PROPERTY_USAGE_STORAGE")])])


class ReflectTests(unittest.TestCase):
    def test_dictionary_and_resource_hint(self):
        model = gen.build_model([raw_class()])[0]
        self.assertIn('PropertyInfo(Variant::DICTIONARY, "data"', gen.render_class(model))
        self.assertEqual(gen.bind_type("godot::Ref<meowody::SpriteAtlas>")[3], "SpriteAtlas")

    def test_invalid_accessors(self):
        for mutation in ["missing", "wrong_type", "private", "overload"]:
            with self.subTest(mutation=mutation):
                raw = raw_class()
                if mutation == "missing": raw["methods"].pop()
                if mutation == "wrong_type": raw["methods"][1]["args"][0]["type"] = "int"
                if mutation == "private": raw["methods"][0]["access"] = "private"
                if mutation == "overload": raw["methods"].append(copy.deepcopy(raw["methods"][0]))
                with self.assertRaisesRegex(RuntimeError, "accessor.*Probe.h"):
                    gen.build_model([raw])

    def test_duplicate_property_and_method(self):
        raw = raw_class()
        raw["fields"].append(copy.deepcopy(raw["fields"][0]))
        with self.assertRaisesRegex(RuntimeError, "duplicate property"):
            gen.build_model([raw])
        raw = raw_class()
        raw["methods"][0]["attributes"] = [("gdmethod", "")]
        raw["methods"].append(copy.deepcopy(raw["methods"][0]))
        raw["fields"] = []
        with self.assertRaisesRegex(RuntimeError, "overload"):
            gen.build_model([raw])

    def test_accessor_also_marked_as_method_binds_once(self):
        raw = raw_class()
        raw["methods"][0]["attributes"] = [("gdmethod", "")]
        output = gen.render_class(gen.build_model([raw])[0])
        self.assertEqual(output.count('D_METHOD("get_data"'), 1)

    def test_automatic_accessor_retains_namespace(self):
        raw = raw_class()
        raw["methods"] = []
        raw["fields"][0]["attributes"] = [("gdproperty", "")]
        output = gen.render_class(gen.build_model([raw])[0])
        self.assertIn("godot::Dictionary sample::Probe::get_data()", output)

    def test_real_headers_dictionary_and_gs(self):
        raw = gen.scan_headers(ROOT / "src", ROOT / "godot-cpp", ROOT / "build", [
            ROOT / "src/runtime/ui/sprite_atlas.h", ROOT / "src/test_node.h"])
        models = {item["name"]: item for item in gen.build_model(raw)}
        atlas = models["SpriteAtlas"]
        self.assertEqual(atlas["properties"][0]["variant"], "Variant::DICTIONARY")
        self.assertFalse(atlas["properties"][0]["generate_setter"])
        hp = next(item for item in models["TestNode"]["properties"] if item["name"] == "hp")
        self.assertEqual(hp["getter"], "get_hp")
        self.assertFalse(hp["generate_getter"])

    def test_clang_errors_stop_generation(self):
        source = ROOT / "build/tests/reflect-invalid.h"
        source.parent.mkdir(parents=True, exist_ok=True)
        try:
            source.write_text('#include "missing_reflect_test_header.h"\n', encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, r"invalid C\+\+"):
                gen.scan_headers(source.parent, ROOT / "godot-cpp", ROOT / "build", [source])
        finally:
            source.unlink(missing_ok=True)

    def test_shared_translation_unit_preserves_unique_class_locations(self):
        source = ROOT / "build/tests/reflect-headers"
        if source.exists():
            shutil.rmtree(source)
        source.mkdir(parents=True)
        self.addCleanup(shutil.rmtree, source)
        first = source / "First.h"
        second = source / "Second.h"
        first.write_text('#pragma once\nnamespace sample {\nclass First {\n'
                         '  __attribute__((annotate("gdclass_marker"))) void marker();\n};\n}\n', encoding="utf-8")
        second.write_text('#pragma once\n#include "First.h"\nnamespace sample {\nclass Second {\n'
                          '  __attribute__((annotate("gdclass_marker"))) void marker();\n};\n}\n', encoding="utf-8")
        classes = gen.scan_headers(source, ROOT / "godot-cpp", ROOT / "build", [second, first])
        self.assertEqual([item["qualified"] for item in classes], ["sample::First", "sample::Second"])
        self.assertEqual([item["file"] for item in classes], [first, second])
        self.assertEqual([item["line"] for item in classes], [3, 4])

    def test_shared_translation_unit_checks_inline_bodies_in_every_header(self):
        source = ROOT / "build/tests/reflect-bodies"
        if source.exists():
            shutil.rmtree(source)
        source.mkdir(parents=True)
        self.addCleanup(shutil.rmtree, source)
        valid = source / "Valid.h"
        invalid = source / "Invalid.h"
        valid.write_text('#pragma once\nclass Valid {};\n', encoding="utf-8")
        invalid.write_text('#pragma once\nclass Invalid { void run() { missing_symbol(); } };\n', encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, r"Invalid.h:2:.*missing_symbol"):
            gen.scan_headers(source, ROOT / "godot-cpp", ROOT / "build", [valid, invalid])


if __name__ == "__main__":
    unittest.main()
