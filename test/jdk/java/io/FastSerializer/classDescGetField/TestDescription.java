/*
*- @TestCaseID:FastSerializer/classDescGetField
*- @TestCaseName:classDescGetField
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
 * @bug 4180735
 * @summary Make sure that getField() of ObjectStreamClass works correctly for
 *          object types.
 *
 * @library /test/jdk/java/io/Serializable/classDescGetField
 * @clean Foo GetField
 * @build GetField
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer GetField
 *
 */
