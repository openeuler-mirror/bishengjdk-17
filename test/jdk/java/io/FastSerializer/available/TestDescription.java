/*
*- @TestCaseID:FastSerializer/available
*- @TestCaseName:available
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
 * @bug 4400571
 * @summary Verify that ObjectInputStream.available() functions properly when
 *          called at the beginning of a data block, and that it does not
 *          attempt any read operations that may potentially block.
 * @library /test/jdk/java/io/Serializable/available
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Available
 */

