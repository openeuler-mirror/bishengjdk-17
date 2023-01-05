 

/*
 * @bug 4413434
 * @summary Verify that class loaded outside of application class loader is
 *          correctly resolved during deserialization when read in by custom
 *          readObject() method of a bootstrap class (in this case,
 *          java.util.Vector).
 */

import java.io.*;
import java.util.Vector;

public class Foo implements Runnable {

    static class TestElement extends Object implements Serializable {}

    public void run() {
        try {
            Vector<TestElement> container = new Vector<TestElement>();
            container.add(new TestElement());

            // iterate to trigger java.lang.reflect code generation
            for (int i = 0; i < 100; i++) {
                ByteArrayOutputStream bout = new ByteArrayOutputStream();
                ObjectOutputStream oout = new ObjectOutputStream(bout);
                oout.writeObject(container);
                oout.close();
                ObjectInputStream oin = new ObjectInputStream(
                    new ByteArrayInputStream(bout.toByteArray()));
                oin.readObject();
            }
        } catch (Exception ex) {
            throw new Error(
                "Error occured while (de)serializing container: ", ex);
        }
    }
}
