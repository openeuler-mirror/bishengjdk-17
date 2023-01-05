/*
*- @TestCaseID:jdk17/FastSerializer/ClasspathTest
*- @TestCaseName:ClasspathTest
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
 * @bug 4035147 4785472
 * @library /test/lib
 * @build jdk.test.lib.Utils
 * @build jdk.test.lib.Asserts
 * @build jdk.test.lib.JDKToolFinder
 * @build jdk.test.lib.JDKToolLauncher
 * @build jdk.test.lib.Platform
 * @build jdk.test.lib.process.*
 * @build ClasspathTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer serialver.ClasspathTest
 * @summary Test the use of the -classpath switch in the serialver application.
 */

package serialver;

import java.io.File;

import jdk.test.lib.JDKToolLauncher;
import jdk.test.lib.process.ProcessTools;

public class ClasspathTest implements java.io.Serializable {
    int a;
    int b;

    public static void main(String args[]) throws Exception {
        JDKToolLauncher serialver =
                JDKToolLauncher.create("serialver")
                               .addToolArg("-classpath")
                               .addToolArg(System.getProperty("test.class.path"))
                               .addToolArg("serialver.ClasspathTest");
        Process p = ProcessTools.startProcess("serialver",
                        new ProcessBuilder(serialver.getCommand()));
        p.waitFor();
        if (p.exitValue() != 0) {
            throw new RuntimeException("error occurs in serialver");
        }
    }
}
