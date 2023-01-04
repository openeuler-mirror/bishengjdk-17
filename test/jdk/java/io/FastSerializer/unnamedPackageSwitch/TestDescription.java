/*
*- @TestCaseID:jdk17/FastSerializer/unnamedPackageSwitch
*- @TestCaseName:unnamedPackageSwitch
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
 * @bug 4348213
 * @library /test/jdk/java/io/Serializable/unnamedPackageSwitch/
 * @build pkg.A
 * @build UnnamedPackageSwitchTest
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer UnnamedPackageSwitchTest
 * @summary Verify that deserialization allows an incoming class descriptor
 *          representing a class in the unnamed package to be resolved to a
 *          local class with the same name in a named package, and vice-versa.
 */