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
import java.util.NoSuchElementException;
import java.util.Optional;
import java.util.Scanner;

import static java.lang.System.Logger.Level.INFO;
import static java.lang.System.Logger.Level.WARNING;
import static jdk.jbooster.JBooster.LOGGER;

/**
 * The interactive commands of the JBooster server.
 */
public final class Commands {
    private static final Command[] COMMANDS = {new Command(of("h", "help"),
            "Print this help message of JBooster interactive commands.") {
        @Override
        void process(Commands commands, String[] args) {
            printHelp();
        }
    }, new Command(of("q", "quit", "exit"),
            "Exit the JBooster server.") {
        @Override
        void process(Commands commands, String[] args) {
            commands.exit = true;
        }
    }, new Command(of("c", "conn", "connection"),
            "Print network connections with the clients.") {
        @Override
        void process(Commands commands, String[] args) {
            int workingThreads = JBooster.getConnectionPool().getExecutor().getActiveCount();
            LOGGER.log(INFO, "Working threads for connections: {0}", workingThreads);
        }
    }, new Command(of("d", "data"),
            "Print managed client data and cache state (add arg \"all\" for more details).") {
        @Override
        void process(Commands commands, String[] args) {
            boolean printAll = (args.length == 1 && "all".equals(args[0]));
            JBooster.printStoredClientData(printAll);
        }
    }};

    private boolean exit = false;

    /**
     * Open the scanner and receive the interactive commands.
     */
    public boolean interact() {
        try (Scanner scanner = new Scanner(System.in)) {
            do {
                String cmdLine = scanner.nextLine();
                parseLine(cmdLine);
            } while (!exit);
        } catch (NoSuchElementException e) {
            LOGGER.log(WARNING, "Looks like jbooster is running in the background so the interaction "
                    + "is unsupported here. Add \"-b\" or \"--background\" to the command line to "
                    + "suppress this warning.");
            return false;
        }
        return true;
    }

    private void parseLine(String cmdLine) {
        if (cmdLine.isEmpty()) {
            return;
        }
        String[] cmdAndArgs = Arrays.stream(cmdLine.split(" ")).filter(s -> !s.isEmpty()).toArray(String[]::new);
        if (cmdAndArgs.length == 0) {
            return;
        }
        String cmd = cmdAndArgs[0];
        String[] args = Arrays.stream(cmdAndArgs).skip(1).toArray(String[]::new);

        Optional<Command> command = findCommand(cmd);
        if (command.isPresent()) {
            command.get().process(this, args);
        } else {
            LOGGER.log(WARNING, "Unknown command: \"{0}\"", cmd);
            printHelp();
        }
    }

    private static Optional<Command> findCommand(String cmd) {
        for (Command command : COMMANDS) {
            if (command.matches(cmd)) {
                return Optional.of(command);
            }
        }
        return Optional.empty();
    }

    private static void printHelp() {
        int maxLenOfAliases = 0;
        for (Command command : COMMANDS) {
            int lenAliases = Arrays.stream(command.aliases).mapToInt(String::length).sum()
                    + (2 * (command.aliases.length - 1));
            maxLenOfAliases = Math.max(maxLenOfAliases, lenAliases);
        }

        final int margin = 4;
        final String sep = System.lineSeparator();
        StringBuilder sbAll = new StringBuilder("Help of JBooster commands:" + sep);
        for (Command command : COMMANDS) {
            sbAll.append("  ");
            StringBuilder sb = new StringBuilder();
            for (String alias : command.aliases) {
                sb.append(alias).append(", ");
            }
            sb.setLength(sb.length() - 2);
            sbAll.append(sb);
            sbAll.append(" ".repeat(maxLenOfAliases + margin - sb.length()));
            sbAll.append(command.comment);
            sbAll.append(sep);
        }
        LOGGER.log(INFO, sbAll);
    }

    private static String[] of(String... aliases) {
        return aliases;
    }

    abstract static class Command {
        final String[] aliases;
        final String comment;

        protected Command(String[] aliases, String comment) {
            this.aliases = aliases;
            this.comment = comment;
        }

        boolean matches(String cmd) {
            for (String alias : aliases) {
                if (alias.equals(cmd)) {
                    return true;
                }
            }
            return false;
        }

        abstract void process(Commands commands, String[] args);
    }
}
