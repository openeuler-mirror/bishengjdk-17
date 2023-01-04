/*
*- @TestCaseID:FastSerializer/badSubstByReplace
*- @TestCaseName:badSubstByReplace
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
 * @summary Verify that ClassCastException is thrown when deserializing
 *          an object and one of its object fields is  incompatibly replaced
 *          by either replaceObject/resolveObject.
 * @library /test/jdk/java/io/Serializable/badSubstByReplace
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer BadSubstByReplace
 */
