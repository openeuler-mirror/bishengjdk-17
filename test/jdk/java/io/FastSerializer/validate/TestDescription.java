/*
*- @TestCaseID:jdk17/FastSerializer/validate
*- @TestCaseName:validate
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
 * @bug 4094892
 * @summary Verify that an object is not validated more than once during deserialization.
 * @library /test/jdk/java/io/Serializable/validate/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Validate
 *
 */