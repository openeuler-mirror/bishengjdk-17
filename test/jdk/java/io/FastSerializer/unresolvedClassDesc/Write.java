

/* @test
 * @bug 4482471
 *
 * @clean Write Read Foo
 * @build Write Foo
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Write
 * @clean Write Foo
 * @build Read
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer Read
 * @clean Read
 *
 * @summary Verify that even if an incoming ObjectStreamClass is not resolvable
 *          to a local class, the ObjectStreamClass object itself is still
 *          deserializable (without incurring a ClassNotFoundException).
 */

import java.io.*;

public class Write {
    public static void main(String[] args) throws Exception {
        ObjectOutputStream oout =
            new ObjectOutputStream(new FileOutputStream("tmp.ser"));
        ObjectStreamClass desc = ObjectStreamClass.lookup(Foo.class);
        Foo foo = new Foo();
        oout.writeObject(desc);
        oout.writeObject(new Object[]{ desc }); // test indirect references
        oout.writeObject(foo);
        oout.writeObject(new Object[]{ foo });
        oout.close();
    }
}
