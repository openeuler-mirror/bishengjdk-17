/*
*- @TestCaseID:FastSerializer/DefaultDataEnd
*- @TestCaseName:DefaultDataEnd
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
 * @bug 4360508
 * @summary Verify that a custom readObject() method reading in data written
 *          via default serialization cannot read past the end of the default
 *          data.
 * @library /test/jdk/java/io/Serializable/defaultDataEnd
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer DefaultDataEnd
 */