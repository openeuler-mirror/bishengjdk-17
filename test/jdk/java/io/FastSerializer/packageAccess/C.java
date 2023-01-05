 

/*
 * @bug 4765255
 * @summary Verify proper functioning of package equality checks used to
 *          determine accessibility of superclass constructor and inherited
 *          writeReplace/readResolve methods.
 */

import java.io.*;

public class C implements Serializable {
    Object writeReplace() throws ObjectStreamException {
        throw new Error("package-private writeReplace called");
    }

    Object readResolve() throws ObjectStreamException {
        throw new Error("package-private readResolve called");
    }
}
