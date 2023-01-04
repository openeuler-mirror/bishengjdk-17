/*
*- @TestCaseID:FastSerializer/DefaultReadObjectCNFException
*- @TestCaseName:DefaultReadObjectCNFException
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
 * @bug 4662327
 * @summary Verify that ObjectInputStream.defaultReadObject() throws a
 *          ClassNotFoundException if any of the non-primitive field values it
 *          reads in are tagged with ClassNotFoundExceptions.
 * @library /test/jdk/java/io/Serializable/defaultReadObjectCNFException
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer DefaultReadObjectCNFException
 */
