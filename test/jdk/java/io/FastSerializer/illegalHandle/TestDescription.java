/*
*- @TestCaseID:FastSerializer/illegalHandle
*- @TestCaseName:illegalHandle
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
 * @bug 4357979
 * @summary Verify that ObjectInputStream throws a StreamCorruptedException if
 *          it reads in an out-of-bounds handle value.
 * @library /test/jdk/java/io/Serializable/illegalHandle
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer -DfastSerializerEscapeMode=true Test
 */
