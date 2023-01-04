 
/*
 * @bug 4482471
 * @summary Verify that even if an incoming ObjectStreamClass is not resolvable
 *          to a local class, the ObjectStreamClass object itself is still
 *          deserializable (without incurring a ClassNotFoundException).
 */

import java.io.*;

public class Read {
    public static void main(String[] args) throws Exception {
        FileInputStream in = new FileInputStream("tmp.ser");
        try {
            ObjectInputStream oin = new ObjectInputStream(in);
            oin.readObject();
            oin.readObject();
            try {
                oin.readObject();
                throw new Error("read of Foo instance succeeded");
            } catch (ClassNotFoundException ex) {
            }
            try {
                oin.readObject();
                throw new Error("indirect read of Foo instance succeeded");
            } catch (ClassNotFoundException ex) {
            }
        } finally {
            in.close();
        }
    }
}
