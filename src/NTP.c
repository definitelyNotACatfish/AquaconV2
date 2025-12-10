// Includes
#include "main.h"
//#include "delay.h"		// Ist in main.h als Inline
#include "NTP.h"
#include "ENC28J60.h"
#include "Stack.h"
#include "ds1307.h"
#include "Allerlei.h"
#include "DHCP.h"
#include "time.h"
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include "FAT32.h"
#include "INI_Parser.h"
#ifdef NTP_Debug
	#include "USART.h"
#endif

/*

************************************************************************************************
* NTP Protokoll											  									   *
* 2019 � Frederinn															 		   *
************************************************************************************************

*/

// Variablen
struct NTP_Head gl_NTP_Head_Read;																	// Speicher fuer Paketdaten

float gl_NTP_Timezone;																				// Standard Zeitzone
uint8_t gl_NTP_Summertime=0;																		// Sommer Winterzeit
uint8_t gl_NTP_Paket_received;																		// 0=Anfrage gesendet, 1=Antwort erhalten

// Funktionen
uint8_t NTP_Init(void)
{
	FAT32_Directory_Change("/");
	char Buffer[16];																				// Zwischenpuffer zum Konvertieren
	INI_Read_Key_String("basic.ini","NTP","Timezone",&Buffer[0]);									// Zeitzone laden
	sscanf(&Buffer[0],"%f",&gl_NTP_Timezone);
	INI_Read_Key_String("basic.ini","NTP","Summertime",&Buffer[0]);									// Sommerzeit laden
	sscanf(&Buffer[0],"%hhu",&gl_NTP_Summertime);

	if (NTP_Request_Time_from_Server(&gl_Webserver.NTP_IP[0])==0)									// Zeitstempel anfordern und eintragen
	{
		return 0;
	}
	return 1;
}

void NTP_Read_Header_from_Buffer(struct NTP_Head *Head)
{
	Head->Flags = gl_ENC_Readbuffer[42];
	Head->Peer_Clock_Stratum = gl_ENC_Readbuffer[43];
	Head->Peer_Polling_Interval = gl_ENC_Readbuffer[44];
	Head->Peer_Clock_Precision = gl_ENC_Readbuffer[45];
	Head->Root_Delay = char_to_long_int(gl_ENC_Readbuffer[46],gl_ENC_Readbuffer[47],gl_ENC_Readbuffer[48],gl_ENC_Readbuffer[49]);
	Head->Root_Dispersion = char_to_long_int(gl_ENC_Readbuffer[50],gl_ENC_Readbuffer[51],gl_ENC_Readbuffer[52],gl_ENC_Readbuffer[53]);
	Head->Reference_ID = char_to_long_int(gl_ENC_Readbuffer[54],gl_ENC_Readbuffer[55],gl_ENC_Readbuffer[56],gl_ENC_Readbuffer[57]);
	Head->Reference_Timestamp = char_to_long_long_int(&gl_ENC_Readbuffer[58]);
	Head->Origin_Timestamp = char_to_long_long_int(&gl_ENC_Readbuffer[66]);
	Head->Receive_Timestamp = char_to_long_long_int(&gl_ENC_Readbuffer[74]);
	Head->Transmit_Timestamp = char_to_long_long_int(&gl_ENC_Readbuffer[82]);
}

uint8_t NTP_Request_Time_from_Server(char *IP_Serv)														// Macht einen Request auf die gl_Webserver.NTP_IP auf Port 123
{	
	struct NTP_Head NTP_Head_Write;
	struct IP_Head IP_Head_Write;
	struct UDP_Head UDP_Head_Write;
	struct ENC_Head ENC_Head_Write;
	char MAC[6];
	
	memset(&NTP_Head_Write,0,sizeof(NTP_Head_Write));													// Buffer leeren
	memset(&IP_Head_Write,0,sizeof(IP_Head_Write));														// Buffer leeren
	memset(&UDP_Head_Write,0,sizeof(UDP_Head_Write));													// Buffer leeren
	memset(&ENC_Head_Write,0,sizeof(ENC_Head_Write));													// Buffer leeren
	
	
	if (ARP_Get_MAC_from_IP(&MAC[0],&IP_Serv[0]))														// Hole die Mac des TCP Senders
	{
		#ifdef NTP_Debug
			USART_Write_String("NTP: MAC Addresse konnte nicht ermittelt werden\r\n");
		#endif
		return 1;
	}
	
	NTP_Head_Write.Flags = 0xdb;																		// Client, NTP Version 3
	NTP_Head_Write.Peer_Clock_Stratum = 0x00;
	NTP_Head_Write.Peer_Polling_Interval = 17;
	NTP_Head_Write.Peer_Clock_Precision = 0x00;
	// Werte sind LSB first
	NTP_Head_Write.Root_Delay = 0x0;
	NTP_Head_Write.Root_Dispersion = 0x0;
	NTP_Head_Write.Reference_ID = 0x00000000;
	NTP_Head_Write.Reference_Timestamp = 0;
	NTP_Head_Write.Origin_Timestamp = 0;
	NTP_Head_Write.Receive_Timestamp = 0;
	NTP_Head_Write.Transmit_Timestamp = 0;
	//
	
	UDP_Head_Write.Sourceport = NTP_Sourceport;
	UDP_Head_Write.Destport = NTP_Destport;
	UDP_Head_Write.Length = 56;
	UDP_Head_Write.Data = (char*) &NTP_Head_Write;
	
	IP_Head_Write.Totallength = 76;																// Totale Laenge IP + UDP + NTP
	//IP_Header.Identifikation = 0x0005;														// Wird generiert
	IP_Head_Write.Flags = 0x00;																	// Flags 0
	IP_Head_Write.Fragment_Offset = 0x0000;														// Fragmentoffset 0
	IP_Head_Write.TTL = 128;																	// Time to live
	IP_Head_Write.Protocol = 17;																// UDP
	memcpy(&IP_Head_Write.Source_IP[0],&gl_Webserver.IP_address[0],4);							// Eigene IP addresse
	memcpy(&IP_Head_Write.Dest_IP[0],&IP_Serv[0],4);											// Server IP
	
	memcpy(&ENC_Head_Write.Dest_MAC[0],&MAC[0],6);												// Kopiere MAC in ENC Struct
	ENC_Head_Write.Type = 0x0800;																// Type IP
	
	for(uint8_t g=0;g<NTP_Retransmission_Max;g++)												// Fuehre maximal 10 Wiederholungen aus um eine NTP Antwort zu erhalten
	{
		gl_NTP_Paket_received=0;																// Status zuruecksezen
		if (UDP_Send_Packet(&UDP_Head_Write, &IP_Head_Write, &ENC_Head_Write)!=0)				// Paket senden, NTP Header liegt als Pointer auf UDP_Header->Daten
		{
			return 1;
		}

		for (uint16_t g=0;g<NTP_Transmission_Frequency;g++)										// Warte ungefaehr eine Sekunde bei 84MHz
		{
			_delay_us(1000);
			Stack_Packetloop(0);																// Loope vor dich hin, Kein ARP Replyfilter
			if (gl_NTP_Paket_received)															// Sobald Antwort erhalten, beende die Schleife
			{
				return 0;
			}
		}
		
		#ifdef NTP_Debug
			USART_Write_String("NTP: Warte auf Antwort von Zeitserver..\r\n");
		#endif
	}
	#ifdef NTP_Debug
		USART_Write_String("NTP: Keine Antwort von Zeitserver erhalten\r\n");
	#endif
	return 1;
}

uint8_t NTP_Monthlen(uint8_t isleapyear,uint8_t month)		// Gibt die Monatslaenge in Tage zurueck, abhaenging von Schaltjahr
{
	switch (month)
	{
		case 1: return 31;	//Jan
		break;
		case 2: return 28+isleapyear;	// Feb
		break;
		case 3: return 31;		//Maerz
		break;
		case 4: return 30;		// Apr
		break;
		case 5: return 31;		// Mai
		break;
		case 6: return 30;		// Juni
		break;
		case 7: return 31;		// Juli
		break;
		case 8: return 31;		// August
		break;
		case 9: return 30;		// Sept
		break;
		case 10: return 31;		// Okt
		break;
		case 11: return 30;		// Nov
		break;
		case 12: return 31;		// Dez
		break;
	}
	return 31;
}

void NTP_Convert_UTC_Seconds_to_Time(uint32_t Time, float Timezone, struct Timestamp *Timestamp_read)						// NTP UTC Convert mit Sommer- bzw. Winterzeitberechnung
{
	uint32_t UTC_seconds=0, Summertimestart=0, Summertimeend=0;
	uint8_t Weekday = 0;
	struct Timestamp temp_timestamp;
	
	Time_UTC_to_Timestamp(Time + (long int)(Timezone * 3600.0),Timestamp_read);														// Wandle erstmal die Sekunden in unser Datum um
	
	// M�gliches Datum des diesj�hrigen Sommerzeitanfang
	temp_timestamp.Day=25;
	temp_timestamp.Month=3;
	temp_timestamp.Year=Timestamp_read->Year;
	temp_timestamp.Hour=2;
	temp_timestamp.Minute=0;
	temp_timestamp.Seconds=0;
	//temp_timestamp.Weekday=0;
	
	UTC_seconds = Time_Timestamp_to_UTC(&temp_timestamp);																			// Wandle den Zeitstempel in UTC Sekunden um
	Weekday = (UTC_seconds / 86400) % 7;																							// Berechne den Wochentag
	
	if(Weekday == 6)																												// Falls gleich der Sommmerzeit am 25.3. Sommerzeit ist fahre fort
	{
		Summertimestart = UTC_seconds;
	}
	else
	{
		temp_timestamp.Day=25 + (6-Weekday);																						// Erechne den richtigen Tag
		temp_timestamp.Month=3;
		temp_timestamp.Year=Timestamp_read->Year;
		temp_timestamp.Hour=2;
		temp_timestamp.Minute=0;
		temp_timestamp.Seconds=0;
		//temp_timestamp.Weekday=0;
	
		Summertimestart = Time_Timestamp_to_UTC(&temp_timestamp);																	// Wandle ihn um
	}
	Summertimestart -= (long int)(Timezone * 3600.0);																				// Zeitzone muss mit einbezogen werden
	
	// M�gliches Datum der diesj�hrigen Sommerzeitende
	temp_timestamp.Day=25;
	temp_timestamp.Month=10;
	temp_timestamp.Year=Timestamp_read->Year;
	temp_timestamp.Hour=3;
	temp_timestamp.Minute=0;
	temp_timestamp.Seconds=0;
	//temp_timestamp.Weekday=0;
	
	UTC_seconds = Time_Timestamp_to_UTC(&temp_timestamp);																			// Wandle den Zeitstempel in UTC Sekunden um
	Weekday = (UTC_seconds / 86400) % 7;																							// Berechne den Wochentag
	
	if(Weekday == 6)																												// Falls gleich der Winterzeit am 25.10. ist fahre fort
	{
		Summertimeend = UTC_seconds;																								// Sommerzeitende setzen
	}
	else
	{
		temp_timestamp.Day=25 + (6-Weekday);																						// Erechne den richtigen Tag
		temp_timestamp.Month=10;
		temp_timestamp.Year=Timestamp_read->Year;
		temp_timestamp.Hour=2;
		temp_timestamp.Minute=0;
		temp_timestamp.Seconds=0;
		//temp_timestamp.Weekday=0;
	
		Summertimeend = Time_Timestamp_to_UTC(&temp_timestamp);																		// Wandle ihn um
	}
	Summertimeend -= (long int)(Timezone * 3600.0);																					// Zeitzone muss mit einbezogen werden
    
 	if((Time >= Summertimestart) && (Time < Summertimeend))
 	{
 		gl_NTP_Summertime = 1;
        Time += 3600UL + (long int)(Timezone * 3600.0);
        Time_UTC_to_Timestamp(Time ,Timestamp_read);											                                    // Wandle erstmal die Sekunden in unser Datum um
 	}
 	else
 	{
 		gl_NTP_Summertime = 0;
 	}
}

void NTP_Request_Func_for_Tasktimer(void)																							// Tasktimerfunktion
{
	NTP_Request_Time_from_Server(&gl_Webserver.NTP_IP[0]);
}

void NTP_Porthandler(void)
{
	NTP_Read_Header_from_Buffer(&gl_NTP_Head_Read);
	if (gl_NTP_Head_Read.Flags & 0x04)																								// Wenn Antwort von Server erhalten
	{
		gl_NTP_Paket_received=1;
		NTP_Convert_UTC_Seconds_to_Time((gl_NTP_Head_Read.Transmit_Timestamp>>32)&0xffffffff,gl_NTP_Timezone,&gl_Time);				// Setzt die Systemzeit mit Berechnung von Sommer- bzw. Winterzeit
		DS1307_Set_Timestamp(&gl_Time);																								// Setzte die Zeit
		DS1307_Read_Timestamp(&gl_Time);																							// Lese sie nochmal ein
		#ifdef NTP_Debug
			USART_Write_String("NTP: Zeitantwort erhalten\r\n");
		#endif
	}
}






