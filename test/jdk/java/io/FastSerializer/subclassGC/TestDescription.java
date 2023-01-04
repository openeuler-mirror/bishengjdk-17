/*
*- @TestCaseID:jdk17/FastSerializer/subclassGC
*- @TestCaseName:subclassGC
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
 * @bug 6232010
 * @summary this test checks that replacing SoftCache class with ConcurrentMap
 *          in ObjectInputStream/ObjectOutputStream gives an opportunity to
 *          classes which are inherited from OIS and OOS and loaded through
 *          separete ClassLoaders be available for garbage collection
 *
 * @author Andrey Ozerov
 * @library /test/jdk/java/io/Serializable/subclassGC/
 * @build SubclassOfOOS
 * @run main/othervm/policy=security.policy -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer SubclassGC
 */
