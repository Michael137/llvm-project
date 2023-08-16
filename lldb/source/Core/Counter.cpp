#include "lldb/Core/Counter.h"

namespace lldb_private::astutil {
ASTCounter &getASTCounter() {
  static ASTCounter ctr;
  return ctr;
}
} // namespace lldb_private::astutil
