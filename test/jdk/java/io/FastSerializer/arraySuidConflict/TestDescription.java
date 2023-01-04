/*
*- @TestCaseID:FastSerializer/arraySuidConflict
*- @TestCaseName:arraySuidConflict
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
 * @bug 4490677
 * @summary Verify that array serialVersionUID conflicts caused by changes in
 *          package scope do not cause deserialization to fail.
 *
 * @library /test/jdk/java/io/Serializable/arraySuidConflict
 * @clean Write Read Foo
 * @build Write Foo
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Write
 * @clean Write Read Foo
 * @build Read
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Read
 */
