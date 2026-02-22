import importlib.util
from pathlib import Path

import lldb

def __lldb_init_module(debugger, dict):
    formatter_impls_dir = Path(__file__).parent / "formatter_impls"

    for py_file in sorted(formatter_impls_dir.glob("*.py")):
        spec = importlib.util.spec_from_file_location(py_file.stem, py_file)
        if spec is None or spec.loader is None:
            continue
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        if hasattr(module, "init_formatter"):
            module.init_formatter(debugger, dict)

    debugger.HandleCommand(f'type category enable cplusplus-py')
