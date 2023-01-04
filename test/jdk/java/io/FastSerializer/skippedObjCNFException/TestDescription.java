/*
*- @TestCaseID:jdk17/FastSerializer/skippedObjCNFException
*- @TestCaseName:skippedObjCNFException
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
 * @bug 4313167
 *
 * @library /test/jdk/java/io/Serializable/skippedObjCNFException/
 * @clean Write Read A B C
 * @build Write
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Write
 * @clean Write Read A B C
 * @build Read
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Read
 *
 * @summary Verify that ClassNotFoundExceptions caused by values referenced
 *          (perhaps transitively) by "skipped" fields will not cause
 *          deserialization failure.
 */
