/*
*- @TestCaseID:jdk17/FastSerializer/optionalDataEnd
*- @TestCaseName:jdk17_optionalDataEnd
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
 * @bug 4402870
 * @summary Verify that an OptionalDataException with eof == true is thrown
 *          when a call to ObjectInputStream.readObject() attempts to read past
 *          the end of custom data.
 * @library /test/jdk/java/io/Serializable/optionalDataEnd/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer OptionalDataEnd
 */
