 
package failureAtomicity;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.Serializable;

// For verification purposes only.

public class SerialRef implements Serializable {
    static final long serialVersionUID = -0L;
    public static Object obj;

    private final Object ref;

    public SerialRef(Object ref) {
        this.ref = ref;
    }

    private void readObject(ObjectInputStream in)
            throws IOException, ClassNotFoundException {
        in.defaultReadObject();
        SerialRef.obj = ref;
    }
}
