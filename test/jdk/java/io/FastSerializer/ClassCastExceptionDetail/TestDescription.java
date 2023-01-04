/*
*- @TestCaseID:FastSerializer/ClassCastExceptionDetail
*- @TestCaseName:ClassCastExceptionDetail
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
 * @bug 4511532
 * @summary Verify that the message string of a ClassCastException thrown by
 *          ObjectInputStream when attempting to assign a value to a field of
 *          an incompatible type contains the names of the value's class, the
 *          field's declaring class, the field's type, and the field itself.
 *
 * @library /test/jdk/java/io/Serializable/ClassCastExceptionDetail
 * @clean Write Read Foo Gub
 * @build Write
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Write
 * @clean Write Read Foo Gub
 * @build Read
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Read
 *
 */
