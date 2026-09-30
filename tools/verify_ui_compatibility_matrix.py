"""Validate the declarative Python-to-C++ darkeye_ui compatibility contract.

The checker deliberately uses AST instead of importing the Python package, so it
can run in CI without PySide6 or an application context.  It verifies that every
Python public component has a complete matrix record and that its C++ target is
discoverable from the public facade (or is an explicitly documented boundary
exception).
"""

from __future__ import annotations

import argparse
import ast
import json
from pathlib import Path
import re
import sys


REQUIRED_FIELDS = {"cpp", "constructor", "methods", "signals", "theme", "snapshot"}
PYTHON_DEMO_COVERAGE = {"demo", "external_harness", "unavailable_baseline_export"}
EXPLICIT_BOUNDARY_EXCEPTIONS: set[str] = set()
THEME_BEHAVIORS = {
    "global QSS",
    "injected ThemeService",
    "QSS + emoji font",
    "self-drawn",
    "transparent",
}
MAPPING_WORDS = {
    "api",
    "callback",
    "constructor",
    "cxx",
    "or",
    "python",
}
QT_OVERRIDE_METHODS = {
    "changeEvent",
    "closeEvent",
    "enterEvent",
    "event",
    "eventFilter",
    "focusInEvent",
    "focusOutEvent",
    "hideEvent",
    "keyPressEvent",
    "leaveEvent",
    "mouseMoveEvent",
    "mousePressEvent",
    "mouseReleaseEvent",
    "paintEvent",
    "resizeEvent",
    "showEvent",
    "sizeHint",
    "wheelEvent",
}


def read_python_all(path: Path) -> list[str]:
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
            isinstance(target, ast.Name) and target.id == "__all__" for target in node.targets
        ):
            value = ast.literal_eval(node.value)
            if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
                raise ValueError("components.__all__ must be a list of strings")
            return value
    raise ValueError("components.__all__ was not found")


def python_public_methods(path: Path) -> dict[str, set[str]]:
    """Read each exported class's own non-Qt public methods without imports."""
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    result: dict[str, set[str]] = {}
    for import_node in tree.body:
        if not isinstance(import_node, ast.ImportFrom) or import_node.level != 1:
            continue
        if import_node.module is None:
            continue
        source = path.parent.joinpath(*import_node.module.split(".")).with_suffix(".py")
        if not source.is_file():
            raise ValueError(f"component import refers to missing module: {import_node.module}")
        module_tree = ast.parse(source.read_text(encoding="utf-8"), filename=str(source))
        classes = {
            node.name: node for node in module_tree.body if isinstance(node, ast.ClassDef)
        }
        assignments = {
            target.id: node.value.id
            for node in module_tree.body
            if isinstance(node, ast.Assign) and isinstance(node.value, ast.Name)
            for target in node.targets
            if isinstance(target, ast.Name)
        }
        for imported in import_node.names:
            exported_name = imported.asname or imported.name
            class_name = imported.name
            seen: set[str] = set()
            while class_name in assignments and class_name not in seen:
                seen.add(class_name)
                class_name = assignments[class_name]
            class_node = classes.get(class_name)
            if class_node is None:
                continue
            result[exported_name] = {
                method.name
                for method in class_node.body
                if isinstance(method, (ast.FunctionDef, ast.AsyncFunctionDef))
                and not method.name.startswith("_")
                and method.name not in QT_OVERRIDE_METHODS
            }
    return result


def python_constructor_parameters(path: Path) -> dict[str, set[str]]:
    """Return each exported class's explicit ``__init__`` parameter names.

    The compatibility matrix remains readable prose, but its Python-side
    constructor contract must not silently lose a public keyword parameter.
    Alias exports are resolved to their implementation class in the same way as
    the method/signal readers above.
    """
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    result: dict[str, set[str]] = {}
    for import_node in tree.body:
        if not isinstance(import_node, ast.ImportFrom) or import_node.level != 1:
            continue
        if import_node.module is None:
            continue
        source = path.parent.joinpath(*import_node.module.split(".")).with_suffix(".py")
        if not source.is_file():
            raise ValueError(f"component import refers to missing module: {import_node.module}")
        module_tree = ast.parse(source.read_text(encoding="utf-8"), filename=str(source))
        classes = {node.name: node for node in module_tree.body if isinstance(node, ast.ClassDef)}
        assignments = {
            target.id: node.value.id
            for node in module_tree.body
            if isinstance(node, ast.Assign) and isinstance(node.value, ast.Name)
            for target in node.targets
            if isinstance(target, ast.Name)
        }
        for imported in import_node.names:
            exported_name = imported.asname or imported.name
            class_name = imported.name
            seen: set[str] = set()
            while class_name in assignments and class_name not in seen:
                seen.add(class_name)
                class_name = assignments[class_name]
            class_node = classes.get(class_name)
            if class_node is None:
                continue
            init = next(
                (
                    method for method in class_node.body
                    if isinstance(method, (ast.FunctionDef, ast.AsyncFunctionDef))
                    and method.name == "__init__"
                ),
                None,
            )
            if init is None:
                result[exported_name] = set()
                continue
            positional = [*init.args.posonlyargs, *init.args.args]
            result[exported_name] = {
                argument.arg for argument in [*positional, *init.args.kwonlyargs]
                if argument.arg != "self"
            }
    return result


def python_public_signals(path: Path) -> dict[str, set[str]]:
    """Read custom Qt ``Signal`` declarations on exported classes without imports."""
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    result: dict[str, set[str]] = {}
    for import_node in tree.body:
        if not isinstance(import_node, ast.ImportFrom) or import_node.level != 1:
            continue
        if import_node.module is None:
            continue
        source = path.parent.joinpath(*import_node.module.split(".")).with_suffix(".py")
        module_tree = ast.parse(source.read_text(encoding="utf-8"), filename=str(source))
        classes = {
            node.name: node for node in module_tree.body if isinstance(node, ast.ClassDef)
        }
        assignments = {
            target.id: node.value.id
            for node in module_tree.body
            if isinstance(node, ast.Assign) and isinstance(node.value, ast.Name)
            for target in node.targets
            if isinstance(target, ast.Name)
        }
        for imported in import_node.names:
            exported_name = imported.asname or imported.name
            class_name = imported.name
            seen: set[str] = set()
            while class_name in assignments and class_name not in seen:
                seen.add(class_name)
                class_name = assignments[class_name]
            class_node = classes.get(class_name)
            if class_node is None:
                continue
            result[exported_name] = {
                target.id
                for node in class_node.body
                if isinstance(node, ast.Assign)
                and isinstance(node.value, ast.Call)
                and isinstance(node.value.func, ast.Name)
                and node.value.func.id == "Signal"
                for target in node.targets
                if isinstance(target, ast.Name)
            }
    return result


def cpp_symbol(record: dict[str, str]) -> str | None:
    match = re.match(r"([A-Za-z_]\w*)", record["cpp"])
    return match.group(1) if match else None


def resolved_cpp_symbol(symbol: str, facade_text: str) -> str:
    """Resolve public ``using Alias = Target`` declarations to their concrete type."""
    seen: set[str] = set()
    current = symbol
    while current not in seen:
        seen.add(current)
        match = re.search(
            rf"\busing\s+{re.escape(current)}\s*=\s*([A-Za-z_]\w*)\s*;", facade_text
        )
        if match is None:
            return current
        current = match.group(1)
    raise ValueError(f"cyclic C++ alias declaration involving {symbol}")


def mapped_cpp_methods(record: dict[str, str]) -> set[str]:
    """Extract explicit C++ method names from the RHS of ``Python -> C++`` maps.

    Free-form notes after a semicolon deliberately remain documentation only.  This
    keeps the JSON concise while making every explicit API arrow statically
    enforceable against the public C++ headers.
    """
    methods = record["methods"]
    if "->" not in methods:
        return set()
    rhs = methods.split("->", 1)[1].split(";", 1)[0]
    return {
        word
        for word in re.findall(r"\b[a-z][A-Za-z0-9_]*\b", rhs)
        if word not in MAPPING_WORDS
    }


def mapped_cpp_signals(record: dict[str, str]) -> set[str]:
    """Return custom C++ signals documented by a matrix record.

    Qt inherited signals and Python-only implementation notes do not belong to a
    component's public declaration, so records mark those with ``inherited`` or
    explanatory prose and this checker intentionally skips them.
    """
    signals = record["signals"].strip()
    if signals == "none" or signals.startswith("inherited ") or "Python " in signals:
        return set()
    return set(re.findall(r"\b[A-Za-z_]\w*\b", signals))


def facade_headers(facade: Path) -> list[Path]:
    include_pattern = re.compile(r'#include "([^"]+)"')
    headers: list[Path] = []
    for include in include_pattern.findall(facade.read_text(encoding="utf-8")):
        if not include.startswith("darkeye_ui/components/"):
            continue
        header = facade.parents[1] / include.removeprefix("darkeye_ui/")
        if not header.is_file():
            raise ValueError(f"public facade refers to missing header: {include}")
        headers.append(header)
    return headers


def gallery_page_ids(gallery_source: Path) -> set[str]:
    source = gallery_source.read_text(encoding="utf-8")
    return set(re.findall(r'QStringLiteral\("([a-z0-9-]+)"\)', source))


def python_gallery_page_ids(gallery_source: Path) -> set[str]:
    """Read the public Gallery page identifiers from Python's ``menu_defs``."""
    tree = ast.parse(gallery_source.read_text(encoding="utf-8"), filename=str(gallery_source))
    for node in ast.walk(tree):
        if not isinstance(node, ast.Assign) or not any(
            isinstance(target, ast.Name) and target.id == "menu_defs" for target in node.targets
        ):
            continue
        if not isinstance(node.value, ast.List):
            raise ValueError("Python gallery menu_defs must be a list")
        page_ids: set[str] = set()
        for item in node.value.elts:
            if not isinstance(item, ast.Tuple) or not item.elts:
                raise ValueError("Python gallery menu_defs must contain tuples")
            page_id = ast.literal_eval(item.elts[0])
            if not isinstance(page_id, str):
                raise ValueError("Python gallery page identifiers must be strings")
            page_ids.add(page_id.replace("_", "-"))
        page_ids.add("setting")
        return page_ids
    raise ValueError("Python gallery menu_defs was not found")


def python_gallery_symbols(gallery_source: Path) -> set[str]:
    """Return symbols used by the Python Demo, excluding import declarations."""
    tree = ast.parse(gallery_source.read_text(encoding="utf-8"), filename=str(gallery_source))
    return {
        node.id
        for node in ast.walk(tree)
        if isinstance(node, ast.Name) and isinstance(node.ctx, ast.Load)
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--matrix", type=Path, required=True)
    parser.add_argument("--python-components", type=Path, required=True)
    parser.add_argument("--cpp-facade", type=Path, required=True)
    parser.add_argument("--gallery-source", type=Path, required=True)
    parser.add_argument("--python-gallery-source", type=Path, required=True)
    parser.add_argument("--python-harness-source", type=Path, required=True)
    arguments = parser.parse_args()

    matrix = json.loads(arguments.matrix.read_text(encoding="utf-8"))
    records = matrix.get("components")
    if not isinstance(records, dict):
        raise ValueError("matrix.components must be an object")

    exports = read_python_all(arguments.python_components)
    public_python_methods = python_public_methods(arguments.python_components)
    python_constructor_args = python_constructor_parameters(arguments.python_components)
    public_python_signals = python_public_signals(arguments.python_components)
    facade_text = "\n".join(
        header.read_text(encoding="utf-8") for header in facade_headers(arguments.cpp_facade)
    )
    gallery_pages = gallery_page_ids(arguments.gallery_source)
    python_gallery_pages = python_gallery_page_ids(arguments.python_gallery_source)
    python_gallery_used_symbols = python_gallery_symbols(arguments.python_gallery_source)
    if not arguments.python_harness_source.is_file():
        raise ValueError(f"Python harness source is missing: {arguments.python_harness_source}")
    python_harness_source = arguments.python_harness_source.read_text(encoding="utf-8")
    cpp_gallery_source = arguments.gallery_source.read_text(encoding="utf-8")
    errors: list[str] = []
    if len(exports) != len(set(exports)):
        errors.append("Python components.__all__ contains duplicate names")

    for name in exports:
        record = records.get(name)
        if not isinstance(record, dict):
            errors.append(f"{name}: missing matrix record")
            continue
        absent = REQUIRED_FIELDS - record.keys()
        if absent:
            errors.append(f"{name}: missing fields {', '.join(sorted(absent))}")
            continue
        if any(not isinstance(record[key], str) or not record[key].strip() for key in REQUIRED_FIELDS):
            errors.append(f"{name}: every required field must be non-empty text")
        elif record["theme"] not in THEME_BEHAVIORS:
            errors.append(f"{name}: unknown theme behavior {record['theme']!r}")
        implementation_helpers = record.get("python_implementation_helpers", [])
        if not isinstance(implementation_helpers, list) or not all(
            isinstance(helper, str) and helper for helper in implementation_helpers
        ):
            errors.append(f"{name}: python_implementation_helpers must be a list of method names")
            implementation_helpers = []
        for method in sorted(public_python_methods.get(name, set())):
            if (
                not re.search(rf"\b{re.escape(method)}\b", record["methods"])
                and method not in implementation_helpers
            ):
                errors.append(f"{name}: Python public method {method} is absent from methods mapping")
        for signal in sorted(public_python_signals.get(name, set())):
            if not re.search(rf"\b{re.escape(signal)}\b", record["signals"]):
                errors.append(f"{name}: Python custom signal {signal} is absent from signals mapping")
        api_alias = record.get("api_alias_of")
        if api_alias is not None:
            if not isinstance(api_alias, str) or api_alias not in records or api_alias == name:
                errors.append(f"{name}: api_alias_of must name a different matrix component")
            elif "->" in record["constructor"]:
                errors.append(f"{name}: alias constructor must refer to its target contract")
        elif "->" not in record["constructor"]:
            errors.append(f"{name}: constructor mapping must use '->' or api_alias_of")
        if api_alias is None and name in python_constructor_args:
            constructor_lhs = record["constructor"].split("->", 1)[0]
            documented_args = set(re.findall(r"\b[A-Za-z_]\w*\b", constructor_lhs))
            missing_args = python_constructor_args[name] - documented_args
            if missing_args:
                errors.append(
                    f"{name}: constructor mapping omits Python parameter(s) "
                    f"{', '.join(sorted(missing_args))}"
                )
        if name not in EXPLICIT_BOUNDARY_EXCEPTIONS:
            symbol = cpp_symbol(record)
            if symbol is None:
                errors.append(f"{name}: C++ symbol cannot be read")
                continue
            if not re.search(rf"\b(?:class|struct|using)\s+{re.escape(symbol)}\b", facade_text):
                errors.append(f"{name}: C++ symbol {symbol} is absent from the public facade")
            if isinstance(api_alias, str) and api_alias in records:
                target_symbol = cpp_symbol(records[api_alias])
                if target_symbol is not None and (
                    resolved_cpp_symbol(symbol, facade_text)
                    != resolved_cpp_symbol(target_symbol, facade_text)
                ):
                    errors.append(
                        f"{name}: C++ alias {symbol} does not resolve to {target_symbol}"
                    )
            for method in sorted(mapped_cpp_methods(record)):
                if not re.search(rf"\b{re.escape(method)}\s*\(", facade_text):
                    errors.append(
                        f"{name}: mapped C++ method {method} is absent from the public facade"
                    )
            for signal in sorted(mapped_cpp_signals(record)):
                if not re.search(rf"\b{re.escape(signal)}\s*\(", facade_text):
                    errors.append(
                        f"{name}: mapped C++ signal {signal} is absent from the public facade"
                    )
        python_demo_coverage = record.get("python_demo_coverage", "demo")
        if python_demo_coverage not in PYTHON_DEMO_COVERAGE:
            errors.append(
                f"{name}: python_demo_coverage must be one of "
                f"{', '.join(sorted(PYTHON_DEMO_COVERAGE))}"
            )
            python_demo_coverage = "demo"
        snapshot = record["snapshot"]
        cpp_snapshot = snapshot.replace("_", "-")
        if name not in EXPLICIT_BOUNDARY_EXCEPTIONS and cpp_snapshot not in gallery_pages:
            errors.append(
                f"{name}: snapshot page {snapshot!r} is absent from the component gallery"
            )
        if (name not in EXPLICIT_BOUNDARY_EXCEPTIONS
                and python_demo_coverage == "demo"
                and cpp_snapshot not in python_gallery_pages):
            errors.append(
                f"{name}: snapshot page {snapshot!r} is absent from the Python component demo"
            )
        visual_alias = record.get("visual_alias_of")
        if visual_alias is not None and not isinstance(visual_alias, str):
            errors.append(f"{name}: visual_alias_of must be text when present")
            continue
        python_visual_symbol = visual_alias or name
        cpp_visual_symbol = visual_alias or symbol
        if (name not in EXPLICIT_BOUNDARY_EXCEPTIONS
                and python_demo_coverage == "demo"
                and python_visual_symbol not in python_gallery_used_symbols):
            errors.append(
                f"{name}: Python gallery does not use visual type {python_visual_symbol}"
            )
        if (name not in EXPLICIT_BOUNDARY_EXCEPTIONS
                and python_demo_coverage == "external_harness"
                and not re.search(rf"\b{re.escape(python_visual_symbol)}\b", python_harness_source)):
            errors.append(
                f"{name}: external Python snapshot harness does not use "
                f"visual type {python_visual_symbol}"
            )
        if name not in EXPLICIT_BOUNDARY_EXCEPTIONS and not re.search(
            rf"\b{re.escape(cpp_visual_symbol)}\b", cpp_gallery_source
        ):
            errors.append(
                f"{name}: C++ gallery does not use visual type {cpp_visual_symbol}"
            )

    extra = sorted(set(records) - set(exports))
    if extra:
        errors.append("matrix has non-exported component records: " + ", ".join(extra))
    if errors:
        print("darkeye_ui compatibility matrix failed:", file=sys.stderr)
        print("\n".join(f"- {error}" for error in errors), file=sys.stderr)
        return 1
    print(f"darkeye_ui compatibility matrix verified ({len(exports)} Python exports)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
