/*
*- @TestCaseID:jdk17/FastSerializer/transientSerializ
*- @TestCaseName:transientSerializ
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
* @summary test transient field cannot be deserialized
* @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer -DfastSerializerEscapeMode=true -DprintFastSerializer=true transientSerializ
*/


import java.io.*;


class Foo implements Serializable {
	Foo (int i, String s) {
		this.i = i;
		this.s = s;
	}
    transient int i = 0;
    String s ;
	
    public boolean equals(Foo obj) {
        return (this.i == obj.i && this.s.equals(obj.s));
    }
}

public class transientSerializ {
    public static void main(String[] args) throws Exception{
		String configFile = System.getProperty("test.src") + "/../logging.properties";
		System.setProperty("java.util.logging.config.file",configFile);
		Foo f1 = new Foo(1,"Hello");
		Foo f2 = new Foo(2,"World");
		Foo f3 = new Foo(3,"Good");
		Foo f4 = new Foo(4,"Bye");
		Foo fc1 = new Foo(0,"Hello");
		Foo fc2 = new Foo(0,"World");
		Foo fc3 = new Foo(0,"Good");
		Foo fc4 = new Foo(0,"Bye");
		ObjectOutputStream oout = new ObjectOutputStream(new FileOutputStream("Foo.ser"));
		oout.writeObject(f1);
        oout.writeObject(f2);
        oout.writeObject(f3);
        oout.writeObject(f4);
        oout.close();
		//System.out.println(f1.i);

        ObjectInputStream oin = new ObjectInputStream(new FileInputStream("Foo.ser"));
        try{
			Foo fout1 = (Foo)oin.readObject();
			Foo fout2 = (Foo)oin.readObject();
			Foo fout3 = (Foo)oin.readObject();
			Foo fout4 = (Foo)oin.readObject();
			oin.close();

			if(!fout1.equals(fc1) || !fout2.equals(fc2) ||!fout3.equals(fc3) ||!fout4.equals(fc4)){
				throw new Exception("deserialized obj different");
			} 
		}catch (ClassCastException ex){
			return;
		}
		ObjectInputStream oin2 = new ObjectInputStream(new FileInputStream("Foo.ser"));
		try {
			Foo fout5 = (Foo)oin2.readObject();
			Foo fout6 = (Foo)oin2.readObject();
			Foo fout7 = (Foo)oin2.readObject();
			Foo fout8 = (Foo)oin2.readObject();
			
			
			if(!fout5.equals(fc1) || !fout6.equals(fc2) ||!fout7.equals(fc3) ||!fout8.equals(fc4)){
				throw new Exception("deserialized obj different");
			} 
		}catch (ClassCastException ex){
			return;
		}
    }
}
