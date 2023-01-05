/*
*- @TestCaseID:jdk17/FastSerializer/subclass
*- @TestCaseName:subclass
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
 * @bug 4100915
 * @summary Verify that [write/read]ObjectOverride methods get called.
 *          Test verifies that ALL methods to write an object can
 *          be overridden. However, the testing for reading an object
 *          is incomplete. Only test that readObjectOverride is called.
 *          An entire protocol would need to be implemented and written
 *          out before being able to test the input side of the API.
 *
 *          Also, would be appropriate that this program verify
 *          that if SerializablePermission "enableSubclassImplementation"
 *          is not in the security policy and security is enabled, that
 *          a security exception is thrown when constructing the
 *          ObjectOutputStream subclass.
 *
 *
 * @library /test/jdk/java/io/Serializable/subclass/
 * @build AbstractObjectInputStream AbstractObjectOutputStream
 * @build XObjectInputStream XObjectOutputStream
 * @build SubclassTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer SubclassTest
 * @run main/othervm/policy=Allow.policy -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer SubclassTest -expectSecurityException
 */