/*
 * Copyright (c) 1999, 2022, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2014, Red Hat Inc. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

// no precompiled headers
#include "jvm.h"
#include "asm/macroAssembler.hpp"
#include "classfile/vmSymbols.hpp"
#include "code/codeCache.hpp"
#include "code/icBuffer.hpp"
#include "code/vtableStubs.hpp"
#include "code/nativeInst.hpp"
#include "interpreter/interpreter.hpp"
#include "memory/allocation.inline.hpp"
#include "os_share_linux.hpp"
#include "prims/jniFastGetField.hpp"
#include "prims/jvm_misc.hpp"
#include "runtime/arguments.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/interfaceSupport.inline.hpp"
#include "runtime/java.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/osThread.hpp"
#include "runtime/safepointMechanism.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/thread.inline.hpp"
#include "runtime/timer.hpp"
#include "signals_posix.hpp"
#include "utilities/debug.hpp"
#include "utilities/events.hpp"
#include "utilities/vmError.hpp"

// put OS-includes here
# include <sys/types.h>
# include <sys/mman.h>
# include <pthread.h>
# include <signal.h>
# include <errno.h>
# include <dlfcn.h>
# include <stdlib.h>
# include <stdio.h>
# include <unistd.h>
# include <sys/resource.h>
# include <pthread.h>
# include <sys/stat.h>
# include <sys/time.h>
# include <sys/utsname.h>
# include <sys/socket.h>
# include <sys/wait.h>
# include <pwd.h>
# include <poll.h>
# include <ucontext.h>

#define REG_FP 29
#define REG_LR 30

NOINLINE address os::current_stack_pointer() {
  return (address)__builtin_frame_address(0);
}

char* os::non_memory_address_word() {
  // Must never look like an address returned by reserve_memory,
  // even in its subfields (as defined by the CPU immediate fields,
  // if the CPU splits constants across multiple instructions).

  return (char*) 0xffffffffffff;
}

address os::Posix::ucontext_get_pc(const ucontext_t * uc) {
  return (address)uc->uc_mcontext.pc;
}

void os::Posix::ucontext_set_pc(ucontext_t * uc, address pc) {
  uc->uc_mcontext.pc = (intptr_t)pc;
}

intptr_t* os::Linux::ucontext_get_sp(const ucontext_t * uc) {
  return (intptr_t*)uc->uc_mcontext.sp;
}

intptr_t* os::Linux::ucontext_get_fp(const ucontext_t * uc) {
  return (intptr_t*)uc->uc_mcontext.regs[REG_FP];
}

address os::fetch_frame_from_context(const void* ucVoid,
                    intptr_t** ret_sp, intptr_t** ret_fp) {

  address epc;
  const ucontext_t* uc = (const ucontext_t*)ucVoid;

  if (uc != NULL) {
    epc = os::Posix::ucontext_get_pc(uc);
    if (ret_sp) *ret_sp = os::Linux::ucontext_get_sp(uc);
    if (ret_fp) *ret_fp = os::Linux::ucontext_get_fp(uc);
  } else {
    epc = NULL;
    if (ret_sp) *ret_sp = (intptr_t *)NULL;
    if (ret_fp) *ret_fp = (intptr_t *)NULL;
  }

  return epc;
}

frame os::fetch_frame_from_context(const void* ucVoid) {
  intptr_t* sp;
  intptr_t* fp;
  address epc = fetch_frame_from_context(ucVoid, &sp, &fp);
  if (!is_readable_pointer(epc)) {
    // Try to recover from calling into bad memory
    // Assume new frame has not been set up, the same as
    // compiled frame stack bang
    return fetch_compiled_frame_from_context(ucVoid);
  }
  return frame(sp, fp, epc);
}

frame os::fetch_compiled_frame_from_context(const void* ucVoid) {
  const ucontext_t* uc = (const ucontext_t*)ucVoid;
  // In compiled code, the stack banging is performed before LR
  // has been saved in the frame.  LR is live, and SP and FP
  // belong to the caller.
  intptr_t* fp = os::Linux::ucontext_get_fp(uc);
  intptr_t* sp = os::Linux::ucontext_get_sp(uc);
  address pc = (address)(uc->uc_mcontext.regs[REG_LR]
                         - NativeInstruction::instruction_size);
  return frame(sp, fp, pc);
}

// By default, gcc always saves frame pointer rfp on this stack. This
// may get turned off by -fomit-frame-pointer.
frame os::get_sender_for_C_frame(frame* fr) {
  return frame(fr->link(), fr->link(), fr->sender_pc());
}

NOINLINE frame os::current_frame() {
  intptr_t *fp = *(intptr_t **)__builtin_frame_address(0);
  frame myframe((intptr_t*)os::current_stack_pointer(),
                (intptr_t*)fp,
                CAST_FROM_FN_PTR(address, os::current_frame));
  if (os::is_first_C_frame(&myframe)) {
    // stack is not walkable
    return frame();
  } else {
    return os::get_sender_for_C_frame(&myframe);
  }
}

bool PosixSignals::pd_hotspot_signal_handler(int sig, siginfo_t* info,
                                             ucontext_t* uc, JavaThread* thread) {

/*
  NOTE: does not seem to work on linux.
  if (info == NULL || info->si_code <= 0 || info->si_code == SI_NOINFO) {
    // can't decode this kind of signal
    info = NULL;
  } else {
    assert(sig == info->si_signo, "bad siginfo");
  }
*/
  // decide if this trap can be handled by a stub
  address stub = NULL;

  address pc          = NULL;

  //%note os_trap_1
  if (info != NULL && uc != NULL && thread != NULL) {
    pc = (address) os::Posix::ucontext_get_pc(uc);

    address addr = (address) info->si_addr;

    // Make sure the high order byte is sign extended, as it may be masked away by the hardware.
    if ((uintptr_t(addr) & (uintptr_t(1) << 55)) != 0) {
      addr = address(uintptr_t(addr) | (uintptr_t(0xFF) << 56));
    }

    // Handle ALL stack overflow variations here
    if (sig == SIGSEGV) {
      // check if fault address is within thread stack
      if (thread->is_in_full_stack(addr)) {
        if (os::Posix::handle_stack_overflow(thread, addr, pc, uc, &stub)) {
          return true; // continue
        }
      }
    }

    if (thread->thread_state() == _thread_in_Java) {
      // Java thread running in Java code => find exception handler if any
      // a fault inside compiled code, the interpreter, or a stub

      // Handle signal from NativeJump::patch_verified_entry().
      if ((sig == SIGILL || sig == SIGTRAP)
          && nativeInstruction_at(pc)->is_sigill_zombie_not_entrant()) {
        if (TraceTraps) {
          tty->print_cr("trap: zombie_not_entrant (%s)", (sig == SIGTRAP) ? "SIGTRAP" : "SIGILL");
        }
        stub = SharedRuntime::get_handle_wrong_method_stub();
      } else if (sig == SIGSEGV && SafepointMechanism::is_poll_address((address)info->si_addr)) {
        stub = SharedRuntime::get_poll_stub(pc);
      } else if (sig == SIGBUS /* && info->si_code == BUS_OBJERR */) {
        // BugId 4454115: A read from a MappedByteBuffer can fault
        // here if the underlying file has been truncated.
        // Do not crash the VM in such a case.
        CodeBlob* cb = CodeCache::find_blob_unsafe(pc);
        CompiledMethod* nm = (cb != NULL) ? cb->as_compiled_method_or_null() : NULL;
        bool is_unsafe_arraycopy = (thread->doing_unsafe_access() && UnsafeCopyMemory::contains_pc(pc));
        if ((nm != NULL && nm->has_unsafe_access()) || is_unsafe_arraycopy) {
          address next_pc = pc + NativeCall::instruction_size;
          if (is_unsafe_arraycopy) {
            next_pc = UnsafeCopyMemory::page_error_continue_pc(pc);
          }
          stub = SharedRuntime::handle_unsafe_access(thread, next_pc);
        }
      } else if (sig == SIGILL && nativeInstruction_at(pc)->is_stop()) {
        // Pull a pointer to the error message out of the instruction
        // stream.
        const uint64_t *detail_msg_ptr
          = (uint64_t*)(pc + NativeInstruction::instruction_size);
        const char *detail_msg = (const char *)*detail_msg_ptr;
        const char *msg = "stop";
        if (TraceTraps) {
          tty->print_cr("trap: %s: (SIGILL)", msg);
        }

        // End life with a fatal error, message and detail message and the context.
        // Note: no need to do any post-processing here (e.g. signal chaining)
        va_list va_dummy;
        VMError::report_and_die(thread, uc, NULL, 0, msg, detail_msg, va_dummy);
        va_end(va_dummy);

        ShouldNotReachHere();

      }
      else

      if (sig == SIGFPE  &&
          (info->si_code == FPE_INTDIV || info->si_code == FPE_FLTDIV)) {
        stub =
          SharedRuntime::
          continuation_for_implicit_exception(thread,
                                              pc,
                                              SharedRuntime::
                                              IMPLICIT_DIVIDE_BY_ZERO);
      } else if (sig == SIGSEGV &&
                 MacroAssembler::uses_implicit_null_check((void*)addr)) {
          // Determination of interpreter/vtable stub/compiled code null exception
          stub = SharedRuntime::continuation_for_implicit_exception(thread, pc, SharedRuntime::IMPLICIT_NULL);
      }
    } else if ((thread->thread_state() == _thread_in_vm ||
                 thread->thread_state() == _thread_in_native) &&
               sig == SIGBUS && /* info->si_code == BUS_OBJERR && */
               thread->doing_unsafe_access()) {
      address next_pc = pc + NativeCall::instruction_size;
      if (UnsafeCopyMemory::contains_pc(pc)) {
        next_pc = UnsafeCopyMemory::page_error_continue_pc(pc);
      }
      stub = SharedRuntime::handle_unsafe_access(thread, next_pc);
    }

    // jni_fast_Get<Primitive>Field can trap at certain pc's if a GC kicks in
    // and the heap gets shrunk before the field access.
    if ((sig == SIGSEGV) || (sig == SIGBUS)) {
      address addr = JNI_FastGetField::find_slowcase_pc(pc);
      if (addr != (address)-1) {
        stub = addr;
      }
    }
  }

  if (stub != NULL) {
    // save all thread context in case we need to restore it
    if (thread != NULL) thread->set_saved_exception_pc(pc);

    os::Posix::ucontext_set_pc(uc, stub);
    return true;
  }

  return false; // Mute compiler
}

void os::Linux::init_thread_fpu_state(void) {
}

int os::Linux::get_fpu_control_word(void) {
  return 0;
}

void os::Linux::set_fpu_control_word(int fpu_control) {
}

////////////////////////////////////////////////////////////////////////////////
// thread stack

// Minimum usable stack sizes required to get to user code. Space for
// HotSpot guard pages is added later.
size_t os::Posix::_compiler_thread_min_stack_allowed = 72 * K;
size_t os::Posix::_java_thread_min_stack_allowed = 72 * K;
size_t os::Posix::_vm_internal_thread_min_stack_allowed = 72 * K;

// return default stack size for thr_type
size_t os::Posix::default_stack_size(os::ThreadType thr_type) {
  // default stack size (compiler thread needs larger stack)
  size_t s = (thr_type == os::compiler_thread ? 4 * M : 1 * M);
  return s;
}

/////////////////////////////////////////////////////////////////////////////
// helper functions for fatal error handler

void os::print_context(outputStream *st, const void *context) {
  if (context == NULL) return;

  const ucontext_t *uc = (const ucontext_t*)context;

  st->print_cr("Registers:");
  for (int r = 0; r < 31; r++) {
    st->print_cr(  "R%d=" INTPTR_FORMAT, r, (uintptr_t)uc->uc_mcontext.regs[r]);
  }
  st->cr();
}

void os::print_tos_pc(outputStream *st, const void *context) {
  if (context == NULL) return;

  const ucontext_t* uc = (const ucontext_t*)context;

  address sp = (address)os::Linux::ucontext_get_sp(uc);
  print_tos(st, sp);
  st->cr();

  // Note: it may be unsafe to inspect memory near pc. For example, pc may
  // point to garbage if entry point in an nmethod is corrupted. Leave
  // this at the end, and hope for the best.
  address pc = os::fetch_frame_from_context(uc).pc();
  print_instructions(st, pc);
  st->cr();
}

void os::print_register_info(outputStream *st, const void *context) {
  if (context == NULL) return;

  const ucontext_t *uc = (const ucontext_t*)context;

  st->print_cr("Register to memory mapping:");
  st->cr();

  // this is horrendously verbose but the layout of the registers in the
  // context does not match how we defined our abstract Register set, so
  // we can't just iterate through the gregs area

  // this is only for the "general purpose" registers

  for (int r = 0; r < 31; r++) {
    st->print("R%-2d=", r);
    print_location(st, uc->uc_mcontext.regs[r]);
  }
  st->cr();
}

void os::setup_fpu() {
}

#ifndef PRODUCT
void os::verify_stack_alignment() {
  assert(((intptr_t)os::current_stack_pointer() & (StackAlignmentInBytes-1)) == 0, "incorrect stack alignment");
}
#endif

int os::extra_bang_size_in_bytes() {
  // AArch64 does not require the additional stack bang.
  return 0;
}
extern char** argv_for_execvp;

static bool is_non_negative_integer(const char* buf, int* res) {
  julong v;
  if (!Arguments::atojulong(buf, &v)) {
    return false;
  }
  if (v > INT_MAX) {
    return false;
  }
  *res = (int)v;
  return true;
}

static bool parse_bind_policy(const char* policy, const char* &prefix, int &div) {
  const char* p = policy;
  while (*p != '\0') {
    const char* eq = strchr(p, '=');
    if (!eq) break;

    const char* key = p;
    int key_len = eq - key;
    const char* val = eq + 1;

    const char* comma = strchr(val, ',');
    int val_len = comma ? (comma - val) : (int)strlen(val);

    if (key_len == 6 && strncmp(key, "prefix", 6) == 0) {
      if (val_len == 0) {
        if (LogNUMANodes) {
          warning("NUMABindPolicy: prefix cannot be empty.");
        }
        return false;
      }
      char* tmp = NEW_C_HEAP_ARRAY(char, val_len + 1, mtInternal);
      memcpy(tmp, val, val_len);
      tmp[val_len] = '\0';
      prefix = tmp;
    } else if (key_len == 3 && strncmp(key, "div", 3) == 0) {
      if (!is_non_negative_integer(val, &div) || div == 0) {
        if (LogNUMANodes) {
          warning("NUMABindPolicy: div must be a positive integer, got '%s'", val);
        }
        return false;
      }
    }

    if (!comma) break;
    p = comma + 1;
  }
  if (prefix == NULL || div == 0) {
    if (LogNUMANodes) {
      warning("Lack of prefix/div. Feature disabled.");
    }
    return false;
  }
  return true;
}

void os::Linux::chose_numa_nodes() {
  const char* numa_chosen_env = getenv("_JVM_NUMA_BINDING_DONE");
  if (numa_chosen_env != NULL && strcmp(numa_chosen_env, "1") == 0) {
    if (LogNUMANodes) {
      warning("NUMA binding already done (detected via environment variable), skipping");
    }
    return;
  }

  if (NUMANodes == NULL && NUMANodesRandom == 0) {
    if (LogNUMANodes) {
      warning("Numa binding will not work without NUMANodes or NUMANodesRandom.");
    }
    return;
  }

  const int MAX_DISTANCE = 999999;

  int nodes_num = Linux::numa_max_node() + 1;
  const int MAXNODE = 100;
  if (nodes_num <= 0 || nodes_num >= MAXNODE) {
    if (LogNUMANodes) {
      warning("Invalid NUMA nodes number: %d", nodes_num);
    }
    return;
  }

  // Parse the NUMANodes
  bool user_specified_nodes[MAXNODE] = {false};
  bool has_user_constraint = false;

  if (NUMANodes != NULL) {
    if (LogNUMANodes) {
      warning("NUMANodes parameter specified: %s", NUMANodes);
    }

    // Parse the nodestring
    bitmask* user_nodes_mask = os::Linux::numa_parse_nodestring_all(NUMANodes);
    if (user_nodes_mask != NULL) {
      has_user_constraint = true;
      for (int i = 0; i < nodes_num; i++) {
        if (_numa_bitmask_isbitset(user_nodes_mask, i)) {
          user_specified_nodes[i] = true;
          if (LogNUMANodes) {
            warning("User specified node %d is allowed", i);
          }
        }
      }
      os::Linux::numa_bitmask_free(user_nodes_mask);
    } else {
      if (LogNUMANodes) {
        warning("Failed to parse NUMANodes: %s", NUMANodes);
        warning("Skip NUMANodes parameter.");
      }
    }
  }

  // Save the original CPU mask specified by system(numactl)
  cpu_set_t original_cpu_mask;
  if (sched_getaffinity(0, sizeof(cpu_set_t), &original_cpu_mask) == -1) {
    perror("sched_getaffinity");
    return;
  }

  // Within the limit, count available cpus each NUMA node has
  int cpu_count_per_node[MAXNODE] = {0};
  bool node_has_cpu[MAXNODE] = {false};

  int cpus_num = os::Linux::numa_num_configured_cpus();

  // Traverse all cpus and count which cpus each Node has
  for (int i = 0; i < cpus_num; i++) {
    if (CPU_ISSET(i, &original_cpu_mask)) {
      int node_id = _numa_node_of_cpu(i);
      if (node_id == -1) {
        if (LogNUMANodes) {
          warning("Failed to get NUMA node for CPU %d", i);
        }
      } else if (node_id >= 0 && node_id < MAXNODE) {
        node_has_cpu[node_id] = true;
        cpu_count_per_node[node_id]++;
        if (LogNUMANodes) {
          warning("CPU %d belongs to Node %d", i, node_id);
        }
      }
    }
  }

  // Build a list of available nodes
  int available_nodes[MAXNODE];
  int available_nodes_count = 0;
  for (int i = 0; i < nodes_num; i++) {
    if (node_has_cpu[i]) {
      // Set the NUMANodes
      if (has_user_constraint && !user_specified_nodes[i]) {
        if (LogNUMANodes) {
          warning("Node %d: has CPUs but excluded by NUMANodes parameter", i);
        }
        continue;
      }

      available_nodes[available_nodes_count++] = i;
      if (LogNUMANodes) {
        warning("Node %d: available, CPU count = %d", i, cpu_count_per_node[i]);
      }
    } else if (has_user_constraint && user_specified_nodes[i]) {
      // Specified this node, but no available CPU under the system(numactl) limit
      if (LogNUMANodes) {
        warning("Node %d: specified in NUMANodes but has no available CPUs in numactl range", i);
      }
    }
  }

  if (available_nodes_count == 0) {
    if (LogNUMANodes) {
      if (has_user_constraint) {
        warning("No available NUMA nodes found in NUMANodes range: %s", NUMANodes);
      } else {
        warning("No available NUMA nodes found");
      }
    }
    return;
  }

  if (LogNUMANodes) {
    warning("Total available nodes (after NUMANodes filter): %d", available_nodes_count);
  }

  // Check the memory binding permissions
  bitmask * mem_allowed = _numa_get_mems_allowed();
  if (!mem_allowed) {
    if (LogNUMANodes) {
      warning("Failed to get mems allowed");
    }
    return;
  }

  // Determine the number of nodes to be selected
  int nodes_to_select_cpu = NUMANodesRandom;
  int nodes_to_select_mem = NUMAMemNodesRandom;

  // If NUMANodesRandom is not set or the value is invalid, use all available nodes
  if (nodes_to_select_cpu <= 0 || nodes_to_select_cpu > available_nodes_count) {
    nodes_to_select_cpu = available_nodes_count;
  }

  // If NUMAMemNodesRandom is invalid, use the same nodes as cpu.
  if (nodes_to_select_mem < nodes_to_select_cpu || nodes_to_select_mem > available_nodes_count) {
    nodes_to_select_mem = nodes_to_select_cpu;
  }

  if (LogNUMANodes) {
    if (has_user_constraint) {
      warning("NUMANodes filter applied: %s, available nodes after filter: %d",
              NUMANodes, available_nodes_count);
    }
    warning("NUMANodesRandom=%lu, will select %d nodes from %d available nodes for cpu",
            NUMANodesRandom, nodes_to_select_cpu, available_nodes_count);
    warning("NUMAMemNodesRandom=%lu, will select %d nodes from %d available nodes for memory",
            NUMAMemNodesRandom, nodes_to_select_mem, available_nodes_count);
  }

  int d0 = _numa_distance(0, 0);
  int d1 = (nodes_num > 1) ? _numa_distance(0, 1) : -1;
  int d2 = (nodes_num > 2) ? _numa_distance(0, 2) : -1;
  if (d1 <= d0 || (d2 != -1 && d2 <= d1)) {
    if (LogNUMANodes) {
      warning("Non-hierarchical NUMA topology detected. Feature disabled.");
    }
    return;
  }

  const char* policy = NUMABindPolicy;
  const char* process_prefix = NULL;
  int process_div = 0;

  int random_number;
  if (policy != NULL) {
    if (!parse_bind_policy(policy, process_prefix, process_div)) { 
      return;
    }

    if (LogNUMANodes) {
      warning("NUMABindPolicy: prefix=%s div=%d",
              process_prefix != NULL ? process_prefix : "<null>",
              process_div);
    }

    int i = 0;
    int prefix_id = -1;
    while (argv_for_execvp[i] != NULL) {
      const char* arg = argv_for_execvp[i];
      if (strcmp(arg, process_prefix) == 0 && argv_for_execvp[i + 1] != NULL) {
        if (!is_non_negative_integer(argv_for_execvp[i + 1], &prefix_id)) {
          if (LogNUMANodes) {
            warning("Invalid prefix id, feature disabled.");
          }
          return;
        }
        break;
      }
      int key_len = strlen(process_prefix);
      if (strncmp(arg, process_prefix, key_len) == 0 && arg[key_len] == '=') {
        const char* val = arg + key_len + 1;
        if (*val == '\0' || !is_non_negative_integer(val, &prefix_id)) {
          if (LogNUMANodes) {
            warning("Invalid prefix id, feature disabled.");
          }
          return;
        }
        break;
      }
      i++;
    }
    if (prefix_id < 0) {
      if (LogNUMANodes) {
        warning("Cannot find corresponding process according to prefix. Feature disabled.");
      }
      return;
    }
    random_number = prefix_id / process_div;
  } else {
    // Use timestamp as the random seed
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int pid = getpid();
    int tid = os::Linux::gettid();
    int ppid = getppid();
    unsigned int seed = (unsigned int)(tv.tv_usec ^ pid ^ tid ^ ppid);
    srand(seed);

    random_number = rand();
  }

  if (LogNUMANodes) {
    warning("Random Number is %d", random_number);
  }
  int start_index = random_number % available_nodes_count;
  int start_node = available_nodes[start_index];

  // Check whether the starting node can be bound to memory
  if (!(_numa_bitmask_isbitset(mem_allowed, start_node) &&
        _numa_bitmask_isbitset(_numa_membind_bitmask, start_node))) {
    // If the starting node is unavailable, find another one
    start_node = -1;
    for (int i = 0; i < available_nodes_count; i++) {
      int node_id = available_nodes[i];
      if (_numa_bitmask_isbitset(mem_allowed, node_id) &&
          _numa_bitmask_isbitset(_numa_membind_bitmask, node_id)) {
        start_node = node_id;
        break;
      }
    }
    if (start_node == -1) {
      os::Linux::numa_bitmask_free(mem_allowed);
      if (LogNUMANodes) {
        warning("No bindable nodes found!");
      }
      return;
    }
  }

  if (LogNUMANodes) {
    warning("Start node: %d", start_node);
  }

  // Select nodes: Choose the nearest nodes_to_select_cpu/nodes_to_select_mem node based on distance
  int selected_nodes[MAXNODE];
  int selected_count = 0;

  // First node is the starting node
  selected_nodes[selected_count++] = start_node;

  if (nodes_to_select_mem == 1) {
    if (LogNUMANodes) {
      warning("Selected node %d", start_node);
    }
  } else {
    // Select by distance
    bool node_selected[MAXNODE] = {false};
    node_selected[start_node] = true;

    while (selected_count < nodes_to_select_mem) {
      int nearest_node = -1;
      int min_total_distance = MAX_DISTANCE;

      // Calculate the total distance from it to all selected nodes
      for (int i = 0; i < available_nodes_count; i++) {
        int candidate = available_nodes[i];

        // Skip the selected nodes and the unbound nodes
        if (node_selected[candidate]) continue;
        if (!(_numa_bitmask_isbitset(mem_allowed, candidate) &&
              _numa_bitmask_isbitset(_numa_membind_bitmask, candidate))) {
          continue;
        }

        // Calculate the total distance from the candidate to all selected nodes
        int total_distance = 0;
        for (int j = 0; j < selected_count; j++) {
          int selected = selected_nodes[j];
          int dist = _numa_distance(candidate, selected);
          total_distance += dist;
          if (LogNUMANodes) {
            warning("Distance from node %d to node %d: %d", candidate, selected, dist);
          }
        }

        // Find the node with the smallest distance
        if (total_distance < min_total_distance) {
          min_total_distance = total_distance;
          nearest_node = candidate;
        }
      }

      // No more available nodes are found, exit
      if (nearest_node == -1) {
        if (LogNUMANodes) {
          warning("No more bindable nodes available, selected %d nodes", selected_count);
        }
        break;
      }

      // Select the nearest node
      selected_nodes[selected_count++] = nearest_node;
      node_selected[nearest_node] = true;

      if (LogNUMANodes) {
        warning("Selected node %d (total distance: %d)", nearest_node, min_total_distance);
      }
    }
  }

  os::Linux::numa_bitmask_free(mem_allowed);

  if (selected_count == 0) {
    if (LogNUMANodes) {
      warning("Cannot find proper nodes to bind!");
    }
    return;
  }

  if (LogNUMANodes) {
    warning("Final selected nodes count: %d", selected_count);
  }

  // New CPU mask: only include the CPU on the selected node
  cpu_set_t new_cpu_mask;
  CPU_ZERO(&new_cpu_mask);

  bitmask * node_cpumask = os::Linux::numa_allocate_cpumask();
  if (!node_cpumask) {
    if (LogNUMANodes) {
      warning("Cannot allocate bitmask for cpus!");
    }
    return;
  }

  // Only retain the CPU on the selected node
  for (int i = 0; i < MIN2(selected_count, nodes_to_select_cpu); i++) {
    int node_id = selected_nodes[i];

    if (_numa_node_to_cpus_v2(node_id, node_cpumask) != 0) {
      if (LogNUMANodes) {
        warning("Failed to get CPUs for node %d", node_id);
      }
      continue;
    }

    // Key: Only add cpus that meet both of the following conditions:
    // 1. Belongs to the selected Node
    // 2. Within the original limits of system(numactl)
    for (int cpu = 0; cpu < cpus_num; cpu++) {
      if (_numa_bitmask_isbitset(node_cpumask, cpu) &&
          CPU_ISSET(cpu, &original_cpu_mask)) {
        CPU_SET(cpu, &new_cpu_mask);
        if (LogNUMANodes) {
          warning("Keeping CPU %d from node %d", cpu, node_id);
        }
      }
    }
  }

  os::Linux::numa_bitmask_free(node_cpumask);

  // Set the bit mask
  char buf_cpu[256] = {0};
  int pos_cpu = 0;

  for (int i = 0; i < MIN2(selected_count, nodes_to_select_cpu); i++) {
    if (i > 0) {
      pos_cpu += snprintf(buf_cpu + pos_cpu, sizeof(buf_cpu) - pos_cpu, ",");
    }
    pos_cpu += snprintf(buf_cpu + pos_cpu, sizeof(buf_cpu) - pos_cpu, "%d", selected_nodes[i]);
  }

  char buf_mem[256] = {0};
  int pos_mem = 0;

  for (int i = 0; i < selected_count; i++) {   // mem uses ALL selected nodes
    if (i > 0) {
      pos_mem += snprintf(buf_mem + pos_mem, sizeof(buf_mem) - pos_mem, ",");
    }
    pos_mem += snprintf(buf_mem + pos_mem, sizeof(buf_mem) - pos_mem, "%d", selected_nodes[i]);
  }

  bitmask* mask = numa_allocate_nodemask();
  numa_bitmask_clearall(mask);
  for (int i = 0; i < selected_count; i++) {
    numa_bitmask_setbit(mask, selected_nodes[i]);  // Set the bit directly
  }

  if (os::Linux::numa_bitmask_equal(mask, os::Linux::_numa_membind_bitmask) &&
      CPU_EQUAL(&new_cpu_mask, &original_cpu_mask)) {
    os::Linux::numa_bitmask_free(mask);
    if (LogNUMANodes) {
      warning("CpuPolicy and Mempolicy is not changed, cpu param: %s, mem param: %s", buf_cpu, buf_mem);
    }
    return;
  }

  // Set the NUMA memory binding
  errno = 0;
  os::Linux::numa_run_on_node_mask(mask);
  if (errno) {
    perror("numa_run_on_node_mask");
  }

  errno = 0;
  os::Linux::numa_set_membind(mask);
  int errtmp = errno;
  os::Linux::numa_bitmask_free(mask);
  if (errtmp) {
    perror("numa_set_membind");
  }

  if (LogNUMANodes) {
    warning("Successfully bound mem to %d node(s): %s", selected_count, buf_mem);
  }

  // New CPU affinity
  if (sched_setaffinity(0, sizeof(cpu_set_t), &new_cpu_mask) == -1) {
    perror("sched_setaffinity");
    if (LogNUMANodes) {
      warning("Failed to set CPU affinity");
    }
    return;
  }

  if (LogNUMANodes) {
    warning("Successfully bound cpu to %d node(s): %s", MIN2(selected_count, nodes_to_select_cpu), buf_cpu);
    warning("Final available CPUs:");
    for (int cpu = 0; cpu < cpus_num; cpu++) {
      if (CPU_ISSET(cpu, &new_cpu_mask)) {
        warning("  CPU %d", cpu);
      }
    }
  }

  setenv("_JVM_NUMA_BINDING_DONE", "1", 1);

  execvp(*argv_for_execvp, argv_for_execvp);

  // If execvp fails, perror will be executed
  perror("execvp failed");
}

extern "C" {
  int SpinPause() {
    using spin_wait_func_ptr_t = void (*)();
    spin_wait_func_ptr_t func = CAST_TO_FN_PTR(spin_wait_func_ptr_t, StubRoutines::aarch64::spin_wait());
    assert(func != nullptr, "StubRoutines::aarch64::spin_wait must not be null.");
    (*func)();
    // If StubRoutines::aarch64::spin_wait consists of only a RET,
    // SpinPause can be considered as implemented. There will be a sequence
    // of instructions for:
    // - call of SpinPause
    // - load of StubRoutines::aarch64::spin_wait stub pointer
    // - indirect call of the stub
    // - return from the stub
    // - return from SpinPause
    // So '1' always is returned.
    return 1;
  }

  void _Copy_conjoint_jshorts_atomic(const jshort* from, jshort* to, size_t count) {
    if (from > to) {
      const jshort *end = from + count;
      while (from < end)
        *(to++) = *(from++);
    }
    else if (from < to) {
      const jshort *end = from;
      from += count - 1;
      to   += count - 1;
      while (from >= end)
        *(to--) = *(from--);
    }
  }
  void _Copy_conjoint_jints_atomic(const jint* from, jint* to, size_t count) {
    if (from > to) {
      const jint *end = from + count;
      while (from < end)
        *(to++) = *(from++);
    }
    else if (from < to) {
      const jint *end = from;
      from += count - 1;
      to   += count - 1;
      while (from >= end)
        *(to--) = *(from--);
    }
  }
  void _Copy_conjoint_jlongs_atomic(const jlong* from, jlong* to, size_t count) {
    if (from > to) {
      const jlong *end = from + count;
      while (from < end)
        os::atomic_copy64(from++, to++);
    }
    else if (from < to) {
      const jlong *end = from;
      from += count - 1;
      to   += count - 1;
      while (from >= end)
        os::atomic_copy64(from--, to--);
    }
  }

  void _Copy_arrayof_conjoint_bytes(const HeapWord* from,
                                    HeapWord* to,
                                    size_t    count) {
    memmove(to, from, count);
  }
  void _Copy_arrayof_conjoint_jshorts(const HeapWord* from,
                                      HeapWord* to,
                                      size_t    count) {
    memmove(to, from, count * 2);
  }
  void _Copy_arrayof_conjoint_jints(const HeapWord* from,
                                    HeapWord* to,
                                    size_t    count) {
    memmove(to, from, count * 4);
  }
  void _Copy_arrayof_conjoint_jlongs(const HeapWord* from,
                                     HeapWord* to,
                                     size_t    count) {
    memmove(to, from, count * 8);
  }
};
