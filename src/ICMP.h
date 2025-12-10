#ifndef ICMP_H_
#define ICMP_H_

// Include
#include "ENC28J60.h"
#include "Stack.h"

/*

************************************************************************************************
* ICMP Protokoll																			   *
* 2019 � Frederinn																  	   *
************************************************************************************************

*/

// Defines
//#define ICMP_Debug

// Structs
struct ICMP_Head
{
	uint8_t Type;
	uint8_t Code;
	uint16_t Checksum;
	uint16_t Identifier;
	uint16_t Sequencenumber;
	char *Data;
	uint16_t Datalength;
};

// Globale Variablen
extern struct ICMP_Head		gl_ICMP_Head_read;																			// ICMP Head
extern struct ICMP_Head		gl_ICMP_Head_Write;																			// ICMP Head schreiben


// Funktionen
extern uint8_t ICMP_Read_Header_from_Buffer(struct ICMP_Head *ICMP_Header, struct IP_Head *IP_Header);					// Lese ICMP Header aus Readbufer
extern void ICMP_Create_Header_in_Buffer(struct ICMP_Head *ICMP_Header);												// Erstelle einen ICMP Header im Writebuffer
extern uint8_t ICMP_Send_Packet(struct ICMP_Head *ICMP_Header, struct IP_Head *IP_Header, struct ENC_Head *ENC_Header);	// Erstellt und sendet ein ICMP Paket
extern void ICMP_Packethandler(void);																					// Pakethandler f�r die Stack.c

#endif /* ICMP_H_ */
