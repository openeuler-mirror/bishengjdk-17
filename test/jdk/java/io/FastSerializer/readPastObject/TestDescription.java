/*
*- @TestCaseID:jdk17/FastSerializer/readPastObject
*- @TestCaseName:readPastObject
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
 * @bug 4253271
 * @library /test/jdk/java/io/Serializable/readPastObject/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer ReadPastObject
 * @summary Ensure that ObjectInputStream.readObject() is called, it doesn't
 *          read past the end of the object in the underlying stream.
 */
