/*
*- @TestCaseID:jdk17/FastSerializer/Extern/overWriteAnnotateClass
*- @TestCaseName:jdk17_overWriteAnnotateClass
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
* @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer -DfastSerializerEscapeMode=true -DprintFastSerializer=true overWriteAnnotateClass
*/


import java.io.*;
import java.util.ArrayList;

class Foo implements Externalizable {
    int i ;
    String s ;
	public Foo(){}
    Foo (int i, String s) {
        this.i = i;
        this.s = s;
    }

    @Override
    public void readExternal(ObjectInput in) throws IOException, ClassNotFoundException {
        s = (String)in.readObject();
        i = in.readInt();
    }

    @Override
    public void writeExternal(ObjectOutput out) throws IOException {
        out.writeObject(s);
        out.writeInt(i);
    }
	
    public boolean equals(ArrayList obj) {
        return (this.i == (int)obj.get(0) && this.s.equals(obj.get(1)));
    }

	private Object readResolve() throws ObjectStreamException { 
		ArrayList<Object> list = new ArrayList<>();
		list.add(i);
		list.add(s);
		return list;
	}
}

class MyObjectOutputStream extends ObjectOutputStream{
 
    public MyObjectOutputStream(OutputStream out) throws IOException {
        super(out);
    }
 
    @Override
    protected void annotateClass(Class c)throws IOException
       {
          if(c == Foo.class)
          {
             super.annotateClass(c);
			 return ;
          }else {
			  throw new InvalidClassException(
                    "Unauthorized deserialization attempt");
		  }
       }
     
     
}

public class overWriteAnnotateClass {
    public static void main(String[] args) throws Exception{
		String configFile = System.getProperty("test.src") + "/../logging.properties";
		System.setProperty("java.util.logging.config.file",configFile);
        String testsrc = System.getProperty("test.src");
		Foo f1 = new Foo(1,"Hello");
		Foo f2 = new Foo(2,"World");
		Foo f3 = new Foo(3,"Good");
		Foo f4 = new Foo(4,"Bye");
		MyObjectOutputStream oout = new MyObjectOutputStream(new FileOutputStream("Foo.ser"));
		oout.writeObject(f1);
        oout.writeObject(f2);
        oout.writeObject(f3);
        oout.writeObject(f4);
        oout.close();
        ObjectInputStream oin = new ObjectInputStream(new FileInputStream("Foo.ser"));
        try{
			ArrayList fout1 = (ArrayList)oin.readObject();
			ArrayList fout2 = (ArrayList)oin.readObject();
			ArrayList fout3 = (ArrayList)oin.readObject();
			ArrayList fout4 = (ArrayList)oin.readObject();
			oin.close();
			if(!f1.equals(fout1) || !f2.equals(fout2) ||!f3.equals(fout3) ||!f4.equals(fout4)){
				throw new Exception("deserialized obj different");
			} 
		}catch (ClassCastException ex){
			return;
		}
		ObjectInputStream oin2 = new ObjectInputStream(new FileInputStream("Foo.ser"));
		try {
			ArrayList fout5 = (ArrayList)oin2.readObject();
			ArrayList fout6 = (ArrayList)oin2.readObject();
			ArrayList fout7 = (ArrayList)oin2.readObject();
			ArrayList fout8 = (ArrayList)oin2.readObject();
			if(!f1.equals(fout5) || !f2.equals(fout6) ||!f3.equals(fout7) ||!f4.equals(fout8)){
				throw new Exception("deserialized obj different");
			} 
		}catch (ClassCastException ex){
			return;
		}
    }
}
