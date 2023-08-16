#ifndef AST_COUNTER_H_IN
#define AST_COUNTER_H_IN

#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <string>

namespace lldb_private::astutil {
class ASTCounter {
  uint64_t m_indent = 0;

public:
  void inc() noexcept { ++m_indent; }
  void dec() noexcept { --m_indent; }
  uint64_t get() const noexcept { return m_indent; }
};

ASTCounter &getASTCounter();

struct ScopedCounter {
  ScopedCounter() { getASTCounter().inc(); }

  ~ScopedCounter() { getASTCounter().dec(); }
};

template<typename String>
void logWithIndent(String && Msg) {
  auto indent = getASTCounter().get();
  std::string indent_str;
  for (decltype(indent) i{}; i < indent; ++i)
    indent_str += "  ";

  llvm::errs() << indent_str << std::forward<String>(Msg);
}
} // namespace lldb_private::astutil

#endif // _H_IN
