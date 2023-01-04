/*
*- @TestCaseID:FastSerializer/duplicateSerialFields
*- @TestCaseName:duplicateSerialFields
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
 * @bug 4764280
 * @summary Verify that if a serializable class declares multiple
 *          serialPersistentFields that share the same name, calling
 *          ObjectStreamClass.lookup() for that class will not result in an
 *          InternalError, and that attempts at default serialization or
 *          deserialization of such a class will result in
 *          InvalidClassExceptions.
 *
 * @library /test/jdk/java/io/Serializable/duplicateSerialFields
 * @clean Setup Test A B
 * @build Setup
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Setup
 * @clean Setup A B
 * @build Test
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Test
 *
 */
