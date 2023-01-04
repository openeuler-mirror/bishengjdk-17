/*
*- @TestCaseID:jdk17/FastSerializer/SanityCheck
*- @TestCaseName:SanityCheck
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
 * @library /test/jdk/java/io/Serializable/sanityCheck/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer SanityCheck
 * @summary Basic sanity check to test if default (de)serialization is
 *          transmitting values properly.
 * @key randomness
 */
