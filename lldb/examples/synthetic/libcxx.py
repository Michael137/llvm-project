import importlib
import importlib.util
import sys
import types
from pathlib import Path

import lldb

_PACKAGE = "lldb.formatters.cpp.formatter_impls"

def _ensure_package(dotted_name):
    """Ensure each component of a dotted package path exists in sys.modules
    and is set as an attribute on its parent."""
    parts = dotted_name.split(".")
    for i in range(len(parts)):
        partial = ".".join(parts[: i + 1])
        if partial not in sys.modules:
            sys.modules[partial] = types.ModuleType(partial)
        if i > 0:
            parent = sys.modules[".".join(parts[:i])]
            if not hasattr(parent, parts[i]):
                setattr(parent, parts[i], sys.modules[partial])

def _load_formatters_from_package(debugger, dict):
    """Load formatter modules via normal Python imports (site-packages case)."""
    pkg = importlib.import_module(_PACKAGE)
    for name in dir(pkg):
        module = getattr(pkg, name)
        if hasattr(module, "init_formatter"):
            module.init_formatter(debugger, dict)

def _load_formatters_from_disk(debugger, dict):
    """Load formatter modules from disk and wire up sys.modules (standalone case)."""
    formatter_impls_dir = Path(__file__).parent / "formatter_impls"

    _ensure_package(_PACKAGE)

    for py_file in sorted(formatter_impls_dir.glob("*.py")):
        module_name = f"{_PACKAGE}.{py_file.stem}"
        spec = importlib.util.spec_from_file_location(module_name, py_file)
        if spec is None or spec.loader is None:
            continue
        module = importlib.util.module_from_spec(spec)
        sys.modules[module_name] = module
        setattr(sys.modules[_PACKAGE], py_file.stem, module)
        spec.loader.exec_module(module)
        if hasattr(module, "init_formatter"):
            module.init_formatter(debugger, dict)

def __lldb_init_module(debugger, dict):
    try:
        _load_formatters_from_package(debugger, dict)
    except (ImportError, ModuleNotFoundError):
        _load_formatters_from_disk(debugger, dict)

    debugger.HandleCommand(f'type category enable cplusplus-py')
