/*
*- @TestCaseID:FastSerializer/TestEntry
*- @TestCaseName:TestEntry
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
 * @bug 4075221
 * @summary Enable serialize of nonSerializable Class descriptor.
 */

import java.io.EOFException;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.io.InvalidClassException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamClass;

class TestEntry {
    public static void main(String args[]) throws Exception {
        File f = new File("tmp.ser");
        if (args[0].compareTo("-s") == 0) {
            FileOutputStream of = new FileOutputStream(f);
            ObjectOutputStream oos = new ObjectOutputStream(of);
            Class cl = Class.forName(args[1]);
            oos.writeObject(cl);
            if (ObjectStreamClass.lookup(cl) != null)
                oos.writeObject(cl.newInstance());
            oos.close();
            System.out.println("Serialized Class " + cl.getName());
        } else if (args[0].compareTo("-de") == 0) {
            FileInputStream inf = new FileInputStream(f);
            ObjectInputStream ois = new ObjectInputStream(inf);
            Class cl = null;
            try {
                cl = (Class)ois.readObject();
                throw new Error("Expected InvalidClassException to be thrown");
            } catch (InvalidClassException e) {
                System.out.println("Caught expected exception DeSerializing class " + e.getMessage());
            }
            ois.close();
        } else if (args[0].compareTo("-doe") == 0) {
            FileInputStream inf = new FileInputStream(f);
            ObjectInputStream ois = new ObjectInputStream(inf);
            Class cl = null;
            cl = (Class)ois.readObject();
            try {
                ois.readObject();
                throw new Error("Expected InvalidClassException to be thrown");
            } catch (InvalidClassException e) {
                System.out.println("Caught expected exception DeSerializing class " + e.getMessage());
            }
            ois.close();
        } else if (args[0].compareTo("-d") == 0) {
            FileInputStream inf = new FileInputStream(f);
            ObjectInputStream ois = new ObjectInputStream(inf);
            Class cl = (Class)ois.readObject();
            try {
                ois.readObject();
            } catch (EOFException e) {
            }
            ois.close();
            System.out.println("DeSerialized Class " + cl.getName());
        } else {
            throw new RuntimeException("Unrecognized argument");
        }
    }
}
