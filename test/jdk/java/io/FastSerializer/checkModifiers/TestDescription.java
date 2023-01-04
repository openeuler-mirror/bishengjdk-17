/*
*- @TestCaseID:FastSerializer/checkModifiers
*- @TestCaseName:checkModifiers
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
 * @bug 4214888
 * @summary Make sure that serialpersistentFields data member is used to
 *          represent tyhe serializable fields only if it has the modfiers
 *          static, final, private and the type is ObjectStreamField.
 *          No need to check for static, as ObjectStreamField class is not
 *          serializable.
 * @library /test/jdk/java/io/Serializable/checkModifiers
 * @clean CheckModifiers TestClass1 TestClass2
 * @build CheckModifiers
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer CheckModifiers
 *
 */
