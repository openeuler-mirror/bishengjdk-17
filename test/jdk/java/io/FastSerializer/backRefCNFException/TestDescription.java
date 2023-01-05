/*
*- @TestCaseID:FastSerializer/backRefCNFException
*- @TestCaseName:backRefCNFException
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
 * @bug 4312433
 * @summary Verify that reading a back reference to a previously skipped value
 *          whose class is not available will throw a ClassNotFoundException
 * @library /test/jdk/java/io/Serializable/backRefCNFException
 *          /test/jdk/java/io/FastSerializer/util
 * @run driver CleanActualClass Write.class Read.class A.class B.class
 * @build Write
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Write
 * @run driver CleanActualClass Write.class Read.class A.class B.class
 * @build Read
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Read
 *
 */
