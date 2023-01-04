/*
*- @TestCaseID:FastSerializer/ModifyStaticFields
*- @TestCaseName:ModifyStaticFields
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
 * @bug 4334623
 * @summary Verify that serialPersistentFields cannot be used to cause
 *          deserialization to set the value of a static field.
 * @library /test/jdk/java/io/Serializable/modifyStaticFields
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer ModifyStaticFields
 */