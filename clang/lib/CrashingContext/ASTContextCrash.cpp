#include "clang/CrashingContext/ASTContextCrash.h"

#include "clang/AST/ASTContextAllocate.h"
#include "clang/Basic/Builtins.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceManager.h"

namespace clang {
ASTContext* CreateASTContext() {
  using namespace clang;
  auto* opts = new LangOptions();
  auto* identTable = new IdentifierTable(*opts, nullptr);
  auto* builtinCtx = new Builtin::Context();
  auto* selTable = new SelectorTable();

  clang::FileSystemOptions file_system_options;
  auto* fileMgr = new FileManager(file_system_options, nullptr);

  llvm::IntrusiveRefCntPtr<DiagnosticIDs> diag_id_sp(new DiagnosticIDs());
  auto* diagEngine = new DiagnosticsEngine(diag_id_sp, new DiagnosticOptions());

  auto* srcMgr = new SourceManager(*diagEngine, *fileMgr);

  ASTContext* ast = new ASTContext(
      *opts, *srcMgr, *identTable,
      *selTable, *builtinCtx, TU_Complete);

  return ast;
}
}
