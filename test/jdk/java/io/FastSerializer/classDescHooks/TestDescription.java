/*
*- @TestCaseID:FastSerializer/classDescHooks
*- @TestCaseName:classDescHooks
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
 * @bug 4227189
 * @summary Ensure that class descriptor read, write hooks exist, are backwards
 *          compatible, and work as advertised.
 * @library /test/jdk/java/io/Serializable/classDescHooks
 * @ignore
 * @build ClassDescHooks
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer ClassDescHooks
 */

/* @test
 * @bug 4931314
 * @summary Verify that a ClassNotFoundException thrown by the
 *          readClassDescriptor method is reflected to the caller as an
 *          InvalidClassException with the ClassNotFoundException as its cause.
 * @library /test/jdk/java/io/Serializable/classDescHooks
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer CNFException
 */

/* @test
 * @bug 4461737
 * @summary Verify that serialization functions properly for externalizable
 *          classes if ObjectInputStream.readClassDescriptor() returns a local
 *          class descriptor.
 * @library /test/jdk/java/io/Serializable/classDescHooks
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer ExternLoopback
 */

/* @test
 * @bug 4461299
 * @summary Verify that serialization functions properly if
 *          ObjectInputStream.readClassDescriptor() returns a local class
 *          descriptor for which the serialVersionUID has not yet been
 *          calculated.
 * @library /test/jdk/java/io/Serializable/classDescHooks
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Loopback
 */
