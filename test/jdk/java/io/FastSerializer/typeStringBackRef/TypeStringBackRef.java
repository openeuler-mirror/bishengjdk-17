/*
*- @TestCaseID:jdk17/FastSerializer/TypeStringBackRef
*- @TestCaseName:TypeStringBackRef
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
 * @bug 4405949
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer TypeStringBackRef
 * @summary Verify that back references are used when writing multiple type
 *          strings that are equal() to one another.
 */

import java.io.*;

public class TypeStringBackRef implements Serializable {

    String a, b, c, d, e, f, g;

    public static void main(String[] args) throws Exception {
        ByteArrayOutputStream bout = new ByteArrayOutputStream();
        ObjectOutputStream oout = new ObjectOutputStream(bout);
        oout.writeObject(ObjectStreamClass.lookup(TypeStringBackRef.class));
        oout.close();
        if (bout.size() != 25) {
            throw new Error("Wrong data length: " + bout.size());
        }
    }
}
