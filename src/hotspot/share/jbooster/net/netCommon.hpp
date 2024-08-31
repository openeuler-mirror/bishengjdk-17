/*
 * Copyright (c) 2020, 2023, Huawei Technologies Co., Ltd. All rights reserved.
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
 */

#ifndef SHARE_JBOOSTER_NET_NETCOMMON_HPP
#define SHARE_JBOOSTER_NET_NETCOMMON_HPP

#include "jbooster/net/errorCode.hpp"
#include "jbooster/net/messageType.hpp"

// C++ exceptions are disabled in Hotspot (see hotspot-style.md).
// And Thread::current() is not always available in our cases so we do not choose
// to use the tools in exceptions.hpp.
// Instead we choose to return the error code just like ZGC does (see zErrno.hpp).
//
// Here is a simple implementation of the "try&catch" statements. They are used
// only for CommunicationStream. How to use:
//
//    // try (no support for "break" or "continue" when in a for/switch block)
//    JB_TRY {
//      JB_THROW(func());
//      if (...) JB_THROW(err_code1);
//      while (...) {...} JB_THROW(JB_ERR);
//    } JB_TRY_END
//
//    // or use this (supports "break" and "continue" when in a for/switch block))
//    JB_TRY_BREAKABLE {
//      JB_THROW(func());
//      if (...) JB_THROW(err_code1);
//      while (...) {...} JB_THROW(JB_ERR);
//      if (...) continue; else break;
//    } JB_TRY_BREAKABLE_END
//
//    // catch (do not insert any code between "JB_TRY_END" and "JB_CATCH")
//    JB_CATCH(err_code2) {...}
//    JB_CATCH(err_code3, err_code4, err_code5) {...}
//    JB_CATCH_REST() {
//      if (JB_ERR != err_code6) break;
//      else continue;
//    } JB_CATCH_END;
//
// Note1: Use JB_TRY_BREAKABLE if you are in "while/for/switch" statements and want
//        to use "break" or "continue" in our "try" block. Use JB_TRY if not.
// Note2: Always use JB_THROW in a JB_TRY or JB_TRY_BREAKABLE block!
//        It does not support being thrown to the upper layer.
// Note3: We use "do {...} while (false);" to implement the "try/catch" statements.
//        So if you have another "while/for/switch" statement inside the JB_TRY,
//        insert a JB_THROW(JB_ERR) after the statement.
#define JB_ERR              __jbooster_errcode__

// do not call JB_INNER_ macros
#define JB_INNER_CONTROL    __jbooster_control__
#define JB_INNER_NORMAL     0
#define JB_INNER_CONTINUE   1
#define JB_INNER_BREAK      2

#define JB_INNER_CMP_1(e)             JB_ERR == (e)
#define JB_INNER_CMP_2(e, ...)        JB_INNER_CMP_1(e) || JB_INNER_CMP_1(__VA_ARGS__)
#define JB_INNER_CMP_3(e, ...)        JB_INNER_CMP_1(e) || JB_INNER_CMP_2(__VA_ARGS__)
#define JB_INNER_CMP_4(e, ...)        JB_INNER_CMP_1(e) || JB_INNER_CMP_3(__VA_ARGS__)
#define JB_INNER_CMP_5(e, ...)        JB_INNER_CMP_1(e) || JB_INNER_CMP_4(__VA_ARGS__)
#define JB_INNER_CMP_6(e, ...)        JB_INNER_CMP_1(e) || JB_INNER_CMP_5(__VA_ARGS__)
#define JB_INNER_CMP_7(e, ...)        JB_INNER_CMP_1(e) || JB_INNER_CMP_6(__VA_ARGS__)
#define JB_INNER_CMP_8(e, ...)        JB_INNER_CMP_1(e) || JB_INNER_CMP_7(__VA_ARGS__)

#define JB_INNER_CMP_GET(_1, _2, _3, _4, _5, _6, _7, _8, V, ...) V
#define JB_INNER_CMP(...) JB_INNER_CMP_GET(__VA_ARGS__,     \
                                          JB_INNER_CMP_8,   \
                                          JB_INNER_CMP_7,   \
                                          JB_INNER_CMP_6,   \
                                          JB_INNER_CMP_5,   \
                                          JB_INNER_CMP_4,   \
                                          JB_INNER_CMP_3,   \
                                          JB_INNER_CMP_2,   \
                                          JB_INNER_CMP_1)   \
                                          (__VA_ARGS__)

// the try/catch statements
#define JB_TRY                  { int JB_ERR = 0; do
#define JB_TRY_END                while (false);                                      \
                                  if (JB_ERR == 0) { /* do nothing */ }

#define JB_TRY_BREAKABLE        { int JB_ERR = 0;                                     \
                                  uint8_t JB_INNER_CONTROL = JB_INNER_BREAK;          \
                                  do {
#define JB_TRY_BREAKABLE_END        JB_INNER_CONTROL = JB_INNER_NORMAL;               \
                                    break;                                            \
                                  } while ((JB_INNER_CONTROL = JB_INNER_CONTINUE)     \
                                           != JB_INNER_CONTINUE);                     \
                                  if (JB_ERR == 0) {                                  \
                                    if (JB_INNER_CONTROL != JB_INNER_NORMAL) {        \
                                      if (JB_INNER_CONTROL == JB_INNER_BREAK) break;  \
                                      else continue;                                  \
                                    }                                                 \
                                  }

// Here we use "expression" instead of "(expression)" to be compatible with "CHECK"
// in exceptions.hpp.
#define JB_THROW(expression)        { JB_ERR = expression;                            \
                                      if (JB_ERR != 0) break;                         \
                                    }

#define JB_CATCH(...)             else if (JB_INNER_CMP(__VA_ARGS__))
#define JB_CATCH_REST()           else
#define JB_CATCH_END            }

// Here we use "expression" instead of "(expression)" to be compatible with "CHECK"
// in exceptions.hpp.
#define JB_RETURN(expression)   { int JB_ERR = expression;                            \
                                  if (JB_ERR != 0) return JB_ERR;                     \
                                }

// End of the definition of JB_TRY/JB_CATCH.

class MessageConst {
public:
  enum: uint32_t {
    /**
     * The layout of the message in the buffer:
     * | msg_size | msg_type | ... (all of the arguments) ... |
     * | 4 bytes  | 2 bytes  |     msg_size - 4 - 2 bytes     |
     */
    meta_size = sizeof(uint32_t) + sizeof(MessageType),
    /**
     * The layout of each argument in the buffer:
     * | arg_size | ... (payload of the argument) ... |
     * | 4 bytes  |          arg_size bytes           |
     */
    arg_meta_size = sizeof(uint32_t),

    NULL_PTR = 0xffffffff   // @see ArrayWrapper::NULL_PTR
  };
};

#endif // SHARE_JBOOSTER_NET_NETCOMMON_HPP
