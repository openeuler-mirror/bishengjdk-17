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

#ifndef SHARE_JBOOSTER_JBOOSTER_GLOBALS_HPP
#define SHARE_JBOOSTER_JBOOSTER_GLOBALS_HPP

#include "runtime/globals_shared.hpp"

#define JBOOSTER_FLAGS(develop,                                             \
                       develop_pd,                                          \
                       product,                                             \
                       product_pd,                                          \
                       notproduct,                                          \
                       range,                                               \
                       constraint)                                          \
                                                                            \
  product(bool, UseJBooster, false, EXPERIMENTAL,                           \
          "Use and connect to a JBooster server.")                          \
                                                                            \
  product(bool, AsJBooster, false, EXPERIMENTAL,                            \
          "Play the role of the JBooster server. "                          \
          "This flag is automatically set in VM.")                          \
                                                                            \
  product(ccstr, JBoosterAddress, "127.0.0.1", EXPERIMENTAL,                \
          "Address of the JBooster server. Default: '127.0.0.1'.")          \
                                                                            \
  product(ccstr, JBoosterPort, NULL, EXPERIMENTAL,                          \
          "Port of the JBooster server.")                                   \
                                                                            \
  product(uint, JBoosterTimeout, 4'000, EXPERIMENTAL,                       \
          "Timeout of the JBooster connection. Default: 4,000 ms.")         \
                                                                            \
  product(bool, JBoosterExitIfUnsupported, true, EXPERIMENTAL,              \
          "Exit the VM if the client uses features "                        \
          "that are not supported by the server.")                          \
                                                                            \
  product(bool, JBoosterCrashIfNoServer, false, DIAGNOSTIC,                 \
          "Exit the VM if the server is not available.")                    \
                                                                            \
  product(ccstr, JBoosterProgramName, NULL, EXPERIMENTAL,                   \
          "Unique name of current app.")                                    \
                                                                            \
  product(ccstr, JBoosterCachePath, NULL, EXPERIMENTAL,                     \
          "The directory path for JBooster caches "                         \
          "(default: $HOME/.jbooster/client).")                             \
                                                                            \
  product(bool, JBoosterLocalMode, false, EXPERIMENTAL,                     \
          "No connection to the server and uses only the local cache.")     \
                                                                            \
  product(ccstr, JBoosterStartupSignal, NULL, EXPERIMENTAL,                 \
          "The first invocation of the signal method means the end of "     \
          "the client start-up phase. "                                     \
          "The relevant logic is executed at exit if it's not set.")        \
                                                                            \
  product(int, JBoosterStartupMaxTime, 600, EXPERIMENTAL,                   \
          "Max seconds required for the start-up phase (0 means off). "     \
          "A plan B when JBoosterStartupSignal fails.")                     \
          range(0, max_jint)                                                \
                                                                            \
  product(int, BoostStopAtLevel, 3, EXPERIMENTAL,                           \
          "0 for no optimization; 1 with class loader resource cache; "     \
          "2 with aggressive CDS; 3 with lazy AOT; 4 with PGO.")            \
          range(0, 4)                                                       \
                                                                            \
  product(ccstr, UseBoostPackages, "all", DIAGNOSTIC,                       \
          "\"all\" means \"aot+cds+clr+pgo\".")                             \
                                                                            \
  product(bool, JBoosterClientStrictMatch, false, DIAGNOSTIC,               \
          "Be strict when matching the client data.")                       \
                                                                            \
  product(bool, PrintAllClassInfo, false, DIAGNOSTIC,                       \
          "Print info of all class loaders and all classes "                \
          "at startup or at exit.")                                         \
                                                                            \
  product(bool, UseAggressiveCDS, false, EXPERIMENTAL,                      \
          "An aggressive stratage to improve start-up "                     \
          "because we avoid decoding the classfile.")                       \
                                                                            \
  product(bool, CheckClassFileTimeStamp, true, EXPERIMENTAL,                \
          "Check whether the modification time of the"                      \
          "class file is changed during UseAggressiveCDS.")                 \
                                                                            \
  product(bool, UseClassLoaderResourceCache, false, EXPERIMENTAL,           \
          "Cache and share the name-url pairs in "                          \
          "java.net.URLClassLoader#findResource.")                          \
                                                                            \
  product(ccstr, DumpClassLoaderResourceCacheFile, NULL, EXPERIMENTAL,      \
          "The file path to dump class loader resource cache.")             \
                                                                            \
  product(ccstr, LoadClassLoaderResourceCacheFile, NULL, EXPERIMENTAL,      \
          "The file path to laod class loader resource cache.")             \
                                                                            \
  product(uint, ClassLoaderResourceCacheSizeEachLoader, 2000, EXPERIMENTAL, \
          "Max number of entries that can be cached in each "               \
          "class loader (delete old values based on LRU).")                 \
                                                                            \
  product(bool, ClassLoaderResourceCacheVerboseMode, false, DIAGNOSTIC,     \
          "Dump/load more data for verification and debugging.")            \


// end of JBOOSTER_FLAGS

DECLARE_FLAGS(JBOOSTER_FLAGS)

#endif // SHARE_JBOOSTER_JBOOSTER_GLOBALS_HPP
