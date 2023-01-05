/*
*- @TestCaseID:jdk17/FastSerializer/DeserializeButtonTest
*- @TestCaseName:DeserializeButtonTest
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
 * @bug 4413434
 * @library  /test/lib
 * @build jdk.test.lib.util.JarUtils  Foo
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer DeserializeButtonTest
 * @summary Verify that class loaded outside of application class loader is
 *          correctly resolved during deserialization when read in by custom
 *          readObject() method of a bootstrap class (in this case,
 *          java.util.Vector).
 */

import java.net.URLClassLoader;
import java.net.URL;
import java.nio.file.Path;
import java.nio.file.Paths;
import jdk.test.lib.util.JarUtils;
public class DeserializeButtonTest {
    public static void main(String[] args) throws Exception {
        setup();

        try (URLClassLoader ldr =
            new URLClassLoader(new URL[]{ new URL("file:cb.jar") })) {
            Runnable r = (Runnable) Class.forName("Foo", true, ldr).newInstance();
            r.run();
        }
    }

    private static void setup() throws Exception {
        Path classes = Paths.get(System.getProperty("test.classes", ""));
        JarUtils.createJarFile(Paths.get("cb.jar"),
                               classes,
                               classes.resolve("Foo.class"),
                               classes.resolve("Foo$TestElement.class"));
    }
}
