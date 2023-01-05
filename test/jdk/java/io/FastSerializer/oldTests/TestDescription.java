/*
*- @TestCaseID:jdk17/FastSerializer/oldTests
*- @TestCaseName:jdk17_oldTests
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
 * @summary it is new version of old test which was under
 *          /src/share/test/serialization/piotest.java
 *          Test of serialization/deserialization of
 *          objects as arrays of arrays
 * @library /test/jdk/java/io/Serializable/oldTests/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer ArraysOfArrays
 */

/* @test
 * @summary it is a new version of an old test which was
 *          /src/share/test/serialization/piotest.java
 *          Test of serialization/deserialization of
 *          objects with Arrays types
 *
 * @library /test/jdk/java/io/Serializable/oldTests/
 * @build ArrayOpsTest PrimitivesTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer SimpleArrays
 */

/* @test
 * @summary it is a new version of an old test which was
 *          /src/share/test/serialization/piotest.java
 *          Test of serialization when there is
 *          exceptions on the I/O stream
 *
 * @library /test/jdk/java/io/Serializable/oldTests/
 * @build PrimitivesTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer SerializeWithException
 */

/* @test
 * @summary it is new version of old test which was
 *          /src/share/test/serialization/psiotest.java
 *          Test pickling and unpickling an object with derived classes
 *          and using a read special to serialize the "middle" class.
 * @library /test/jdk/java/io/Serializable/oldTests/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer CheckingEquality
 */

/* @test
 * @summary it is new version of old test which was
 *          /src/share/test/serialization/piotest.java
 *          Test of serialization/deserialization of
 *          objects with BinaryTree types
 * @library /test/jdk/java/io/Serializable/oldTests/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer BinaryTree
 */


/* @test
 * @summary it is new version of old test which was
 *          /src/share/test/serialization/subtest.java
 *          This test verifies of invocation
 *          annotateClass/replaceObject methods
 * @library /test/jdk/java/io/Serializable/oldTests/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer AnnotateClass
 */

/* @test
 * @summary it is new version of old test which was
 *          /src/share/test/serialization/piotest.java
 *          Test of serialization/deserialization of
 *          objects with CircularListType types
 * @library /test/jdk/java/io/Serializable/oldTests/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer CircularList
 */

/* @test
 * @summary it is new version of old test which was under
 *          /src/share/test/serialization/psiotest.java
 *          Test validation callbacks
 * @library /test/jdk/java/io/Serializable/oldTests/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer ValidateClass
 */

/* @test
 * @summary it is a new version of an old test which was
 *          /src/share/test/serialization/piotest.java
 *          Test of serialization/deserialization of
 *          primitives
 *
 * @library /test/jdk/java/io/Serializable/oldTests/
 * @build PrimitivesTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer WritePrimitive
 */

/* @test
 * @summary it is a new version of an old test which was
 *          /src/share/test/serialization/piotest.java
 *          Test of serialization/deserialization
 *          of objects with fields of array type
 *
 * @library /test/jdk/java/io/Serializable/oldTests/
 * @build ArrayTest PrimitivesTest ArrayOpsTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer ArrayFields
 */

/* @test
 * @summary it is new version of old test which was
 *          /src/share/test/serialization/psiotest.java
 *          Test pickling and unpickling an object with derived classes
 *          and using a read special to serialize the "middle" class,
 *          which raises NotSerializableException inside writeObject()
 *          and readObject() methods.
 * @library /test/jdk/java/io/Serializable/oldTests/
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer CheckForException
 */