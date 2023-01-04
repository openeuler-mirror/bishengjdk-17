/*
*- @TestCaseID:jdk17/FastSerializer/nullExtern
*- @TestCaseName:jdk17_nullExtern
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
* @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer -DfastSerializerEscapeMode=true -DprintFastSerializer=true nullExtern
*/


import java.io.*;


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
	public boolean equals(Foo obj) {
        return (this.i == obj.i && this.s.equals(obj.s) );
    }
}

public class nullExtern {
    public static void main(String[] args) throws Exception{
		String configFile = System.getProperty("test.src") + "/../logging.properties";
		System.setProperty("java.util.logging.config.file",configFile);
        String testsrc = System.getProperty("test.src");
		Foo f1 = null;
		Foo f2 = null;
		Foo f3 = null;
		Foo f4 = null;
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
			if(fout1!=null || fout2!=null ||fout3!=null ||fout4!=null){
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
			if(fout5!=null || fout6!=null ||fout7!=null ||fout8!=null){
				throw new Exception("deserialized obj different");
			} 
		}catch (ClassCastException ex){
			return;
		}
    }
}
