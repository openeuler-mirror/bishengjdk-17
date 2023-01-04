/*
*- @TestCaseID:jdk17/FastSerializer/recursiveClassDescLookup
*- @TestCaseName:recursiveClassDescLookup
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
 * @bug 4803747
 * @library /test/jdk/java/io/Serializable/recursiveClassDescLookup/
 * @run main/othervm/timeout=20 -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Test
 * @summary Verify that a nested call to ObjectStreamClass.lookup from within
 *          the static initializer of a serializable class will not cause
 *          deadlock.
 */
