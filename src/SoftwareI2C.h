#ifndef I2C_H_
#define I2C_H_

// Includes
#include <stm32f4xx.h>

/*
******************************************************************
* Software I2C ~100kHz Master auf dem STM32F407VGT6				 *
* 2019 � Frederinn		                                 *
******************************************************************

*/

// Definitionen
#define I2C_Read	0b00000001												// Readbit
#define I2C_Write	0b00000000												// Writebit

#define SoftwareI2C_RCC_IOPENR		RCC_AHB1ENR_GPIOBEN						// Clock enable f�r GPIO
#define SoftwareI2C_GPIO			GPIOB									// An welchem Port sind die Pins angeschlossen
#define SoftwareI2C_DIR				SoftwareI2C_GPIO->MODER					// Dir
#define SoftwareI2C_IN				SoftwareI2C_GPIO->IDR					// In
#define SoftwareI2C_OUT				SoftwareI2C_GPIO->ODR					// Out
#define SoftwareI2C_SDA				0										// SDA
#define SoftwareI2C_SCL				1										// SCL

// Structs

// Globale Variablen

// Funktionen
extern void SoftwareI2C_Master_Init();										// I2C Master
extern uint8_t SoftwareI2C_Start(uint8_t address, uint8_t R_W);				// Schicke den Start und die Slaveaddress
extern uint8_t SoftwareI2C_Write_Byte(uint8_t Data);						// Schicke ein Byte ueber den I2C
extern void SoftwareI2C_Stop(void);											// Schreibe die Stopbedinung
extern uint8_t SoftwareI2C_Read_Ack (void);									// Lese das Byte vom Slave und best�tige mit ACK
extern uint8_t SoftwareI2C_Read_Nack (void);								// Lese das Byte vom Slave und best�tige mit NACK


#endif /* I2C_H_ */
