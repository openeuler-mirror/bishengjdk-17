/*
*- @TestCaseID:FastSerializer/classLoaderSer
*- @TestCaseName:classLoaderSer
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
* @summary test change field to static on deserialize end
* @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer -DfastSerializerEscapeMode=true -DprintFastSerializer=true classLoaderSer
*/


import java.io.*;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;

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
}
class MyObjectInputStream extends ObjectInputStream {
	protected ClassLoader classLoader = this.getClass().getClassLoader();
	public MyObjectInputStream(InputStream in) throws IOException {
		super(in);
	}
	
	public MyObjectInputStream(InputStream in, ClassLoader cl) throws IOException{
		super(in);
		this.classLoader = cl;
	}
	@Override
	protected Class<?>resolveClass(ObjectStreamClass desc)throws IOException,ClassNotFoundException {
		String name = desc.getName();
		try {
			return Class.forName(name,false,this.classLoader);
		}catch(ClassNotFoundException Ex) {
			return super.resolveClass(desc);
		}
	}
}

class MyClassLoader extends ClassLoader {
    protected Class<?> findClass(String name) throws ClassNotFoundException {
        Class clazz = null;
        String classFilename = name + ".class";
        File classFile = new File(classFilename);
        if (classFile.exists()) {
            try (FileChannel fileChannel = new FileInputStream(classFile)
                    .getChannel();) {
                MappedByteBuffer mappedByteBuffer = fileChannel
                        .map(FileChannel.MapMode.READ_ONLY, 0, fileChannel.size());
                byte[] b = mappedByteBuffer.array();
                clazz = defineClass(name, b, 0, b.length);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
        if (clazz == null) {
            throw new ClassNotFoundException(name);
        }
        return clazz;
    }
}

public class classLoaderSer {
    public static void main(String[] args) throws Exception{
		String configFile = System.getProperty("test.src") + "/../logging.properties";
		System.setProperty("java.util.logging.config.file",configFile);
        String testsrc = System.getProperty("test.src");
		Foo f1 = new Foo(1,"Hello");
		Foo f2 = new Foo(2,"World");
		Foo f3 = new Foo(3,"Good");
		Foo f4 = new Foo(4,"Bye");
		ObjectOutputStream oout = new ObjectOutputStream(new FileOutputStream("Foo.ser"));
		oout.writeObject(f1);
        oout.writeObject(f2);
        oout.writeObject(f3);
        oout.writeObject(f4);
        oout.close();
        ObjectInputStream oin = new ObjectInputStream(new FileInputStream("Foo.ser"));
        try{
			Foo fout1 = (Foo)oin.readObject();
			Foo fout2 = (Foo)oin.readObject();
			Foo fout3 = (Foo)oin.readObject();
			Foo fout4 = (Foo)oin.readObject();
			oin.close();
			if(!fout1.equals(f1) || !fout2.equals(f2) ||!fout3.equals(f3) ||!fout4.equals(f4)){
				throw new Exception("deserialized obj different");
			} 
		}catch (ClassCastException ex){
			return;
		}
		MyClassLoader myClassLoader = new MyClassLoader();
		MyObjectInputStream oin2 = new MyObjectInputStream(new FileInputStream("Foo.ser"), myClassLoader);
		try {
			Foo fout5 = (Foo)oin2.readObject();
			Foo fout6 = (Foo)oin2.readObject();
			Foo fout7 = (Foo)oin2.readObject();
			Foo fout8 = (Foo)oin2.readObject();
			if(!fout5.equals(f1) || !fout6.equals(f2) ||!fout7.equals(f3) ||!fout8.equals(f4)){
				throw new Exception("deserialized obj different");
			} 
		}catch (ClassCastException ex){
			return;
		}
    }
}
