// Includes
#include "main.h"
#include "enc28j60.h"
#include "SPI.h"
#ifdef ENC_Debug
	#include "USART.h"
#endif
#include "stm32f4xx.h"
#include <string.h>
#include <stdio.h>
#include "FAT32.h"
#include "INI_Parser.h"

/*

Receivebuffer und Writebuffer sind fest eingestellt auf Haelfte Haeflte
WOL Pin wird nicht benutzt

************************************************************************************************
* Lib fuer das Ansteuern des Lanchips ENC28J60 ueber den SPI auf dem STM32F401RET6		   	   *
* 2019 � Frederinn															 	  	   *
************************************************************************************************

*/

// Globale Variablen
volatile char gl_ENC_Interrupt_Pending=0;												// Flag f�r Interrupt
char gl_ENC_Readbuffer[1545];															// Packet Read Buffer nur 1500 Byte daten nutzbar, der rest ist fuer interne Zwecke
char gl_ENC_Write_Datapayload[1545];													// Packet Write Buffer nur 1500 Byte daten nutzbar, der rest ist fuer interne Zwecke
uint16_t gl_ENC_Nextpacketpointer=0;
struct ENC_Head gl_ENC_Head_read;

// Funktionen
void ENC_INT_Inthandler (void)
{
	if(EXTI->PR & (1<<ENC_INT))
	{
		EXTI->PR |= (1<<ENC_INT);														// Int reset
		gl_ENC_Interrupt_Pending=1;														// Int pending setzen
	}
}

void ENC_Portinit(void)
{
	RCC->AHB1ENR |= ENC_RCC_IOPENR;														// Port Clock an
	ENC_DIR = (ENC_DIR & ~((0b11 << (ENC_CS*2)) | (0b11 << (ENC_RST*2)))) | (0b01 << (ENC_CS*2)) | (0b01 << (ENC_RST*2));		// Maskiere die alte Pinfunktion raus und setze auf Ausgang
	ENC_GPIO->OSPEEDR = (ENC_GPIO->OSPEEDR & ~((0b11 << (ENC_CS*2)) | (0b11 << (ENC_RST*2)))) | (0b10 << (ENC_CS*2))| (0b10 << (ENC_RST*2));		// High Speed

	RCC->AHB1ENR |= ENC_INT_RCC_IOPENR;													// Port Clock an
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;												// SYSCFGEN Clock an
	ENC_INT_DIR = (ENC_INT_DIR & ~(0b11 << (ENC_INT*2))) | (0b00 << (ENC_INT*2));		// Maskiere die alte Pinfunktion raus und setze auf Eingang
	SYSCFG->ENC_INT_EXTICR = ENC_INT_EXTIx;												// GPIO aktvieren
	EXTI->FTSR |= (1<<ENC_INT);															// Fallende Flanke
	EXTI->IMR |= (1<<ENC_INT);															// Interrupt f�r INT_EN maskieren

	NVIC_SetPriority(ENC_INT_Interupt,NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 3, 0));
	NVIC_EnableIRQ(ENC_INT_Interupt);													// IRQ aktivieren

	ENC_RST_HIGH;
	ENC_CS_HIGH;
	_delay_us(1);																		// Warte noch kurz nachdem der Pin auf High gezogen wurde
}

void ENC_Hardreset(void)																// Mit Vorsicht zu genie�en, da die Register nicht immer zurueck gesetzt werden
{
	ENC_RST_LOW
	_delay_us(20000);																	// Min 400ns low
	ENC_RST_HIGH
	_delay_us(5000);																	// 50�S warten um sicherzustellen, das die PHY Register nicht beschrieben werden
}

void ENC_Bankjump(uint8_t Bank)
{
	if((ENC_Read_ETH_Register(ECON1_B0)&03)==Bank)
	{
		return;
	}
	uint8_t Register=Bank|(ENC_Read_ETH_Register(ECON1_B0)&0b11111100);					// Lese Statusregister aus maskiere die unteren 2 Bit aus und odere es mit der neuen Bank zusammen
	ENC_Write_ETH_Register(ECON1_B0,Register);
}

uint8_t ENC_Read_ETH_Register(uint8_t Register)
{
	uint8_t Returnwert=0;
	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock							// Wenn die default SPI Frequenz h�her als die maximal f�r den ENC zul�ssig ist, passe die Clock an
		# if F_CPU/2 <= ENC_SPI_Max_Clock
			SPI_Init(_2);
		#elif F_CPU/4 <= ENC_SPI_Max_Clock
			SPI_Init(_4);
		#elif F_CPU/8 <= ENC_SPI_Max_Clock
			SPI_Init(_8);
		#elif F_CPU/16 <= ENC_SPI_Max_Clock
			SPI_Init(_16);
		#elif F_CPU/32 <= ENC_SPI_Max_Clock
			SPI_Init(_32);
		#elif F_CPU/64 <= ENC_SPI_Max_Clock
			SPI_Init(_64);
		#elif F_CPU/128 <= ENC_SPI_Max_Clock
			SPI_Init(_128);
		#elif F_CPU/256 <= ENC_SPI_Max_Clock
			SPI_Init(_256);
		#else
		# error "ENC_Read_ETH_Register Clock konnte nicht aufgel�st werden"
		#endif
	#endif
	ENC_CS_LOW;
	_delay_us(1);
	SPI_Read_Write_Byte(ENC_Read_Control_Register|Register);
	Returnwert = SPI_Read_Write_Byte(0xff);
	_delay_us(1);
	ENC_CS_HIGH
	_delay_us(1);																		// Warte noch kurz nachdem der Pin auf High gezogen wurde
	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock							// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
		SPI_Init(SPI_Clockdiv_default);
	#endif
	return Returnwert;
}

void ENC_Write_ETH_Register(uint8_t Register,uint8_t Data)
{
	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock							// Wenn die default SPI Frequenz h�her als die maximal f�r den ENC zul�ssig ist, passe die Clock an
		# if F_CPU/2 <= ENC_SPI_Max_Clock
			SPI_Init(_2);
		#elif F_CPU/4 <= ENC_SPI_Max_Clock
			SPI_Init(_4);
		#elif F_CPU/8 <= ENC_SPI_Max_Clock
			SPI_Init(_8);
		#elif F_CPU/16 <= ENC_SPI_Max_Clock
			SPI_Init(_16);
		#elif F_CPU/32 <= ENC_SPI_Max_Clock
			SPI_Init(_32);
		#elif F_CPU/64 <= ENC_SPI_Max_Clock
			SPI_Init(_64);
		#elif F_CPU/128 <= ENC_SPI_Max_Clock
			SPI_Init(_128);
		#elif F_CPU/256 <= ENC_SPI_Max_Clock
			SPI_Init(_256);
		#else
		# error "ENC_Read_ETH_Register Clock konnte nicht aufgel�st werden"
		#endif
	#endif
	ENC_CS_LOW
	_delay_us(1);
	SPI_Read_Write_Byte(ENC_Write_Control_Register|Register);
	SPI_Read_Write_Byte(Data);
	_delay_us(1);
	ENC_CS_HIGH
	_delay_us(1);																		// Warte noch kurz nachdem der Pin auf High gezogen wurde
	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock							// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
		SPI_Init(SPI_Clockdiv_default);
	#endif
}

uint8_t ENC_Read_MAC_MII_Register(uint8_t Register)
{
	uint8_t Returnwert=0;
	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock							// Wenn die default SPI Frequenz h�her als die maximal f�r den ENC zul�ssig ist, passe die Clock an
		# if F_CPU/2 <= ENC_SPI_Max_Clock
			SPI_Init(_2);
		#elif F_CPU/4 <= ENC_SPI_Max_Clock
			SPI_Init(_4);
		#elif F_CPU/8 <= ENC_SPI_Max_Clock
			SPI_Init(_8);
		#elif F_CPU/16 <= ENC_SPI_Max_Clock
			SPI_Init(_16);
		#elif F_CPU/32 <= ENC_SPI_Max_Clock
			SPI_Init(_32);
		#elif F_CPU/64 <= ENC_SPI_Max_Clock
			SPI_Init(_64);
		#elif F_CPU/128 <= ENC_SPI_Max_Clock
			SPI_Init(_128);
		#elif F_CPU/256 <= ENC_SPI_Max_Clock
			SPI_Init(_256);
		#else
		# error "ENC_Read_ETH_Register Clock konnte nicht aufgel�st werden"
		#endif
	#endif
	ENC_CS_LOW
	_delay_us(1);
	SPI_Read_Write_Byte(ENC_Read_Control_Register|Register);
	SPI_Read_Write_Byte(0xff);															// Dummybyte
	Returnwert = SPI_Read_Write_Byte(0xff);
	_delay_us(1);
	ENC_CS_HIGH
	_delay_us(1);																		// Warte noch kurz nachdem der Pin auf High gezogen wurde
	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock							// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
		SPI_Init(SPI_Clockdiv_default);
	#endif
	return Returnwert;
}

void ENC_Write_MAC_MII_Register(uint8_t Register,uint8_t Data)
{
	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock							// Wenn die default SPI Frequenz h�her als die maximal f�r den ENC zul�ssig ist, passe die Clock an
		# if F_CPU/2 <= ENC_SPI_Max_Clock
			SPI_Init(_2);
		#elif F_CPU/4 <= ENC_SPI_Max_Clock
			SPI_Init(_4);
		#elif F_CPU/8 <= ENC_SPI_Max_Clock
			SPI_Init(_8);
		#elif F_CPU/16 <= ENC_SPI_Max_Clock
			SPI_Init(_16);
		#elif F_CPU/32 <= ENC_SPI_Max_Clock
			SPI_Init(_32);
		#elif F_CPU/64 <= ENC_SPI_Max_Clock
			SPI_Init(_64);
		#elif F_CPU/128 <= ENC_SPI_Max_Clock
			SPI_Init(_128);
		#elif F_CPU/256 <= ENC_SPI_Max_Clock
			SPI_Init(_256);
		#else
		# error "ENC_Read_ETH_Register Clock konnte nicht aufgel�st werden"
		#endif
	#endif
	ENC_CS_LOW
	_delay_us(1);
	SPI_Read_Write_Byte(ENC_Write_Control_Register|Register);
	SPI_Read_Write_Byte(Data);
	_delay_us(1);
	ENC_CS_HIGH
	_delay_us(1);																			// Warte noch kurz nachdem der Pin auf High gezogen wurde
	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock								// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
		SPI_Init(SPI_Clockdiv_default);
	#endif
}

void ENC_Softreset(void)
{
	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock								// Wenn die default SPI Frequenz h�her als die maximal f�r den ENC zul�ssig ist, passe die Clock an
		# if F_CPU/2 <= ENC_SPI_Max_Clock
			SPI_Init(_2);
		#elif F_CPU/4 <= ENC_SPI_Max_Clock
			SPI_Init(_4);
		#elif F_CPU/8 <= ENC_SPI_Max_Clock
			SPI_Init(_8);
		#elif F_CPU/16 <= ENC_SPI_Max_Clock
			SPI_Init(_16);
		#elif F_CPU/32 <= ENC_SPI_Max_Clock
			SPI_Init(_32);
		#elif F_CPU/64 <= ENC_SPI_Max_Clock
			SPI_Init(_64);
		#elif F_CPU/128 <= ENC_SPI_Max_Clock
			SPI_Init(_128);
		#elif F_CPU/256 <= ENC_SPI_Max_Clock
			SPI_Init(_256);
		#else
		# error "ENC_Read_ETH_Register Clock konnte nicht aufgel�st werden"
		#endif
	#endif
	ENC_CS_LOW
	_delay_us(1);
	SPI_Read_Write_Byte(0xff);
	_delay_us(1);
	ENC_CS_HIGH
	_delay_us(2000);
	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock								// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
		SPI_Init(SPI_Clockdiv_default);
	#endif
}

uint16_t ENC_Read_PHY_Register(uint8_t Register)
{
	ENC_Bankjump(ENC_Bank2);
	ENC_Write_MAC_MII_Register(MIREGADR_B2,Register);			// Trage die Adresse des zu lesenden Registers ein
	ENC_Write_MAC_MII_Register(MICMD_B2,0x01);					// Setze das Bit fuers lesen
	
	ENC_Bankjump(ENC_Bank3);
	do
	{
		_delay_us(11);
	}
	while ((ENC_Read_MAC_MII_Register(MISTAT_B3)&0x01)!=0x00);	// Solange der ENC die Werte eintraegt warte die Zeit ab
	
	ENC_Bankjump(ENC_Bank2);
	ENC_Write_MAC_MII_Register(MICMD_B2,0x00);					// Loesche das Bit fuers lesen
	
	ENC_Bankjump(ENC_Bank2);
	return (ENC_Read_MAC_MII_Register(MIRDH_B2)<<8) | ENC_Read_MAC_MII_Register(MIRDL_B2);
}

void ENC_Write_PHY_Register(uint8_t Register,uint16_t Data)
{
	ENC_Bankjump(ENC_Bank2);
	ENC_Write_MAC_MII_Register(MIREGADR_B2,Register);			// Trage die Adresse des zu schreiben Registers ein
		
	ENC_Write_MAC_MII_Register(MIWRL_B2,Data & 0xff);
	ENC_Write_MAC_MII_Register(MIWRH_B2,(Data>>8) & 0xff);		// Beim schreiben in dieses Register beginnt der Prozess mit der uebertragung in das PHY Register
	
	ENC_Bankjump(ENC_Bank3);
	do
	{
		_delay_us(11);
	}
	while ((ENC_Read_MAC_MII_Register(MISTAT_B3)&0x01)!=0x00);	// Solange der ENC die Werte eintraegt warte die Zeit ab
}

void ENC_Clear_Readbuffer(void)
{
	memset(&gl_ENC_Readbuffer[0],0x00,sizeof(gl_ENC_Readbuffer));
}

void ENC_Clear_Writebuffer(void)
{
	memset(&gl_ENC_Write_Datapayload[0],0x00,sizeof(gl_ENC_Write_Datapayload));
}

uint8_t ENC_Read_Hardwareversion(void)
{
	ENC_Bankjump(ENC_Bank3);
	return ENC_Read_ETH_Register(EREVID_B3);
}

uint8_t ENC_Init(void)													// Receive im unteren Speicher des Buffer, Vollduplex, KEIN HARDWARE DMA!!!
{	
	uint8_t Timeout=0;
	char Buffer[20];
	ENC_Hardreset();

	FAT32_Directory_Change("/");
	INI_Read_Key_String("basic.ini","Netz","MAC",&Buffer[0]);			// Lese die MAC als String aus der basic.ini
	sscanf(&Buffer[0],"%02hx:%02hx:%02hx:%02hx:%02hx:%02hx",(uint16_t *)&gl_Webserver.MAC[0],(uint16_t *)&gl_Webserver.MAC[1],(uint16_t *)&gl_Webserver.MAC[2],(uint16_t *)&gl_Webserver.MAC[3],(uint16_t *)&gl_Webserver.MAC[4],(uint16_t *)&gl_Webserver.MAC[5]);	// Scan hex address

	while((ENC_Read_ETH_Register(ESTAT_B1)&0x01)!=0x01)							// Pruefe ob die Clock auf dem ENC laeuft
	{
		if (Timeout==255)
		{
			return 1;
		}
		_delay_us(1000);
		Timeout++;
	}
	
	ENC_Bankjump(ENC_Bank0);													// Springe zur Bank 0
	ENC_Write_ETH_Register(ERXSTL_B0,0x00);										// Trage den untersten Speicherplatz fuer den Receivebuffer ein
	ENC_Write_ETH_Register(ERXSTH_B0,0x00);
	ENC_Write_ETH_Register(ERXNDL_B0,0xff);										// Trage den obersten Speicherplatz fuer den Receivebuffer ein
	ENC_Write_ETH_Register(ERXNDH_B0,0x0f);
	ENC_Write_ETH_Register(ERXRDPTL_B0,0x00);									// Setze den Lesepoiter auf die unterste SpeicherplatzAdresse
	ENC_Write_ETH_Register(ERXRDPTH_B0,0x00);
	ENC_Write_ETH_Register(ERDPTL_B0,0x00);										// Setze den Lesepoiter auf die unterste SpeicherplatzAdresse
	ENC_Write_ETH_Register(ERDPTH_B0,0x00);
	
	ENC_Bankjump(ENC_Bank1);													// Springe zu Bank 1
	ENC_Write_ETH_Register(ERXFCON_B1,0b10100001);								// Filter auf OR, Broadcastpackets erlauben, CRC Pruefung erlaubt
	
	ENC_Bankjump(ENC_Bank2);
	ENC_Write_MAC_MII_Register(MACON2_B2,0b00000000);							// Loesche das MARST Bit
	ENC_Write_MAC_MII_Register(MACON1_B2,0b00001101);							// MAC Receive an, Flowcontrol fuer Vollduplex an
	ENC_Write_MAC_MII_Register(MACON3_B2,0b00110101);							// Alle kurzen Frames werden auf 60 Bytes von der Hardware vergroe�ert, Transmit CRC erlaubt, Gro�e Frames erlaubt, Framelaenge wird nicht geprueft, Vollduplex Modus
	ENC_Write_PHY_Register(PHCON1,0x0100);										// Setze das Bit in PHY fuer Vollduplex
	ENC_Bankjump(ENC_Bank2);
	ENC_Write_MAC_MII_Register(MACON4_B2,0b01000000);							// F�r IEEE 802.3 Konformit�t setze DEFER Bit (Endloses Warten, falls Medium besetzt ist)
	ENC_Write_MAC_MII_Register(MAMXFLL_B2,0xee);								// Maximale Framelength auf 1518 Bytes
	ENC_Write_MAC_MII_Register(MAMXFLH_B2,0x05);	
	ENC_Write_MAC_MII_Register(MABBIPG_B2,0x15);								// Wert aus Datenblatt uebernommen
	ENC_Write_MAC_MII_Register(MAIPGL_B2,0x12);									// Wert aus Datenblatt uebernommen
	ENC_Write_MAC_MII_Register(MAIPGH_B2,0x0c);									// Wert aus Datenblatt uebernommen
	// Keine aenderung in MACLCON1 und 2 wegen Fullduplex
	ENC_Bankjump(ENC_Bank3);
	ENC_Write_MAC_MII_Register(MAADR0_B3,gl_Webserver.MAC[5]);					// Programmiere die MacAdresse ein
	ENC_Write_MAC_MII_Register(MAADR1_B3,gl_Webserver.MAC[4]);
	ENC_Write_MAC_MII_Register(MAADR2_B3,gl_Webserver.MAC[3]);
	ENC_Write_MAC_MII_Register(MAADR3_B3,gl_Webserver.MAC[2]);
	ENC_Write_MAC_MII_Register(MAADR4_B3,gl_Webserver.MAC[1]);
	ENC_Write_MAC_MII_Register(MAADR5_B3,gl_Webserver.MAC[0]);
	
	// Keine aenderung in PHY
	ENC_Write_PHY_Register(PHLCON,0b0000010001110010);							// Eine LED fuer Linkactivity, eine LED als TX und RX Anzeige
	
	ENC_Bankjump(ENC_Bank0);
	ENC_Write_ETH_Register(EIE_B0, 0b11010000);									// INTIE, PKTIE und LINKIE an (Bei Empfang von Paket oder Linkabbruch wird Interrupt per INT Pin ausgel�st)
	ENC_Write_ETH_Register(ECON1_B0,0b00000100);								// RX enable erst hier unten, niemals vor der Einstellung der Start und Endpointer vom Read-Speicherbereich
	
	gl_ENC_Nextpacketpointer=0x0000;											// Zeiger fuer das erste empfangene Packet liegt auf der RX Startadreses 0x0000
	ENC_Clear_Readbuffer();
	ENC_Clear_Writebuffer();
	#ifdef ENC_Debug
		USART_Write_String("ENC: Init erfolgreich\r\n");
	#endif
	return 0;
}

uint8_t ENC_Send_Packet(char MAC[6],uint16_t Type_Length,uint16_t Start, uint16_t Datalength)				// Momentan noch ohne Interrupt
{
	uint16_t Ende = Start + Datalength +14, Stallcounter=0;				// EndAdresse ist richtig mit itoa getestet, Stallcounter resetet die Transmitlogic
	char Statusvektor[7];
		
	ENC_Bankjump(ENC_Bank0);
	ENC_Write_ETH_Register(ETXSTL_B0,(Start&0xff));								// Trage den untersten Speicherplatz fuer den Transmitbuffer ein
	ENC_Write_ETH_Register(ETXSTH_B0,((Start>>8)&0xff));
	
	ENC_Write_ETH_Register(EWRPTL_B0,(Start&0xff));								// Setze den Schreibpointer auf die StartAdresse
	ENC_Write_ETH_Register(EWRPTH_B0,((Start>>8)&0xff));

	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock					// Wenn die default SPI Frequenz h�her als die maximal f�r den ENC zul�ssig ist, passe die Clock an
		# if F_CPU/2 <= ENC_SPI_Max_Clock
			SPI_Init(_2);
		#elif F_CPU/4 <= ENC_SPI_Max_Clock
			SPI_Init(_4);
		#elif F_CPU/8 <= ENC_SPI_Max_Clock
			SPI_Init(_8);
		#elif F_CPU/16 <= ENC_SPI_Max_Clock
			SPI_Init(_16);
		#elif F_CPU/32 <= ENC_SPI_Max_Clock
			SPI_Init(_32);
		#elif F_CPU/64 <= ENC_SPI_Max_Clock
			SPI_Init(_64);
		#elif F_CPU/128 <= ENC_SPI_Max_Clock
			SPI_Init(_128);
		#elif F_CPU/256 <= ENC_SPI_Max_Clock
			SPI_Init(_256);
		#else
		# error "ENC_Read_ETH_Register Clock konnte nicht aufgel�st werden"
		#endif
	#endif

	ENC_CS_LOW
	_delay_us(1);
	
	SPI_Read_Write_Byte(ENC_Write_Buffer_Memory);
	
	SPI_Read_Write_Byte(0x00);													// Configbyte fuer ENC, alles aus Config wird von ENC uebernommen
	
	SPI_Read_Write_Byte(MAC[0]);												// ZielAdresse
	SPI_Read_Write_Byte(MAC[1]);
	SPI_Read_Write_Byte(MAC[2]);
	SPI_Read_Write_Byte(MAC[3]);
	SPI_Read_Write_Byte(MAC[4]);
	SPI_Read_Write_Byte(MAC[5]);
	
	SPI_Read_Write_Byte(gl_Webserver.MAC[0]);									// UrsprungsAdresse
	SPI_Read_Write_Byte(gl_Webserver.MAC[1]);
	SPI_Read_Write_Byte(gl_Webserver.MAC[2]);
	SPI_Read_Write_Byte(gl_Webserver.MAC[3]);
	SPI_Read_Write_Byte(gl_Webserver.MAC[4]);
	SPI_Read_Write_Byte(gl_Webserver.MAC[5]);
	
	SPI_Read_Write_Byte((Type_Length>>8)&0xff);								// Typ/Laenge
	SPI_Read_Write_Byte(Type_Length & 0xff);
	
	for (uint16_t g=0;g<Datalength;g++)
	{
		SPI_Read_Write_Byte(gl_ENC_Write_Datapayload[g]);
	}
	
	#ifdef ENC_Debug
		USART_Write_X_Bytes(&gl_ENC_Write_Datapayload[0],0,Datalength);
	#endif
	
	_delay_us(1);
	ENC_CS_HIGH
	_delay_us(1);																// Warte noch kurz nachdem der Pin auf High gezogen wurde

	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock					// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
		SPI_Init(SPI_Clockdiv_default);
	#endif
	
	ENC_Write_ETH_Register(ETXNDL_B0,(Ende&0xff));								// Trage den obersten Speicherplatz fuer den Transmitbuffer ein
	ENC_Write_ETH_Register(ETXNDH_B0,((Ende>>8)&0xff));
	
	ENC_Write_ETH_Register(ECON1_B0,0b00001100);								// Starte transmit
	while(ENC_Read_ETH_Register(ECON1_B0)&0x08)									// Warte bis der Transmit fertig ist
	{
		if (Stallcounter==500)													// Nach 500ms resete die Transmitlogic
		{
			#ifdef ENC_Debug
				USART_Write_String("ENC: TRANSMITSTALL!!\r\n");
			#endif
			
			ENC_Write_ETH_Register(ECON1_B0,0b00000100);						// Stoppe transmit
			ENC_Write_ETH_Register(ECON1_B0,0b10000100);						// Resete Transmitlogic
			_delay_us(1000);
			ENC_Write_ETH_Register(ECON1_B0,0b00001100);						// Starte transmit erneut
			Stallcounter=0;
		}
		
		_delay_us(1000);
		Stallcounter++;
	}
	Ende++;																		// Erhoehe fuer die nachfolgenden Operationen die EndAdresse um 1
	
	ENC_Write_ETH_Register(ERDPTL_B0,(Ende&0xff));								// Trage den obersten Speicherplatz fuer den Transmitbuffer ein
	ENC_Write_ETH_Register(ERDPTH_B0,((Ende>>8)&0xff));
	
	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock					// Wenn die default SPI Frequenz h�her als die maximal f�r den ENC zul�ssig ist, passe die Clock an
		# if F_CPU/2 <= ENC_SPI_Max_Clock
			SPI_Init(_2);
		#elif F_CPU/4 <= ENC_SPI_Max_Clock
			SPI_Init(_4);
		#elif F_CPU/8 <= ENC_SPI_Max_Clock
			SPI_Init(_8);
		#elif F_CPU/16 <= ENC_SPI_Max_Clock
			SPI_Init(_16);
		#elif F_CPU/32 <= ENC_SPI_Max_Clock
			SPI_Init(_32);
		#elif F_CPU/64 <= ENC_SPI_Max_Clock
			SPI_Init(_64);
		#elif F_CPU/128 <= ENC_SPI_Max_Clock
			SPI_Init(_128);
		#elif F_CPU/256 <= ENC_SPI_Max_Clock
			SPI_Init(_256);
		#else
		# error "ENC_Read_ETH_Register Clock konnte nicht aufgel�st werden"
		#endif
	#endif

	ENC_CS_LOW																	// Lese den Stausvektor aus und wandle direkt in Big Endian um
	_delay_us(1);
	SPI_Read_Write_Byte(ENC_Read_Buffer_Memory);
	Statusvektor[0]=SPI_Read_Write_Byte(0xff);
	Statusvektor[1]=SPI_Read_Write_Byte(0xff);
	Statusvektor[2]=SPI_Read_Write_Byte(0xff);
	Statusvektor[3]=SPI_Read_Write_Byte(0xff);
	Statusvektor[4]=SPI_Read_Write_Byte(0xff);
	Statusvektor[5]=SPI_Read_Write_Byte(0xff);
	Statusvektor[6]=SPI_Read_Write_Byte(0xff);
	_delay_us(1);
	ENC_CS_HIGH
	_delay_us(1);																// Warte noch kurz nachdem der Pin auf High gezogen wurde

	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock					// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
		SPI_Init(SPI_Clockdiv_default);
	#endif

	if ((Statusvektor[2]&0x80) && (!(Statusvektor[2]&0x10)))					// Transission done und kein CRC Fehler
	{
		#ifdef ENC_Debug
			USART_Write_String("ENC: Packet gesendet\r\n");
			USART_Write_Byte(Statusvektor[6]);
			USART_Write_Byte(Statusvektor[5]);
			USART_Write_Byte(Statusvektor[4]);
			USART_Write_Byte(Statusvektor[3]);
			USART_Write_Byte(Statusvektor[2]);
			USART_Write_Byte(Statusvektor[1]);
			USART_Write_Byte(Statusvektor[0]);
			USART_Write_String("\r\n");
		#endif
		return 0;
	}
	else if(Statusvektor[3] & 0x20)												// Late Collision
	{
		#ifdef ENC_Debug
			USART_Write_String("ENC: Packet Late Collision\r\n");
		#endif
		return 1;
	}
	else
	{
		#ifdef ENC_Debug
			USART_Write_String("ENC: Packet konnte nicht gesendet werden\r\n");
		#endif
		return 1;
	}
}

uint8_t ENC_Read_Next_Packet(void)
{
	uint8_t Statusvektor[4];
	uint16_t Buffercounter=0, Receivebuffercounter=0,RXBufferEnd=0, RXBufferStart=0,Nextpacketstart=0;
	#ifdef ENC_Debug
		uint16_t buffstart=0;
	#endif
	uint8_t Register=0, Lowbyte=0;
	
	ENC_Clear_Readbuffer();
	
	ENC_Bankjump(ENC_Bank0);
	RXBufferStart = (ENC_Read_ETH_Register(ERXSTH_B0) <<8)|ENC_Read_ETH_Register(ERXSTL_B0);
	RXBufferEnd = (ENC_Read_ETH_Register(ERXNDH_B0) <<8)|ENC_Read_ETH_Register(ERXNDL_B0);
	
	#ifdef ENC_Debug
		buffstart=gl_ENC_Nextpacketpointer;
	#endif
	
	ENC_Write_ETH_Register(ERDPTL_B0,(gl_ENC_Nextpacketpointer&0xff));				// Setze den aktuellen Nextpacketpointer
	ENC_Write_ETH_Register(ERDPTH_B0,((gl_ENC_Nextpacketpointer>>8)&0xff));
	
	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock					// Wenn die default SPI Frequenz h�her als die maximal f�r den ENC zul�ssig ist, passe die Clock an
		# if F_CPU/2 <= ENC_SPI_Max_Clock
			SPI_Init(_2);
		#elif F_CPU/4 <= ENC_SPI_Max_Clock
			SPI_Init(_4);
		#elif F_CPU/8 <= ENC_SPI_Max_Clock
			SPI_Init(_8);
		#elif F_CPU/16 <= ENC_SPI_Max_Clock
			SPI_Init(_16);
		#elif F_CPU/32 <= ENC_SPI_Max_Clock
			SPI_Init(_32);
		#elif F_CPU/64 <= ENC_SPI_Max_Clock
			SPI_Init(_64);
		#elif F_CPU/128 <= ENC_SPI_Max_Clock
			SPI_Init(_128);
		#elif F_CPU/256 <= ENC_SPI_Max_Clock
			SPI_Init(_256);
		#else
		# error "ENC_Read_ETH_Register Clock konnte nicht aufgel�st werden"
		#endif
	#endif

	ENC_CS_LOW																		// Lese den neuen Nextpacketpointer aus
	_delay_us(1);
	SPI_Read_Write_Byte(ENC_Read_Buffer_Memory);
	
	Lowbyte = SPI_Read_Write_Byte(0xff);
	Nextpacketstart = (SPI_Read_Write_Byte(0xff)<<8) | Lowbyte;
	
	Statusvektor[3]=SPI_Read_Write_Byte(0xff);										// Statusvektor auslesen
	Statusvektor[2]=SPI_Read_Write_Byte(0xff);
	Statusvektor[1]=SPI_Read_Write_Byte(0xff);
	Statusvektor[0]=SPI_Read_Write_Byte(0xff);
	_delay_us(1);
	ENC_CS_HIGH
	_delay_us(1);																	// Warte noch kurz nachdem der Pin auf High gezogen wurde

	#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock						// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
		SPI_Init(SPI_Clockdiv_default);
	#endif

	gl_ENC_Nextpacketpointer+=6;													// Passe den Pointer fuer das direkte Packet an
	Buffercounter=gl_ENC_Nextpacketpointer;											// Buffercounter Vorladen
	
	if (((Statusvektor[1]&0x80)) && (!(Statusvektor[1]&0x10)))						// Empfang ist erfolgt und kein CRC Fehler sonst abbruch
	{
		#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock					// Wenn die default SPI Frequenz h�her als die maximal f�r den ENC zul�ssig ist, passe die Clock an
			# if F_CPU/2 <= ENC_SPI_Max_Clock
				SPI_Init(_2);
			#elif F_CPU/4 <= ENC_SPI_Max_Clock
				SPI_Init(_4);
			#elif F_CPU/8 <= ENC_SPI_Max_Clock
				SPI_Init(_8);
			#elif F_CPU/16 <= ENC_SPI_Max_Clock
				SPI_Init(_16);
			#elif F_CPU/32 <= ENC_SPI_Max_Clock
				SPI_Init(_32);
			#elif F_CPU/64 <= ENC_SPI_Max_Clock
				SPI_Init(_64);
			#elif F_CPU/128 <= ENC_SPI_Max_Clock
				SPI_Init(_128);
			#elif F_CPU/256 <= ENC_SPI_Max_Clock
				SPI_Init(_256);
			#else
			# error "ENC_Read_ETH_Register Clock konnte nicht aufgel�st werden"
			#endif
		#endif

		ENC_CS_LOW																	// Lese den neuen Nextpacketpointer aus
		_delay_us(1);
		SPI_Read_Write_Byte(ENC_Read_Buffer_Memory);

		while (Buffercounter != Nextpacketstart)
		{
			gl_ENC_Readbuffer[Receivebuffercounter] = SPI_Read_Write_Byte(0xff);
			#ifdef ENC_Debug
				USART_Write_Byte(gl_ENC_Readbuffer[Receivebuffercounter]);
			#endif
			Buffercounter++;
			Receivebuffercounter++;
			
			if(Buffercounter > RXBufferEnd)											// Wenn das Ende des RX Ramsspeicher erreicht wurde, springe auf die StartAdresse zurueck (Ringbuffer)
			{
				Buffercounter = RXBufferStart;
			}
		}
		_delay_us(1);
		ENC_CS_HIGH
		_delay_us(1);																// Warte noch kurz nachdem der Pin auf High gezogen wurde

		#if (F_CPU/SPI_Default_Clockdivider) >= ENC_SPI_Max_Clock					// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
			SPI_Init(SPI_Clockdiv_default);
		#endif

		// if(Nextpacketstart-1 > Bufferend || Nextpacketstart-1 < Bufferstart )
		//if((Nextpacketstart-1) > 0x0fff || (Nextpacketstart-1) < 0x0000 )			// Wenn 0x0000-1 = 0xffff > 0x0fff(Ende Lesebuffer), dann setze auf Lesebuffer; zweites Statement ist hier sinnfrei, weil ja 0x0000
		if((Nextpacketstart-1) > 0x0fff)											// Wenn 0x0000-1 = 0xffff > 0x0fff(Ende Lesebuffer), dann setze auf Lesebuffer
		{
			ENC_Write_ETH_Register(ERXRDPTL_B0,0xff);								// Markiere nach dem lesen den Speicherbereich als frei
			ENC_Write_ETH_Register(ERXRDPTH_B0,0x0f);
		}
		else 
		{
			if(Nextpacketstart%2==0)												// Falls Adresse gerade ist speicher eine ungerade ein
			{
				ENC_Write_ETH_Register(ERXRDPTL_B0,(Nextpacketstart-1) & 0xff);		// Markiere nach dem lesen den Speicherbereich als frei
				ENC_Write_ETH_Register(ERXRDPTH_B0,((Nextpacketstart-1)>>8) & 0xff);
			}
			else
			{
				ENC_Write_ETH_Register(ERXRDPTL_B0,Nextpacketstart & 0xff);			// Markiere nach dem lesen den Speicherbereich als frei
				ENC_Write_ETH_Register(ERXRDPTH_B0,(Nextpacketstart>>8) & 0xff);	
			}
		}

		gl_ENC_Nextpacketpointer = Nextpacketstart;									// Setze die Adresse fuer das neue Packet
		
		Register=0x40|(ENC_Read_ETH_Register(ECON2_B0));							// Setze PKTDEC um das EPKTCNT um 1 zu verringern und zu zeigen das dieses Packet gelsen wurde
		ENC_Write_ETH_Register(ECON2_B0,Register);
		
		#ifdef ENC_Debug
			printf("\r\nPacketStart: %04x, NextPackPointer: %04x, Nextpacketstart: %04x\r\n\r\n",buffstart,gl_ENC_Nextpacketpointer,Nextpacketstart);
		#endif	
		
		#ifdef ENC_Debug
			USART_Write_String("ENC: Packet gelesen\r\n");
		#endif
		return 0;

	}
	#ifdef ENC_Debug
		USART_Write_String("ENC: Fehler beim Lesen des Packetes\r\n");
	#endif
	return 1;
}

uint8_t ENC_Check_for_Packets(void)												// Gibt die Anzahl der zu lesenden Packet im Buffer wieder
{
	uint8_t g=0;
	volatile static uint8_t Counter=0;											// Rufe an und ab mal den Paketcounter ab, das Intsytem vom ENC ist nicht so zuverl�ssig

	if (Counter==100)															// Alle halbe Sekunde oder so den Interrupt triggern; 100 deswegen, da in der main loop while 5ms immer gewartet werden
	{
		gl_ENC_Interrupt_Pending=1;
	}

	if (gl_ENC_Interrupt_Pending)												// Wenn ein oder mehrere Pakete gelesen werden soll
	{
		//TODO Entfernt, da sonst die Steuerung laufend neustartet w�hrend des Boot
		/*if (ENC_Check_Link_Status())											// Wenn der Link unterbrochen wurde, Neustart
		{
			#ifdef ENC_Debug
				USART_Write_String("ENC: Linkfehler, Neustart!\r\n");
			#endif
			IWDG->KR = 0x0000cccc;												// Watchdogtimer einschalten
			while(1);															// Endlosschleife loest Watchdog aus
		}*/

		ENC_Bankjump(ENC_Bank1);
		g=ENC_Read_ETH_Register(EPKTCNT_B1);									// Frage nach wie viele es sind
		if(g)																	// Wenn noch welche da sind, gebe >0 zur�ck, sonst setze das INT bit zur�ck und gebe 0 zur�ck
		{
			Counter++;
			return g;
		}
		else
		{
			Counter++;
			gl_ENC_Interrupt_Pending=0;
			return 0;
		}
	}
	Counter++;
	return 0;
}

uint8_t ENC_Check_Frametype(uint16_t Type)							// Prueft was fuer ein Typ das Packet hat
{
	if ((gl_ENC_Readbuffer[12]==((Type>>8)&0xff))&&(gl_ENC_Readbuffer[13]==(Type&0xff)))
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

void ENC_Read_Header_from_Frame(struct ENC_Head *ENC_Head)
{
	memcpy(&ENC_Head->Dest_MAC[0], &gl_ENC_Readbuffer[0],6);
	memcpy(&ENC_Head->Source_MAC[0], &gl_ENC_Readbuffer[6],6);
	memcpy(&ENC_Head->Type, &gl_ENC_Readbuffer[12],2);
	#ifdef ENC_Debug
		USART_Write_String("ENC: Ethernetframe Header gelesen\r\n");
	#endif
}

void ENC_Init_Dump(void)
{
	ENC_Bankjump(ENC_Bank0);																	// Springe zur Bank 0
	printf("Soll Bank0, ist %02x\r\n",ENC_Read_ETH_Register(ECON1_B0)&0x03);
	printf("RXStart(0x0000): 0x%02x%02x\r\n",ENC_Read_ETH_Register(ERXSTH_B0),ENC_Read_ETH_Register(ERXSTL_B0));
	printf("RXEnd(0x0fff): 0x%02x%02x\r\n",ENC_Read_ETH_Register(ERXNDH_B0),ENC_Read_ETH_Register(ERXNDL_B0));
	printf("Aktueller RX Freigabepointer(0xXXXX): 0x%02x%02x\r\n",ENC_Read_ETH_Register(ERXRDPTH_B0),ENC_Read_ETH_Register(ERXRDPTL_B0));
	printf("Aktueller RX Lesepointer(0xXXXX): 0x%02x%02x\r\n",ENC_Read_ETH_Register(ERDPTH_B0),ENC_Read_ETH_Register(ERDPTL_B0));
	
	printf("TXStart(0x1000): 0x%02x%02x\r\n",ENC_Read_ETH_Register(ETXSTH_B0),ENC_Read_ETH_Register(ETXSTL_B0));
	printf("TXEnd(0xXXXX): 0x%02x%02x\r\n",ENC_Read_ETH_Register(ETXNDH_B0),ENC_Read_ETH_Register(ETXNDL_B0));
	printf("Aktueller TX Schreibpointer(0xXXXX): 0x%02x%02x\r\n",ENC_Read_ETH_Register(EWRPTH_B0),ENC_Read_ETH_Register(EWRPTL_B0));
	
	ENC_Bankjump(ENC_Bank1);																	// Springe zu Bank 1
	printf("Soll Bank1, ist %02x\r\n",ENC_Read_ETH_Register(ECON1_B0)&0x03);
	
	printf("Soll ERXFCON(0xa1), ist 0x%02x\r\n",ENC_Read_ETH_Register(ERXFCON_B1));		// Filter auf OR, Broadcastpackets erlauben, CRC Pruefung erlaubt
	printf("Soll CLKRDY(0x01), ist 0x%02x\r\n",ENC_Read_ETH_Register(ECON1_B0)&0x01);

	ENC_Bankjump(ENC_Bank2);
	printf("Soll Bank2, ist %02x\r\n",ENC_Read_ETH_Register(ECON1_B0)&0x03);
	
	printf("Soll MACON2(0x00), ist 0x%02x\r\n",ENC_Read_MAC_MII_Register(MACON2_B2));
	printf("Soll MACON1(0x0d), ist 0x%02x\r\n",ENC_Read_MAC_MII_Register(MACON1_B2));
	printf("Soll MACON3(0x35), ist 0x%02x\r\n",ENC_Read_MAC_MII_Register(MACON3_B2));
	printf("Soll MACON4(0x40), ist 0x%02x\r\n",ENC_Read_MAC_MII_Register(MACON4_B2));
	printf("Soll PHCON1(0x0100), ist 0x%04x\r\n",ENC_Read_PHY_Register(PHCON1));
	// Keine aenderung in MACON4
	ENC_Bankjump(ENC_Bank2);
	printf("Soll Bank2, ist %02x\r\n",ENC_Read_ETH_Register(ECON1_B0)&0x03);
	
	printf("Maximale Framelaenge(0x05ee): 0x%02x%02x\r\n",ENC_Read_MAC_MII_Register(MAMXFLH_B2),ENC_Read_MAC_MII_Register(MAMXFLL_B2));
	printf("Soll MABBIPG(0x15), ist 0x%02x\r\n",ENC_Read_MAC_MII_Register(MABBIPG_B2));
	printf("MAIPGL(0x0c12): 0x%02x%02x\r\n",ENC_Read_MAC_MII_Register(MAIPGH_B2),ENC_Read_MAC_MII_Register(MAIPGL_B2));
	
	ENC_Bankjump(ENC_Bank3);
	printf("Soll Bank3, ist %02x\r\n",ENC_Read_ETH_Register(ECON1_B0)&0x03);
	
	printf("Soll MAADR0(0x%02x), ist 0x%02x\r\n",gl_Webserver.MAC[5],ENC_Read_MAC_MII_Register(MAADR0_B3));
	printf("Soll MAADR1(0x%02x), ist 0x%02x\r\n",gl_Webserver.MAC[4],ENC_Read_MAC_MII_Register(MAADR1_B3));
	printf("Soll MAADR2(0x%02x), ist 0x%02x\r\n",gl_Webserver.MAC[3],ENC_Read_MAC_MII_Register(MAADR2_B3));
	printf("Soll MAADR3(0x%02x), ist 0x%02x\r\n",gl_Webserver.MAC[2],ENC_Read_MAC_MII_Register(MAADR3_B3));
	printf("Soll MAADR4(0x%02x), ist 0x%02x\r\n",gl_Webserver.MAC[1],ENC_Read_MAC_MII_Register(MAADR4_B3));
	printf("Soll MAADR5(0x%02x), ist 0x%02x\r\n",gl_Webserver.MAC[0],ENC_Read_MAC_MII_Register(MAADR5_B3));

	printf("Soll PHLCON(0x0472), ist 0x%04x\r\n",ENC_Read_PHY_Register(PHLCON));

	ENC_Bankjump(ENC_Bank0);
	printf("Soll Bank0, ist %02x\r\n",ENC_Read_ETH_Register(ECON1_B0)&0x03);
	printf("Soll EIE(0xd0), ist 0x%02x\r\n",ENC_Read_ETH_Register(EIE_B0));
	printf("Soll ECNON1 RX an(0x04), ist 0x%02x\r\n",ENC_Read_ETH_Register(ECON1_B0)&0b00000100);
}

uint8_t ENC_Check_Link_Status (void)
{
	if((ENC_Read_PHY_Register(PHSTAT1)&0x0004))								// Warte bis der Link stabil ist
	{
		return 0;
	}
	return 1;
}

void ENC_LED_Test(void)
{
	ENC_Write_PHY_Register(PHLCON,0b0000101110110010);						// Beide LEDs blinken langsam
}

