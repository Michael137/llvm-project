# Expression Evaluation (Clang)

This document outlines the various pieces that make
LLDB's `expression` command.

The `expression` command allows a user to run arbitrary
expressions of the target's source language (or the language
specified with the `--language` option). At a high level, LLDB
facilitates this by dispatching the expression text to an
embedded compiler instance (e.g., Clang in the case of C++
expressions) which does type-checking and AST lowering with
the help of callbacks into LLDB. The result of parsing the
expression is an LLVM module that LLDB JIT compiles and runs
in the target. This document focuses on C++ expression evaluation,
which is built into upstream LLDB. The [Swift language plugin](https://github.com/swiftlang/llvm-project/tree/next/lldb/source/Plugins/LanguageRuntime/Swift) is
another example of production-grade expression evaluation.

- architecture (explain with references to files)
  - Wrap expression~>ParseAST~>CodeGen (AST lowering)~>IRInterpreter/JIT~>Evaluate~>Synthesize result~>Dump ValueObject
- modelling of types/decls using CompilerType/TypeSystemClang (relation to ValueObject)
- creation of types from DWARF
- interaction with language plugin
- compiler instance creation
- Clang<->LLDB interaction
- ASTContext management and ASTImporter

## Caveats

* Template representations
* AST fidelity (layout altering attributes)

## Links
