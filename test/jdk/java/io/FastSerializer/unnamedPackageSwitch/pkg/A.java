 
/*
 * @bug 4348213
 * @summary Verify that deserialization allows an incoming class descriptor
 *          representing a class in the default package to be resolved to a
 *          local class with the same name in a non-default package, and
 *          vice-versa.
 */

package pkg;

public class A implements java.io.Serializable {
    private static final long serialVersionUID = 0L;
}
