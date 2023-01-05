/*
*- @TestCaseID:jdk17/FastSerializer/ResolveClassException
*- @TestCaseName:ResolveClassException
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
 * @bug 4191941
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer ResolveClassException
 * @summary Ensure that original ClassNotFoundException thrown inside of
 *          ObjectInputStream.resolveClass() is preserved (and thrown).
 */

import java.io.*;

class BrokenObjectInputStream extends ObjectInputStream {

    static final String message = "bodega";

    BrokenObjectInputStream(InputStream in) throws IOException {
        super(in);
    }

    protected Class resolveClass(ObjectStreamClass desc)
        throws IOException, ClassNotFoundException
    {
        throw new ClassNotFoundException(message);
    }
}

public class ResolveClassException {
    public static void main(String[] args) throws Exception {
        ByteArrayOutputStream bout;
        ObjectOutputStream oout;
        ByteArrayInputStream bin;
        BrokenObjectInputStream oin;
        Object obj;

        // write and read an object
        obj = new Integer(5);
        bout = new ByteArrayOutputStream();
        oout = new ObjectOutputStream(bout);
        oout.writeObject(obj);
        bin = new ByteArrayInputStream(bout.toByteArray());
        oin = new BrokenObjectInputStream(bin);
        try {
            oin.readObject();
        } catch (ClassNotFoundException e) {
            if (! BrokenObjectInputStream.message.equals(e.getMessage()))
                throw new Error("Original exception not preserved");
        }

        // write and read an array of objects
        obj = new Integer[] { new Integer(5) };
        bout = new ByteArrayOutputStream();
        oout = new ObjectOutputStream(bout);
        oout.writeObject(obj);
        bin = new ByteArrayInputStream(bout.toByteArray());
        oin = new BrokenObjectInputStream(bin);
        try {
            oin.readObject();
        } catch (ClassNotFoundException e) {
            if (! BrokenObjectInputStream.message.equals(e.getMessage()))
                throw new Error("Original exception not preserved");
        }
    }
}
