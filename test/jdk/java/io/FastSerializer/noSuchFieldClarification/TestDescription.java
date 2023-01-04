/*
*- @TestCaseID:jdk17/FastSerializer/NoSuchFieldClarification
*- @TestCaseName:jdk17_NoSuchFieldClarification
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
 * @bug 6323008
 * @summary this test verifies that exception from GetField.get method
 *              will be a more comprehensible
 *
 * @author Andrey Ozerov
 * @library /test/jdk/java/io/Serializable/noSuchFieldClarification/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer NoSuchFieldClarification
 */
