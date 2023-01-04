/*
*- @TestCaseID:jdk17/FastSerializer/arrayVersion1
*- @TestCaseName:arrayVersion1
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
* @summary test serialize and deserialize of Array object
* @run main/othervm -XX:+UnlockExperimentalVMOptions -XX:+UseFastSerializer -DfastSerializerEscapeMode=true -DprintFastSerializer=true arrayVersion1
*/


import java.io.*;
import java.util.*;


public class arrayVersion1 {
    public static void main(String[] args) throws Exception{
		String configFile = System.getProperty("test.src") + "/../logging.properties";
		System.setProperty("java.util.logging.config.file",configFile);
        ObjectOutputStream oout = new ObjectOutputStream(new FileOutputStream("Foo2.ser"));
		oout.useProtocolVersion(ObjectInputStream.PROTOCOL_VERSION_1);
		int num = 100;
		Random r = new Random();
		int[] A1 = new int[num];
		int[] A2 = new int[num];
		int[] A3 = new int[num];
		int[] A4 = new int[num];
        for(int i = 0; i < num; i++) {
			A1[i] = r.nextInt();
			A2[i] = r.nextInt();
			A3[i] = r.nextInt();
			A4[i] = r.nextInt();
		}
        oout.writeObject(A1);
		oout.writeObject(A2);
		oout.writeObject(A3);
		oout.writeObject(A4);
        oout.close();
        ObjectInputStream oin = new ObjectInputStream(new FileInputStream("Foo2.ser"));
        int[] Aout1 = (int[])oin.readObject();
		int[] Aout2 = (int[])oin.readObject();
		int[] Aout3 = (int[])oin.readObject();
		int[] Aout4 = (int[])oin.readObject();
        oin.close();
		if(!Arrays.equals(A1,Aout1)||!Arrays.equals(A2,Aout2)||!Arrays.equals(A3,Aout3)||!Arrays.equals(A4,Aout4)){
			throw new Exception("deserialized Array different");
		} 
		ObjectInputStream oin2 = new ObjectInputStream(new FileInputStream("Foo2.ser"));
        int[] Aout5 = (int[])oin2.readObject();
		int[] Aout6 = (int[])oin2.readObject();
		int[] Aout7 = (int[])oin2.readObject();
		int[] Aout8 = (int[])oin2.readObject();
		if(!Arrays.equals(A1,Aout5)||!Arrays.equals(A2,Aout6)||!Arrays.equals(A3,Aout7)||!Arrays.equals(A4,Aout8)){
			throw new Exception("deserialized Array different");
		} 
		oin2.close();
    }
}
