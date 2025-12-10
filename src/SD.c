// Includes
#include "main.h"
#include "SD.h"
#include "stm32f4xx.h"
#include "SPI.h"
#include <string.h>
#ifdef SD_Debug
	#include <stdio.h>
	#include "USART.h"
#endif
#include "Allerlei.h"


/*

************************************************************************************************
* Header fuer das Ansteuern einer SD-Karte ueber den SPI auf dem STM32F401RET6				   *
* 2019 - 2020 � Frederinn													 	  	   *
************************************************************************************************

*/

// Globale Variablen
struct SD_Card gl_SD_Card;																	// Struct fuer alle globalen Variablen der SD Karte
struct MBR gl_SD_MBR;																		// MBR einer SD Karte

void SD_Portinit(void)
{
	RCC->AHB1ENR |= SD_RCC_IOPENR;															// Port A Clock an
	SD_OUT |= (1<<SD_CS);																	// Pin erstmal Ausgang high
	SD_DIR = (SD_DIR & ~(0b11 << (SD_CS*2))) | (0b01 << (SD_CS*2));							// Maskiere die alte Pinfunktion raus und setze auf Ausgang
	SD_GPIO->OSPEEDR = (SD_GPIO->OSPEEDR & ~((0b11 << (SD_CS*2)))) | (0b10 << (SD_CS*2));	// High Speed

	#ifdef Use_Carddetect
		SD_OUT |= (1<<CARD_SD);																// SS auf 1, Pullup fuer Karten detektieren auf 1
	#endif
}

#ifdef Use_Carddetect
uint8_t SD_Card_Check_if_present(void)
{
	if (SD_IN & (1<<CARD_SD))
	{
		#ifdef SD_Debug
			USART_Write_String("SD: Karte nicht eingesteckt\r\n");
		#endif
		return 1;
	}
	return 0;
}
#endif

extern uint8_t SD_Card_mount(void)															// Bindet die SD Karte ein
{
	uint8_t isHC=0;

	if (SD_Card_CMD0() == 0)																// Reset
	{
		if (SD_Card_CMD8()==0)																// Wenn akzeptiert, ist die Karte >= SDV2; Muss vor ACMD41 ausgef�hrt werden
		{
			for(uint8_t g=0;g<10;g++)
			{
				SD_Card_CMD55();															// Kommt vor einem ACMD
				if (SD_Card_ACMD41()==0)													// SD Karte Init, wenn erfolgreich, gehe raus
				{
					gl_SD_Card.Initstate = 1;												// Karte initialisert
					_delay_us(300000);														// Warte noch mal kurz nach der Init, das die Karte ihren Shit together bekommt

					isHC = SD_Card_CMD58();													// Pr�fe was f�r eine Karte es ist
					if (isHC==1) return 1;													// Bei Problem breche ab
					if (isHC==0) gl_SD_Card.isHCXD = 0;										// Karte ist keine HCXD
					if (isHC==2) gl_SD_Card.isHCXD = 1;										// Karte ist HCXD
					return 0;
				}
				_delay_us(300000);
			}
			gl_SD_Card.Initstate = 0;														// Karte nicht initialisert
			return 1;
		}
		else																				// SDV1
		{
			for(uint8_t g=0;g<10;g++)
			{
				if (SD_Card_CMD1()==0)														// SD Karte Init, wenn erfolgreich, gehe raus
				{
					gl_SD_Card.isHCXD = 0;													// Bei SD oder MMCs gibts keine HC oder XDs
					gl_SD_Card.Initstate = 1;												// Karte initialisert
					_delay_us(300000);														// Warte noch mal kurz nach der Init, das die Karte ihren Shit together bekommt
					return 0;
				}
				_delay_us(300000);
			}
			gl_SD_Card.Initstate = 0;														// Karte nicht initialisert
			return 1;
		}
	}
	return 1;
}

uint8_t SD_Card_CMD0(void)																	// Reset die SD-Karte	return 0=erfolgreich, 1=fehlerhaft
{
	uint8_t Response;																		// Antwort von SD-Karte

	#if (F_CPU_Init/256UL) > 400000UL
	# error "F_CPU_Init ist zu hoch um die SD_Karte reseten zu k�nnen"
	#else
		SPI_Init(_256);
	#endif

	#ifdef Use_Carddetect
		if (SD_Card_Check_if_present())														// Wenn die Karte nicht eingesteckt ist, dann beende die Funktion
		{
			SPI_Init(SPI_Clockdiv_default);													// Stelle wieder auf die Defaultgeschwindigkeit um
			return 1;
		}
	#endif

	SD_OUT |= (1<<SD_CS);																	// SS auf 1

	_delay_us_init(1000);

	for(uint8_t i=0;i<10;i++)																// 74+ Clocks
	{
		SPI_Read_Write_Byte(0xff);
	}

	_delay_us_init(1000);																	// Warte nach den Clocks kurz

	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SD_OUT &= ~(1<<SD_CS);																	// SS auf 0
	_delay_us_init(1);

	SPI_Read_Write_Byte(0x40);																// Command (CMD0)
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x95);

	//_delay_us_init(1000);																	// Warte nach dem Commando 1ms ab

	for(uint8_t i=0;i<10;i++)
	{
		Response = SPI_Read_Write_Byte(0xff);
		if (Response == 0x01)
		{
			_delay_us_init(1);
			SD_OUT |= (1<<SD_CS);
			_delay_us_init(1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};											// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: Karte gefunden\r\n");
			#endif

			SPI_Init(SPI_Clockdiv_default);													// Stelle wieder auf die Defaultgeschwindigkeit um
			return 0;
		}
		_delay_us_init(10);
		#ifdef SD_Debug
			USART_Write_String("SD: Pruefe ob die Karte eingesteckt ist...\r\n");
		#endif
	}
	_delay_us_init(1);
	SD_OUT |= (1<<SD_CS);
	_delay_us_init(1);
	while(SPI_Read_Write_Byte(0xff)!=0xff){};													// Clock bis Tri-State

	#ifdef SD_Debug
		USART_Write_String("SD: Karte nicht gefunden\r\n");
	#endif

	SPI_Init(SPI_Clockdiv_default);															// Stelle wieder auf die Defaultgeschwindigkeit um
	return 1;
}

uint8_t SD_Card_CMD8(void)
{
	#if (F_CPU_Init/256UL) > 400000UL
	# error "F_CPU_Init ist zu hoch um die SD_Karte initialisieren zu k�nnen"
	#else
		SPI_Init(_256);
	#endif

	#ifdef Use_Carddetect
	if (SD_Card_Check_if_present())														// Wenn die Karte nicht eingesteckt ist, dann beende die Funktion
	{
		SPI_Init(SPI_Clockdiv_default);													// Stelle wieder auf die Defaultgeschwindigkeit um
		return 1;
	}
	#endif

	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SD_OUT &= ~(1<<SD_CS);																// SS auf 0
	_delay_us_init(1);

	SPI_Read_Write_Byte(0x40 + 8);														// CMD8
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x01);
	SPI_Read_Write_Byte(0xaa);
	SPI_Read_Write_Byte(0x87);

	for(uint8_t z=0;z<10;z++)															// Zaehlschleife
	{
		if(SPI_Read_Write_Byte(0xff) == 0x01)
		{
			SPI_Read_Write_Byte(0xff);													// Lese restlichen Bytes der R7 Antwort
			SPI_Read_Write_Byte(0xff);
			SPI_Read_Write_Byte(0xff);
			SPI_Read_Write_Byte(0xff);

			_delay_us_init(1);
			SD_OUT |= (1<<SD_CS);
			_delay_us_init(1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: CMD8 Antwort\r\n");
			#endif
			SPI_Init(SPI_Clockdiv_default);												// Stelle wieder auf die Defaultgeschwindigkeit um
			return 0;																	// Funktion vorzeichtig verlassen wenn die Karte initialisiert ist
		}
		_delay_us_init(10);
		#ifdef SD_Debug
			USART_Write_String("SD: warte auf CMD8 Antwort...\r\n");
		#endif
	}
	_delay_us_init(1);
	SD_OUT |= (1<<SD_CS);
	_delay_us_init(1);
	while(SPI_Read_Write_Byte(0xff)!=0xff){};												// Clock bis Tri-State

	#ifdef SD_Debug
		USART_Write_String("SD: Keine Antwort auf CMD8\r\n");
	#endif
	SPI_Init(SPI_Clockdiv_default);														// Stelle wieder auf die Defaultgeschwindigkeit um
	return 1;																			// Funktion abrechen
}

uint8_t SD_Card_CMD58(void)																// Liest OCR Register aus, 1=Fehler, 0=Keine HCXD Karte, 2=HCXD Karte
{
	uint8_t g=0;
	uint32_t OCR=0;

	#if (F_CPU_Init/256UL) > 400000UL
	# error "F_CPU_Init ist zu hoch um die SD_Karte initialisieren zu k�nnen"
	#else
		SPI_Init(_256);
	#endif

	#ifdef Use_Carddetect
	if (SD_Card_Check_if_present())														// Wenn die Karte nicht eingesteckt ist, dann beende die Funktion
	{
		SPI_Init(SPI_Clockdiv_default);													// Stelle wieder auf die Defaultgeschwindigkeit um
		return 1;
	}
	#endif

	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SD_OUT &= ~(1<<SD_CS);																// SS auf 0
	_delay_us_init(1);

	SPI_Read_Write_Byte(0x40 + 58);														// CMD58
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0xff);

	for(uint8_t z=0;z<10;z++)															// Zaehlschleife
	{
		g = SPI_Read_Write_Byte(0xff);
		if(g == 0x01 || g == 0x00)														// Wenn Antwort erhalten
		{
			OCR |= SPI_Read_Write_Byte(0xff); OCR <<= 8;								// Lese R3 ein
			OCR |= (SPI_Read_Write_Byte(0xff) & 0xff); OCR <<= 8;
			OCR |= (SPI_Read_Write_Byte(0xff) & 0xff); OCR <<= 8;
			OCR |= (SPI_Read_Write_Byte(0xff) & 0xff);

			_delay_us_init(1);
			SD_OUT |= (1<<SD_CS);
			_delay_us_init(1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: CMD58 Antwort\r\n");
			#endif
			SPI_Init(SPI_Clockdiv_default);												// Stelle wieder auf die Defaultgeschwindigkeit um
			return (OCR & (1<<30)) ? 2 : 0;												// Funktion vorzeichtig verlassen wenn die Karte initialisiert ist
		}
		_delay_us_init(10);
		#ifdef SD_Debug
			USART_Write_String("SD: warte auf CMD58 Antwort...\r\n");
		#endif
	}
	_delay_us_init(1);
	SD_OUT |= (1<<SD_CS);
	_delay_us_init(1);
	while(SPI_Read_Write_Byte(0xff)!=0xff){};												// Clock bis Tri-State

	#ifdef SD_Debug
		USART_Write_String("SD: Keine Antwort auf CMD58\r\n");
	#endif
	SPI_Init(SPI_Clockdiv_default);														// Stelle wieder auf die Defaultgeschwindigkeit um
	return 1;																			// Funktion abrechen
}

uint8_t SD_Card_CMD55(void)
{
	#if (F_CPU_Init/256UL) > 400000UL
	# error "F_CPU_Init ist zu hoch um die SD_Karte initialisieren zu k�nnen"
	#else
		SPI_Init(_256);
	#endif

	#ifdef Use_Carddetect
	if (SD_Card_Check_if_present())														// Wenn die Karte nicht eingesteckt ist, dann beende die Funktion
	{
		SPI_Init(SPI_Clockdiv_default);													// Stelle wieder auf die Defaultgeschwindigkeit um
		return 1;
	}
	#endif

	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SD_OUT &= ~(1<<SD_CS);																// SS auf 0
	_delay_us_init(1);

	SPI_Read_Write_Byte(0x40 + 55);														// CMD55
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x65);															// CRC ist egal, hier aber eingetragen

	for(uint8_t z=0;z<10;z++)															// Zaehlschleife
	{
		uint8_t g=SPI_Read_Write_Byte(0xff);
		if(g==0x01 || g==0x00)
		{
			_delay_us_init(1);
			SD_OUT |= (1<<SD_CS);
			_delay_us_init(1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: CMD55 Antwort\r\n");
			#endif
			SPI_Init(SPI_Clockdiv_default);												// Stelle wieder auf die Defaultgeschwindigkeit um
			return 0;																	// Funktion vorzeichtig verlassen wenn die Karte initialisiert ist
		}
		_delay_us_init(10);
		#ifdef SD_Debug
			USART_Write_String("SD: warte auf CMD55 Antwort...\r\n");
		#endif
	}
	_delay_us_init(1);
	SD_OUT |= (1<<SD_CS);
	_delay_us_init(1);
	while(SPI_Read_Write_Byte(0xff)!=0xff){};												// Clock bis Tri-State

	#ifdef SD_Debug
		USART_Write_String("SD: Keine Antwort auf CMD55\r\n");
	#endif
	SPI_Init(SPI_Clockdiv_default);														// Stelle wieder auf die Defaultgeschwindigkeit um
	return 1;																			// Funktion abrechen
}

uint8_t SD_Card_ACMD41(void)															// Intialisiert die SD-Karte 5 Versuche fuer die Init
{
	#if (F_CPU_Init/256UL) > 400000UL
	# error "F_CPU_Init ist zu hoch um die SD_Karte initialisieren zu k�nnen"
	#else
		SPI_Init(_256);
	#endif

	#ifdef Use_Carddetect
	if (SD_Card_Check_if_present())														// Wenn die Karte nicht eingesteckt ist, dann beende die Funktion
	{
		SPI_Init(SPI_Clockdiv_default);													// Stelle wieder auf die Defaultgeschwindigkeit um
		return 1;
	}
	#endif

	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SD_OUT &= ~(1<<SD_CS);																// SS auf 0
	_delay_us_init(1);

	SPI_Read_Write_Byte(0x69);															// Init Command (ACMD41)
	SPI_Read_Write_Byte(0x40);															// Bit f�r HC Karten setzen
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x77);															// CRC wird ignoriert, hier aber trotzdem mal eingetragen

	for(uint8_t z=0;z<10;z++)															// Zaehlschleife
	{
		uint8_t r = SPI_Read_Write_Byte(0xff);
		if( r == 0x00)
		{
			_delay_us_init(1);
			SD_OUT |= (1<<SD_CS);
			_delay_us_init(1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: Karte initialisiert\r\n");
			#endif
			SPI_Init(SPI_Clockdiv_default);												// Stelle wieder auf die Defaultgeschwindigkeit um
			return 0;																	// Funktion vorzeichtig verlassen wenn die Karte initialisiert ist
		}
		if( r == 0x01)
		{
			_delay_us_init(1);
			SD_OUT |= (1<<SD_CS);
			_delay_us_init(1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: ACMD41 initialisiert noch\r\n");
			#endif
			SPI_Init(SPI_Clockdiv_default);												// Stelle wieder auf die Defaultgeschwindigkeit um
			return 1;																	// Funktion vorzeichtig verlassen wenn die Karte initialisiert ist
		}
		_delay_us_init(10);
		#ifdef SD_Debug
			USART_Write_String("SD: warte auf ACMD41 Antwort...\r\n");
		#endif
	}
	_delay_us_init(1);
	SD_OUT |= (1<<SD_CS);
	_delay_us_init(1);
	while(SPI_Read_Write_Byte(0xff)!=0xff){};												// Clock bis Tri-State

	#ifdef SD_Debug
		USART_Write_String("SD: ACMD41 fehlgeschlagen\r\n");
	#endif
	SPI_Init(SPI_Clockdiv_default);														// Stelle wieder auf die Defaultgeschwindigkeit um
	return 1;																			// Funktion abrechen
}

uint8_t SD_Card_CMD1(void)																// Intialisiert die SD-Karte 5 Versuche fuer die Init
{
	#if (F_CPU_Init/256UL) > 400000UL
	# error "F_CPU_Init ist zu hoch um die SD_Karte initialisieren zu k�nnen"
	#else
		SPI_Init(_256);
	#endif

	#ifdef Use_Carddetect
	if (SD_Card_Check_if_present())														// Wenn die Karte nicht eingesteckt ist, dann beende die Funktion
	{
		SPI_Init(SPI_Clockdiv_default);													// Stelle wieder auf die Defaultgeschwindigkeit um
		return 1;
	}
	#endif

	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SD_OUT &= ~(1<<SD_CS);																// SS auf 0
	_delay_us_init(1);

	SPI_Read_Write_Byte(0x41);															// Init Command (ACMD41)
	SPI_Read_Write_Byte(0x00);															// Bit f�r HC Karten setzen
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0x00);
	SPI_Read_Write_Byte(0xff);															// CRC wird ignoriert, hier aber trotzdem mal eingetragen

	for(uint8_t z=0;z<10;z++)															// Zaehlschleife
	{
		uint8_t r = SPI_Read_Write_Byte(0xff);
		if( r == 0x00)
		{
			_delay_us_init(1);
			SD_OUT |= (1<<SD_CS);
			_delay_us_init(1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: Karte initialisiert\r\n");
			#endif
			SPI_Init(SPI_Clockdiv_default);												// Stelle wieder auf die Defaultgeschwindigkeit um
			return 0;																	// Funktion vorzeichtig verlassen wenn die Karte initialisiert ist
		}
		if( r == 0x01)
		{
			_delay_us_init(1);
			SD_OUT |= (1<<SD_CS);
			_delay_us_init(1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: CMD1 initialisiert noch\r\n");
			#endif
			SPI_Init(SPI_Clockdiv_default);												// Stelle wieder auf die Defaultgeschwindigkeit um
			return 1;																	// Funktion vorzeichtig verlassen wenn die Karte initialisiert ist
		}
		_delay_us_init(10);
		#ifdef SD_Debug
			USART_Write_String("SD: warte auf CMD1 Antwort...\r\n");
		#endif
	}
	_delay_us_init(1);
	SD_OUT |= (1<<SD_CS);
	_delay_us_init(1);
	while(SPI_Read_Write_Byte(0xff)!=0xff){};												// Clock bis Tri-State
	#ifdef SD_Debug
		USART_Write_String("SD: CMD1 fehlgeschlagen\r\n");
	#endif
	SPI_Init(SPI_Clockdiv_default);														// Stelle wieder auf die Defaultgeschwindigkeit um
	return 1;																			// Funktion abrechen
}

uint8_t SD_Card_CMD24(uint32_t Address, char *Buffer)
{
	uint32_t g=0;

	#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock									// Wenn die default SPI Frequenz h�her als die maximal f�r die SD Karte zul�ssig ist, passe die Clock an
		# if F_CPU/2 <= SD_SPI_Max_Clock
			SPI_Init(_2);
		#elif F_CPU/4 <= SD_SPI_Max_Clock
			SPI_Init(_4);
		#elif F_CPU/8 <= SD_SPI_Max_Clock
			SPI_Init(_8);
		#elif F_CPU/16 <= SD_SPI_Max_Clock
			SPI_Init(_16);
		#elif F_CPU/32 <= SD_SPI_Max_Clock
			SPI_Init(_32);
		#elif F_CPU/64 <= SD_SPI_Max_Clock
			SPI_Init(_64);
		#elif F_CPU/128 <= SD_SPI_Max_Clock
			SPI_Init(_128);
		#elif F_CPU/256 <= SD_SPI_Max_Clock
			SPI_Init(_256);
		#else
		# error "SD_Card_CMD24 Clock konnte nicht aufgel�st werden"
		#endif
	#endif

	#ifdef Use_Carddetect
	if (SD_Card_Check_if_present())															// Wenn die Karte nicht eingesteckt ist, dann beende die Funktion
	{
		#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock								// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
			SPI_Init(SPI_Clockdiv_default);
		#endif
		return 1;
	}
	#endif

	if (gl_SD_Card.Initstate==0) return 1;													// Wenn Karte nicht initialisiert, dann breche ab

	if (gl_SD_Card.isHCXD) Address >>= 9;													// Bei HC XD Karten wird nicht mehr das Byte sondern nur noch der Sektor angesprochen. Jeder Sektor ist 512 Bytes gro�

	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SD_OUT &= ~(1<<SD_CS);																	// SS auf 0
	_delay_us(.1);

	SPI_Read_Write_Byte(0x58);
	SPI_Read_Write_Byte((Address>>24) & 0xff);												// 0x00
	SPI_Read_Write_Byte((Address>>16) & 0xff);												// 0x00
	SPI_Read_Write_Byte((Address>>8) & 0xff);												// 0x00
	SPI_Read_Write_Byte((Address>>0) & 0xff);												// 0x00
	SPI_Read_Write_Byte(0xff);
	do
	{
		if(g==2550)
		{
			_delay_us(.1);
			SD_OUT |= (1<<SD_CS);
			_delay_us(.1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State

			#ifdef SD_Debug
				USART_Write_String("SD: Keine Rueckantwort von der Karte nach dem Schreibbefehl erhalten\r\n");
			#endif
			#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock							// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
				SPI_Init(SPI_Clockdiv_default);
			#endif
			return 1;
		}
		g++;
	}while(SPI_Read_Write_Byte(0xff) != 0x00);												// Warten bis 0x00 kommt von der SD Karte das das Komando verstanden wurde

	SPI_Read_Write_Byte(0xff);																// Dummyclocks
	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);																//

	SPI_Read_Write_Byte(0xfe);																// �bertragung beginnt

	for(char *i=Buffer;i<(Buffer+512);i++)
	{
		SPI_Read_Write_Byte(*i);
	}

	SPI_Read_Write_Byte(0xff);																// CRC Unbedeutend
	SPI_Read_Write_Byte(0xff);

	if((0b00000101 & SPI_Read_Write_Byte(0xff)) == 0x05)
	{
		g=0;
		do																					// Solange die Karte busy ist warte
		{
			if (g==200000)
			{
				_delay_us(.1);
				SD_OUT |= (1<<SD_CS);
				_delay_us(.1);
				while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
				#ifdef SD_Debug
					USART_Write_String("SD: Karte ist zu lange beschaeftigt\r\n");
				#endif
				#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock						// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
					SPI_Init(SPI_Clockdiv_default);
				#endif
				return 1;
			}
			g++;
		}while(SPI_Read_Write_Byte(0xff)==0x00);

		_delay_us(.1);
		SD_OUT |= (1<<SD_CS);
		_delay_us(.1);
		while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State

		#ifdef SD_Debug
			printf("SD: Bei HCXD Karten ist dies der Sektor nicht Byteadresse\r\nSD: Byteadresse: %lu 0x%08lx beschrieben\r\n",Address, Address);
		#endif
		#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock								// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
			SPI_Init(SPI_Clockdiv_default);
		#endif
		return 0;
	}
	else
	{
		_delay_us(.1);
		SD_OUT |= (1<<SD_CS);
		_delay_us(.1);
		while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
		#ifdef SD_Debug
			USART_Write_String("SD: Datenblock nicht akzeptiert\r\n");
		#endif
		#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock								// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
			SPI_Init(SPI_Clockdiv_default);
		#endif
		return 1;
	}
}

uint8_t SD_Card_CMD25(uint32_t Address, uint32_t Sectorcount, char *Buffer)
{
	uint32_t g=0;

	#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock									// Wenn die default SPI Frequenz h�her als die maximal f�r die SD Karte zul�ssig ist, passe die Clock an
		# if F_CPU/2 <= SD_SPI_Max_Clock
			SPI_Init(_2);
		#elif F_CPU/4 <= SD_SPI_Max_Clock
			SPI_Init(_4);
		#elif F_CPU/8 <= SD_SPI_Max_Clock
			SPI_Init(_8);
		#elif F_CPU/16 <= SD_SPI_Max_Clock
			SPI_Init(_16);
		#elif F_CPU/32 <= SD_SPI_Max_Clock
			SPI_Init(_32);
		#elif F_CPU/64 <= SD_SPI_Max_Clock
			SPI_Init(_64);
		#elif F_CPU/128 <= SD_SPI_Max_Clock
			SPI_Init(_128);
		#elif F_CPU/256 <= SD_SPI_Max_Clock
			SPI_Init(_256);
		#else
		# error "SD_Card_CMD24 Clock konnte nicht aufgel�st werden"
		#endif
	#endif

	#ifdef Use_Carddetect
	if (SD_Card_Check_if_present())															// Wenn die Karte nicht eingesteckt ist, dann beende die Funktion
	{
		#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock								// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
			SPI_Init(SPI_Clockdiv_default);
		#endif
		return 1;
	}
	#endif

	if (gl_SD_Card.Initstate==0) return 1;													// Wenn Karte nicht initialisiert, dann breche ab

	if (gl_SD_Card.isHCXD) Address >>= 9;													// Bei HC XD Karten wird nicht mehr das Byte sondern nur noch der Sektor angesprochen. Jeder Sektor ist 512 Bytes gro�

	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SD_OUT &= ~(1<<SD_CS);																	// SS auf 0
	_delay_us(.1);

	SPI_Read_Write_Byte(0x40 + 25);
	SPI_Read_Write_Byte((Address>>24) & 0xff);
	SPI_Read_Write_Byte((Address>>16) & 0xff);
	SPI_Read_Write_Byte((Address>>8) & 0xff);
	SPI_Read_Write_Byte((Address>>0) & 0xff);
	SPI_Read_Write_Byte(0xff);

	do
	{
		if(g==2550)
		{
			_delay_us(.1);
			SD_OUT |= (1<<SD_CS);
			_delay_us(.1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State

			#ifdef SD_Debug
				USART_Write_String("SD: Keine Rueckantwort von der Karte nach dem Multiblock-Schreibbefehl erhalten\r\n");
			#endif
			#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock							// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
				SPI_Init(SPI_Clockdiv_default);
			#endif
			return 1;
		}
		g++;
	}while(SPI_Read_Write_Byte(0xff) != 0x00);												// Warten bis 0x00 kommt von der SD Karte das das Komando verstanden wurde

	for(uint8_t f=0;f<Sectorcount;f++)
	{
		SPI_Read_Write_Byte(0xfc);

		for(uint32_t i=0;i<512;i++)
		{
			SPI_Read_Write_Byte(*Buffer++);
		}

		SPI_Read_Write_Byte(0xff);																// CRC Unbedeutend
		SPI_Read_Write_Byte(0xff);

		if((0b00000101 & SPI_Read_Write_Byte(0xff)) == 0x05)
		{
			g=0;
			do																					    // Solange die Karte busy ist warte
			{
				if (g==200000)
				{
					_delay_us(.1);
					SD_OUT |= (1<<SD_CS);
					_delay_us(.1);
					while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
					#ifdef SD_Debug
						USART_Write_String("SD: Karte ist zu lange beschaeftigt\r\n");
					#endif
					#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock						   // Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
						SPI_Init(SPI_Clockdiv_default);
					#endif
					return 1;
				}
				g++;
			}while(SPI_Read_Write_Byte(0xff)==0x00);
		}
		else
		{
			_delay_us(.1);
			SD_OUT |= (1<<SD_CS);
			_delay_us(.1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: Multidatenblock nicht akzeptiert\r\n");
			#endif
			#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock						// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
				SPI_Init(SPI_Clockdiv_default);
			#endif
			return 1;
		}
	}

	// https://www.mikrocontroller.net/topic/165309 mit dem Token funktioniert es, mit CMD12 leider nicht
	SPI_Read_Write_Byte(0xfd);				// Stop token
	SPI_Read_Write_Byte(0xff);				// Dummy
	g=0;
	do																							// Solange die Karte busy ist warte
	{
		if (g==1000000)
		{
			_delay_us(.1);
			SD_OUT |= (1<<SD_CS);
			_delay_us(.1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: Karte keine Antwort nach Stop Token beim Schreiben mehrer Sektoren\r\n");
			#endif
			#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock						// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
				SPI_Init(SPI_Clockdiv_default);
			#endif
			return 1;
		}
		g++;
	}while(SPI_Read_Write_Byte(0xff)==0x00);

	_delay_us(.1);
	SD_OUT |= (1<<SD_CS);
	_delay_us(.1);
	while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State

	#ifdef SD_Debug
		printf("SD: Bei HCXD Karten ist dies der Sektor nicht Byteadresse\r\nSD: Multiblockadresse: %lu 0x%08lx beschrieben, %lu Sektoren\r\n",Address, Address,Sectorcount);
	#endif
	#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock								// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
		SPI_Init(SPI_Clockdiv_default);
	#endif
	return 0;
/*

	USART_Write_String("I\r\n");
	SPI_Read_Write_Byte(0x40 + 12);																// CMD12 beendet den Schreibvorgang
	USART_Write_String("I1\r\n");

	SPI_Read_Write_Byte((Address>>24) & 0xff);
	USART_Write_String("I2\r\n");

	SPI_Read_Write_Byte((Address>>16) & 0xff);
	USART_Write_String("I3\r\n");

	SPI_Read_Write_Byte((Address>>8) & 0xff);
	USART_Write_String("I4\r\n");

	SPI_Read_Write_Byte(Address & 0xff);
	USART_Write_String("I5\r\n");

	SPI_Read_Write_Byte(0xff);
	USART_Write_String("I6\r\n");
//Irgendwo hier bei CMD12 ist der Hund begraben warum die Karte irgendwann nicht mehr antwortet
	g=0;
	do																							// Solange die Karte busy ist warte
	{
		if (g==1000000)
		{
			USART_Write_String("K\r\n");

			_delay_us(.1);
			SD_OUT |= (1<<SD_CS);
			_delay_us(.1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: Karte keine Antwort nach CMD12 beim Schreiben mehrer Sektoren\r\n");
			#endif
			#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock						// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
				SPI_Init(SPI_Clockdiv_default);
			#endif
			return 1;
		}
		g++;
	}while(SPI_Read_Write_Byte(0xff)!=0x00);

	USART_Write_String("J\r\n");

	g=0;
	while(SPI_Read_Write_Byte(0xff)!=0xff)
	{
		if (g==200000)
		{
			_delay_us(.1);
			SD_OUT |= (1<<SD_CS);
			_delay_us(.1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: Karte keine Antwort nach CMD12 beim Schreiben mehrer Sektoren1\r\n");
			#endif
			#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock						// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
				SPI_Init(SPI_Clockdiv_default);
			#endif
			return 1;
		}
		g++;
	}

	USART_Write_String("K\r\n");

	_delay_us(.1);
	SD_OUT |= (1<<SD_CS);
	_delay_us(.1);
	while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State

	#ifdef SD_Debug
		printf("SD: Bei HCXD Karten ist dies der Sektor nicht Byteadresse\r\nSD: Multiblockadresse: %lu 0x%08lx beschrieben, %lu Sektoren\r\n",Address, Address,Sectorcount);
	#endif
	#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock								// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
		SPI_Init(SPI_Clockdiv_default);
	#endif
	return 0;
*/
}

uint8_t SD_Card_CMD17(uint32_t Address, char *Buffer)										// Bsp 0x1234678  12 34 56 78 MSB First		Liest einen Sektor der SD-Karte aus		return 1=Fehler, 0=erfolgreich
{
	uint8_t h=0xff;
	uint32_t g=0,z=0;

	#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock									// Wenn die default SPI Frequenz h�her als die maximal f�r die SD Karte zul�ssig ist, passe die Clock an
		# if F_CPU/2 <= SD_SPI_Max_Clock
			SPI_Init(_2);
		#elif F_CPU/4 <= SD_SPI_Max_Clock
			SPI_Init(_4);
		#elif F_CPU/8 <= SD_SPI_Max_Clock
			SPI_Init(_8);
		#elif F_CPU/16 <= SD_SPI_Max_Clock
			SPI_Init(_16);
		#elif F_CPU/32 <= SD_SPI_Max_Clock
			SPI_Init(_32);
		#elif F_CPU/64 <= SD_SPI_Max_Clock
			SPI_Init(_64);
		#elif F_CPU/128 <= SD_SPI_Max_Clock
			SPI_Init(_128);
		#elif F_CPU/256 <= SD_SPI_Max_Clock
			SPI_Init(_256);
		#else
		# error "SD_Card_CMD17 Clock konnte nicht aufgel�st werden"
		#endif
	#endif

	#ifdef Use_Carddetect
	if (SD_Card_Check_if_present())															// Wenn die Karte nicht eingesteckt ist, dann beende die Funktion
	{
		#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock								// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
			SPI_Init(SPI_Clockdiv_default);
		#endif
		return 1;
	}
	#endif

	if (gl_SD_Card.Initstate==0) return 1;													// Wenn Karte nicht initialisiert, dann breche ab

	if (gl_SD_Card.isHCXD) Address >>= 9;													// Bei HC XD Karten wird nicht mehr das Byte sondern nur noch der Sektor angesprochen. Jeder Sektor ist 512 Bytes gro�

	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SD_OUT &= ~(1<<SD_CS);																	// SS auf 0
	_delay_us(0.1);

	SPI_Read_Write_Byte(0x40 + 17);
	SPI_Read_Write_Byte((Address>>24) & 0xff);
	SPI_Read_Write_Byte((Address>>16) & 0xff);
	SPI_Read_Write_Byte((Address>>8) & 0xff);
	SPI_Read_Write_Byte(Address & 0xff);
	SPI_Read_Write_Byte(0xff);

	while (h!=0x00)
	{
		h = SPI_Read_Write_Byte(0xff);
		if (h == 0x20)
		{
			#ifdef SD_Debug
				USART_Write_String("SD: Zu lesende Adresse ist ung�ltig\r\n");
			#endif
			#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock							// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
				SPI_Init(SPI_Clockdiv_default);
			#endif
			return 2;
		}
		if (z > 255000)																		// > schneller als ==
		{
			_delay_us(.1);
			SD_OUT |= (1<<SD_CS);
			_delay_us(.1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: Keine Rueckantwort von der Karte nach dem Lesebefehl erhalten\r\n");
			#endif
			#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock							// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
				SPI_Init(SPI_Clockdiv_default);
			#endif
			return 1;
		}
		//_delay_us(.1);
		z++;
	}
	g=0; h=0;
	while(h != 0xfe)																		// Warten bist Start Token kommt
	{
		h = SPI_Read_Write_Byte(0xff);
		if (h == 0x08)
		{
			_delay_us(0.1);
			SD_OUT |= (1<<SD_CS);
			_delay_us(0.1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: Errortoken OutOfRange erhalten\r\n");
			#endif
			#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock							// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
				SPI_Init(SPI_Clockdiv_default);
			#endif
			return 1;
		}
		if(g > 6553500)																		// > geht schneller als ==
		{
			_delay_us(0.1);
			SD_OUT |= (1<<SD_CS);
			_delay_us(0.1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: Kein Starttoken erhalten\r\n");
			#endif
			#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock							// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
				SPI_Init(SPI_Clockdiv_default);
			#endif
			return 1;
		}
		//_delay_us(.1);
		g++;
	}

	for(char *v=Buffer;v<(Buffer+512);v++)													// Pointer ist schneller als indexed Array											// Schleife zum auslesen der 512 Bytes
	{
		*v = SPI_Read_Write_Byte(0xff);														// Schreibe in gl_SD_Buffer
	}

	SPI_Read_Write_Byte(0xff);																// CRC
	SPI_Read_Write_Byte(0xff);																// CRC

	_delay_us(0.1);
	SD_OUT |= (1<<SD_CS);
	_delay_us(0.1);
	while(SPI_Read_Write_Byte(0xff)!=0xff){};												// Clock bis Tri-State
	#ifdef SD_Debug
		printf("SD: Bei HCXD Karten ist dies der Sektor nicht Byteadresse\r\nSD: Byteadresse 0x%08lx gelesen\r\n",Address);
	#endif
	#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock									// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
		SPI_Init(SPI_Clockdiv_default);
	#endif
	return 0;	 	  	 																	// return to main
}

uint8_t SD_Card_CMD18(uint32_t Address,uint8_t Sectorcount, char *Buffer)										// Bsp 0x1234678  12 34 56 78 MSB First		Liest mehrere Sektoren der SD-Karte aus		return 1=Fehler, 0=erfolgreich
{
	uint8_t h=0xff;
	uint32_t g=0,z=0;

	#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock									// Wenn die default SPI Frequenz h�her als die maximal f�r die SD Karte zul�ssig ist, passe die Clock an
		# if F_CPU/2 <= SD_SPI_Max_Clock
			SPI_Init(_2);
		#elif F_CPU/4 <= SD_SPI_Max_Clock
			SPI_Init(_4);
		#elif F_CPU/8 <= SD_SPI_Max_Clock
			SPI_Init(_8);
		#elif F_CPU/16 <= SD_SPI_Max_Clock
			SPI_Init(_16);
		#elif F_CPU/32 <= SD_SPI_Max_Clock
			SPI_Init(_32);
		#elif F_CPU/64 <= SD_SPI_Max_Clock
			SPI_Init(_64);
		#elif F_CPU/128 <= SD_SPI_Max_Clock
			SPI_Init(_128);
		#elif F_CPU/256 <= SD_SPI_Max_Clock
			SPI_Init(_256);
		#else
		# error "SD_Card_CMD17 Clock konnte nicht aufgel�st werden"
		#endif
	#endif

	#ifdef Use_Carddetect
	if (SD_Card_Check_if_present())															// Wenn die Karte nicht eingesteckt ist, dann beende die Funktion
	{
		#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock								// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
			SPI_Init(SPI_Clockdiv_default);
		#endif
		return 1;
	}
	#endif

	if (gl_SD_Card.Initstate==0) return 1;													// Wenn Karte nicht initialisiert, dann breche ab

	if (gl_SD_Card.isHCXD) Address >>= 9;													// Bei HC XD Karten wird nicht mehr das Byte sondern nur noch der Sektor angesprochen. Jeder Sektor ist 512 Bytes gro�

	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SD_OUT &= ~(1<<SD_CS);																	// SS auf 0
	_delay_us(0.1);

	SPI_Read_Write_Byte(0x40 + 18);
	SPI_Read_Write_Byte((Address>>24) & 0xff);
	SPI_Read_Write_Byte((Address>>16) & 0xff);
	SPI_Read_Write_Byte((Address>>8) & 0xff);
	SPI_Read_Write_Byte(Address & 0xff);
	SPI_Read_Write_Byte(0xff);

	while (h!=0x00)
	{
		h = SPI_Read_Write_Byte(0xff);
		if (h == 0x20)
		{
			#ifdef SD_Debug
				USART_Write_String("SD: Zu lesende Adresse ist ung�ltig\r\n");
			#endif
			#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock							// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
				SPI_Init(SPI_Clockdiv_default);
			#endif
			return 2;
		}
		if (z > 255000)																		// > schneller als ==
		{
			_delay_us(.1);
			SD_OUT |= (1<<SD_CS);
			_delay_us(.1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: Keine Rueckantwort von der Karte nach dem Multiblock-Lesebefehl erhalten\r\n");
			#endif
			#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock							// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
				SPI_Init(SPI_Clockdiv_default);
			#endif
			return 1;
		}
		//_delay_us(.1);
		z++;
	}

	for (uint8_t i=0;i < Sectorcount;i++)
	{
		g=0; h=0;
		while(h != 0xfe)																		// Warten bist Start Token kommt
		{
			h = SPI_Read_Write_Byte(0xff);
			/*
			When the last block of user area is read using CMD18, the host should ignore the OUT_OF_RANGE error
			that may occur even the sequence is correct
			if (h == 0x08)
			{
				_delay_us(0.1);
				SD_OUT |= (1<<SD_CS);
				_delay_us(0.1);
				while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
				#ifdef SD_Debug
					USART_Write_String("SD: Errortoken OutOfRange erhalten\r\n");
				#endif
				#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock							// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
					SPI_Init(SPI_Clockdiv_default);
				#endif
				return 1;
			}*/
			if(g > 6553500)																		// > geht schneller als ==
			{
				_delay_us(0.1);
				SD_OUT |= (1<<SD_CS);
				_delay_us(0.1);
				while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
				#ifdef SD_Debug
					USART_Write_String("SD: Kein Starttoken erhalten\r\n");
				#endif
				#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock							// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
					SPI_Init(SPI_Clockdiv_default);
				#endif
				return 1;
			}
			//_delay_us(.1);
			g++;
		}

		for(char *v=Buffer;v<(Buffer+512);v++)	    											// Pointer ist schneller als indexed Array											// Schleife zum auslesen der 512 Bytes
		{
			*v = SPI_Read_Write_Byte(0xff);														// Schreibe in gl_SD_Buffer
		}

		SPI_Read_Write_Byte(0xff);																// CRC
		SPI_Read_Write_Byte(0xff);																// CRC

		Buffer+=512;																			// Z�hle den Pointer um 512 Bytes weiter
	}

	SPI_Read_Write_Byte(0x40 + 12);																// CMD12 beendet den Lesevorgang
	SPI_Read_Write_Byte((Address>>24) & 0xff);
	SPI_Read_Write_Byte((Address>>16) & 0xff);
	SPI_Read_Write_Byte((Address>>8) & 0xff);
	SPI_Read_Write_Byte(Address & 0xff);
	SPI_Read_Write_Byte(0xff);

	g=0;
	do																							// Solange die Karte busy ist warte
	{
		if (g==200000)
		{
			_delay_us(.1);
			SD_OUT |= (1<<SD_CS);
			_delay_us(.1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: Karte keine Antwort nach CMD12 beim Lesen mehrer Sektoren\r\n");
			#endif
			#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock						// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
				SPI_Init(SPI_Clockdiv_default);
			#endif
			return 1;
		}
		g++;
	}while(SPI_Read_Write_Byte(0xff)!=0x00);
	g=0;
	while(SPI_Read_Write_Byte(0xff)!=0xff)
	{
		if (g==200000)
		{
			_delay_us(.1);
			SD_OUT |= (1<<SD_CS);
			_delay_us(.1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: Karte keine Antwort nach CMD12 beim Lesen mehrer Sektoren1\r\n");
			#endif
			#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock						// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
				SPI_Init(SPI_Clockdiv_default);
			#endif
			return 1;
		}
		g++;
	}

	_delay_us(0.1);
	SD_OUT |= (1<<SD_CS);
	_delay_us(0.1);
	while(SPI_Read_Write_Byte(0xff)!=0xff){};												// Clock bis Tri-State
	#ifdef SD_Debug
		printf("SD: Bei HCXD Karten ist dies der Sektor nicht Byteadresse\r\nSD: Byteadresse 0x%08lx gelesen\r\n",Address);
	#endif
	#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock									// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
		SPI_Init(SPI_Clockdiv_default);
	#endif
	return 0;	 	  	 																	// return to main
}

uint8_t SD_Card_CMD16 (uint32_t Blocklength)												// return 1=fehlgeschlagen, 0=erfolgreich
{
	uint16_t g=0;

	#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock									// Wenn die default SPI Frequenz h�her als die maximal f�r die SD Karte zul�ssig ist, passe die Clock an
		# if F_CPU/2 <= SD_SPI_Max_Clock
			SPI_Init(_2);
		#elif F_CPU/4 <= SD_SPI_Max_Clock
			SPI_Init(_4);
		#elif F_CPU/8 <= SD_SPI_Max_Clock
			SPI_Init(_8);
		#elif F_CPU/16 <= SD_SPI_Max_Clock
			SPI_Init(_16);
		#elif F_CPU/32 <= SD_SPI_Max_Clock
			SPI_Init(_32);
		#elif F_CPU/64 <= SD_SPI_Max_Clock
			SPI_Init(_64);
		#elif F_CPU/128 <= SD_SPI_Max_Clock
			SPI_Init(_128);
		#elif F_CPU/256 <= SD_SPI_Max_Clock
			SPI_Init(_256);
		#else
		# error "SD_Set_Clocklength Clock konnte nicht aufgel�st werden"
		#endif
	#endif

	#ifdef Use_Carddetect
	if (SD_Card_Check_if_present())															// Wenn die Karte nicht eingesteckt ist, dann beende die Funktion
	{
		#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock								// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
			SPI_Init(SPI_Clockdiv_default);
		#endif
		return 1;
	}
	#endif

	if (gl_SD_Card.Initstate==0) return 1;													// Wenn Karte nicht initialisiert, dann breche ab
	if (gl_SD_Card.isHCXD) return 1;														// Bei HCXD Karten ist die Blockl�nge immer 512

	SPI_Read_Write_Byte(0xff);
	SPI_Read_Write_Byte(0xff);
	SD_OUT &= ~(1<<SD_CS);																	// SS auf 0
	_delay_us(1);

	SPI_Read_Write_Byte(0x50);
	SPI_Read_Write_Byte((Blocklength>>24) & 0xff);
	SPI_Read_Write_Byte((Blocklength>>16) & 0xff);
	SPI_Read_Write_Byte((Blocklength>>8) & 0xff);
	SPI_Read_Write_Byte((Blocklength>>0) & 0xff);
	SPI_Read_Write_Byte(0xff);
	do
	{
		if (g==2000)
		{
			_delay_us(1);
			SD_OUT |= (1<<SD_CS);
			_delay_us(1);
			while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
			#ifdef SD_Debug
				USART_Write_String("SD: Fehler waehrend dem setzen der Blocklaenge\r\n");
			#endif
			#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock							// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
				SPI_Init(SPI_Clockdiv_default);
			#endif
			return 1;
		}
		g++;
		_delay_us(10);
	} while (SPI_Read_Write_Byte(0xff) != 0x00);

	_delay_us(1);
	SD_OUT |= (1<<SD_CS);
	_delay_us(1);
	while(SPI_Read_Write_Byte(0xff)!=0xff){};										// Clock bis Tri-State
	#ifdef SD_Debug
		USART_Write_String("SD: Blocklaenge gesetzt\r\n");
	#endif
	#if (F_CPU/SPI_Default_Clockdivider) >= SD_SPI_Max_Clock									// Wenn f�r die SD Karte eine eigene Frequenz genutzt wurde, stelle dies wieder auf den Standard zur�ck
		SPI_Init(SPI_Clockdiv_default);
	#endif
	return 0;
}

uint8_t SD_Card_read_MBR(void)
{
	// SPI Clock wurde schon in Read Block eventuell gesetzt
	if (SD_Card_CMD17(SD_MBR_address,&gl_SD_Card.RWbuffer[0]))								// Lese den MBR in den Readbuffer, bei Lesefehler 1 zurueck
	{
		#ifdef SD_Debug
			USART_Write_String("SD: MBR konnte nicht gelesen werden\r\n");
		#endif
		return 1;
	}

	gl_SD_MBR.Partition1.Partition_Entryaddress = 0x1be;																												// Festwert fuer die erste Partition
	gl_SD_MBR.Partition1.Active_Partition = gl_SD_Card.RWbuffer[gl_SD_MBR.Partition1.Partition_Entryaddress];															// Aktive Partition
	gl_SD_MBR.Partition1.Partition_Type = gl_SD_Card.RWbuffer[gl_SD_MBR.Partition1.Partition_Entryaddress + 0x04];														// Partitiontyp
	gl_SD_MBR.Partition1.Partition_Startaddress = Get_uint32_from_Little_Endian_Buffer(&gl_SD_Card.RWbuffer[gl_SD_MBR.Partition1.Partition_Entryaddress + 0x08])*512;	// Startaddress Sektor*512 Bytes Sektorgroesse
	gl_SD_MBR.Partition1.Sectors_in_Partition = Get_uint32_from_Little_Endian_Buffer(&gl_SD_Card.RWbuffer[gl_SD_MBR.Partition1.Partition_Entryaddress + 0x0c]);			// Groesse der Partition in Sektoren
	gl_SD_MBR.Partition1.Boot_Record_Signature = (gl_SD_Card.RWbuffer[511]<<8)|gl_SD_Card.RWbuffer[510];																													// Boot Record Signatur, hier schon in Big Endian

	#ifdef SD_Debug
		USART_Write_X_Bytes((char*)&gl_SD_MBR.Partition1,0,sizeof(gl_SD_MBR.Partition1));
		USART_Write_String("SD: MBR gelesen\r\n");
	#endif

	return 0;
}
