/*
*- @TestCaseID:FastSerializer/badSerialVersionUID
*- @TestCaseName:badSerialVersionUID
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
 * @bug 4431318
 * @summary Verify that when serialVersionUID is declared with a type other
 *          than long, values that can be promoted to long will be used, and
 *          those that can't be will be ignored (but will not result in
 *          unchecked exceptions).
 * @library /test/jdk/java/io/Serializable/badSerialVersionUID
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer BadSerialVersionUID
 */
