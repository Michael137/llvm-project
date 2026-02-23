import importlib
import importlib.util
import sys
import types
from pathlib import Path

import lldb

""" Summary

  The goal: Load Python formatter modules from libcxx.py and register LLDB synthetic type providers, supporting two deployment scenarios:
  1. Files installed in Python site-packages (normal package imports work)
  2. Files in a standalone directory like /usr/share/lldb/formatters/ (no Python package structure)

  The core problem: When LLDB processes a HandleCommand like type synthetic add -l lldb.formatters.cpp.formatter_impls.libcxx_atomic_formatter.LibcxxStdAtomicSyntheticFrontEnd, it resolves the class by walking the dotted path via recursive getattr calls — lldb → .formatters → .cpp → .formatter_impls → .libcxx_atomic_formatter →
  .LibcxxStdAtomicSyntheticFrontEnd. Every intermediate module must exist in sys.modules and be set as an attribute on its parent.

  What didn't work:
  - Just registering leaf modules in sys.modules wasn't enough — the getattr chain broke at intermediate levels.
  - Creating placeholder types.ModuleType objects for the intermediate packages worked for the standalone case but broke the site-packages case — the placeholder overwrote the real formatter_impls package (which has __path__, __init__.py, etc.), causing 'lldb.formatters.cpp.formatter_impls' is not a package.

  The fix: Try-except with two code paths:
  1. First, try importlib.import_module("lldb.formatters.cpp.formatter_impls") — if the package is importable normally (site-packages), use it directly.
  2. If that fails with ImportError (standalone install), fall back to manual loading: create placeholder intermediate packages, load each .py file via importlib.util.spec_from_file_location, and wire everything into sys.modules + parent attributes.
"""

""" Why do we need placeholders?

  Because of how LLDB resolves the class path. When it encounters lldb.formatters.cpp.formatter_impls.libcxx_atomic_formatter.LibcxxStdAtomicSyntheticFrontEnd, it starts with the lldb module and calls getattr at each dot:                                                                                                                                             
                                         
  getattr(lldb, "formatters")                                                                                                                                                                                                                                                                                                                                             
  getattr(lldb.formatters, "cpp")                                                                                                                                                                                                                                                                                                                                         
  getattr(lldb.formatters.cpp, "formatter_impls")                                                                                                                                                                                                                                                                                                                         
  getattr(lldb.formatters.cpp.formatter_impls, "libcxx_atomic_formatter")
  getattr(..., "LibcxxStdAtomicSyntheticFrontEnd")

  In the standalone case (/usr/share/lldb/formatters/), lldb.formatters and lldb.formatters.cpp were never imported as packages — they don't exist on sys.path. So getattr(lldb, "formatters") fails immediately at the first step, even though the leaf module was properly loaded and registered in sys.modules.

  The placeholders bridge those gaps. _ensure_package creates a bare types.ModuleType for each intermediate component and sets it as an attribute on its parent, so the full getattr chain succeeds all the way down to the formatter module.
"""

""" Could we get rid of placehoders if we changed the path of the formatters in the SDK?

  The placeholders are needed because of the dotted class path in the HandleCommand strings, not because of the physical directory layout. The path lldb.formatters.cpp.formatter_impls.libcxx_atomic_formatter.LibcxxStdAtomicSyntheticFrontEnd is what forces the intermediate modules to exist.

  If you changed the class path in every HandleCommand string to something shorter — say just formatter_impls.libcxx_atomic_formatter.LibcxxStdAtomicSyntheticFrontEnd or even libcxx_atomic_formatter.LibcxxStdAtomicSyntheticFrontEnd — you'd need fewer or no placeholders.

  The physical directory under /usr/share doesn't matter at all. What matters is:
  1. The dotted path string in the -l argument of HandleCommand
  2. That LLDB can resolve that path via getattr starting from its interpreter dictionary

  So the question is really: can you change the class paths used in the HandleCommand strings? If you used a flat name like libcxx_atomic_formatter.LibcxxStdAtomicSyntheticFrontEnd and registered each module in sys.modules under just that name (plus set it in the interpreter's __main__ dict), you wouldn't need any placeholders. The tradeoff is losing the
  namespacing that lldb.formatters.cpp.formatter_impls.* provides.

"""

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
