/*
*- @TestCaseID:jdk17/FastSerializer/enumVersion
*- @TestCaseName:enumVersion
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
* @summary test serialize and deserialize of Enum object
* @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer -DfastSerializerEscapeMode=true -DprintFastSerializer=true enumVersion
*/


import java.io.*;
import java.util.*;

enum E {
			A,
			B,
			C,
			D,
			;
		}

public class enumVersion {
    public static void main(String[] args) throws Exception{
		String configFile = System.getProperty("test.src") + "/../logging.properties";
		System.setProperty("java.util.logging.config.file",configFile);
        ObjectOutputStream oout = new ObjectOutputStream(new FileOutputStream("Foo3.ser"));
		oout.useProtocolVersion(ObjectInputStream.PROTOCOL_VERSION_1);
		int num = 100;
		Random r = new Random();
		E E1 = E.A;
		E E2 = E.B;
		E E3 = E.C;
		E E4 = E.D;
        oout.writeObject(E1);
		oout.writeObject(E2);
		oout.writeObject(E3);
		oout.writeObject(E4);
        oout.close();
        ObjectInputStream oin = new ObjectInputStream(new FileInputStream("Foo3.ser"));
        E Eout1 = (E)oin.readObject();
		E Eout2 = (E)oin.readObject();
		E Eout3 = (E)oin.readObject();
		E Eout4 = (E)oin.readObject();
        oin.close();
		if(!E1.equals(Eout1)||!E2.equals(Eout2)||!E3.equals(Eout3)||!E4.equals(Eout4)){
			throw new Exception("deserialized Enum different");
		} 
		ObjectInputStream oin2 = new ObjectInputStream(new FileInputStream("Foo3.ser"));
        E Eout5 = (E)oin2.readObject();
		E Eout6 = (E)oin2.readObject();
		E Eout7 = (E)oin2.readObject();
		E Eout8 = (E)oin2.readObject();
		if(!E1.equals(Eout5)||!E2.equals(Eout6)||!E3.equals(Eout7)||!E4.equals(Eout8)){
			throw new Exception("deserialized Enum different");
		} 
		oin2.close();
    }
}
