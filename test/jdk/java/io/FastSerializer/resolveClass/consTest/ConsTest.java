 

/*
 * @test
 * @bug 4413434
 * @library /test/lib
 * @build jdk.test.lib.util.JarUtils SetupJar Boot
 * @run driver SetupJar
 * @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer -Xbootclasspath/a:boot.jar ConsTest
 * @summary Verify that generated java.lang.reflect implementation classes do
 *          not interfere with serialization's class resolution mechanism.
 */

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.lang.reflect.Constructor;

public class ConsTest implements Serializable {
    public static void main(String[] args) throws Exception {
        Constructor cons = Boot.class.getConstructor(
            new Class[] { ObjectInputStream.class });
        ByteArrayOutputStream bout = new ByteArrayOutputStream();
        ObjectOutputStream oout = new ObjectOutputStream(bout);
        oout.writeObject(new ConsTest());
        oout.close();

        for (int i = 0; i < 100; i++) {
            ObjectInputStream oin = new ObjectInputStream(
                new ByteArrayInputStream(bout.toByteArray()));
            cons.newInstance(new Object[]{ oin });
        }
    }
}
