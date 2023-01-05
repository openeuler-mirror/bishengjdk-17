/*
*- @TestCaseID:FastSerializer/Extern/deletField
*- @TestCaseName:Extern/deletField
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
* @summary test delet field on deserialize end
* @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer -DfastSerializerEscapeMode=true -DprintFastSerializer=true deletField
*/


import java.io.*;

class Foo implements Externalizable {
    int i ;
	
	public Foo () {	
	}
    public Foo (int i) {
        this.i = i;
    }

    @Override
    public void readExternal(ObjectInput in) throws IOException, ClassNotFoundException {
        i = in.readInt();
    }

    @Override
    public void writeExternal(ObjectOutput out) throws IOException {
        out.writeInt(i);
    }
	public boolean equals(Foo obj) {
        return (this.i == obj.i);
    }
}

public class deletField {
    public static void main(String[] args) throws Exception{
		String configFile = System.getProperty("test.src") + "/../logging.properties";
		System.setProperty("java.util.logging.config.file",configFile);
        Foo f1 = new Foo(1);
		Foo f2 = new Foo(2);
		Foo f3 = new Foo(3);
		Foo f4 = new Foo(4);
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
		}catch (EOFException ex2){
                        System.out.println("Expected Exception");
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
		}catch (EOFException ex2){
                        System.out.println("Expected Exception");
                }
    }
}
