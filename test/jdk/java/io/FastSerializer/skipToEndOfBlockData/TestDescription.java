/*
*- @TestCaseID:jdk17/FastSerializer/skipToEndOfBlockData
*- @TestCaseName:skipToEndOfBlockData
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
 * @bug 4228592
 * @library /test/jdk/java/io/Serializable/skipToEndOfBlockData/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer SkipToEndOfBlockData
 * @summary Ensure that ObjectInputStream properly skips over block data when a
 *          class that defines readObject() or readExternal() fails to read all
 *          of the data written by the corresponding writeObject() or
 *          writeExternal() method.
 */
