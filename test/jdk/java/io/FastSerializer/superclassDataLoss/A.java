 

/*
 * @bug 4325590
 * @summary Verify that superclass data is not lost when incoming superclass
 *          descriptor is matched with local class that is not a superclass of
 *          the deserialized instance's class.
 */

public class A implements java.io.Serializable {
    protected final int i;
    protected A(int i) { this.i = i; }
}
