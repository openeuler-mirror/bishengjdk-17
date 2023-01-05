 

import java.io.*;

public class SubclassOfOOS extends ObjectOutputStream {
        public SubclassOfOOS(OutputStream os) throws IOException {
                super(os);
        }

        public SubclassOfOOS() throws IOException {
                super();
        }
}
