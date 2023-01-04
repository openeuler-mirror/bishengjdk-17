/*
*- @TestCaseID:jdk17/FastSerializer/PutField
*- @TestCaseName:PutField
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
 * @bug 4453723
 *
 * @library /test/jdk/java/io/Serializable/PutField/
 * @clean Write2 Read2 Foo
 * @build Write2
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Write2
 * @clean Write2 Read2 Foo
 * @build Read2
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Read2
 *
 * @summary Verify that ObjectOutputStream.PutField.write() works for objects
 *          that do not define primitive serializable fields.
 */

/* @test
 *
 * @library /test/jdk/java/io/Serializable/PutField/
 * @clean Write Read Foo
 * @build Write
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Write
 * @clean Write Read Foo
 * @build Read
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Read
 *
 * @summary Verify that the ObjectOutputStream.PutField API works as
 *          advertised.
 */