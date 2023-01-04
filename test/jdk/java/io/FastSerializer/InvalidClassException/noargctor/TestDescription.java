/*
*- @TestCaseID:FastSerializer/noargctor
*- @TestCaseName:noargctor
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
 * @bug 4093279
 * @summary Validate accessibility checking to NonSerializable superclass constuctor.
 * @library /test/jdk/java/io/Serializable/InvalidClassException/noargctor
 * @build NonSerialize.PrivateCtor
 *        NonSerialize.ProtectedCtor
 *        NonSerialize.PackageCtor
 *        NonSerialize.PublicCtor
 * @build Serialize.SubclassAcrossPackage
 *        Serialize.SamePackageCtor
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Test
 */

/* @test
 * @bug 4093279
 * @summary Raise InvalidClassException if 1st NonSerializable superclass' no-arg constructor is not accessible. This test verifies default package access.
 * @library /test/jdk/java/io/Serializable/InvalidClassException/noargctor
 * @build DefaultPackage
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer DefaultPackage
 */

