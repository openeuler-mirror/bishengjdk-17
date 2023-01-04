

package extension;

import java.util.Hashtable;
import java.io.*;

public class ExtendedObjectInputStream extends ObjectInputStream {

    private static Hashtable renamedClassMap;

    public ExtendedObjectInputStream(InputStream si)
        throws IOException, StreamCorruptedException
    {
        super(si);
    }

    protected Class resolveClass(ObjectStreamClass v)
        throws IOException, ClassNotFoundException
    {
        if (renamedClassMap != null) {
            //      System.out.println("resolveClass(" + v.getName() + ")");
            Class newClass = (Class)renamedClassMap.get(v.getName());
            if (newClass != null) {
                v = ObjectStreamClass.lookup(newClass);
            }
        }
        return super.resolveClass(v);
    }

    public static void addRenamedClassName(String oldName, String newName)
        throws ClassNotFoundException
    {
        Class cl = null;

        if (renamedClassMap == null)
            renamedClassMap = new Hashtable(10);
        if (newName.startsWith("[L")) {
            //      System.out.println("Array processing");
            Class componentType =
                Class.forName(newName.substring(2));
            //System.out.println("ComponentType=" + componentType.getName());
            Object dummy =
                java.lang.reflect.Array.newInstance(componentType, 3);

            cl = dummy.getClass();
            //      System.out.println("Class=" + cl.getName());
        }
        else
            cl = Class.forName(newName);
        //System.out.println("oldName=" + oldName +
        //                   " newName=" + cl.getName());
        renamedClassMap.put(oldName, cl);
    }

}
