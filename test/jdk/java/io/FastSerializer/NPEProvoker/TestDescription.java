/*
*- @TestCaseID:jdk17/FastSerializer/NPEProvoker
*- @TestCaseName:jdk17_NPEProvoker
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
 * @bug 6541870
 * @summary this test checks that ObjectInputStream throws an IOException
 *          instead of a NullPointerException when deserializing an ArrayList
 *          of Externalizables if there is an IOException while deserializing
 *          one of these Externalizables.
 *
 * @author Andrey Ozerov
 * @library /test/jdk/java/io/Serializable/NPEProvoker/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer NPEProvoker
 */
