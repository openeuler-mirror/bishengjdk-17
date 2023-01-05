 

/*
 * @bug 4325590
 * @summary Verify that superclass data is not lost when incoming superclass
 *          descriptor is matched with local class that is not a superclass of
 *          the deserialized instance's class.
 */

public class B extends A implements Runnable {
    public B() { super(0xDEADBEEF); }

    // verify superclass data still present
    public void run() {
        if (i != 0xDEADBEEF) {
            throw new Error("superclass data erased");
        }
    }
}
