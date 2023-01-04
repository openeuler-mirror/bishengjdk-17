/*
*- @TestCaseID:FastSerializer/CloneArray
*- @TestCaseName:CloneArray
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
 * @bug 6990094
 * @summary Verify ObjectInputStream.cloneArray works on many kinds of arrays
 * @author Stuart Marks, Joseph D. Darcy
 * @library /test/jdk/java/io/Serializable/cloneArray
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer CloneArray
 */
