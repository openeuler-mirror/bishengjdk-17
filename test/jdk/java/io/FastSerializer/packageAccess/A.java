 

/*
 * @bug 4765255
 * @summary Verify proper functioning of package equality checks used to
 *          determine accessibility of superclass constructor and inherited
 *          writeReplace/readResolve methods.
 */

public class A {

    static boolean packagePrivateConstructorInvoked = false;

    A() {
        packagePrivateConstructorInvoked = true;
    }

    public A(int throwaway) {
    }
}
