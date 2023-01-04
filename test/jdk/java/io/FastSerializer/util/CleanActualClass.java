// !!!
// NOTE: this class is only used for FastSerializer test.
// !!!
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

/**
 * Clean class file , using relative paths which are actual serialalbe directory.
 */
public class CleanActualClass {
    public static final String TEST_ClASS = System.getProperty("test.classes", "").trim();

    /**
     * @param args files
     * @throws IOException if an I/O error occurs
     */
    public static void main(String[] args) throws IOException {
        if (args.length == 0) {
            throw new IllegalArgumentException("At least one file must be specified to clean");
        }
        String base = TEST_ClASS.replace("TestDescription.d", "");
	String rep = "/java/io/FastSerializer/";
	int pos = base.lastIndexOf(rep);
	if (pos != -1) {
	    StringBuilder builder = new StringBuilder();
	    builder.append(base.substring(0, pos));
	    builder.append("/test/jdk/java/io/Serializable/");
	    builder.append(base.substring(pos + rep.length()));
	    base = builder.toString();
	}

        for (String arg:args) {
            Path path = Paths.get(base, arg).toAbsolutePath().normalize();
            if (Files.deleteIfExists(path)) {
                System.out.printf("delete " + path + " success\n");
            } else {
                System.out.printf("file " + path + " doesn't exist\n");
            }
        }
    }
}

