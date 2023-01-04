/*
*- @TestCaseID:jdk17/FastSerializer/unshared
*- @TestCaseName:unshared
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
 * @bug 4311991
 *
 * @library /test/jdk/java/io/Serializable/unshared/
 * @clean Write Read Foo Bar
 * @build Write
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Write
 * @clean Write Read Foo Bar
 * @build Read
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Read
 *
 * @summary Test ObjectOutputStream.writeUnshared/readUnshared functionality.
 */
