/*
*- @TestCaseID:FastSerializer/evolution/AddedSuperClass
*- @TestCaseName:evolution/AddedSuperClass
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
 * @bug 4070080
 *
 * @summary  Test reading an evolved class serialization into the original class
 *           version. Class evolved by adding a superclass.
 *
 * @library /test/jdk/java/io/Serializable/evolution/AddedSuperClass
 * @clean A WriteAddedSuperClass ReadAddedSuperClass ReadAddedSuperClass2
 * @build WriteAddedSuperClass
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer WriteAddedSuperClass
 * @clean A AddedSuperClass
 * @build ReadAddedSuperClass
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer ReadAddedSuperClass
 * @clean A
 * @build WriteAddedSuperClass
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer WriteAddedSuperClass
 * @clean A AddedSuperClass
 * @build ReadAddedSuperClass2
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer ReadAddedSuperClass2
 *
 */

 /*
 *  Part a of test serializes an instance of an evolved class to a serialization stream.
 *  Part b of test deserializes the serialization stream into an instance of
 *  the original class.
 *
 */