/*
*- @TestCaseID:FastSerializer/dynamicExtern
*- @TestCaseName:dynamicExtern
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
* @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer -DfastSerializerEscapeMode=true -DprintFastSerializer=true dynamicExtern
*/


import java.io.*;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

interface ITest {

}
class Foo implements ITest, Externalizable {
    static int i ;
    static String s ;
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

class DynamicProxy implements InvocationHandler,Externalizable{
    private Foo subject;
    public DynamicProxy(){}
    public DynamicProxy(Foo subject){
        this.subject = subject;
    }
    @Override
    public Object invoke(Object object, Method method, Object[] args)throws Throwable{
        method.invoke(subject, args);
        return null;
    }
    @Override
    public void readExternal(ObjectInput in) throws IOException, ClassNotFoundException {
        subject.s = (String)in.readObject();
        subject.i = in.readInt();
    }

    @Override
    public void writeExternal(ObjectOutput out) throws IOException {
        out.writeObject(subject.s);
        out.writeInt(subject.i);
    }
}

public class dynamicExtern {
	public static void main(String[] args) throws Exception{
		String configFile = System.getProperty("test.src") + "/../logging.properties";
		System.setProperty("java.util.logging.config.file",configFile);
		Foo fr1 = new Foo(1,"Hello");
		Foo fr2 = new Foo(2,"World");
		Foo fr3 = new Foo(3,"Good");
		Foo fr4 = new Foo(4,"Bye");
		
		InvocationHandler handler1 = new DynamicProxy(fr1);
		InvocationHandler handler2 = new DynamicProxy(fr2);
		InvocationHandler handler3 = new DynamicProxy(fr3);
		InvocationHandler handler4 = new DynamicProxy(fr4);
		Class[] proxyInter = new Class[]{ITest.class};
		ITest f1 = (ITest)Proxy.newProxyInstance(handler1.getClass().getClassLoader(), proxyInter, handler1);
		ITest f2 = (ITest)Proxy.newProxyInstance(handler2.getClass().getClassLoader(), proxyInter, handler2);
		ITest f3 = (ITest)Proxy.newProxyInstance(handler3.getClass().getClassLoader(), proxyInter, handler3);
		ITest f4 = (ITest)Proxy.newProxyInstance(handler4.getClass().getClassLoader(), proxyInter, handler4);
		
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
		ObjectInputStream oin2 = new ObjectInputStream(new FileInputStream("Foo.ser"));
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
