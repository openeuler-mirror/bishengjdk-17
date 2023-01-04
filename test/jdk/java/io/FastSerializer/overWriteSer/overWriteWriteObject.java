/*
*- @TestCaseID:jdk17/FastSerializer/overWriteWriteObject
*- @TestCaseName:overWriteWriteObject
*- @TestCaseType:Function test
*- @RequirementID:AR.SR.IREQ02478866.001.001
*- @RequirementName:FastSeralizer 功能实现
*- @Condition:UseFastSerializer
*- @Brief:
*   -#step1 将对象写入数据流
*   -#step2 从数据流中读取对象
*- @Expect: 读取对象与写入对象相同
*- @Priority:Level 1
*/

/*
* @test
* @summary test serialize and deserialize of normal object
* @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer -DfastSerializerEscapeMode=true -DprintFastSerializer=true overWriteWriteObject
*/


import java.io.*;

class Foo implements Serializable {
	Foo (int i, String s) {
		this.i = i;
		this.s = s;
	}
    int i;
    String s ;
	
    public boolean equals(Foo obj) {
        return (this.i == obj.i && this.s.equals(obj.s));
    }
	
	private void writeObject(java.io.ObjectOutputStream stream)
            throws IOException {
        stream.writeObject(s);
        stream.writeInt(i);
    }
	
	private void readObject(java.io.ObjectInputStream stream)
            throws IOException, ClassNotFoundException {
        s = (String) stream.readObject();
        i = stream.readInt();
    }
}

public class overWriteWriteObject {
    public static void main(String[] args) throws Exception{
		String configFile = System.getProperty("test.src") + "/../logging.properties";
		System.setProperty("java.util.logging.config.file",configFile);
        ObjectOutputStream oout = new ObjectOutputStream(new FileOutputStream("Foo4.ser"));
        Foo f1 = new Foo(1,"Hello");
		Foo f2 = new Foo(2,"World");
		Foo f3 = new Foo(3,"Good");
		Foo f4 = new Foo(4,"Bye");
        oout.writeObject(f1);
		oout.writeObject(f2);
		oout.writeObject(f3);
		oout.writeObject(f4);
        oout.close();
        ObjectInputStream oin = new ObjectInputStream(new FileInputStream("Foo4.ser"));
        Foo fout1 = (Foo)oin.readObject();
		Foo fout2 = (Foo)oin.readObject();
		Foo fout3 = (Foo)oin.readObject();
		Foo fout4 = (Foo)oin.readObject();
        oin.close();
		if(!fout1.equals(f1) || !fout2.equals(f2) ||!fout3.equals(f3) ||!fout4.equals(f4)){
			throw new Exception("deserialized obj different");
		} 
		ObjectInputStream oin2 = new ObjectInputStream(new FileInputStream("Foo4.ser"));
        Foo fout5 = (Foo)oin2.readObject();
		Foo fout6 = (Foo)oin2.readObject();
		Foo fout7 = (Foo)oin2.readObject();
		Foo fout8 = (Foo)oin2.readObject();
		if(!fout5.equals(f1) || !fout6.equals(f2) ||!fout7.equals(f3) ||!fout8.equals(f4)){
			throw new Exception("deserialized obj different");
		} 
    }
}
