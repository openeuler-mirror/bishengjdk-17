/*
*- @TestCaseID:jdk17/FastSerializer/ResolveClassException2
*- @TestCaseName:ResolveClassException2
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
 * @bug 4191941
 * @library /test/jdk/java/io/Serializable/resolveClassException/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer ResolveClassException
 * @summary Ensure that original ClassNotFoundException thrown inside of
 *          ObjectInputStream.resolveClass() is preserved (and thrown).
 */
