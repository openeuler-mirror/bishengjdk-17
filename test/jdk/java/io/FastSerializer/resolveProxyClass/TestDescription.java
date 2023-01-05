/*
*- @TestCaseID:jdk17/FastSerializer/NonPublicInterface
*- @TestCaseName:NonPublicInterface
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
 * @bug 4413817 8004928
 * @library /test/jdk/java/io/Serializable/resolveProxyClass/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer NonPublicInterface
 * @summary Verify that ObjectInputStream.resolveProxyClass can properly
 *          resolve a dynamic proxy class which implements a non-public
 *          interface not defined in the latest user defined class loader.
 */
