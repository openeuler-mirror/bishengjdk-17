/*
*- @TestCaseID:jdk17/FastSerializer/writeObjectMemory
*- @TestCaseName:writeObjectMemory
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
 * @library /test/jdk/java/io/Serializable/writeObjectMemory/
 * @clean A WriteObjectMemory
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer WriteObjectMemory
 * @bug 4146453 5011410
 * @summary Test that regrow of object/handle table of ObjectOutputStream works.
 */
