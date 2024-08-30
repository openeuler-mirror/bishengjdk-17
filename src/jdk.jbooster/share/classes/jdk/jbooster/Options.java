/*
 * Copyright (c) 2023, Huawei Technologies Co., Ltd. All rights reserved.
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

import java.util.Arrays;
import java.util.Optional;
import java.util.Properties;

import static java.lang.System.Logger.Level.INFO;
import static jdk.jbooster.JBooster.LOGGER;

/**
 * Options of the JBooster server.
 */
public final class Options {
    private static final int UNSET_PORT = 0;
    private static final int CONNECTION_TIMEOUT = 32 * 1000;
    private static final int CLEANUP_TIMEOUT = 2 * 24 * 60 * 60 * 1000;

    private static final Option[] OPTIONS = {new Option(of("-h", "-?", "-help", "--help"), null,
            "Print this help message of the JBooster server.") {
        @Override
        protected void process(Options options, String arg) {
            printHelp();
            System.exit(0);
        }
    }, new Option(of("-p", "--server-port"), "port-num",
            "The listening port of JBooster server (1024~65535). No default value.") {
        @Override
        protected void process(Options options, String arg) {
            int port = -1;
            try {
                port = Integer.parseInt(arg);
            } catch (NumberFormatException e) {
                throw new IllegalArgumentException("Failed to convert the arg \"" + arg + "\" to a int!");
            }
            if (port < 1024 || port > 65535) {
                throw new IllegalArgumentException("Port should be in 1024~65535!");
            }
            options.serverPort = port;
        }
    }, new Option(of("-t", "--connection-timeout"), "timeout-ms",
            "The connection timeout of JBooster server. Default: " + CONNECTION_TIMEOUT + " ms.") {
        @Override
        protected void process(Options options, String arg) {
            int timeout = Integer.parseInt(arg);
            if (timeout <= 0) {
                throw new IllegalArgumentException("Timeout should be greater than 0!");
            }
            options.connectionTimeout = timeout;
        }
    }, new Option(of("--unused-cleanup-timeout"), "timeout-ms",
            "The cleanup timeout for unused shared data. Default: " + CLEANUP_TIMEOUT + " ms (2 days).") {
        @Override
        protected void process(Options options, String arg) {
            int timeout = Integer.parseInt(arg);
            if (timeout <= 0) {
                throw new IllegalArgumentException("Timeout should be greater than 0!");
            }
            options.cleanupTimeout = timeout;
        }
    }, new Option(of("--cache-path"), "dir-path",
            "The directory path for JBooster caches. Default: \"$HOME/.jbooster/server\".") {
        @Override
        protected void process(Options options, String arg) {
            // verification is on c++ side
            options.cachePath = arg;
        }
    }, new Option(of("-b", "--background"), null,
            "Disable the scanner for inputting commands (use it if running in the background).") {
        @Override
        protected void process(Options options, String arg) {
            options.interactive = false;
        }
    }};

    private int serverPort = UNSET_PORT;
    private int connectionTimeout = CONNECTION_TIMEOUT;
    private int cleanupTimeout = CLEANUP_TIMEOUT;
    private String cachePath = null;   // set on C++ side
    private boolean interactive = true;

    public int getServerPort() {
        return serverPort;
    }

    public int getConnectionTimeout() {
        return connectionTimeout;
    }

    public int getCleanupTimeout() {
        return cleanupTimeout;
    }

    public String getCachePath() {
        return cachePath;
    }

    public boolean isInteractive() {
        return interactive;
    }

    /**
     * Parse the args of main().
     *
     * @param args args of main()
     */
    public void parseArgs(String[] args) {
        for (int i = 0; i < args.length; ++i) {
            String[] tmp = args[i].split("=", 2);
            String op = tmp[0];
            String arg = tmp.length == 2 ? tmp[1] : null;
            Optional<Option> opt = findOption(op);

            if (opt.isEmpty()) {
                onEmptyArg(op, args[i]);
                return;
            }

            Option option = opt.get();
            if (option.needArg()) {
                if (arg == null) {
                    ++i;
                    if (i >= args.length) {
                        throw new IllegalArgumentException("The option \"" + op + "\" needs argument!");
                    }
                    arg = args[i];
                }
            } else if (arg != null) {
                throw new IllegalArgumentException("The option \"" + op + "\" does not need any argument!");
            }

            option.process(this, arg);
        }

        checkIllegalArgruments();
        onAllCmdParsed();
    }

    public static void printHelp() {
        StringBuilder res = new StringBuilder();
        res.append("Usage:");
        res.append(System.lineSeparator());

        final int argBorder = 2;    // '<' & '>'
        int maxLenOfAliases = 0;
        int maxLenOfArg = 0;
        for (Option option : OPTIONS) {
            int lenAliases = Arrays.stream(option.aliases).mapToInt(String::length).sum()
                    + (2 * (option.aliases.length - 1));
            if (option.aliases[0].startsWith("--")) {   // no short alias
                lenAliases += 4;
            }
            maxLenOfAliases = Math.max(maxLenOfAliases, lenAliases);
            maxLenOfArg = Math.max(maxLenOfArg, option.arg == null ? 0 : (option.arg.length() + argBorder));
        }

        final int margin1 = 1;
        final int margin2 = 2;
        for (Option option : OPTIONS) {
            res.append("  ");
            StringBuilder sb = new StringBuilder();
            if (option.aliases[0].startsWith("--")) {
                sb.append("    ");
            }
            for (String alias : option.aliases) {
                sb.append(alias).append(", ");
            }
            sb.setLength(sb.length() - 2);  // the last ", "
            res.append(sb);
            res.append(" ".repeat(maxLenOfAliases + margin1 - sb.length()));
            res.append(option.arg == null ? "" : ("<" + option.arg + ">"));   // argBorder
            int spacePadding = maxLenOfArg + margin2 - (option.arg == null ? 0 : (option.arg.length() + argBorder));
            res.append(" ".repeat(spacePadding));
            res.append(option.comment);
            res.append(System.lineSeparator());
        }
        if (LOGGER.isLoggable(INFO)) {
            LOGGER.log(INFO, "{0}", res);
        } else {
            System.out.println(res);
        }
    }

    private void onEmptyArg(String op, String mainArg) {
        if (op.startsWith("-X") || op.startsWith("-D")) {
            String msg = "The JVM option \"" + mainArg + "\" should be written as "
                    + "\"-J" + mainArg + "\" on jbooster!";
            throw new IllegalArgumentException(msg);
        } else {
            throw new IllegalArgumentException("Unknown option \"" + mainArg + "\" on jbooster!");
        }
    }

    private void checkIllegalArgruments() {
        Properties properties = System.getProperties();
        for (String key : properties.stringPropertyNames()) {
            String value = properties.getProperty(key, "");
            String msg = "The graal option -J-D" + key + " should not set on jbooster!";
            if (key.startsWith("graal.")) {
                if ("graal.RemoveNeverExecutedCode".equals(key) ||
                    "graal.UseExceptionProbability".equals(key)) {
                    if (!"false".equals(value)) {
                        throw new IllegalArgumentException(msg);
                    }
                } else {
                    throw new IllegalArgumentException(msg);
                }
            }
        }
    }

    private void onAllCmdParsed() {
        if (serverPort == UNSET_PORT) {
            throw new IllegalArgumentException("The option \"--server-port\" (or \"-p\") must be set!");
        }
    }

    private static Optional<Option> findOption(String arg) {
        for (Option option : OPTIONS) {
            if (option.matches(arg)) {
                return Optional.of(option);
            }
        }
        return Optional.empty();
    }

    private static String[] of(String... aliases) {
        return aliases;
    }

    abstract static class Option {
        final String[] aliases;
        final String arg;
        final String comment;

        Option(String[] aliases, String arg, String comment) {
            if (aliases == null || aliases.length == 0) {
                throw new IllegalArgumentException("aliases should contain at least one alias");
            }
            this.aliases = aliases;
            this.arg = arg;
            this.comment = comment;
        }

        boolean matches(String op) {
            for (String alias : aliases) {
                if (alias.equals(op)) {
                    return true;
                }
            }
            return false;
        }

        boolean needArg() {
            return arg != null;
        }

        abstract void process(Options options, String arg);
    }
}
