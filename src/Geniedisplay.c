// Includes
#include "main.h"
#include "Geniedisplay.h"
#include "stm32f4xx.h"
#include <string.h>


/*

************************************************************************************************
* Genie Display �ber Software SPI am STM32F401RET6						  					   *
* 2019 � Frederinn															 		   *
************************************************************************************************

*/

// Defines


// Structs


// Variablen
char gl_Genie_Frame[8][128];																	// Framebuffer
const char gl_Font_5_7[] = {	0,0,0,0,0,														// Space
								0,0,95,0,0,														// !
								0,7,0,7,0,														// "
								20,127,20,127,20,												// #
								0,36,107,18,0,													// $
								99,19,8,100,99,													// %
								54,73,73,54,80,													// &
								0,0,7,0,0,														// '
								0,62,65,0,0,													// (
								0,0,65,62,0,													// )
								0,5,2,5,0,														// *
								0,8,28,8,0,														// +
								0,128,96,0,0,													// ,
								0,8,8,8,0,														// -
								0,0,64,0,0,														// .
								96,16,8,4,3,													// /
								62,81,73,69,62,													// 0
								8,4,2,127,0,													// 1
								66,97,81,73,70,													// 2
								34,65,73,73,54,													// 3
								24,20,18,127,16,												// 4
								39,69,69,69,57,													// 5
								62,73,73,73,50,													// 6
								97,17,9,5,3,													// 7
								54,73,73,73,54,													// 8
								38,73,73,73,62,													// 9
								0,0,20,0,0,														// :
								0,128,116,0,0,													// ;
								0,8,20,34,0,													// <
								0,20,20,20,0,													// =
								0,34,20,8,0,													// >
								2,1,81,9,6,														// ?
								32,80,112,8,124,												// @
								124,18,17,18,124,												// A
								127,73,73,73,54,												// B
								62,65,65,65,34,													// C
								127,65,65,65,62,												// D
								127,73,73,73,65,												// E
								127,9,9,9,1,													// F
								62,65,73,73,121,												// G
								127,8,8,8,127,													// H
								65,65,127,65,65,												// I
								65,129,129,129,127,												// J
								127,8,20,34,65,													// K
								127,64,64,64,64,												// L
								127,2,4,2,127,													// M
								127,6,8,48,127,													// N
								62,65,65,65,62,													// O
								127,17,17,17,14,												// P
								62,65,81,97,126,												// Q
								127,17,17,49,78,												// R
								38,73,73,73,50,													// S
								1,1,127,1,1,													// T
								63,64,64,64,63,													// U
								31,32,64,32,31,													// V
								63,64,63,64,63,													// W
								65,54,8,54,65,													// X
								3,4,120,4,3,													// Y
								97,81,73,69,67,													// Z
								0,127,65,0,0,													// [
								3,4,8,16,96,													// "\"
								0,65,127,0,0,													// ]
								4,2,1,2,4,														// ^
								128,128,128,128,128,											// _
								0,0,1,2,4,														// '
								36,84,84,120,0,													// a
								127,72,72,48,0,													// b
								56,68,68,68,40,													// c
								56,68,68,68,127,												// d
								56,84,84,84,8,													// e
								0,126,9,9,0,													// f
								152,164,164,120,0,												// g
								127,4,4,4,120,													// h
								0,0,125,0,0,													// i
								64,128,122,0,0,													// j
								127,16,40,68,0,													// k
								64,65,127,64,64,												// l
								120,4,120,4,120,												// m
								124,4,4,4,120,													// n
								56,68,68,68,56,													// o
								252,36,36,24,0,													// p
								24,36,36,252,0,													// q
								124,8,4,4,8,													// r
								72,84,84,84,36,													// s
								4,4,127,4,4,													// t
								60,64,64,64,60,													// u
								28,32,64,32,28,													// v
								60,64,32,64,60,													// w
								68,40,16,40,68,													// x
								28,160,64,32,28,												// y
								68,100,84,76,68,												// z
								0,8,54,65,0,													// {
								0,0,127,0,0,													// |
								0,65,54,8,0,													// }
								16,8,8,16,8														// ~
};																								// Font im Flash

// Funktionen
void Genie_Init(void)
{
	RCC->AHB1ENR |= Genie_RCC_IOPENR;														// Port B Clock an
	// Maskiere die alte Pinfunktion raus und setze auf Ausgang
	Genie_DIR = (Genie_DIR & ~((0b11 << (Genie_A0*2)) | (0b11 << (Genie_Reset*2)) | (0b11 << (Genie_CS*2)) | (0b11 << (Genie_SI*2)) | (0b11 << (Genie_SCL*2)))) | (0b01 << (Genie_A0*2)) | (0b01 << (Genie_Reset*2)) | (0b01 << (Genie_CS*2)) | (0b01 << (Genie_SI*2)) | (0b01 << (Genie_SCL*2));

	#ifdef Genie_GPIO_VDD
		Genie_DIR = (Genie_DIR & ~(0b11 << (Genie_VDD*2))) | (0b01 << (Genie_VDD*2));							// VDD auf Ausgang
		Genie_OUT |= (1<<Genie_VDD);
		_delay_us(20000);																						// 20ms warten
	#endif

	Genie_OUT &= ~(1<<Genie_Reset);																				// Reset ein
	_delay_us(1000);																							// Kurz warten
	Genie_OUT |= (1<<Genie_Reset)|(1<<Genie_CS);																// Reset aus, CS auf 1

	Genie_Write_Command(0x89);																					// (29)DCDC/Setup Multiplikator (Byte1)
	Genie_Write_Command(0x00);																					// (29)DCDC/Setup x5 (Byte2)
//	Genie_Write_Command(0xA2);																					// (11)LCD Bias setup 1/9
	Genie_Write_Command(0xA3);																					// (11)LCD Bias setup 1/7
 	Genie_Write_Command(0xA1);																					// (08)ADC-Select => Bild spiegeln
 	Genie_Write_Command(0xC0);																					// (15)Common Output Mode scan dir => Bild 180grad drehen
	Genie_Write_Command(0x25);																					// (17)Setting the built in resistance rad for reg.V0
	Genie_Write_Command(0x81);																					// (18)Elec. Vol. Set V0 Command
	Genie_Write_Command(40 & 0x3F);																				// (18)Value for Vol.Set => default Kontrast Wert 40
	Genie_Write_Command(0x2F);																					// (16)Power Control Set
	Genie_Write_Command(0xAF);																					// (01)LCD-Panel ON
//	Genie_Write_Command(0xA5);																					// (10)Entire Display ON
	Genie_Write_Command(0xA4);																					// (10)Entire Display OFF
	
	Genie_Clear_Display();
}

void Genie_Write_Command(uint8_t Command)
{
	Genie_OUT &= ~(1<<Genie_CS);																				// CS auf 0
	Genie_OUT &= ~(1<<Genie_A0);																				// Command
	_delay_us(.1);
	Genie_Write_Byte(Command);																					// Schreibe Byte
	_delay_us(.1);
	Genie_OUT |= (1<<Genie_CS);
}

void Genie_Write_Data(uint8_t Data)
{
	Genie_OUT &= ~(1<<Genie_CS);																				// CS auf 0
	Genie_OUT |= (1<<Genie_A0);																					// Data
	_delay_us(.1);
	Genie_Write_Byte(Data);																						// Schreibe Byte
	_delay_us(.1);
	Genie_OUT |= (1<<Genie_CS);
}

void Genie_Write_Byte(uint8_t Data)
{
	uint8_t loop, mask;
	for (loop=0,mask=0x80;loop<8;loop++,mask=mask>>1)
	{
		Genie_OUT &= ~(1<<Genie_SCL);
		_delay_us(.1);
		if (Data & mask)
		{
			Genie_OUT |= (1<<Genie_SI);
		}
		else
		{
			Genie_OUT &= ~(1<<Genie_SI);
		}
		_delay_us(.1);
		Genie_OUT |= (1<<Genie_SCL);
		_delay_us(.1);
	}
	Genie_OUT &= ~(1<<Genie_SCL);
	_delay_us(.1);
}

void Genie_Clear_Display(void)
{
	uint8_t Column=0, Row=0;
	
	for(Row=0;Row<8;Row++)
	{
		Genie_Write_Command(0xB0 | Row);																		// Row + Befehl
		for(Column=0;Column<128;Column++)
		{
			Genie_Write_Command(0x10 | ((Column>>4) & 0x0f));													// Highnibble
			Genie_Write_Command(Column & 0x0f);																	// Lownibble
			Genie_Write_Data(0x00);
			gl_Genie_Frame[Row][Column]=0;
		}
	}
}

void Genie_Write_Char_5_7(uint8_t x, uint8_t y,uint8_t Character)
{
	
	uint8_t startbit_in_page = y&7;																		// Ermittle Anzahl der bit die in dieser Page angezeigt werden m�ssen
	uint16_t Pos_in_Array = (Character-32)*5;
	uint8_t bits_to_draw=0;
	
	for (uint8_t x_counter=x;x_counter<(x+5);x_counter++,Pos_in_Array++)									// Zeichne die ersten Bits
	{
		if (startbit_in_page>0)																					// Falls die nachfolgende Page ben�tigt wird, Zeichne auch dort die Bits
		{
			Genie_Write_Command(((y/8)+1) | 0xB0);																// (03)Page Adress SET
			Genie_Write_Command(0x10 | ((x_counter>>4) & 0x0f));												// Highnibble
			Genie_Write_Command(x_counter & 0x0f);																// Lownibble
			bits_to_draw = gl_Font_5_7[Pos_in_Array]>>(8-startbit_in_page);										// Berechne die Startposition im Byte und schiebe die Bits da hin
			Genie_Write_Data(bits_to_draw | gl_Genie_Frame[y/8+1][x_counter]);	 								// Schreibe Byte mit Frame�bernahme
			gl_Genie_Frame[y/8+1][x_counter] |= bits_to_draw;													// Frame aktualsieren
		}
		Genie_Write_Command((y/8) | 0xB0);																		// Startpage ermitteln
		Genie_Write_Command(0x10 | ((x_counter>>4) & 0x0f));													// Highnibble
		Genie_Write_Command(x_counter & 0x0f);																	// Lownibble
		bits_to_draw = gl_Font_5_7[Pos_in_Array]<<startbit_in_page;												// Berechne die Startposition im Byte und schiebe die Bits da hin
		Genie_Write_Data(bits_to_draw | gl_Genie_Frame[y/8][x_counter]); 										// Byte schreiben mit Frame�bernahme
		gl_Genie_Frame[y/8][x_counter] |= bits_to_draw;															// Frame aktualsieren
	}
}

void Genie_Write_String_5_7(uint8_t x, uint8_t y, char *String)
{
	while (*String)
	{
		Genie_Write_Char_5_7(x,y,*String);																		// Ascii Offest rausrechnen
		String++;
		x+=6;
	}
}
