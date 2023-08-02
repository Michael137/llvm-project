#ifndef LLVM_SUPPORT_TIMER2_H
#define LLVM_SUPPORT_TIMER2_H

#include "llvm/Support/Chrono.h"
#include <atomic>
#include <cstdint>

namespace llvm {

/// \class Timer Timer.h "lldb/Utility/Timer.h"
/// A timer class that simplifies common timing metrics.

class Timer2 {
public:
  class Category {
  public:
    explicit Category(const char *category_name);
    llvm::StringRef GetName() { return m_name; }

  private:
    friend class Timer2;
    const char *m_name;
    std::atomic<uint64_t> m_nanos;
    std::atomic<uint64_t> m_nanos_total;
    std::atomic<uint64_t> m_count;
    std::atomic<Category *> m_next;

    Category(const Category &) = delete;
    const Category &operator=(const Category &) = delete;
  };

  /// Default constructor.
  Timer2(Category &category, const char *format, ...)
#if !defined(_MSC_VER)
  // MSVC appears to have trouble recognizing the this argument in the constructor.
      __attribute__((format(printf, 3, 4)))
#endif
    ;

  /// Destructor
  ~Timer2();

  void Dump();

  static void SetDisplayDepth(uint32_t depth);

  static void SetQuiet(bool value);

  static void ResetCategoryTimes();

protected:
  using TimePoint = std::chrono::steady_clock::time_point;
  void ChildDuration(TimePoint::duration dur) { m_child_duration += dur; }

  Category &m_category;
  TimePoint m_total_start;
  TimePoint::duration m_child_duration{0};

  static std::atomic<bool> g_quiet;
  static std::atomic<unsigned> g_display_depth;

private:
  Timer2(const Timer2 &) = delete;
  const Timer2 &operator=(const Timer2 &) = delete;
};

} // namespace llvm

// Use a format string because LLVM_PRETTY_FUNCTION might not be a string
// literal.
#define LLVM_SCOPED_TIMER()                                                    \
  static ::llvm::Timer2::Category _cat(LLVM_PRETTY_FUNCTION);           \
  ::llvm::Timer2 _scoped_timer(_cat, "%s", LLVM_PRETTY_FUNCTION)
#define LLVM_SCOPED_TIMERF(...)                                                \
  static ::llvm::Timer2::Category _cat(LLVM_PRETTY_FUNCTION);           \
  ::llvm::Timer2 _scoped_timer(_cat, __VA_ARGS__)

#endif // _H
