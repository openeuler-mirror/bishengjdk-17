/*
*- @TestCaseID:jdk17/FastSerializer/resolveClass
*- @TestCaseName:resolveClass
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

/* @test
 * @bug 4413434
 * @library /test/jdk/java/io/Serializable/resolveClass/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer MethodTest
 * @summary Verify that generated java.lang.reflect implementation classes do
 *          not interfere with serialization's class resolution mechanism.
 */
