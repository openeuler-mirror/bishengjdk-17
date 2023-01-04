/*
*- @TestCaseID:FastSerializer/innerExtern
*- @TestCaseName:innerExtern
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
* @summary test static field cannot be deserialized
* @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer -DfastSerializerEscapeMode=true -DprintFastSerializer=true innerExtern
*/


import java.io.*;


class Foo implements Externalizable {
	static class Foo1 implements Externalizable {
		public Foo1(){}
		Foo1 (int i, String s) {
		this.i = i;
		this.s = s;
		}
		int i = 0;
		String s ;
		
		public boolean equals(Foo1 obj) {
			return (this.i == obj.i && this.s.equals(obj.s));
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

	}
	@Override
		public void readExternal(ObjectInput in) throws IOException, ClassNotFoundException {
			
		}

		@Override
		public void writeExternal(ObjectOutput out) throws IOException {
			
		}
	
}

public class innerExtern {
    public static void main(String[] args) throws Exception{
		String configFile = System.getProperty("test.src") + "/../logging.properties";
		System.setProperty("java.util.logging.config.file",configFile);
		Foo.Foo1 f1 = new Foo.Foo1(1,"Hello");
		Foo.Foo1 f2 = new Foo.Foo1(2,"World");
		Foo.Foo1 f3 = new Foo.Foo1(3,"Good");
		Foo.Foo1 f4 = new Foo.Foo1(4,"Bye");

		ObjectOutputStream oout = new ObjectOutputStream(new FileOutputStream("Foo.ser"));
		oout.writeObject(f1);
        oout.writeObject(f2);
        oout.writeObject(f3);
        oout.writeObject(f4);
        oout.close();

        ObjectInputStream oin = new ObjectInputStream(new FileInputStream("Foo.ser"));
        try{
			Foo.Foo1 fout1 = (Foo.Foo1)oin.readObject();
			Foo.Foo1 fout2 = (Foo.Foo1)oin.readObject();
			Foo.Foo1 fout3 = (Foo.Foo1)oin.readObject();
			Foo.Foo1 fout4 = (Foo.Foo1)oin.readObject();
			oin.close();

			if(!fout1.equals(f1) || !fout2.equals(f2) ||!fout3.equals(f3) ||!fout4.equals(f4)){
				throw new Exception("deserialized obj different");
			} 
		}catch (ClassCastException ex){
			return;
		}
		ObjectInputStream oin2 = new ObjectInputStream(new FileInputStream("Foo.ser"));
		try {
			Foo.Foo1 fout5 = (Foo.Foo1)oin2.readObject();
			Foo.Foo1 fout6 = (Foo.Foo1)oin2.readObject();
			Foo.Foo1 fout7 = (Foo.Foo1)oin2.readObject();
			Foo.Foo1 fout8 = (Foo.Foo1)oin2.readObject();
			
			if(!fout5.equals(f1) || !fout6.equals(f2) ||!fout7.equals(f3) ||!fout8.equals(f4)){
				throw new Exception("deserialized obj different");
			} 
		}catch (ClassCastException ex){
			return;
		}
    }
}
