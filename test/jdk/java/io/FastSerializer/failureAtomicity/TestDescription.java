/*
*- @TestCaseID:FastSerializer/FailureAtomicity
*- @TestCaseName:FailureAtomicity
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
 * @bug 8071474
 * @summary Better failure atomicity for default read object.
 * @modules jdk.compiler
 * @library /test/lib /test/jdk/java/io/Serializable
 * @build jdk.test.lib.Platform
 *        jdk.test.lib.util.FileUtils
 * @build failureAtomicity.FailureAtomicity failureAtomicity.SerialRef
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer failureAtomicity.FailureAtomicity
 */