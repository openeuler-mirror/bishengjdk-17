/*
*- @TestCaseID:jdk17/FastSerializer/wrongReturnTypes
*- @TestCaseName:wrongReturnTypes
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
 * @bug 4337857
 *
 * @library /test/jdk/java/io/Serializable/wrongReturnTypes/
 * @clean Write Read A B
 * @build Write
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Write
 * @clean Write Read A B
 * @build Read
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Read
 *
 * @summary Verify that custom serialization methods declared with incorrect
 *          return types are not invoked.
 */
