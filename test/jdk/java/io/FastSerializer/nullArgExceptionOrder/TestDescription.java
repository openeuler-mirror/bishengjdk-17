/*
*- @TestCaseID:jdk17/FastSerializer/nullArgExceptionOrder
*- @TestCaseName:jdk17_nullArgExceptionOrder
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
 * @bug 4771562
 * @summary Verify that if ObjectInputStream.read(byte[], int, int) is called
 *          with a null byte array and invalid offset/length values, a
 *          NullPointerException is thrown rather than an
 *          IndexOutOfBoundsException.
 * @library /test/jdk/java/io/Serializable/nullArgExceptionOrder/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Test
 */
