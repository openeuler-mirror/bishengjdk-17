/*
*- @TestCaseID:FastSerializer/LongString
*- @TestCaseName:LongString
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
 * @bug 4217676
 * @summary Ensure that object streams support serialization of long strings
 *          (strings whose UTF representation > 64k in length)
 * @key randomness
 * @library /test/jdk/java/io/Serializable/longString
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer LongString
 */
