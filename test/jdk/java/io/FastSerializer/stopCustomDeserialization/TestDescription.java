/*
*- @TestCaseID:jdk17/FastSerializer/stopCustomDeserialization
*- @TestCaseName:stopCustomDeserialization
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
 * @bug 4663191
 * @summary Verify that readObject and readObjectNoData methods will not be
 *          called on an object being deserialized if that object is already
 *          tagged with a ClassNotFoundException.
 *
 * @library /test/jdk/java/io/Serializable/stopCustomDeserialization/
 *          /test/jdk/java/io/FastSerializer/util/
 * @run driver CleanActualClass Write.class Read.class A.class B.class C.class X.class
 * @build Write
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Write
 * @run driver CleanActualClass Write.class Read.class A.class B.class C.class X.class
 * @build Read
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Read
 */
