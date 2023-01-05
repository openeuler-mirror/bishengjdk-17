/*
*- @TestCaseID:FastSerializer/CorruptedUTFConsumption
*- @TestCaseName:CorruptedUTFConsumption
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
 * @bug 4450867
 * @summary Although technically the behavior of ObjectInputStream following a
 *          UTFDataFormatException is unspecified, verify that
 *          ObjectInputStream consumes at most the expected number of utf
 *          bytes, even if the last byte(s) of the utf string indicate that the
 *          string overflows its expected length.
 * @key randomness
 * @library /test/jdk/java/io/Serializable/corruptedUTFConsumption
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer CorruptedUTFConsumption
 */
