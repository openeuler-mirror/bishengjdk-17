/*
 * Copyright (c) 2005, 2021, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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

package sun.tools.jmap;

import java.io.BufferedInputStream;
import java.io.Console;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;
import java.nio.CharBuffer;
import java.security.NoSuchAlgorithmException;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.KeySpec;
import java.util.Arrays;
import java.util.Collection;
import java.util.regex.Pattern;

import com.sun.tools.attach.VirtualMachine;
import com.sun.tools.attach.VirtualMachineDescriptor;
import com.sun.tools.attach.AttachNotSupportedException;
import sun.tools.attach.HotSpotVirtualMachine;
import sun.tools.common.ProcessArgumentMatcher;

import javax.crypto.SecretKey;
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.PBEKeySpec;

/*
 * This class is the main class for the JMap utility. It parses its arguments
 * and decides if the command should be satisfied using the VM attach mechanism
 * or an SA tool. At this time the only option that uses the VM attach mechanism
 * is the -dump option to get a heap dump of a running application. All other
 * options are mapped to SA tools.
 */
public class JMap {
    // encrypt
    private static int SALT_MIN_LENGTH = 8;
    private static int HASH_BIT_SIZE = 256;
    private static int HASH_ITERATIONS_COUNT = 10000;

    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
            usage(1); // no arguments
        }

        checkForUnsupportedOptions(args);

        // the chosen option
        String option = null;

        // First iterate over the options (arguments starting with -).  There should be
        // one.
        int optionCount = 0;
        while (optionCount < args.length) {
            String arg = args[optionCount];
            if (!arg.startsWith("-")) {
                break;
            }
            if (arg.equals("-?") ||
                arg.equals("-h") ||
                arg.equals("--help") ||
                // -help: legacy. Undocumented.
                arg.equals("-help")) {
                usage(0);
            } else {
                if (option != null) {
                    usage(1);  // option already specified
                }
                option = arg;
            }
            optionCount++;
        }

        // if no option provided then use default.
        if (option == null) {
            usage(0);
        }

        // Next we check the parameter count.
        int paramCount = args.length - optionCount;
        if (paramCount != 1) {
            usage(1);
        }

        String pidArg = args[1];
        // Here we handle the built-in options
        // As more options are added we should create an abstract tool class and
        // have a table to map the options
        ProcessArgumentMatcher ap = new ProcessArgumentMatcher(pidArg);
        Collection<String> pids = ap.getVirtualMachinePids(JMap.class);

        if (pids.isEmpty()) {
            System.err.println("Could not find any processes matching : '" + pidArg + "'");
            System.exit(1);
        }

        for (String pid : pids) {
            if (pids.size() > 1) {
                System.out.println("Pid:" + pid);
            }
            if (option.equals("-histo")) {
                histo(pid, "");
            } else if (option.startsWith("-histo:")) {
                histo(pid, option.substring("-histo:".length()));
            } else if (option.startsWith("-dump:")) {
                dump(pid, option.substring("-dump:".length()));
            } else if (option.equals("-finalizerinfo")) {
                executeCommandForPid(pid, "jcmd", "GC.finalizer_info");
            } else if (option.equals("-clstats")) {
                executeCommandForPid(pid, "jcmd", "VM.classloader_stats");
            } else {
              usage(1);
            }
        }
    }

    private static void executeCommandForPid(String pid, String command, Object ... args)
        throws AttachNotSupportedException, IOException,
               UnsupportedEncodingException {
        VirtualMachine vm = VirtualMachine.attach(pid);

        // Cast to HotSpotVirtualMachine as this is an
        // implementation specific method.
        HotSpotVirtualMachine hvm = (HotSpotVirtualMachine) vm;
        try (InputStream in = hvm.executeCommand(command, args)) {
          // read to EOF and just print output
          byte b[] = new byte[256];
          int n;
          do {
              n = in.read(b);
              if (n > 0) {
                  String s = new String(b, 0, n, "UTF-8");
                  System.out.print(s);
              }
          } while (n > 0);
        }
        vm.detach();
    }

    private static String parseFileName(String opt) {
        // opt starts with "file="
        if (opt.length() > 5) {
            //  pass whole "file=" string
            String filename = opt.substring(5);
            try {
                // Get the canonical path - important to avoid just
                // passing a "heap.bin" and having the dump created
                // in the target VM working directory rather than the
                // directory where jmap is executed.
                return new File(filename).getCanonicalPath();
            } catch (IOException ioe) {
              return null;
            }
        }
        // no filename
        return null;
    }

    private static void histo(String pid, String options)
        throws AttachNotSupportedException, IOException,
               UnsupportedEncodingException {
        String liveopt = "-all";
        String filename = null;
        String parallel = null;
        String subopts[] = options.split(",");

        for (int i = 0; i < subopts.length; i++) {
            String subopt = subopts[i];
            if (subopt.equals("") || subopt.equals("all")) {
                // pass
            } else if (subopt.equals("live")) {
                liveopt = "-live";
            } else if (subopt.startsWith("file=")) {
                filename = parseFileName(subopt);
                if (filename == null) {
                    System.err.println("Fail: invalid option or no file name '" + subopt + "'");
                    usage(1);
                }
            } else if (subopt.startsWith("parallel=")) {
               parallel = subopt.substring("parallel=".length());
               if (parallel == null) {
                    System.err.println("Fail: no number provided in option: '" + subopt + "'");
                    usage(1);
               }
            } else {
                System.err.println("Fail: invalid option: '" + subopt + "'");
                usage(1);
            }
        }

        System.out.flush();

        // inspectHeap is not the same as jcmd GC.class_histogram
        executeCommandForPid(pid, "inspectheap", liveopt, filename, parallel);
    }

    private static void dump(String pid, String options)
        throws AttachNotSupportedException, IOException,
               UnsupportedEncodingException {

        String subopts[] = options.split(",");
        String filename = null;
        String liveopt = "-all";
        String compress_level = null;
        RedactParams redactParams = new RedactParams();
        String redactPassword = ",RedactPassword=";

        for (int i = 0; i < subopts.length; i++) {
            String subopt = subopts[i];
            if (subopt.equals("") || subopt.equals("all")) {
                // pass
            } else if (subopt.equals("live")) {
                liveopt = "-live";
            } else if (subopt.startsWith("file=")) {
                filename = parseFileName(subopt);
                if (filename == null) {
                    System.err.println("Fail: invalid option or no file name '" + subopt + "'");
                    usage(1);
                }
            } else if (subopt.equals("format=b")) {
                // ignore format (not needed at this time)
            } else if (subopt.startsWith("gz=")) {
                compress_level = subopt.substring("gz=".length());
                if (compress_level.length() == 0) {
                    System.err.println("Fail: no number provided in option: '" + subopt + "'");
                    usage(1);
                }
            } else if (subopt.startsWith("HeapDumpRedact=")) {
                if (!redactParams.setAndCheckHeapDumpRedact(subopt.substring("HeapDumpRedact=".length()))) {
                    usage(1);
                }
            } else if (subopt.startsWith("RedactMap=")) {
                redactParams.setRedactMap(subopt.substring("RedactMap=".length()));
            } else if (subopt.startsWith("RedactMapFile=")) {
                redactParams.setRedactMapFile(subopt.substring("RedactMapFile=".length()));
            } else if (subopt.startsWith("RedactClassPath")) {
                redactParams.setRedactClassPath(subopt.substring("RedactClassPath=".length()));
            }  else if (subopt.startsWith("RedactPassword")) {
                redactPassword = getRedactPassword(pid);
            } else {
                System.err.println("Fail: invalid option: '" + subopt + "'");
                usage(1);
            }
        }

        if (filename == null) {
            System.err.println("Fail: invalid option or no file name");
            usage(1);
        }

        checkRedactParams(redactParams);

        System.out.flush();

        // dumpHeap is not the same as jcmd GC.heap_dump
        String heapDumpRedactParams = redactParams.isEnableRedact() ? ";" + redactParams.toDumpArgString() + redactPassword : "";
        executeCommandForPid(pid, "dumpheap", filename + heapDumpRedactParams, liveopt, compress_level);
    }

    private static void checkRedactParams(RedactParams redactParams) {
        if (redactParams.getHeapDumpRedact() == null) {
            if (redactParams.getRedactMap() == null && redactParams.getRedactMapFile() == null) {
                redactParams.setEnableRedact(false);
            } else {
                System.err.println("Error: HeapDumpRedact must be specified to enable heap-dump-redacting");
                usage(1);
            }
        }
    }

    private static String getRedactPassword(String pid) {
        String redactPassword = ",RedactPassword=";
        // heap dump may need a password
        Console console = System.console();
        char[] passwords = null;
        if (console == null) {
            return redactPassword;
        }

        try {
            passwords = console.readPassword("redact authority password:");
        } catch (Exception e) {
        }
        if(passwords == null) {
            return redactPassword;
        }

        String digestStr = null;
        try {
            CharBuffer cb = CharBuffer.wrap(passwords);
            String passwordPattern = "^[0-9a-zA-Z!@#$]{1,9}$";
            if(!Pattern.matches(passwordPattern, cb)) {
                return redactPassword;
            }

            String salt = getSalt(pid);
            if(salt == null) {
                return redactPassword;
            }
            byte[] saltBytes = salt.getBytes("UTF-8");
            if(saltBytes.length < SALT_MIN_LENGTH) {
                return redactPassword;
            }
            digestStr = getEncryptValue(passwords, saltBytes);
        } catch (Exception e) {
        }finally {
            // clear all password
            if(passwords != null) {
                Arrays.fill(passwords, '0');
            }
        }

        redactPassword += (digestStr == null ? "" : digestStr);
        return redactPassword;
    }

    private static String getSalt(String pid) throws Exception {
        String salt = null;
        StringBuilder redactAuth = new StringBuilder();

        VirtualMachine vm = VirtualMachine.attach(pid);
        HotSpotVirtualMachine hvm = (HotSpotVirtualMachine) vm;
        String flag = "RedactPassword";
        try (InputStream in = hvm.printFlag(flag); BufferedInputStream bis = new BufferedInputStream(in);
            InputStreamReader isr = new InputStreamReader(bis, "UTF-8")) {
            char c[] = new char[256];
            int n;
            do {
                n = isr.read(c);

                if (n > 0) {
                    redactAuth.append(n == c.length ? c : Arrays.copyOf(c, n));
                }
            } while (n > 0);
        }
        vm.detach();

        if(redactAuth.length() > 0) {
            String[] auths = redactAuth.toString().split(",");
            if(auths.length != 2) {
                return salt;
            }
            return auths[1].trim();
        }

        return salt;
    }

    private static String getEncryptValue(char[] passwordValue, byte[] saltBytes) throws InvalidKeySpecException, NoSuchAlgorithmException {
        StringBuilder digestStrBuilder = new StringBuilder();

        KeySpec spec = new PBEKeySpec(passwordValue, saltBytes, HASH_ITERATIONS_COUNT, HASH_BIT_SIZE);
        SecretKeyFactory secretKeyFactory = SecretKeyFactory.getInstance("PBKDF2WithHmacSHA256");
        SecretKey secretKey = secretKeyFactory.generateSecret(spec);
        byte[] digestBytes = secretKey.getEncoded();
        for (byte b : digestBytes) {
            String hex = Integer.toHexString(0xff & b);
            if (hex.length() == 1) {
                digestStrBuilder.append('0');
            }
            digestStrBuilder.append(hex);
        }
        String digestStr = digestStrBuilder.toString();

        return digestStr;
    }

    private static void checkForUnsupportedOptions(String[] args) {
        // Check arguments for -F, -m, and non-numeric value
        // and warn the user that SA is not supported anymore

        int paramCount = 0;

        for (String s : args) {
            if (s.equals("-F")) {
                SAOptionError("-F option used");
            }

            if (s.equals("-heap")) {
                SAOptionError("-heap option used");
            }

            /* Reimplemented using jcmd, output format is different
               from original one

            if (s.equals("-clstats")) {
                warnSA("-clstats option used");
            }

            if (s.equals("-finalizerinfo")) {
                warnSA("-finalizerinfo option used");
            }
            */

            if (! s.startsWith("-")) {
                paramCount += 1;
            }
        }

        if (paramCount > 1) {
            SAOptionError("More than one non-option argument");
        }
    }

    private static void SAOptionError(String msg) {
        System.err.println("Error: " + msg);
        System.err.println("Cannot connect to core dump or remote debug server. Use jhsdb jmap instead");
        System.exit(1);
    }

    // print usage message
    private static void usage(int exit) {
        System.err.println("Usage:");
        System.err.println("    jmap -clstats <pid>");
        System.err.println("        to connect to running process and print class loader statistics");
        System.err.println("    jmap -finalizerinfo <pid>");
        System.err.println("        to connect to running process and print information on objects awaiting finalization");
        System.err.println("    jmap -histo[:[<histo-options>]] <pid>");
        System.err.println("        to connect to running process and print histogram of java object heap");
        System.err.println("    jmap -dump:<dump-options> <pid>");
        System.err.println("        to connect to running process and dump java heap");
        System.err.println("    jmap -? -h --help");
        System.err.println("        to print this help message");
        System.err.println("");
        System.err.println("    dump-options:");
        System.err.println("      live         dump only live objects (takes precedence if both \"live\" and \"all\" are specified)");
        System.err.println("      all          dump all objects in the heap (default if one of \"live\" or \"all\" is not specified)");
        System.err.println("      format=b     binary format");
        System.err.println("      file=<file>  dump heap to <file>");
        System.err.println("      gz=<number>  If specified, the heap dump is written in gzipped format using the given compression level.");
        System.err.println("                   1 (recommended) is the fastest, 9 the strongest compression.");
        System.err.println("      HeapDumpRedact=<basic|names|full|diyrules|annotation|off>     redact the heapdump information to remove sensitive data,");
        System.err.println("      RedactMap=<name1:value1;name2:value2;...>            Redact the class and field names to other strings");
        System.err.println("      RedactMapFile=<file>                                 file path of the redact map");
        System.err.println("      RedactClassPath=<classpath>                          full path of the redact annotation");
        System.err.println("      RedactPassword                                       maybe redact feature has an authority, will wait for a password, ");
        System.err.println("                                                           without a correct password, heap dump with default redact level");
        System.err.println("");
        System.err.println("    Example: jmap -dump:live,format=b,file=heap.bin <pid>");
        System.err.println("");
        System.err.println("    histo-options:");
        System.err.println("      live                count only live objects (takes precedence if both \"live\" and \"all\" are specified)");
        System.err.println("      all                 count all objects in the heap (default if one of \"live\" or \"all\" is not specified)");
        System.err.println("      file=<file>         dump data to <file>");
        System.err.println("      parallel=<number>   Number of parallel threads to use for heap inspection:");
        System.err.println("                          0 (the default) means let the VM determine the number of threads to use");
        System.err.println("                          1 means use one thread (disable parallelism).");
        System.err.println("                          For any other value the VM will try to use the specified number of threads, but might use fewer.");
        System.err.println("");
        System.err.println("    Example: jmap -histo:live,file=/tmp/histo.data <pid>");
        System.exit(exit);
    }

    public static class RedactParams {
        private boolean enableRedact = false;
        private String heapDumpRedact;
        private String redactMap;
        private String redactMapFile;
        private String redactClassPath;

        public RedactParams() {
        }

        public RedactParams(String heapDumpRedact, String redactMap, String redactMapFile, String redactClassPath) {
            if (heapDumpRedact != null && checkLauncherHeapdumpRedactSupport(heapDumpRedact)) {
                enableRedact = true;
            }
            this.heapDumpRedact = heapDumpRedact;
            this.redactMap = redactMap;
            this.redactMapFile = redactMapFile;
            this.redactClassPath = redactClassPath;
        }

        @Override
        public String toString() {
            StringBuilder builder = new StringBuilder();
            if (heapDumpRedact != null) {
                builder.append("HeapDumpRedact=");
                builder.append(heapDumpRedact);
                builder.append(",");
            }
            if (redactMap != null) {
                builder.append("RedactMap=");
                builder.append(redactMap);
                builder.append(",");
            }
            if (redactMapFile != null) {
                builder.append("RedactMapFile=");
                builder.append(redactMapFile);
                builder.append(",");
            }
            if (redactClassPath != null) {
                builder.append("RedactClassPath=");
                builder.append(redactClassPath);
            }
            return builder.toString();
        }

        public String toDumpArgString() {
            return "-HeapDumpRedact=" + (heapDumpRedact == null ? "off" : heapDumpRedact) +
                    ",RedactMap=" + (redactMap == null ? "" : redactMap) +
                    ",RedactMapFile=" + (redactMapFile == null ? "" : redactMapFile) +
                    ",RedactClassPath=" + (redactClassPath == null ? "" : redactClassPath);
        }

        public static boolean checkLauncherHeapdumpRedactSupport(String value) {
            String[] validValues = {"basic", "names", "full", "diyrules", "annotation", "off"};
            for (String validValue : validValues) {
                if (validValue.equals(value)) {
                    return true;
                }
            }
            return false;
        }

        public boolean isEnableRedact() {
            return enableRedact;
        }

        public void setEnableRedact(boolean enableRedact) {
            this.enableRedact = enableRedact;
        }

        public String getHeapDumpRedact() {
            return heapDumpRedact;
        }

        public boolean setAndCheckHeapDumpRedact(String heapDumpRedact) {
            if (!checkLauncherHeapdumpRedactSupport(heapDumpRedact)) {
                return false;
            }
            this.heapDumpRedact = heapDumpRedact;
            this.enableRedact = true;
            return true;
        }

        public String getRedactMap() {
            return redactMap;
        }

        public void setRedactMap(String redactMap) {
            this.redactMap = redactMap;
        }

        public String getRedactMapFile() {
            return redactMapFile;
        }

        public void setRedactMapFile(String redactMapFile) {
            this.redactMapFile = redactMapFile;
        }

        public String getRedactClassPath() {
            return redactClassPath;
        }

        public void setRedactClassPath(String redactClassPath) {
            this.redactClassPath = redactClassPath;
        }
    }
}
