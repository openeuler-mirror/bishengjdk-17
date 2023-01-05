/*
*- @TestCaseID:jdk17/FastSerializer/NestedTest
*- @TestCaseName:NestedTest
*- @TestCaseType:Function test
*- @RequirementID:AR.SR.IREQ02478866.001.001
*- @RequirementName:FastSeralizer 功能实现
*- @Condition:UseFastSerializer
*- @Brief:
*   -#step1 将对象写入数据流
*   -#step2 从数据流中读取对象
*- @Expect: 读取对象与写入对象相同
*- @Priority:Level 1
*/

/*
 * @test
 * @bug 4312217 4785473
 * @library /test/lib
 * @build jdk.test.lib.Utils
 *        jdk.test.lib.Asserts
 *        jdk.test.lib.JDKToolFinder
 *        jdk.test.lib.JDKToolLauncher
 *        jdk.test.lib.Platform
 *        jdk.test.lib.process.*
 * @build NestedTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer serialver.NestedTest
 * @summary  To test the use of nested class specification using the '.'
 *           notation instead of the '$' notation.
 */

package serialver;

import java.io.Serializable;

import jdk.test.lib.JDKToolLauncher;
import jdk.test.lib.process.ProcessTools;

public class NestedTest implements Serializable {
    public static class Test1 implements Serializable {
        public static class Test2 implements Serializable{
            private static final long serialVersionUID = 100L;
        }
    }

    public static void main(String args[]) throws Exception {
        JDKToolLauncher serialver =
                JDKToolLauncher.create("serialver")
                               .addToolArg("-classpath")
                               .addToolArg(System.getProperty("test.class.path"))
                               .addToolArg("serialver.NestedTest.Test1.Test2");
        Process p = ProcessTools.startProcess("serialver",
                        new ProcessBuilder(serialver.getCommand()));
        p.waitFor();
        if (p.exitValue() != 0) {
            throw new RuntimeException("error occurs in serialver.");
        }
    }
}
