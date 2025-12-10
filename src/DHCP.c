// Includes
#include "main.h"
#include "dhcp.h"
//#include "delay.h"		// Ist in main.h als Inline
#include "ENC28J60.h"
#include "Stack.h"
#include "Allerlei.h"
#include "NTP.h"
#include "Tasktimer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef DHCP_Debug
	#include "USART.h"
#endif
#include "FAT32.h"
#include "INI_Parser.h"

/*

************************************************************************************************
* DHCP Client											  									   *
* 2019 � Frederinn															  		   *
************************************************************************************************

*/

// Variablen
struct DHCP_Head gl_DHCP_Head_Read;												// Globale Variablen
struct DHCP_Cache gl_DHCP_Cache;												// DHCP Cache
struct DHCP_Option gl_DHCP_Option;												// DHCP Option aus Header
uint8_t gl_DHCP_Packet_received;												// Paket empfangen

// Funktionen
uint8_t DHCP_Init(void)
{	
	memset(&gl_DHCP_Cache,0,sizeof(gl_DHCP_Cache));								// Cache l�schen
	gl_DHCP_Packet_received=0;													// Flag zur�cksetzen
	FAT32_Directory_Change("/");
	char Buffer[16];															// Zwischenpuffer zum Konvertieren
	INI_Read_Key_String("basic.ini","Netz","IP",&Buffer[0]);					// Standard IP laden
	sscanf(&Buffer[0],"%hhu.%hhu.%hhu.%hhu",&gl_Webserver.IP_address[0],&gl_Webserver.IP_address[1],&gl_Webserver.IP_address[2],&gl_Webserver.IP_address[3]);
	INI_Read_Key_String("basic.ini","Netz","Sub",&Buffer[0]);					// Subnetzmaske
	sscanf(&Buffer[0],"%hhu.%hhu.%hhu.%hhu",&gl_Webserver.Subnetmask[0],&gl_Webserver.Subnetmask[1],&gl_Webserver.Subnetmask[2],&gl_Webserver.Subnetmask[3]);
	INI_Read_Key_String("basic.ini","Netz","Gate",&Buffer[0]);					// Gateway
	sscanf(&Buffer[0],"%hhu.%hhu.%hhu.%hhu",&gl_Webserver.Gateway_IP[0],&gl_Webserver.Gateway_IP[1],&gl_Webserver.Gateway_IP[2],&gl_Webserver.Gateway_IP[3]);
	INI_Read_Key_String("basic.ini","Netz","NTP",&Buffer[0]);					// NTP Server IP
	sscanf(&Buffer[0],"%hhu.%hhu.%hhu.%hhu",&gl_Webserver.NTP_IP[0],&gl_Webserver.NTP_IP[1],&gl_Webserver.NTP_IP[2],&gl_Webserver.NTP_IP[3]);
	
	// Setze den seed
	srand(Time_Timestamp_to_UTC(&gl_Time));

	#ifdef DHCP_Debug
		USART_Write_String("DHCP: Defaultdaten aus EEPROM geladen\r\n");
	#endif
	
	if(DHCP_Request_IP()==DHCP_ACK)																	// Falls Anforderung fehlerhaft lade EEPROM Werte
	{
		Tasktimer_Add_Task(2,Time_Timestamp_to_UTC(&gl_Time)+gl_DHCP_Cache.Lease,gl_DHCP_Cache.Lease,(void*)&DHCP_Request_IP,"DHCP");
		return 0;
	}
	else
	{
		memset(&gl_DHCP_Cache.Name[0],0,sizeof(gl_DHCP_Cache.Name));								// L�sche den DHCP Namen wieder raus
		return 1;
	}
}

uint8_t DHCP_Request_IP(void)
{
	gl_DHCP_Packet_received=0;																		// Flag zur�cksetzen
	FAT32_Directory_Change("/");
	INI_Read_Key_String("basic.ini","DHCP","Name",&gl_DHCP_Cache.Name[0]);							// DHCP Namen laden

	gl_DHCP_Cache.Status = DHCP_Idle;																// Idle Stellung
	gl_DHCP_Cache.TransID = rand() & rand();
	
	if (DHCP_Send_Packet(DHCP_Discover,gl_DHCP_Cache.TransID)==0)									// Frage in die Runde nach einem DHCP Server
	{
		if (DHCP_Read_Option(53,&gl_DHCP_Option,&gl_DHCP_Head_Read)==0)
		{
			if (gl_DHCP_Option.Data[0] != DHCP_Offer)
			{
				return 1;																			// Falls keine Antwort kommt, lasse es sein
			}
		}
	}
	else
	{
		#ifdef DHCP_Debug
			USART_Write_String("DHCP: Keine Antwort nach Discovery\r\n");
		#endif
		return 1;
	}
	gl_DHCP_Cache.Status = DHCP_Offer;																// Setze Status auf Offer recvd
	memcpy(&gl_DHCP_Cache.Own_IP[0], &gl_DHCP_Head_Read.Your_IP_address[0], 4);						// Kopiere vorgeschlagene IP
			
	if(DHCP_Read_Option(1,&gl_DHCP_Option,&gl_DHCP_Head_Read)==0)									// Suche nach der Subnetzmaske in den Optionen
	{
		memcpy(&gl_DHCP_Cache.Subnetmask[0], &gl_DHCP_Option.Data[0], 4);
		memcpy(&gl_Webserver.Subnetmask[0],&gl_DHCP_Cache.Subnetmask[0],4);							// Kopiere Subnetzmaske
	}
	if(DHCP_Read_Option(3,&gl_DHCP_Option,&gl_DHCP_Head_Read)==0)									// Suche nach der Gateway IP in den Optionen
	{
		memcpy(&gl_DHCP_Cache.Gateway_IP[0], &gl_DHCP_Option.Data[0], 4);
		memcpy(&gl_Webserver.Gateway_IP[0],&gl_DHCP_Cache.Gateway_IP[0],4);							// Kopiere Gateway IP
	}
	if(DHCP_Read_Option(51,&gl_DHCP_Option,&gl_DHCP_Head_Read)==0)									// Suche nach der Lease in den Optionen
	{
		gl_DHCP_Cache.Lease = char_to_long_int(gl_DHCP_Option.Data[0],gl_DHCP_Option.Data[1],gl_DHCP_Option.Data[2],gl_DHCP_Option.Data[3]);
	}
	if(DHCP_Read_Option(42,&gl_DHCP_Option,&gl_DHCP_Head_Read)==0)									// Suche nach der NTP in den Optionen
	{
		memcpy(&gl_DHCP_Cache.NTP[0], &gl_DHCP_Option.Data[0], 4);
		memcpy(&gl_Webserver.NTP_IP[0],&gl_DHCP_Cache.NTP[0],4);									// NTP kopieren
	}
	if(DHCP_Read_Option(54,&gl_DHCP_Option,&gl_DHCP_Head_Read)==0)									// Suche nach der DHCP Server IP
	{
		memcpy(&gl_DHCP_Cache.DHCP_Server_IP[0], &gl_DHCP_Option.Data[0], 4);
	}
	
	gl_DHCP_Cache.Status = DHCP_Request;															// Setze Status auf Request
	if (DHCP_Send_Packet(DHCP_Request,gl_DHCP_Cache.TransID)==0)									// Frage in die Runde nach einem DHCP Server
	{
		if (gl_DHCP_Option.Data[0] != DHCP_ACK)
		{
			return 1;																				// Falls keine Antwort kommt, lasse es sein
		}
	}
	else
	{
		#ifdef DHCP_Debug
			USART_Write_String("DHCP: Keine Antwort nach Request\r\n");
		#endif
		return 1;
	}
	gl_DHCP_Cache.Status = DHCP_ACK;																// DHCP ACK
	memcpy(&gl_Webserver.IP_address[0],&gl_DHCP_Cache.Own_IP[0],4);									// Kopiere eigene IP als Webserv IP wenn ein DHCP_ACK empfangen wurde
	
	//Tasktimer_Add_Task(2,Time_Timestamp_to_UTC(&gl_Time)+gl_DHCP_Cache.Lease,gl_DHCP_Cache.Lease,(void*)&DHCP_Request_IP,PSTR("DHCP"));
	
	#ifdef DHCP_Debug
		USART_Write_String("DHCP: Netzwerkeinstellungen per DHCP bezogen\r\n");
	#endif
	return DHCP_ACK;
}

void DHCP_Read_Header(struct DHCP_Head *Head, struct IP_Head *IP_Header)
{
	if (char_to_long_int(gl_ENC_Readbuffer[278],gl_ENC_Readbuffer[279],gl_ENC_Readbuffer[280],gl_ENC_Readbuffer[281]) != 0x63825363)
	{
		#ifdef DHCP_Debug
			USART_Write_String("DHCP: Magic Cookie konnte nicht gelesen werden\r\n");
		#endif
	}
	
	Head->Message_Type = gl_ENC_Readbuffer[42];
	Head->Hardware_Type = gl_ENC_Readbuffer[43];
	Head->Hardware_address_length = gl_ENC_Readbuffer[44];
	Head->Hops = gl_ENC_Readbuffer[45];
	memcpy(&Head->Transaction_ID,&gl_ENC_Readbuffer[46],4);
	Head->Seconds_elapsed = (gl_ENC_Readbuffer[50]<<8)|gl_ENC_Readbuffer[51];
	Head->Bootp_flags = (gl_ENC_Readbuffer[52]<<8)|gl_ENC_Readbuffer[53];
	memcpy(&Head->Client_IP_address[0], &gl_ENC_Readbuffer[54],4);
	memcpy(&Head->Your_IP_address[0], &gl_ENC_Readbuffer[58],4);
	memcpy(&Head->Next_server_IP_address[0], &gl_ENC_Readbuffer[62],4);
	memcpy(&Head->Relay_agent_IP_address[0], &gl_ENC_Readbuffer[66],4);
	memcpy(&Head->Client_MAC_address[0],&gl_ENC_Readbuffer[70],16);
	memcpy(&Head->Server_Name[0],&gl_ENC_Readbuffer[86],64);
	memcpy(&Head->File[0],&gl_ENC_Readbuffer[150],128);
	Head->Magic_cookie = char_to_long_int(gl_ENC_Readbuffer[278],gl_ENC_Readbuffer[279],gl_ENC_Readbuffer[280],gl_ENC_Readbuffer[281]);
	memcpy(&Head->Options[0],&gl_ENC_Readbuffer[282],IP_Header->Totallength-20-8-240);
	
	#ifdef DHCP_Debug
		USART_Write_String("DHCP: Header gelesen\r\n");
	#endif
}

uint8_t DHCP_Read_Option(uint8_t Option, struct DHCP_Option *Return, struct DHCP_Head *Head)
{
	for (uint16_t g=0;g<308;g++)
	{
		if (Head->Options[g]==255)																	// Suchbereich durchlaufen und nichts gefunden
		{
			return 1;
		}
		
		if (Head->Options[g] == Option)
		{
			Return->Length = Head->Options[g+1];
			Return->Data = &Head->Options[g+2];
			return 0;
		}
		else
		{
			g += Head->Options[g+1]+1;																// 1+ kommt durch g++
		}		
	}
	return 1;																						// Einfach mal auf 1 falls was schiefgeht
}

uint8_t DHCP_Send_Packet(uint8_t Type, uint32_t Transaction_ID)
{	
	struct UDP_Head		UDP_Head_Write;
	struct IP_Head		IP_Head_Write;
	struct ENC_Head		ENC_Head_Write;
	struct DHCP_Head	DHCP_Head_Write;
	
	memset(&UDP_Head_Write,0,sizeof(UDP_Head_Write));											// Lokale Structs l�schen
	memset(&IP_Head_Write,0,sizeof(IP_Head_Write));
	memset(&ENC_Head_Write,0,sizeof(ENC_Head_Write));
	memset(&DHCP_Head_Write,0,sizeof(DHCP_Head_Write));
	
	uint8_t Namelen=strlen(gl_DHCP_Cache.Name);
	
	DHCP_Head_Write.Message_Type = 1;															// Request
	DHCP_Head_Write.Hardware_Type = 1;															// Ethernet
	DHCP_Head_Write.Hardware_address_length = 6;												// MAC is 6 Byte lang
	
	DHCP_Head_Write.Transaction_ID = Transaction_ID;
		
	DHCP_Head_Write.Seconds_elapsed=0;
	DHCP_Head_Write.Bootp_flags = Byteswap16(0x8000);											// Broadcastflag
	
	memcpy(&DHCP_Head_Write.Client_MAC_address[0],&gl_Webserver.MAC[0],6);						// MAC eintragen
	
	DHCP_Head_Write.Magic_cookie=0x63538263;
	
	DHCP_Head_Write.Options[0] = 0x35;															// DHCP Type
	DHCP_Head_Write.Options[1] = 0x01;
	DHCP_Head_Write.Options[2] = Type;
	
	DHCP_Head_Write.Options[3] = 0x0c;															// Hostname
	DHCP_Head_Write.Options[4] = Namelen;
	memcpy(&DHCP_Head_Write.Options[5],&gl_DHCP_Cache.Name[0],Namelen);
	
	if (Type == DHCP_Discover)
	{
		DHCP_Head_Write.Options[Namelen+5] = 0x37;												// Request Parameter List
		DHCP_Head_Write.Options[Namelen+6] = 3;
		DHCP_Head_Write.Options[Namelen+7] = 0x01;												// Subnet Mask
		DHCP_Head_Write.Options[Namelen+8] = 0x0f;												// Domain Name
		DHCP_Head_Write.Options[Namelen+9] = 0x03;												// Router
		DHCP_Head_Write.Options[Namelen+10] = 0xff;												// END
		UDP_Head_Write.Length = 240 + 8 + Namelen + 11;
		IP_Head_Write.Totallength = 240 + 8 + 20 + Namelen + 11;								// Totale Laenge IP + UDP + DHCP
	}
	if (Type == DHCP_Request)
	{
		memcpy(&DHCP_Head_Write.Next_server_IP_address[0],&gl_DHCP_Cache.DHCP_Server_IP[0],4);	// Bei Request die DHCP Server IP eintragen
		DHCP_Head_Write.Options[Namelen+5] = 50;												// Requested IP
		DHCP_Head_Write.Options[Namelen+6] = 4;
		DHCP_Head_Write.Options[Namelen+7] = gl_DHCP_Cache.Own_IP[0];
		DHCP_Head_Write.Options[Namelen+8] = gl_DHCP_Cache.Own_IP[1];
		DHCP_Head_Write.Options[Namelen+9] = gl_DHCP_Cache.Own_IP[2];
		DHCP_Head_Write.Options[Namelen+10] = gl_DHCP_Cache.Own_IP[3];
		DHCP_Head_Write.Options[Namelen+11] = 54;												// DHCP Server IP
		DHCP_Head_Write.Options[Namelen+12] = 4;
		DHCP_Head_Write.Options[Namelen+13] = gl_DHCP_Cache.DHCP_Server_IP[0];
		DHCP_Head_Write.Options[Namelen+14] = gl_DHCP_Cache.DHCP_Server_IP[1];
		DHCP_Head_Write.Options[Namelen+15] = gl_DHCP_Cache.DHCP_Server_IP[2];
		DHCP_Head_Write.Options[Namelen+16] = gl_DHCP_Cache.DHCP_Server_IP[3];
		DHCP_Head_Write.Options[Namelen+17] = 0xff;												// END
		
		UDP_Head_Write.Length = 240 + 8 + Namelen + 18;
		IP_Head_Write.Totallength = 240 + 8 + 20 + Namelen + 18;								// Totale Laenge IP + UDP + DHCP
	}

	UDP_Head_Write.Sourceport = DHCP_Destport;
	UDP_Head_Write.Destport = DHCP_Sourceport;

	UDP_Head_Write.Data = (char*) &DHCP_Head_Write;
	
	//IP_Header.Identifikation = 0x0005;															// Wird generiert
	IP_Head_Write.Flags = 0x00;																		// Flags 0
	IP_Head_Write.Fragment_Offset = 0x0000;															// Fragmentoffset 0
	IP_Head_Write.TTL = 128;																		// Time to live
	IP_Head_Write.Protocol = 17;																	// UDP
	memset(&IP_Head_Write.Dest_IP[0],0xff,4);														// Server Broadcast IP 
	memset(&ENC_Head_Write.Dest_MAC[0],0xff,6);														// Kopiere Broadcast MAC in ENC Struct
	memset(&IP_Head_Write.Source_IP[0],0,4);														// SourceIP auf 0
	if (Type != DHCP_Discover && Type != DHCP_Request)												// Wenn kein Discover oder Request trage die IP ein
	{
		memcpy(&IP_Head_Write.Source_IP[0],&gl_Webserver.IP_address[0],4);
	}	
	ENC_Head_Write.Type = 0x0800;																	// Type IP
	
	for(uint8_t g=0;g<DHCP_Retransmission_Max;g++)													// Fuehre maximal 10 Wiederholungen aus um eine DHCP Antwort zu erhalten
	{
		gl_DHCP_Packet_received=0;																	// Status zuruecksezen
		if (UDP_Send_Packet(&UDP_Head_Write, &IP_Head_Write, &ENC_Head_Write)!=0)					// Paket senden, DHCP Header liegt als Pointer auf UDP_Header->Daten
		{
			return 1;
		}
	
		for (uint16_t g=0;g<DHCP_Transmission_Frequency;g++)										// Warte ungefaehr eine Sekunde
		{
			_delay_us(500);
			Stack_Packetloop(0);																	// Loope vor dich hin, Kein ARP Reply Filtern
			if (gl_DHCP_Packet_received==1)															// Sobald Antwort erhalten, beende die Schleife
			{
				return 0;
			}
		}
		
		#ifdef DHCP_Debug
			USART_Write_String("DHCP: Warte auf Antwort von Server..\r\n");
		#endif
	}
	#ifdef DHCP_Debug
		USART_Write_String("DHCP: Keine Antwort von Server erhalten\r\n");
	#endif
	return 1;
}






