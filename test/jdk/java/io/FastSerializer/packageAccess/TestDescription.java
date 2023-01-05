/*
*- @TestCaseID:jdk17/FastSerializer/packageAccess
*- @TestCaseName:packageAccess
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
 * @bug 4765255
 * @library  /test/lib /test/jdk/java/io/Serializable/packageAccess
 * @build jdk.test.lib.util.JarUtils A B C D PackageAccessTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer PackageAccessTest
 * @summary Verify proper functioning of package equality checks used to
 *          determine accessibility of superclass constructor and inherited
 *          writeReplace/readResolve methods.
 */
