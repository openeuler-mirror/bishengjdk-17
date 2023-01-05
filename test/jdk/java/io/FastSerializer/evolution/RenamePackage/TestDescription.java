/*
*- @TestCaseID:FastSerializer/evolution/RenamePackage
*- @TestCaseName:evolution/RenamePackage
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
 * @bug 4087295 4785472
 * @library /test/lib /test/jdk/java/io/Serializable/evolution/RenamePackage
 * @build jdk.test.lib.compiler.CompilerUtils
 *        jdk.test.lib.Utils
 *        jdk.test.lib.Asserts
 *        jdk.test.lib.JDKToolFinder
 *        jdk.test.lib.JDKToolLauncher
 *        jdk.test.lib.Platform
 *        jdk.test.lib.process.*
 * @build RenamePackageTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer RenamePackageTest
 * @summary Enable resolveClass() to accommodate package renaming.
 *          This fix enables one to implement a resolveClass method that maps a
 *          Serialiazable class within a serialization stream to the same class
 *          in a different package within the JVM runtime. See run shell script
 *          for instructions on how to run this test.
 */
