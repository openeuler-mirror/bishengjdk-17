/*
*- @TestCaseID:jdk17/FastSerializer/replaceStringArray
*- @TestCaseName:replaceStringArray
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
 * @bug 4099013
 * @library /test/jdk/java/io/Serializable/replaceStringArray/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer ReplaceStringArray
 * @summary Enable substitution of String and Array by ObjectStreams.
 */
