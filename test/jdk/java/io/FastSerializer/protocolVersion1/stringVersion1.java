/*
* @test
* @summary test serialize and deserialize of String object
* @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer -DfastSerializerEscapeMode=true -DprintFastSerializer=true stringVersion1
*/


import java.io.*;


public class stringVersion1 {
    public static void main(String[] args) throws Exception{
		String configFile = System.getProperty("test.src") + "/../logging.properties";
		System.setProperty("java.util.logging.config.file",configFile);
        ObjectOutputStream oout = new ObjectOutputStream(new FileOutputStream("Foo1.ser"));
		oout.useProtocolVersion(ObjectInputStream.PROTOCOL_VERSION_1);
        String sin1 = "Hello World1";
		String sin2 = "Hello World2";
		String sin3 = "Hello World3";
		String sin4 = "Hello World4";
        oout.writeObject(sin1);
		oout.writeObject(sin2);
		oout.writeObject(sin3);
		oout.writeObject(sin4);
        oout.close();
        ObjectInputStream oin = new ObjectInputStream(new FileInputStream("Foo1.ser"));
        String sout1 = (String)oin.readObject();
		String sout2 = (String)oin.readObject();
		String sout3 = (String)oin.readObject();
		String sout4 = (String)oin.readObject();
        oin.close();
		if(!sout1.equals(sin1) || !sout2.equals(sin2) ||!sout3.equals(sin3) ||!sout4.equals(sin4)){
			throw new Exception("deserialized string different");
		} 
		ObjectInputStream oin2 = new ObjectInputStream(new FileInputStream("Foo1.ser"));
        String sout5 = (String)oin2.readObject();
		String sout6 = (String)oin2.readObject();
		String sout7 = (String)oin2.readObject();
		String sout8 = (String)oin2.readObject();
		if(!sout5.equals(sin1) || !sout6.equals(sin2) ||!sout7.equals(sin3) ||!sout8.equals(sin4)){
			throw new Exception("deserialized string different");
		} 
    }
}
