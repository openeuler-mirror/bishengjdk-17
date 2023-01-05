/*
*- @TestCaseID:jdk17/FastSerializer/superclassDataLoss
*- @TestCaseName:superclassDataLoss
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
 * @bug 4325590
 * @library /test/lib/ /test/jdk/java/io/Serializable/superclassDataLoss/
 * @build jdk.test.lib.util.JarUtils A B
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer SuperclassDataLossTest
 * @summary Verify that superclass data is not lost when incoming superclass
 *          descriptor is matched with local class that is not a superclass of
 *          the deserialized instance's class.
 */
