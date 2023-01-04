 

/*
 * @bug 4765255
 * @summary Verify proper functioning of package equality checks used to
 *          determine accessibility of superclass constructor and inherited
 *          writeReplace/readResolve methods.
 */

public class B extends A implements java.io.Serializable {
    public B() {
        super(0);
    }
}
