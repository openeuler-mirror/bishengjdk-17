/*
*- @TestCaseID:FastSerializer/class
*- @TestCaseName:class
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
 * @bug 4075221
 * @library /test/lib /test/jdk/java/io/Serializable/class
 * @build jdk.test.lib.compiler.CompilerUtils
 *        jdk.test.lib.Utils
 *        jdk.test.lib.Asserts
 *        jdk.test.lib.JDKToolFinder
 *        jdk.test.lib.JDKToolLauncher
 *        jdk.test.lib.Platform
 *        jdk.test.lib.process.*
 * @run testng/othervm/timeout=300 -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer NonSerializableTest
 * @summary Enable serialize of nonSerializable Class descriptor.
 */
