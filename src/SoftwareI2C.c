// Includes
#include "main.h"
//#include "delay.h"		// Ist in main.h als Inline
#include "SoftwareI2C.h"
#include "stm32f4xx.h"

/*
******************************************************************
* Software I2C ~100kHz Master auf dem STM32F407VGT6				 *
* 2019 � Frederinn		                                 *
******************************************************************

*/	

// Globale Variablen


// Funktionen


void SoftwareI2C_Master_Init() 																// I2C Master
{
	RCC->AHB1ENR |= SoftwareI2C_RCC_IOPENR;													// Port A Clock an
	SoftwareI2C_OUT |= (1<<SoftwareI2C_SDA)|(1<<SoftwareI2C_SCL);							// SDA und SCL erstmal auf Ausgang
	SoftwareI2C_DIR = (SoftwareI2C_DIR & ~((0b11 << (SoftwareI2C_SCL*2)) | (0b11 << (SoftwareI2C_SDA*2)))) | (0b01 << (SoftwareI2C_SCL*2))| (0b01 << (SoftwareI2C_SDA*2));				// Maskiere die alte Pinfunktion raus und setze auf Ausgang
}

uint8_t SoftwareI2C_Start(uint8_t address, uint8_t R_W)
{
	address = (address<<1) | R_W;															// Daten in Register
	
	SoftwareI2C_OUT |= (1<<SoftwareI2C_SDA)|(1<<SoftwareI2C_SCL);							// Ausgaenge auf High
	_delay_us(5);																			// Bus Freetime
	
	SoftwareI2C_OUT &= ~(1<<SoftwareI2C_SDA);												// SDA auf Low
	_delay_us(5);																			// Kurz warten
	SoftwareI2C_OUT &= ~(1<<SoftwareI2C_SCL);												// SCL auf Low
	_delay_us(5);																			// Warten auf ersten Clockpuls
	
	if (SoftwareI2C_Write_Byte(address))
	{
		return 1;
	} 
	else
	{
		return 0;
	}
}

uint8_t SoftwareI2C_Write_Byte(uint8_t Data)
{
	uint8_t N_ACK = 0;																// Merker ob Nack=1 oder Ack=0 empfangen wurde
	
	for (unsigned g=0;g<8;g++)
	{
		SoftwareI2C_OUT &= ~(1<<SoftwareI2C_SCL);											// Clocklow
		if ((Data & 0x80)==0)
		{
			SoftwareI2C_OUT &= ~(1<<SoftwareI2C_SDA);										// Loesche SDA wenn Low
		}
		else
		{
			SoftwareI2C_OUT |= (1<<SoftwareI2C_SDA);										// Setze SDA wenn High
		}
		Data = Data << 1;																	// Shift nach rechts
		_delay_us(5);																		// 1�s warten
		SoftwareI2C_OUT |= (1<<SoftwareI2C_SCL);											// Clockhigh
		//while ((SoftwareI2C_IN & (1<<SoftwareI2C_SCL))==0);									// Clockstretching abfangen
		_delay_us(5);
	}
	SoftwareI2C_OUT &= ~(1<<SoftwareI2C_SCL);												// Clocklow
	_delay_us(5);
	
	SoftwareI2C_DIR = (SoftwareI2C_DIR & ~(0b11 << (SoftwareI2C_SDA*2))) | (0b00 << (SoftwareI2C_SDA*2));	// N/Ack auf SDA auf Eingang
	SoftwareI2C_OUT |= (1<<SoftwareI2C_SCL);												// Clockhigh
	_delay_us(2.5);
	if (SoftwareI2C_IN & (1<<SoftwareI2C_SDA))												// Wenn Nack setze auf 1
	{
		N_ACK = 1;
	}
	_delay_us(2.5);
	SoftwareI2C_OUT &= ~(1<<SoftwareI2C_SCL);												// Clocklow
	_delay_us(5);
	SoftwareI2C_OUT |= (1<<SoftwareI2C_SDA);												// SDA wieder auf 1
	SoftwareI2C_DIR = (SoftwareI2C_DIR & ~(0b11 << (SoftwareI2C_SDA*2))) | (0b01 << (SoftwareI2C_SDA*2));	// N/Ack auf SDA auf Ausgang
	
	if (N_ACK)																				// Wenn Nack gebe 1 zurueck
	{
		return 1;
	}
	return 0;
}

void SoftwareI2C_Stop(void)
{
	SoftwareI2C_DIR = (SoftwareI2C_DIR & ~(0b11 << (SoftwareI2C_SDA*2))) | (0b01 << (SoftwareI2C_SDA*2));	// N/Ack auf SDA auf Ausgang
	SoftwareI2C_OUT &= ~(1<<SoftwareI2C_SDA);												// SDA wieder auf 0
	_delay_us(5);																			// Warten
	SoftwareI2C_OUT |= (1<<SoftwareI2C_SCL);												// Clockhigh
	_delay_us(5);																			// Warten
	SoftwareI2C_OUT |= (1<<SoftwareI2C_SDA);												// Datenhigh
}

uint8_t SoftwareI2C_Read_Ack (void)
{
	uint16_t Data=0;																	// Int da einmal auf das 9 Bit gerollt wird
	SoftwareI2C_DIR = (SoftwareI2C_DIR & ~(0b11 << (SoftwareI2C_SDA*2))) | (0b00 << (SoftwareI2C_SDA*2));	// N/Ack auf SDA auf Eingang

	for (uint8_t g=0;g<8;g++)
	{
		SoftwareI2C_OUT |= (1<<SoftwareI2C_SCL);											// Clockhigh
		_delay_us(2.5);
		if (SoftwareI2C_IN & (1<<SoftwareI2C_SDA))											// Wenn High dann Readbyte high
		{
			Data |= (1<<0);
		}
		Data = Data << 1;																	// Shift nach links
		_delay_us(2.5);
		SoftwareI2C_OUT &= ~(1<<SoftwareI2C_SCL);											// Clocklow
		_delay_us(5);
	}
	Data = Data >> 1;																		// Shift nach rechts zum Ausgleich
	SoftwareI2C_DIR = (SoftwareI2C_DIR & ~(0b11 << (SoftwareI2C_SDA*2))) | (0b01 << (SoftwareI2C_SDA*2));	// N/Ack auf SDA auf Ausgang
	SoftwareI2C_OUT &= ~(1<<SoftwareI2C_SDA);												// Loesche SDA
	_delay_us(1);																			// 1�s warten
	SoftwareI2C_OUT |= (1<<SoftwareI2C_SCL);												// Clockhigh
	_delay_us(5);
	SoftwareI2C_OUT &= ~(1<<SoftwareI2C_SCL);												// Clocklow
	_delay_us(1);																			// 1�s warten
	SoftwareI2C_OUT |= (1<<SoftwareI2C_SDA);												// SDA auf High
	_delay_us(4);
	return Data & 0xff;
}
	
uint8_t SoftwareI2C_Read_Nack (void)													// Int da einmal auf das 9 Bit gerollt wird
{
	uint16_t Data=0;
	SoftwareI2C_DIR = (SoftwareI2C_DIR & ~(0b11 << (SoftwareI2C_SDA*2))) | (0b00 << (SoftwareI2C_SDA*2));	// N/Ack auf SDA auf Eingang

	for (uint8_t g=0;g<8;g++)
	{
		SoftwareI2C_OUT |= (1<<SoftwareI2C_SCL);											// Clockhigh
		_delay_us(1);
		if (SoftwareI2C_IN & (1<<SoftwareI2C_SDA))											// Wenn High dann Readbyte high
		{
			Data |= (1<<0);
		}
		Data = Data << 1;																	// Shift nach links
		_delay_us(4);
		SoftwareI2C_OUT &= ~(1<<SoftwareI2C_SCL);											// Clocklow
		_delay_us(5);
	}
	Data = Data >> 1;																		// Shift nach rechts zum Ausgleich
	SoftwareI2C_DIR = (SoftwareI2C_DIR & ~(0b11 << (SoftwareI2C_SDA*2))) | (0b01 << (SoftwareI2C_SDA*2));	// N/Ack auf SDA auf Ausgang
	SoftwareI2C_OUT |= (1<<SoftwareI2C_SDA);												// SDA auf High
	_delay_us(1);																			// 1�s warten
	SoftwareI2C_OUT |= (1<<SoftwareI2C_SCL);												// Clockhigh
	_delay_us(5);
	SoftwareI2C_OUT &= ~(1<<SoftwareI2C_SCL);												// Clocklow
	_delay_us(1);																			// 1�s warten
	SoftwareI2C_OUT |= (1<<SoftwareI2C_SDA);												// SDA auf High
	_delay_us(4);
	
	return Data & 0xff;
}
