//===-- Timer.cpp ---------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "llvm/Support/Timer2.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Signposts.h"

#include <algorithm>
#include <limits>
#include <fstream>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

#include <cassert>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>

using namespace llvm;

#define TIMER_INDENT_AMOUNT 2

namespace {
typedef std::vector<Timer2 *> TimerStack;
static std::atomic<Timer2::Category *> g_categories;
} // end of anonymous namespace

/// Allows llvm::Timer to emit signposts when supported.
static llvm::ManagedStatic<llvm::SignpostEmitter> Signposts;

#if 0
std::atomic<bool> Timer2::g_quiet(false);
std::atomic<unsigned> Timer2::g_display_depth(std::numeric_limits<uint32_t>::max());
#elif 1
std::atomic<bool> Timer2::g_quiet(true);
std::atomic<unsigned> Timer2::g_display_depth(std::numeric_limits<uint32_t>::max());
#else
std::atomic<bool> Timer2::g_quiet(true);
std::atomic<unsigned> Timer2::g_display_depth(0);
#endif
static std::mutex &GetFileMutex() {
  static std::mutex *g_file_mutex_ptr = new std::mutex();
  return *g_file_mutex_ptr;
}

static TimerStack &GetTimerStackForCurrentThread() {
  static thread_local TimerStack g_stack;
  return g_stack;
}

Timer2::Category::Category(const char *cat) : m_name(cat) {
  m_nanos.store(0, std::memory_order_release);
  m_nanos_total.store(0, std::memory_order_release);
  m_count.store(0, std::memory_order_release);
  Category *expected = g_categories;
  do {
    m_next = expected;
  } while (!g_categories.compare_exchange_weak(expected, this));
}

void Timer2::SetQuiet(bool value) { g_quiet = value; }

static FILE *g_output = nullptr;

Timer2::Timer2(Timer2::Category &category, const char *format, ...)
    : m_category(category), m_total_start(std::chrono::steady_clock::now()) {
  static auto initialized = [&]() -> bool {
    g_output = ::fopen("lldb-timers.txt", "w+");
    return true;
  }();

  assert(g_output != nullptr);
  Signposts->startInterval(this, m_category.GetName());
  TimerStack &stack = GetTimerStackForCurrentThread();

  stack.push_back(this);
  if (!g_quiet && stack.size() <= g_display_depth) {
    std::lock_guard<std::mutex> lock(GetFileMutex());

    //auto* out = stdout;
    auto* out = g_output;

    // Indent
    ::fprintf(out, "%*s", int(stack.size() - 1) * TIMER_INDENT_AMOUNT, "");
    // Print formatted string
    va_list args;
    va_start(args, format);
    ::vfprintf(out, format, args);
    va_end(args);

    // Newline
    ::fprintf(out, "\n");
  }
}

Timer2::~Timer2() {
  using namespace std::chrono;

  auto stop_time = steady_clock::now();
  auto total_dur = stop_time - m_total_start;
  auto timer_dur = total_dur - m_child_duration;

  Signposts->endInterval(this, m_category.GetName());

  TimerStack &stack = GetTimerStackForCurrentThread();
  if (!g_quiet && stack.size() <= g_display_depth) {
    std::lock_guard<std::mutex> lock(GetFileMutex());
    //auto *out = stdout;
    auto *out = g_output;
    ::fprintf(out, "%*s%.9f sec (%.9f sec)\n",
              int(stack.size() - 1) * TIMER_INDENT_AMOUNT, "",
              duration<double>(total_dur).count(),
              duration<double>(timer_dur).count());
  }

  assert(stack.back() == this);
  stack.pop_back();
  if (!stack.empty())
    stack.back()->ChildDuration(total_dur);

  // Keep total results for each category so we can dump results.
  m_category.m_nanos += std::chrono::nanoseconds(timer_dur).count();
  m_category.m_nanos_total += std::chrono::nanoseconds(total_dur).count();
  m_category.m_count++;
}

void Timer2::SetDisplayDepth(uint32_t depth) { g_display_depth = depth; }

/* binary function predicate:
 * - returns whether a person is less than another person
 */
namespace {
struct Stats {
  const char *name;
  uint64_t nanos;
  uint64_t nanos_total;
  uint64_t count;
};
} // namespace

void Timer2::ResetCategoryTimes() {
  for (Category *i = g_categories; i; i = i->m_next) {
    i->m_nanos.store(0, std::memory_order_release);
    i->m_nanos_total.store(0, std::memory_order_release);
    i->m_count.store(0, std::memory_order_release);
  }
}
