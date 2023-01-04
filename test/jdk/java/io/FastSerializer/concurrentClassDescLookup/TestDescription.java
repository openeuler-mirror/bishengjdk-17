/*
*- @TestCaseID:FastSerializer/ConcurrentClassDescLookup
*- @TestCaseName:ConcurrentClassDescLookup
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
 * @summary Verify that concurrent class descriptor lookups function properly,
 *          even when class descriptor initialization is slow or throws an
 *          exception.
 * @library /test/jdk/java/io/Serializable/concurrentClassDescLookup
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer ConcurrentClassDescLookup
 */