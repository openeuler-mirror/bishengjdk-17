/*
*- @TestCaseID:FastSerializer/NestedReplace
*- @TestCaseName:NestedReplace
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
 * @bug 4217737
 * @library /test/jdk/java/io/Serializable/nestedReplace/
 * @clean NestedReplace A B C D
 * @build NestedReplace
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer NestedReplace
 * @summary Ensure that replacement objects can nominate their own replacements,
 *          so long as the replacement is not the same class as the
 *          just-replaced object.
 *
 */
