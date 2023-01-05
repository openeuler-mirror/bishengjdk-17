 

/*
 * @bug 4186885
 */

import java.io.*;

public class EvolvedClass {
    public static void main(String args[]) throws Exception{
        ASubClass corg = new ASubClass(1, "SerializedByEvolvedClass");
        ASubClass cnew = null;

        // Deserialize in to new class object
        FileInputStream fi = new FileInputStream("parents.ser");
        try {
            ObjectInputStream si = new ObjectInputStream(fi);
            cnew = (ASubClass) si.readObject();
        } finally {
            fi.close();
        }

        System.out.println("Printing the deserialized class: ");
        System.out.println();
        System.out.println(cnew);
    }
}


/* During deserialization, the no-arg constructor of a serializable base class
 * must not be invoked.
 */
class ASuperClass implements Serializable {
    String name;

    ASuperClass() {
        /*
         * This method is not to be executed during deserialization for this
         * example. Must call no-arg constructor of class Object which is the
         * base class for ASuperClass.
         */
        throw new Error("ASuperClass: Wrong no-arg constructor invoked");
    }

    ASuperClass(String name) {
        this.name = new String(name);
    }

    public String toString() {
        return("Name:  " + name);
    }
}

class ASubClass extends ASuperClass implements Serializable {
    int num;

    private static final long serialVersionUID =6341246181948372513L;
    ASubClass(int num, String name) {
        super(name);
        this.num = num;
    }

    public String toString() {
        return (super.toString() + "\nNum:  " + num);
    }
}
