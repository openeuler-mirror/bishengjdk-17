/*
*- @TestCaseID:jdk17/FastSerializer/typeSafeEnum
*- @TestCaseName:typeSafeEnum
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
 * @bug 4140729
 * @library /test/jdk/java/io/Serializable/typeSafeEnum/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer TypeSafeEnum
 * @summary Verify that writeReplace & readResolve are called by serialization.
 *          readResolve is used to maintain the invariant that the enums
 *          of TypeSafeEnum are singletons.
 */
