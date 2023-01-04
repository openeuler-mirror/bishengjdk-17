
/*
 * @bug 4897937
 * @summary Verify that the presence of the JVM_ACC_SYNTHETIC bit in the
 *          modifiers of fields and methods does not affect default
 *          serialVersionUID calculation.
 */

/*
 * This file is compiled with JDK 1.4.2 in order to obtain the Foo.class
 * "golden file" used in the test, which contains a synthetic field and method
 * for implementing the class literal reference.  This file is not itself used
 * directly by the test, but is kept here for reference.
 */
public class Foo implements java.io.Serializable {
    Foo() {
        Class cl = Integer.class;
    }
}
