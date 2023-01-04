/*
*- @TestCaseID:FastSerializer/Extern/changeModifier
*- @TestCaseName:Extern/changeModifier
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
* @summary test change Modifier on deserialize end
* @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer -DfastSerializerEscapeMode=true -DprintFastSerializer=true changeModifier
*/


import java.io.*;

class Foo implements Externalizable {
    int i ;
    private String s ;
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

public class changeModifier {
    public static void main(String[] args) throws Exception{
	String configFile = System.getProperty("test.src") + "/../logging.properties";
	System.setProperty("java.util.logging.config.file",configFile);
	Foo f1 = new Foo(1,"Hello");
	Foo f2 = new Foo(2,"World");
	Foo f3 = new Foo(3,"Good");
	Foo f4 = new Foo(4,"Bye");
	String testsrc = System.getProperty("test.src");
        ObjectInputStream oin = new ObjectInputStream(new FileInputStream(testsrc+"/Foo.ser"));
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
			throw ex;
		}
		ObjectInputStream oin2 = new ObjectInputStream(new FileInputStream(testsrc+"/Foo.ser"));
		try {
			Foo fout5 = (Foo)oin2.readObject();
			Foo fout6 = (Foo)oin2.readObject();
			Foo fout7 = (Foo)oin2.readObject();
			Foo fout8 = (Foo)oin2.readObject();
			if(!fout5.equals(f1) || !fout6.equals(f2) ||!fout7.equals(f3) ||!fout8.equals(f4)){
				throw new Exception("deserialized obj different");
			} 
		}catch (ClassCastException ex){
			throw ex;
		}
    }
}
