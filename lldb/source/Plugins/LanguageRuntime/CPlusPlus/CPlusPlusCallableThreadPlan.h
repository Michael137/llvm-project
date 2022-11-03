//===-- CPlusPlusCallableThreadPlan.h ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_CPPTHREADPLAN_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_CPPTHREADPLAN_H

#include "lldb/Target/ThreadPlan.h"
#include "lldb/Target/ThreadPlanShouldStopHere.h"
#include "lldb/lldb-enumerations.h"
#include "lldb/lldb-types.h"

namespace lldb_private {

class CPlusPlusCallableThreadPlan : public ThreadPlan,
                                    public ThreadPlanShouldStopHere {
public:
  CPlusPlusCallableThreadPlan(Thread &thread, Address callable_address,
                              StackID return_stack_id, bool stop_others);

  ~CPlusPlusCallableThreadPlan() override;

  void GetDescription(Stream *s, lldb::DescriptionLevel level) override;

  bool ValidatePlan(Stream *error) override { return true; }

  lldb::StateType GetPlanRunState() override { return lldb::eStateRunning; }

  bool ShouldStop(Event *event_ptr) override;

  // The step through code might have to fill in the cache, so it is not safe
  // to run only one thread.
  bool StopOthers() override;

  // The base class MischiefManaged does some cleanup - so you have to call it
  // in your MischiefManaged derived class.
  bool MischiefManaged() override {
    ThreadPlan::MischiefManaged();
    return IsPlanComplete();
  }

  void DidPush() override;

  bool WillStop() override { return true; }

protected:
  bool DoPlanExplainsStop(Event *event_ptr) override { return true; }

  void SetFlagsToDefault() override;

private:
  static bool DefaultShouldStopHereCallback(ThreadPlan *current_plan,
                                            Flags &flags,
                                            lldb::FrameComparison operation,
                                            Status &status, void *baton);

  static bool IsBackAtCallsite(StackFrame *callsite_frame,
                               StackFrame *current_frame, Thread &thread);

  static uint32_t s_default_flag_values;

  Address m_callable_addr; /// Stores the address for our step through
                           /// function result structure.
  bool m_stop_others;

  lldb::ThreadPlanSP m_func_sp; /// This is the function call plan.  We fill it
                                /// at start, then set it to NULL when this plan
                                /// is done.  That way we know to go on to:
  lldb::ThreadPlanSP m_return_plan_sp;
  StackID m_return_stack_id;
};

} // namespace lldb_private

#endif // _H
