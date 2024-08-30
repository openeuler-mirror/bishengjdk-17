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

package jdk.jbooster;

import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

import static java.lang.System.Logger.Level.INFO;
import static jdk.jbooster.JBooster.LOGGER;

/**
 * The network connection thread pool of the JBooster server.
 */
public final class ConnectionPool {
    private static final int CORE_POOL_SIZE = Math.min(25, Math.max(8, 2 * Runtime.getRuntime().availableProcessors()));
    private static final int MAX_POOL_SIZE = 200;
    private static final long KEEP_ALIVE_SECONDS = 60L;

    private final ThreadPoolExecutor executor;

    ConnectionPool() {
        executor = new ConnectionPoolExecutor();
    }

    public ThreadPoolExecutor getExecutor() {
        return executor;
    }

    public boolean execute(Runnable command) {
        try {
            executor.execute(command);
        } catch (RejectedExecutionException e) {
            return false;
        }
        return true;
    }

    private static class ConnectionPoolExecutor extends ThreadPoolExecutor {

        public ConnectionPoolExecutor() {
            super(CORE_POOL_SIZE, MAX_POOL_SIZE, KEEP_ALIVE_SECONDS, TimeUnit.SECONDS,
                    new SynchronousQueue<>(), new ThreadPoolExecutor.AbortPolicy());
        }

        @Override
        protected void terminated() {
            LOGGER.log(INFO, "JBooster connection pool terminated.");
        }
    }
}
