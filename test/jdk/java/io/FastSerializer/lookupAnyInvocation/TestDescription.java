/*
*- @TestCaseID:FastSerializer/LookupAnyInvocation
*- @TestCaseName:LookupAnyInvocation
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
 * @bug 4413615
 * @summary Verify that ObjectStreamClass.lookupAny() returns a non-null
 *          descriptor for class which doesn't implement java.io.Serializable
 *          interface at all.
 * @library /test/jdk/java/io/Serializable/lookupAnyInvocation
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer LookupAnyInvocation
 */