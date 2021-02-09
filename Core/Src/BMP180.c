/**
	************************************************************
	************************************************************
	************************************************************
	*	文件名： 	led.c
	*
	*	作者： 		张继瑞
	*
	*	日期： 		2016-11-23
	*
	*	版本： 		V1.0
	*
	*	说明： 		LED初始化，亮灭LED
	*
	*	修改记录：
	************************************************************
	************************************************************
	************************************************************
**/

//单片机头文件
#include "main.h"

//LED头文件
#include "BMP180.h"
#include "delay.h"
#include "BH1750.h"
#include "math.h"
#include "gpio_iic.h"


_bmp180 bmp180;


//从BMP180读一个16位的数据
short BMP_ReadTwoByte(uint8_t ReadAddr)
{
    short data;
    uint8_t msb,lsb;

    IIC_Start();

    IIC_Send_Byte(0xEE);
    if(IIC_Wait_Ack())		//等待应答
        return IIC_1750_Err;

    IIC_Send_Byte(ReadAddr);
    if(IIC_Wait_Ack())		//等待应答
        return IIC_1750_Err;

    IIC_Start();

    IIC_Send_Byte(0xEF);
    if(IIC_Wait_Ack())		//等待应答
        return IIC_1750_Err;

    msb = IIC_Read_Byte(1);
    IIC_Ack();			//回应ACK
    lsb = IIC_Read_Byte(0);
    IIC_NAck();			//最后一个数据需要回NOACK
    IIC_Stop();

    data = msb*256 + lsb;

    return data;
}

//从BMP180的获取计算参数
void BMP_ReadCalibrationData(void)
{
    bmp180.AC1 = BMP_ReadTwoByte(0xAA);
    bmp180.AC2 = BMP_ReadTwoByte(0xAC);
    bmp180.AC3 = BMP_ReadTwoByte(0xAE);
    bmp180.AC4 = BMP_ReadTwoByte(0xB0);
    bmp180.AC5 = BMP_ReadTwoByte(0xB2);
    bmp180.AC6 = BMP_ReadTwoByte(0xB4);
    bmp180.B1  = BMP_ReadTwoByte(0xB6);
    bmp180.B2  = BMP_ReadTwoByte(0xB8);
    bmp180.MB  = BMP_ReadTwoByte(0xBA);
    bmp180.MC  = BMP_ReadTwoByte(0xBC);
    bmp180.MD  = BMP_ReadTwoByte(0xBE);
}

//从BMP180读取未修正的温度
long BMP_Read_UT(void)
{
    long temp = 0;
    //BMP_WriteOneByte(0xF4,0x2E);
    IIC_Write_One_Byte(0XEE, 0xF4, 0x2E);

    delay_ms(5);
    temp = (long)BMP_ReadTwoByte(0xF6);
    return temp;
}

//从BMP180读取未修正的大气压
long BMP_Read_UP(void)
{
    long pressure = 0;

    //BMP_WriteOneByte(0xF4,0x34);
    IIC_Write_One_Byte(0XEE, 0xF4, 0x34);
    delay_ms(5);

    pressure = (long)BMP_ReadTwoByte(0xF6);
    //pressure = pressure + BMP_ReadOneByte(0xf8);
    pressure &= 0x0000FFFF;

    return pressure;
}

//用获取的参数对温度和大气压进行修正，并计算海拔
void BMP_UncompemstatedToTrue(void)
{
    bmp180.UT = BMP_Read_UT();//第一次读取错误
    bmp180.UT = BMP_Read_UT();//进行第二次读取修正参数
    bmp180.UP = BMP_Read_UP();

    bmp180.X1 = ((bmp180.UT - bmp180.AC6) * bmp180.AC5) >> 15;
    bmp180.X2 = (((long)bmp180.MC) << 11) / (bmp180.X1 + bmp180.MD);
    bmp180.B5 = bmp180.X1 + bmp180.X2;
    bmp180.Temp  = (bmp180.B5 + 8) >> 4;

    bmp180.B6 = bmp180.B5 - 4000;
    bmp180.X1 = ((long)bmp180.B2 * (bmp180.B6 * bmp180.B6 >> 12)) >> 11;
    bmp180.X2 = ((long)bmp180.AC2) * bmp180.B6 >> 11;
    bmp180.X3 = bmp180.X1 + bmp180.X2;

    bmp180.B3 = ((((long)bmp180.AC1) * 4 + bmp180.X3) + 2) /4;
    bmp180.X1 = ((long)bmp180.AC3) * bmp180.B6 >> 13;
    bmp180.X2 = (((long)bmp180.B1) *(bmp180.B6*bmp180.B6 >> 12)) >>16;
    bmp180.X3 = ((bmp180.X1 + bmp180.X2) + 2) >> 2;
    bmp180.B4 = ((long)bmp180.AC4) * (unsigned long)(bmp180.X3 + 32768) >> 15;
    bmp180.B7 = ((unsigned long)bmp180.UP - bmp180.B3) * 50000;

    if(bmp180.B7 < 0x80000000)
    {
        bmp180.p = (bmp180.B7 * 2) / bmp180.B4;
    }
    else
    {
        bmp180.p = (bmp180.B7 / bmp180.B4) * 2;
    }

    bmp180.X1 = (bmp180.p >> 8) * (bmp180.p >>8);
    bmp180.X1 = (((long)bmp180.X1) * 3038) >> 16;
    bmp180.X2 = (-7357 * bmp180.p) >> 16;

    bmp180.p = bmp180.p + ((bmp180.X1 + bmp180.X2 + 3791) >> 4);

    bmp180.altitude = 44330 * (1-pow(((bmp180.p) / 101325.0),(1.0/5.255)));
}

void BMP180_test(void)

{
    unsigned char ID ;
		ID=IIC_Read_One_Byte(0xEE,0xD0);	 
    BMP_ReadCalibrationData();
    BMP_UncompemstatedToTrue();
  printf("BMP180_ID = 0X%X\t  BMP180_temp = %d.%dC\t   BMP180_Pressure = %ldPa\t   BMP180_Altitude = %.5fm\r\n",ID,bmp180.Temp/10,bmp180.Temp%10,bmp180.p,bmp180.altitude);

}




