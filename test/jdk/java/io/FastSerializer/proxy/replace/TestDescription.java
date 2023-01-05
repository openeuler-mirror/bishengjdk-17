/*
*- @TestCaseID:jdk17/FastSerializer/replace
*- @TestCaseName:replace
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
 * @summary Ensure that serialization invokes writeReplace/readResolve methods
 *          on dynamic proxies, just as with normal objects.
 * @library /test/jdk/java/io/Serializable/proxy/replace/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Test
 */
