/*
*- @TestCaseID:jdk17/FastSerializer/unresolvedClassDesc
*- @TestCaseName:unresolvedClassDesc
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
 * @bug 4482471
 *
 * @library /test/jdk/java/io/Serializable/unresolvedClassDesc/
 * @clean Write Read Foo
 * @build Write Foo
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Write
 * @clean Write Foo
 * @build Read
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Read
 * @clean Read
 *
 * @summary Verify that even if an incoming ObjectStreamClass is not resolvable
 *          to a local class, the ObjectStreamClass object itself is still
 *          deserializable (without incurring a ClassNotFoundException).
 */
