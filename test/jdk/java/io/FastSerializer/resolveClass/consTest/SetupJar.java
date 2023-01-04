 

import java.nio.file.Path;
import java.nio.file.Paths;
import jdk.test.lib.util.JarUtils;
public class SetupJar {

    public static void main(String args[]) throws Exception {
        Path classes = Paths.get(System.getProperty("test.classes", ""));
        JarUtils.createJarFile(Paths.get("boot.jar"), classes,
                classes.resolve("Boot.class"));
    }
}
