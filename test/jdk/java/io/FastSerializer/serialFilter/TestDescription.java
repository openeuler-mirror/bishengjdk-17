/*
*- @TestCaseID:jdk17/FastSerializer/CheckArrayTest
*- @TestCaseName:CheckArrayTest
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
 * @library /test/jdk/java/io/Serializable/serialFilter/
 * @build CheckArrayTest SerialFilterTest
 * @bug 8203368
 * @modules java.base/jdk.internal.access
 * @run testng/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer CheckArrayTest
 *
 * @summary Test the SharedSecret access to ObjectInputStream.checkArray works
 *      with overridden subclasses.
 */

/* @test
 * @library /test/jdk/java/io/Serializable/serialFilter/
 * @build FilterWithSecurityManagerTest SerialFilterTest
 * @run testng/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer FilterWithSecurityManagerTest
 * @run testng/othervm/policy=security.policy.without.globalFilter
 *          -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer
 *          -Djava.security.manager=default FilterWithSecurityManagerTest
 * @run testng/othervm/policy=security.policy
 *          -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer
 *          -Djava.security.manager=default
 *          -Djdk.serialFilter=java.lang.Integer FilterWithSecurityManagerTest
 *
 * @summary Test that setting specific filter is checked by security manager,
 *          setting process-wide filter is checked by security manager.
 */


/* @test
 * @library /test/jdk/java/io/Serializable/serialFilter/
 * @build MixedFiltersTest SerialFilterTest
 * @run testng/othervm
 *          -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer
 *          -Djdk.serialFilter=!java.**;!java.lang.Long;maxdepth=5;maxarray=5;maxbytes=90;maxrefs=5 MixedFiltersTest
 * @run testng/othervm
 *          -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer
 *          -Djdk.serialFilter=java.**;java.lang.Long;maxdepth=1000;maxarray=1000;maxbytes=1000;maxrefs=1000 MixedFiltersTest
 *
 * @summary Test that when both global filter and specific filter are set,
 *          global filter will not affect specific filter.
 */

/* @test
 * @library /test/jdk/java/io/Serializable/serialFilter/
 * @build CheckInputOrderTest SerialFilterTest
 * @run testng/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer CheckInputOrderTest
 *
 * @summary Test that when both global filter and specific filter are set,
 *          global filter will not affect specific filter.
 */

/* @test
 * @bug 8231422
 * @library /test/jdk/java/io/Serializable/serialFilter/
 * @build GlobalFilterTest SerialFilterTest
 * @run testng/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer GlobalFilterTest
 * @run testng/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer
 *          -Djdk.serialFilter=java.**
 *          -Dexpected-jdk.serialFilter=java.** GlobalFilterTest
 * @run testng/othervm/policy=security.policy
 *        -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer GlobalFilterTest
 * @run testng/othervm/policy=security.policy
 *        -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer
 *        -Djava.security.properties=${test.src}/java.security-extra1
 *        -Djava.security.debug=properties GlobalFilterTest
 *
 * @summary Test Global Filters
 */


