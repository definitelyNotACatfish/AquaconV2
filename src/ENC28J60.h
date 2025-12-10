#ifndef ENC28J60_H_
#define ENC28J60_H_

// Includes
#include <stm32f4xx.h>

/*

Receivebuffer und Writebuffer sind fest eingestellt auf Haelfte Haeflte
WOL Pin wird nicht benutzt

************************************************************************************************
* Lib fuer das Ansteuern des Lanchips ENC28J60 ueber den SPI auf dem STM32F401RET6		   	   *
* 2019 � Frederinn															 	  	   *
************************************************************************************************

*/

// Debug
//#define ENC_Debug

// Portdefines
#define ENC_RCC_IOPENR		RCC_AHB1ENR_GPIOEEN											// Clock enable f�r GPIO
#define ENC_GPIO			GPIOE														// An welchem Port sind die Pins angeschlossen
#define ENC_DIR				ENC_GPIO->MODER												// Datadirection Port an dem CS und RST angeschlossen sind
#define ENC_OUT				ENC_GPIO->ODR												// Output
#define ENC_CS				3															// Chipselect
#define ENC_RST				2															// Reset
#define ENC_Buffsize 		1545														// Gr��e f�r Sede- und Empfangsbuffer
#define ENC_SPI_Max_Clock	21000000UL													// Maximale SPI Clock

// Interrupt
#define ENC_INT_RCC_IOPENR		RCC_AHB1ENR_GPIOEEN										// Clock enable f�r GPIO
#define ENC_INT_GPIO			GPIOE													// An welchem Port sind die Pins angeschlossen
#define ENC_INT_DIR				ENC_INT_GPIO->MODER										// Datadirection Port an dem INT angeschlossen sind
#define ENC_INT					4														// Interruptpin
#define ENC_INT_Interupt		EXTI4_IRQn												// Pin Interrupt
#define ENC_INT_Inthandler		EXTI4_IRQHandler										// Pin Interrupthandler
#define ENC_INT_EXTICR			EXTICR[1]												// In welchem Register der SYSCFG der Interrupt f�r den GPIO aktiviert werden muss
#define ENC_INT_EXTIx			SYSCFG_EXTICR2_EXTI4_PE									// GPIO aktivieren in EXTICRx


// SPI Opcodes
#define ENC_Read_Control_Register	0b00000000
#define ENC_Read_Buffer_Memory		0b00111010
#define ENC_Write_Control_Register	0b01000000
#define ENC_Write_Buffer_Memory		0b01111010
#define ENC_Bit_Field_Set			0b10000000
#define ENC_Bit_Field_Clear			0b10100000
#define ENC_System_Softreset		0b11111111

//Bankdefines
#define ENC_Bank0					0b00000000
#define ENC_Bank1					0b00000001
#define ENC_Bank2					0b00000010
#define ENC_Bank3					0b00000011

// Macros
#define ENC_CS_HIGH		ENC_OUT |= (1<<ENC_CS);											// Chipselect auf High setzen
#define ENC_CS_LOW		ENC_OUT &= ~(1<<ENC_CS);										// Chipselect auf Low setzen
#define ENC_RST_HIGH	ENC_OUT |= (1<<ENC_RST);										// Reset auf High setzen
#define ENC_RST_LOW		ENC_OUT &= ~(1<<ENC_RST);										// Reset auf Low setzen

// Registerdefines
#define	ERDPTL_B0					0x00
#define	ERDPTH_B0					0x01
#define	EWRPTL_B0					0x02
#define	EWRPTH_B0					0x03
#define	ETXSTL_B0					0x04
#define	ETXSTH_B0					0x05
#define	ETXNDL_B0					0x06
#define	ETXNDH_B0					0x07
#define	ERXSTL_B0					0x08
#define	ERXSTH_B0					0x09
#define	ERXNDL_B0					0x0a
#define	ERXNDH_B0					0x0b
#define	ERXRDPTL_B0					0x0C
#define	ERXRDPTH_B0					0x0D
#define	ERXWRPTL_B0					0x0E
#define	ERXWRPTH_B0					0x0F
#define	EDMASTL_B0					0x10
#define	EDMASTH_B0					0x11
#define	EDMANDL_B0					0x12
#define	EDMANDH_B0					0x13
#define	EDMADSTL_B0					0x14
#define	EDMADSTH_B0					0x15
#define	EDMACSL_B0					0x16
#define	EDMACSH_B0					0x17
#define	EIE_B0						0x1b
#define	EIR_B0						0x1C
#define	ESTAT_B0					0x1d
#define	ECON2_B0					0x1e
#define	ECON1_B0					0x1f

#define	ET0_B1						0x00
#define	ET1_B1						0x01
#define	ET2_B1						0x02
#define	ET3_B1						0x03
#define	ET4_B1						0x04
#define	ET5_B1						0x05
#define	ET6_B1						0x06
#define	ET7_B1						0x07
#define	EPMM0_B1					0x08
#define	EPMM1_B1					0x09
#define	EPMM2_B1					0x0a
#define	EPMM3_B1					0x0b
#define	EPMM4_B1					0x0C
#define	EPMM5_B1					0x0D
#define	EPMM6_B1					0x0E
#define	EPMM7_B1					0x0F
#define	EPMCSL_B1					0x10
#define	EPMCS_B1					0x11
#define	EPMOL_B1					0x14
#define	EPMO_B1						0x15
#define	EWOLIE_B1					0x16
#define	EWOLIR_B1					0x17
#define	ERXFCON_B1					0x18
#define	EPKTCNT_B1					0x19
#define	EIE_B1						0x1b
#define	EIR_B1						0x1C
#define	ESTAT_B1					0x1d
#define	ECON2_B1					0x1e
#define	ECON1_B1					0x1f

#define	MACON1_B2					0x00
#define	MACON2_B2					0x01
#define	MACON3_B2					0x02
#define	MACON4_B2					0x03
#define	MABBIPG_B2					0x04
#define	MAIPGL_B2					0x06
#define	MAIPGH_B2					0x07
#define	MACLCON1_B2					0x08
#define	MACLCON2_B2					0x09
#define	MAMXFLL_B2					0x0a
#define	MAMXFLH_B2					0x0b
#define	MAPSUP_B2					0x0D
#define	MICON_B2					0x11
#define	MICMD_B2					0x12
#define	MIREGADR_B2					0x14
#define	MIWRL_B2					0x16
#define	MIWRH_B2					0x17
#define	MIRDL_B2					0x18
#define	MIRDH_B2					0x19
#define	EIE_B2						0x1b
#define	EIR_B2						0x1C
#define	ESTAT_B2					0x1d
#define	ECON2_B2					0x1e
#define	ECON1_B2					0x1f

#define	MAADR1_B3					0x00
#define	MAADR0_B3					0x01
#define	MAADR3_B3					0x02
#define	MAADR2_B3					0x03
#define	MAADR5_B3					0x04
#define	MAADR4_B3					0x05
#define	EBSTSD_B3					0x06
#define	EBSTCON_B3					0x07
#define	EBSTCSL_B3					0x08
#define	EBSTCS_B3					0x09
#define	MISTAT_B3					0x0a
#define	EREVID_B3					0x12
#define	ECOCON_B3					0x15
#define	EFLOCON_B3					0x17
#define	EPAUSL_B3					0x18
#define	EPAUS_B3					0x19
#define	EIE_B3						0x1b
#define	EIR_B3						0x1C
#define	ESTAT_B3					0x1d
#define	ECON2_B3					0x1e
#define	ECON1_B3					0x1f

#define PHCON1						0x00
#define PHSTAT1						0x01
#define PHID1						0x02
#define PHID2						0x03
#define PHCON2						0x10
#define PHSTAT2						0x11
#define PHIE						0x12
#define PHIR						0x13
#define PHLCON						0x14

// Structures
struct ENC_Head																			// Header des gelesen Frames
{
	char Source_MAC[6];
	char Dest_MAC[6];
	uint16_t Type;
};

// Globale Variablen
extern volatile char gl_ENC_Interrupt_Pending;											// Flag f�r Interrupt
extern char gl_ENC_Readbuffer[ENC_Buffsize];											// Packet Read Buffer nur 1500 Byte daten nutzbar, der rest ist fuer interne Zwecke
extern char gl_ENC_Write_Datapayload[ENC_Buffsize];										// Packet Write Buffer nur 1500 Byte daten nutzbar, der rest ist fuer interne Zwecke
extern uint16_t gl_ENC_Nextpacketpointer;
extern struct ENC_Head gl_ENC_Head_read;

// Funktionen
extern void ENC_Portinit(void);															// Portinit
extern void ENC_Hardreset(void);														// Hardreset
extern void ENC_Bankjump(uint8_t Bank);													// Bankwechsel auf dem Chip
extern uint8_t ENC_Read_ETH_Register(uint8_t Register);									// Lese ETH Register
extern void ENC_Write_ETH_Register(uint8_t Register,uint8_t Data);						// Schreibe ETH Register
extern uint8_t ENC_Read_MAC_MII_Register(uint8_t Register);								// Lese MAC MII Register
extern void ENC_Write_MAC_MII_Register(uint8_t Register,uint8_t Data);					// Schreibe MAC MII Register
extern void ENC_Softreset(void);														// Softreset
extern uint16_t ENC_Read_PHY_Register(uint8_t Register);								// Lese PHY Register
extern void ENC_Write_PHY_Register(uint8_t Register,uint16_t Data);						// Schreibe PHY Register
extern void ENC_Clear_Readbuffer(void);													// Loesche den Readbuffer im �C
extern void ENC_Clear_Writebuffer(void);												// Loesche den Schreibbuffer im �C
extern uint8_t ENC_Read_Hardwareversion(void);											// Hardwareversion des Chips
extern uint8_t ENC_Init(void);															// Receive im unteren Speicher des Buffer, Vollduplex, KEIN HARDWARE DMA!!! 0=erfolgreich, 1=nicht
extern uint8_t ENC_Send_Packet(char MAC[6],uint16_t Type_Length,uint16_t Start, uint16_t Datalength);				// Momentan noch ohne Interrupt
extern uint8_t ENC_Read_Next_Packet(void);												// Lese das naechste Packet aus dem ENC
extern uint8_t ENC_Check_for_Packets(void);												// Gibt die Anzahl der zu lesenden Packet im Buffer wieder
extern uint8_t ENC_Check_Frametype(uint16_t Type);										// Prueft was fuer ein Typ das Packet hat
extern void ENC_Read_Header_from_Frame(struct ENC_Head *ENC_Head);						// Lie�t den Header des empfangen Frame aus
extern void ENC_Init_Dump(void);														// Lie�t die Init aus
extern uint8_t ENC_Check_Link_Status (void);											// Pr�ft Linkstatus 0=stabil, 1=n�
extern void ENC_LED_Test(void);															// LEDs blinken lassen

#endif /* ENC28J60_DEFINES_H_ */
