// Include
#include "main.h"
#include "ICMP.h"
#include "Stack.h"
#include "ENC28J60.h"
#ifdef ICMP_Debug
	#include "USART.h"
#endif
#include <string.h>

/*

************************************************************************************************
* ICMP Protokoll																			   *
* 2019 � Frederinn																  	   *
************************************************************************************************

*/

// Globale Variablen
struct ICMP_Head	gl_ICMP_Head_read;										// ICMP Head lesen
struct ICMP_Head	gl_ICMP_Head_Write;										// ICMP Head schreiben

// Funktionen
uint8_t ICMP_Read_Header_from_Buffer(struct ICMP_Head *ICMP_Header, struct IP_Head *IP_Header)
{
	uint16_t Erg = 0;														// Ergebnis der Checksummenpruefung
	ICMP_Header->Type = gl_ENC_Readbuffer[34];								// Typ
	ICMP_Header->Code = gl_ENC_Readbuffer[35];								// Code
	memcpy(&ICMP_Header->Checksum,&gl_ENC_Readbuffer[36],2);				// Checksumme
	memcpy(&ICMP_Header->Identifier,&gl_ENC_Readbuffer[38],2);				// Identifier
	memcpy(&ICMP_Header->Sequencenumber,&gl_ENC_Readbuffer[40],2);			// Sequencenummer
	ICMP_Header->Data = &gl_ENC_Readbuffer[42];								// Data
	ICMP_Header->Datalength = IP_Header->Totallength - 28;					// IP-Paketlaenge - IP Headerlaenge - ICMP Headerlaenge - Identifier - Sequencenummer

	gl_ENC_Readbuffer[36] = 0;												// Setze die Checksumme auf 0
	gl_ENC_Readbuffer[37] = 0;
	Erg	= IP_Calc_Checksum(ICMP_Header->Datalength + 8,&gl_ENC_Readbuffer[34]);		// Ermittle die Checksumme
	
	if (Erg == Byteswap16(ICMP_Header->Checksum))
	{
		#ifdef ICMP_Debug
			USART_Write_String("ICMP: Paket gelesen, Pruefsumme korrekt.\r\n");
		#endif
		return 0;
	}
	else
	{
		#ifdef ICMP_Debug
			USART_Write_String("ICMP: Paket gelesen, Pruefsumme falsch.\r\n");
		#endif
		return 1;	
	}
	return 1;																				// Wird nie erreicht
}

void ICMP_Create_Header_in_Buffer(struct ICMP_Head *ICMP_Header)
{
	uint16_t Erg = 0;																		// Ergebnis der Checksummenpruefung
	gl_ENC_Write_Datapayload[20] = ICMP_Header->Type;										// Typ
	gl_ENC_Write_Datapayload[21] = ICMP_Header->Code;										// Code
	gl_ENC_Write_Datapayload[22] = 0x00;													// Checksum
	gl_ENC_Write_Datapayload[23] = 0x00;
	memcpy(&gl_ENC_Write_Datapayload[24],&ICMP_Header->Identifier,2);						// Identifier
	memcpy(&gl_ENC_Write_Datapayload[26],&ICMP_Header->Sequencenumber,2);					// Sequencenummer
	memcpy(&gl_ENC_Write_Datapayload[28],ICMP_Header->Data,ICMP_Header->Datalength);		// Daten
	
	Erg = Byteswap16(IP_Calc_Checksum(ICMP_Header->Datalength + 8,&gl_ENC_Write_Datapayload[20]));	// Checksum
	memcpy(&gl_ENC_Write_Datapayload[22],&Erg,2);											// Checksumme eintragen
}

uint8_t ICMP_Send_Packet(struct ICMP_Head *ICMP_Header, struct IP_Head *IP_Header, struct ENC_Head *ENC_Header)
{
	IP_Create_Header_in_Buffer(IP_Header);
	ICMP_Create_Header_in_Buffer(ICMP_Header);
	
	if (ENC_Send_Packet(&ENC_Header->Dest_MAC[0],ENC_Header->Type,0x1000,ICMP_Header->Datalength + 28)==0)	// Sende das ICMP Paket
	{
		#ifdef ICMP_Debug
			USART_Write_String("ICMP: Paket gesendet\r\n");
		#endif
		return 0;
	}
	else
	{
		#ifdef ICMP_Debug
			USART_Write_String("ICMP: Paket konnte nicht gesendet werden\r\n");
		#endif
		return 1;
	}
	return 1;
}

void ICMP_Packethandler(void)
{
	struct IP_Head	IP_Head_Write;
	struct ENC_Head ENC_Head_Write;
	
	memset(&IP_Head_Write,0,sizeof(IP_Head_Write));
	memset(&ENC_Head_Write,0,sizeof(ENC_Head_Write));
	
	memcpy(&ENC_Head_Write.Dest_MAC[0],&gl_ENC_Head_read.Source_MAC[0],6);	// Neue Destmac
	memcpy(&ENC_Head_Write.Source_MAC[0],&gl_ENC_Head_read.Dest_MAC[0],6);	// Neue Sourcemac
	ENC_Head_Write.Type = 0x800;											// IP Paket
	
	IP_Head_Write.Totallength = 28 + gl_ICMP_Head_read.Datalength;		// Totale Laenge IP + ICMP
	//IP_Header.Identifikation = 0x0005;								// Wird generiert
	IP_Head_Write.Flags = 0x00;											// Flags 0
	IP_Head_Write.Fragment_Offset = 0x0000;								// Fragmentoffset 0
	IP_Head_Write.TTL = 128;											// Time to live
	IP_Head_Write.Protocol = 0x01;										// ICMP
	memcpy(&IP_Head_Write.Source_IP[0],&gl_Webserver.IP_address[0],4);	// Eigene IP addresse
	memcpy(&IP_Head_Write.Dest_IP[0],&gl_IP_Head_read.Source_IP[0],4);	// Gelesene Source IP
	
	gl_ICMP_Head_Write.Type = 0x00;										// Type Echo Antwort
	gl_ICMP_Head_Write.Code = 0;										// Code 0
	gl_ICMP_Head_Write.Identifier = gl_ICMP_Head_read.Identifier;		// Identifier
	gl_ICMP_Head_Write.Sequencenumber = gl_ICMP_Head_read.Sequencenumber;// Sequencenummer
	gl_ICMP_Head_Write.Data = gl_ICMP_Head_read.Data;					// Data
	gl_ICMP_Head_Write.Datalength = gl_ICMP_Head_read.Datalength;		// Datenlaenge
	
	ICMP_Send_Packet(&gl_ICMP_Head_Write,&IP_Head_Write,&ENC_Head_Write);
}
