/*
*- @TestCaseID:jdk17/FastSerializer/NotAvailable
*- @TestCaseName:jdk17_NotAvailable
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

/*
 * @test
 * @bug 4085756
 * @summary Ensure readObject() works when InputStream.available() is not implemented.
 *          In JDK 1.1.x, Win32 System Console available() thows IOException
 *          to denote that it is not implemented.
 * @library /test/jdk/java/io/Serializable/notAvailable/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer NotAvailable
 */