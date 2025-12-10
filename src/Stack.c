// Includes
#include "main.h"
#include "Stack.h"
#include "Allerlei.h"
#include "enc28j60.h"
#ifdef Use_external_RAM
	#include "23LC1024.h"
#endif
#include "NTP.h"
#include "ICMP.h"
#include "HTTP.h"
#include "USART.h"
#include "DHCP.h"
#include "ftp.h"
#include "FirmwareUpdater.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stm32f4xx.h>

/*

////////////////////////
char ip[4]={192,168,178,26};
uint8_t i= TCP_Open_Connection(90,4444,&ip[0],TCP_Connection_Active);
while(gl_TCP_Table[i].Status!=TCP_Established)
{
	Stack_Packetloop(0);
}
TCP_Write_Data(i,"Hallo",5);
TCP_Close_Connection(i,TCP_Connection_Active);
////////////////////////

Die Portlistener fuer UDP und TCP befinden sich in der Stack_Packetloop
TCP: MSS des Servers wird mit dem Client abgeglichen und gegebenfalls angepasst. Jedoch nie gr��er als TCP_MSS

************************************************************************************************
* Hier liegen alle Funktionen fuer den TCP/IP Stack drinnen, sammt ARP						   *
* 2019 � Frederinn															 		   *
************************************************************************************************

*/

// Globale Variablen
#ifdef Use_internal_RAM
	char gl_TCP_Retransmit_Packet_Backup[TCP_Max_Entries][ENC_Buffsize];									// TCP Retransmit Packet Backup
#endif
void (*gl_TCP_FP_Table[TCP_Max_Entries])(uint8_t);											// FP Array f�r TCP Retransmission
uint16_t gl_IP_Identification=1;																// Identifikationsnummer fuer die gesendeten Packete
const char gl_TCP_Status_Name_Table[11][13] = {"Closed","Listen","Syn_Sent","Syn_Recd","Established","Fin_Wait1","Fin_Wait2","Close_Wait","Last_Ack","Time_Wait","RST"};
struct ARP_Table		gl_ARP_Table[ARP_Max_Entries];												// ARP Tabelle
struct ARP_Head			gl_ARP_Head_Read;															// ARP Leseheader
struct IP_Head			gl_IP_Head_read;															// IP Leseheader
struct TCP_Head			gl_TCP_Head_read;															// TCP Leseheader
struct TCP_Table		gl_TCP_Table[TCP_Max_Entries];												// TCP Tabelle
struct UDP_Head			gl_UDP_Head_read;															// UDP Leseheader

void ARP_Read_Header_from_Buffer(struct ARP_Head *ARP_Header)
{
	memcpy(&ARP_Header->Hardwaretype,&gl_ENC_Readbuffer[14],2);
	memcpy(&ARP_Header->Protocoltype,&gl_ENC_Readbuffer[16],2);
	ARP_Header->Hardwaresize = gl_ENC_Readbuffer[18];
	ARP_Header->Protocolsize = gl_ENC_Readbuffer[19];
	memcpy(&ARP_Header->Opcode,&gl_ENC_Readbuffer[20],2);
	memcpy(&ARP_Header->Sender_MAC[0],&gl_ENC_Readbuffer[22],6);
	memcpy(&ARP_Header->Sender_IP[0],&gl_ENC_Readbuffer[28],4);
	memcpy(&ARP_Header->Target_MAC[0],&gl_ENC_Readbuffer[32],6);
	memcpy(&ARP_Header->Target_IP[0],&gl_ENC_Readbuffer[38],4);
	#ifdef Stack_Debug
		USART_Write_String("ARP: Header gelesen\r\n");
	#endif
}

void ARP_Create_Header_in_Buffer(struct ARP_Head *ARP_Header)
{
	gl_ENC_Write_Datapayload[0]=0x00;											// addresstyp
	gl_ENC_Write_Datapayload[1]=0x01;
	gl_ENC_Write_Datapayload[2]=0x08;											// Protocoltyp
	gl_ENC_Write_Datapayload[3]=0x00;
	gl_ENC_Write_Datapayload[4]=0x06;											// Hardwareaddressgroe�e
	gl_ENC_Write_Datapayload[5]=0x04;											// Protokolladdressgroe�e
	gl_ENC_Write_Datapayload[6]=0x00;
	gl_ENC_Write_Datapayload[7]=ARP_Header->Opcode;								// 0x02=Answer, 0x01=Request
	
	memcpy(&gl_ENC_Write_Datapayload[8],&ARP_Header->Sender_MAC[0],6);			// Quell MAC
	memcpy(&gl_ENC_Write_Datapayload[14],&ARP_Header->Sender_IP[0],4);			// Quell IP
	memcpy(&gl_ENC_Write_Datapayload[18],&ARP_Header->Target_MAC[0],6);			// Ziel MAC
	memcpy(&gl_ENC_Write_Datapayload[24],&ARP_Header->Target_IP[0],4);			// Ziel IP
}

uint8_t ARP_Send_Packet (struct ARP_Head *ARP_Header)
{		
	ENC_Clear_Writebuffer();													// Setze den Schreibpuffer auf 0
	ARP_Create_Header_in_Buffer(ARP_Header);
	
	if (ENC_Send_Packet(&ARP_Header->Target_MAC[0],0x0806,0x1000,28)==0)			// Sende das ARP Paket
	{
		#ifdef Stack_Debug
			USART_Write_String("ARP: Paket gesendet\r\n");
		#endif
		return 0;
	}
	else
	{
		#ifdef Stack_Debug
			USART_Write_String("ARP: Paket konnte nicht gesendet werden\r\n");
		#endif
		return 1;
	}
	return 1;
}

void ARP_Clear_Table(void)
{
	memset(&gl_ARP_Table,0,sizeof(gl_ARP_Table));
	#ifdef Stack_Debug
		USART_Write_String("ARP: Tabelle geloescht\r\n");
	#endif
}

uint8_t ARP_Position_in_Table(char *IP)															// Gebe die Position im Array zurueck an welcher stelle in der Tabelle sich die IP befindet
{
	for (uint8_t g=0;g<ARP_Max_Entries;g++)														// Suche in der Tabelle nach dem passenden Eintrag
	{		
		if (memcmp(&IP[0],&gl_ARP_Table[g].IP[0],4)==0)													// Wenn gefunden, dann gebe die Position zurueck
		{
			return g;
		}
	}
	return 255;																							// Sonst gebe einen Fehler zurueck
}

uint8_t ARP_Set_Entry(char *MAC, char *IP)
{
Again:
	for (uint8_t g=0;g<ARP_Max_Entries;g++)														// Suche Platz, wenn kein Platz dann loesche die Tabelle und nehme einen freien Platz
	{
		if(gl_ARP_Table[g].Used == 0 || memcmp(&IP[0],&gl_ARP_Table[g].IP[0],4)==0)						// Wenn am Speicherplatz die MAC 0 0 0 0 0 0, oder die IP bereits bekannt ist
		{	
			memcpy(&gl_ARP_Table[g].IP[0],&IP[0],4);													// IP
			memcpy(&gl_ARP_Table[g].MAC[0],&MAC[0],6);													// MAC
			gl_ARP_Table[g].Used = 1;																	// Used
			#ifdef Stack_Debug
				USART_Write_String("ARP: Eintrag erstellt.\r\n");
			#endif
			return 0;
		}
	}
	ARP_Clear_Table();
	goto Again;
	return 1;																							// Wird momentan nie erreicht
}

uint8_t ARP_Clear_Entry(char *IP)
{
	uint8_t Position = ARP_Position_in_Table(&IP[0]);
	
	if (Position!=255)
	{
		memset(&gl_ARP_Table[Position],0,sizeof(gl_ARP_Table[0]));
		#ifdef Stack_Debug
			USART_Write_String("ARP: Eintrag geloescht\r\n");
		#endif
		return 0;
	} 

	#ifdef Stack_Debug
		USART_Write_String("ARP: Eintrag konnte nicht geloescht werden\r\n");
	#endif
	return 1;
}

uint8_t ARP_Get_MAC_from_IP(char *Return_MAC,char *IP)											// Prueft ob die IP in der Tabelle steht und aktuell ist, ansonsten fordert die funktion die aktuelle IP an
{
	uint32_t Subnetzmaske=0,IP1=0,My_IP=0;
	char IP_Temp[4];																					// Temp Speicher f�r IP
	struct ARP_Head ARP_Head_write;
	
	memcpy(&My_IP,&gl_Webserver.IP_address[0],4);														// Lokale Kopie der �C IP Addresse
	memcpy(&Subnetzmaske,&gl_Webserver.Subnetmask[0],4);												// Lokale Kopie der �C Subnetzmaske
	memcpy(&IP1,&IP[0],4);																				// uint32_t der uebergebenen IP									
	memcpy(&IP_Temp[0],&IP[0],4);																		// Tempr�re IP
	
	if ((IP1 & Subnetzmaske) == (My_IP & Subnetzmaske))													// Wenn die IP und der �C sich im gleichen Subnetzwerk befinden fahre fort
	{
		#ifdef Stack_Debug
			USART_Write_String("ARP: Gemeinsames Subnetzwerk\r\n");
		#endif
	} 
	else																								// Oder gebe die IP des Standardgateway an
	{
		memcpy(&IP_Temp[0],&gl_Webserver.Gateway_IP[0],4);												// IP
		#ifdef Stack_Debug
			USART_Write_String("ARP: Ungleiches Subnetzwerk, Weiterleitung an Router\r\n");
		#endif
	}
	
	if (ARP_Position_in_Table(&IP_Temp[0])!=255)														// Gebe fuer die MAC die IP zurueck
	{
		#ifdef Stack_Debug
			USART_Write_String("ARP: Gueltiger Eintrag fuer die IP gefunden\r\n");
		#endif
		ARP_Read_MAC_from_Table(&Return_MAC[0],&IP_Temp[0]);											// Gibt bei Antwort die Mac addresse zurueck, ansonsten ein ff-ff-ff-ff-ff-ff
		return 0;
	}
	
	for(uint8_t g=0;g<10;g++)																			// Fuehre maximal 10 Wiederholungen aus um eine ARP Antwort zu erhalten
	{	
		ARP_Head_write.Opcode = ARP_Request;
		memcpy(&ARP_Head_write.Sender_MAC[0],&gl_Webserver.MAC[0],6);
		memcpy(&ARP_Head_write.Sender_IP[0],&gl_Webserver.IP_address[0],4);
		memset(&ARP_Head_write.Target_MAC[0],0xff,6);													// Standardrequestadresse ff-ff-ff-ff-ff-ff
		memcpy(&ARP_Head_write.Target_IP[0],&IP_Temp[0],4);
		ARP_Send_Packet(&ARP_Head_write);
		
		for (uint16_t i=0;i<1000;i++)																	// Warte ungefaehr eine Sekunde
		{
			_delay_us(1000);
			Stack_Packetloop(1);																		// Loope vor dich hin, Warte bis ARP Reply kommt
			
			if (ARP_Position_in_Table(&IP_Temp[0])!=255)
			{
				ARP_Read_MAC_from_Table(&Return_MAC[0],&IP_Temp[0]);									// Gibt bei Antwort die Mac addresse zurueck, ansonsten ein ff-ff-ff-ff-ff-ff
				return 0;
			}
		}

		#ifdef Stack_Debug
			USART_Write_String("ARP: Warte auf ARP Antwort Paket\r\n");
		#endif
	}
	ARP_Read_MAC_from_Table(&Return_MAC[0],&IP_Temp[0]);												// Gibt bei Antwort die Mac addresse zurueck, ansonsten ein ff-ff-ff-ff-ff-ff
	return 1;														
}

void ARP_Read_MAC_from_Table(char *Return_MAC, char *IP)												// Gibt bei Fehler 0-String zurueck
{
	uint8_t Position_in_Table = ARP_Position_in_Table(&IP[0]);
	
	if (Position_in_Table == 255)
	{
		memset(&Return_MAC[0],0xff,6);																	// Broadcast Addresse eintragen
		return;
	}
	
	memcpy(&Return_MAC[0],&gl_ARP_Table[Position_in_Table].MAC[0],6);
	return;
}

void IP_Create_Header_in_Buffer(struct IP_Head *Head)
{
	uint16_t Erg=0;																		// Ergebnis CRC Rechnung
	
	Head->Identifikation = gl_IP_Identification;											// Identifikation der einzelnen Pakete, einfacher Zaehler
	gl_IP_Identification++;
	
	ENC_Clear_Writebuffer();																// Loesche den Writebuffer

	gl_ENC_Write_Datapayload[0] = 0x45;														// Version 4, IHL 32*5
	gl_ENC_Write_Datapayload[1]	= 0x00;														// Diensttyp, normal 0
	gl_ENC_Write_Datapayload[2] = (Head->Totallength>>8)&0xff;								// Highbyte der Gesamtlaenge
	gl_ENC_Write_Datapayload[3] = Head->Totallength&0xff;									// Lowbyte der Gesamtlaenge
	gl_ENC_Write_Datapayload[4] = (Head->Identifikation>>8)&0xff;							// Highbyte der Kennung
	gl_ENC_Write_Datapayload[5] = Head->Identifikation&0xff;
	gl_ENC_Write_Datapayload[6] = (Head->Flags<<5) | ((Head->Fragment_Offset>>8)&0xff);		// Fuege die Flags und das Fragment_Offset Highbyte ein
	gl_ENC_Write_Datapayload[7] = Head->Fragment_Offset&0xff;								// Lowbyte Fragment_Offset
	gl_ENC_Write_Datapayload[8] = Head->TTL;												// Time to Life
	gl_ENC_Write_Datapayload[9] = Head->Protocol;											// Protokolltyp
	
	memcpy(&gl_ENC_Write_Datapayload[12],&Head->Source_IP[0],4);							// Sender IP
	memcpy(&gl_ENC_Write_Datapayload[16],&Head->Dest_IP[0],4);								// Empf�nger IP
	
	Erg = IP_Calc_Checksum(20,&gl_ENC_Write_Datapayload[0]);								// Checksum fuer den IP Header
	gl_ENC_Write_Datapayload[10] = (Erg>>8)&0xff;
	gl_ENC_Write_Datapayload[11] = Erg&0xff;
}

uint8_t IP_Read_Header_from_Buffer(struct IP_Head *Head)
{
	uint16_t Checksum=0;
	
	Head->Totallength = (gl_ENC_Readbuffer[16]<<8) | gl_ENC_Readbuffer[17];
	Head->Identifikation = (gl_ENC_Readbuffer[18]<<8) | gl_ENC_Readbuffer[19];
	Head->Flags = (gl_ENC_Readbuffer[20]&0xe0)>>5;											// Filtere die 3 obersten Bits aus und rotiere sie um 5 nach rechts
	Head->Fragment_Offset = ((gl_ENC_Readbuffer[20]<<8) | gl_ENC_Readbuffer[21])&0x1fff;	// Filtere die unteren 13 Bits aus
	Head->TTL = gl_ENC_Readbuffer[22];														// TTL
	Head->Protocol = gl_ENC_Readbuffer[23];													// Protokoll
	Head->Checksum = (gl_ENC_Readbuffer[24]<<8) | gl_ENC_Readbuffer[25];					// Checksum
	
	memcpy(&Head->Source_IP[0],&gl_ENC_Readbuffer[26],4);									// SorceIP
	memcpy(&Head->Dest_IP[0],&gl_ENC_Readbuffer[30],4);										// DestIP
	
	gl_ENC_Readbuffer[24]=0x00;																// Loesche die Checksum
	gl_ENC_Readbuffer[25]=0x00;																//
	
	Checksum = IP_Calc_Checksum(20,&gl_ENC_Readbuffer[14]);									// Berechne die Checksum des erhaltenen Headers
	
	if (Checksum == Head->Checksum)
	{
		#ifdef Stack_Debug
			USART_Write_String("IP4: Checksumme korrekt, IP Header akzeptiert\r\n");
		#endif
		return 0;
	}
	else
	{
		#ifdef Stack_Debug
			printf("IP4: IP Checksumme falsch. Ist: 0x%02x, soll: 0x%02x\r\n",Checksum,Head->Checksum);
		#endif
		return 1;
	}
	return 1;
}

uint16_t IP_Calc_Checksum(uint16_t len_ip_header, char *buff)
{
	uint16_t word16=0,i=0;
	uint32_t sum=0;
	uint8_t padding=0;
	
	// If the length is odd then add the padding byte to make it even
	if (len_ip_header%2)
	{
		padding=1;
		buff[len_ip_header] = 0;
	}
	// make 16 bit words out of every two adjacent 8 bit words in the packet
	// and add them up
	for (i=0;i<len_ip_header+padding;i=i+2)
	{
		word16 = (buff[i]<<8)+buff[i+1];
		sum += (uint32_t) word16;
	}
	// take only 16 bits out of the 32 bit sum and add up the carries
	while (sum>>16)
	{
		sum = (sum & 0xFFFF)+(sum >> 16);
	}
	// one's complement the result
	sum = ~sum;
	
	return sum & 0xffff;
}

uint8_t UDP_Read_Header_from_Buffer(struct UDP_Head *UDP_Header, struct IP_Head *IP_Header)		// Lese Daten in Header, ueberpruefe Checksumme 0=ok, 1=nok
{
	uint16_t Checksum=0;

	UDP_Header->Sourceport = (gl_ENC_Readbuffer[34]<<8) | gl_ENC_Readbuffer[35];		// Sourceport
	UDP_Header->Destport = (gl_ENC_Readbuffer[36]<<8) | gl_ENC_Readbuffer[37];			// Destport
	UDP_Header->Length = (gl_ENC_Readbuffer[38]<<8) | gl_ENC_Readbuffer[39];			// Laenge
	UDP_Header->Checksum = (gl_ENC_Readbuffer[40]<<8) | gl_ENC_Readbuffer[41];			// Checksumme
	UDP_Header->Data = &gl_ENC_Readbuffer[42];
	
	gl_ENC_Readbuffer[41]=gl_ENC_Readbuffer[40]=0x00;									// Loesche die Checksum

	memcpy(&UDP_Header->Pseudoheader.Dest_IP[0],&IP_Header->Dest_IP[0],4);				// DestIP
	memcpy(&UDP_Header->Pseudoheader.Source_IP[0],&IP_Header->Source_IP[0],4);			// SourceIP
	UDP_Header->Pseudoheader.Protocoll_ID = 17;
	UDP_Header->Pseudoheader.UDP_Datagram_Length = IP_Header->Totallength - 20;			// Laenge des UDP Datagrams
	
	Checksum = UDP_Calc_Checksum(UDP_Header,&gl_ENC_Readbuffer[34]);					// Berechne die Checksum des erhaltenen Headers
	gl_ENC_Readbuffer[40] = (UDP_Header->Checksum >> 8)&0xff;							// Trage alte Checksumme wieder ein
	gl_ENC_Readbuffer[41] = UDP_Header->Checksum & 0xff;
	
	if (Checksum == UDP_Header->Checksum)
	{
		#ifdef Stack_Debug
			USART_Write_String("IP4: Checksumme korrekt, UDP Header akzeptiert\r\n");
		#endif
		return 0;
	}
	//else if(UDP_Header->Checksum == 0)
	//{
	//	#ifdef Stack_Debug
	//		USART_Write_String("IP4: Checksumme = 0, UDP Header trotzdem akzeptiert\r\n");
	//	#endif
	//	return 0;
	//}
	else
	{
		#ifdef Stack_Debug
			printf("IP4: UDP Checksumme falsch. Ist: 0x%02x, soll: 0x%02x\r\n",Checksum,UDP_Header->Checksum);
		#endif
		return 1;
	}
	return 1;
}

void UDP_Create_Header_in_Buffer(struct UDP_Head *UDP_Header,struct IP_Head *IP_Header)	// Schreibe Daten in Buffer, generiere Checksumme
{
	uint16_t Checksum=0,Templength;
	
	gl_ENC_Write_Datapayload[20] = (UDP_Header->Sourceport >> 8)&0xff;					// Sourceport Highbyte
	gl_ENC_Write_Datapayload[21] = UDP_Header->Sourceport &0xff;						// Sourceport Lowbyte
	gl_ENC_Write_Datapayload[22] = (UDP_Header->Destport >> 8)&0xff;					// Destport Highbyte
	gl_ENC_Write_Datapayload[23] = UDP_Header->Destport &0xff;							// Destport Lowbyte
	gl_ENC_Write_Datapayload[24] = (UDP_Header->Length >> 8)&0xff;						// Length Highbyte
	gl_ENC_Write_Datapayload[25] = UDP_Header->Length &0xff;							// Length Lowbyte
	gl_ENC_Write_Datapayload[26] = gl_ENC_Write_Datapayload[27] = 0x00;					// Loesche die Checksum
	
	Templength = UDP_Header->Length-8;													// Header schon abziehen
																
	for (uint16_t g=0;g<Templength;g++)												// Daten kopieren
	{
		gl_ENC_Write_Datapayload[g+28] = UDP_Header->Data[g];
	}
	
	memcpy(&UDP_Header->Pseudoheader.Dest_IP[0],&IP_Header->Dest_IP[0],4);				// DestIP
	memcpy(&UDP_Header->Pseudoheader.Source_IP[0],&IP_Header->Source_IP[0],4);			// SourceIP
	UDP_Header->Pseudoheader.Protocoll_ID = 17;
	UDP_Header->Pseudoheader.UDP_Datagram_Length = IP_Header->Totallength - 20;			// Laenge des UDP Datagrams
	
	Checksum = UDP_Calc_Checksum(UDP_Header,&gl_ENC_Write_Datapayload[20]);				// Berechne die Checksum des erhaltenen Headers
	gl_ENC_Write_Datapayload[26] = (Checksum >> 8) & 0xff;								// Checksumme Highbyte 
	gl_ENC_Write_Datapayload[27] = Checksum & 0xff;										// Checksumme Lowbyte
}

uint16_t UDP_Calc_Checksum(struct UDP_Head *Head, char *buff)
{
	uint16_t word16=0,i=0;
	uint32_t sum=0;
	uint8_t padding=0;
	
	word16 = (Head->Pseudoheader.Source_IP[0]<<8) + Head->Pseudoheader.Source_IP[1];
	sum += (uint32_t)word16;
	word16 = (Head->Pseudoheader.Source_IP[2]<<8) + Head->Pseudoheader.Source_IP[3];
	sum += (uint32_t)word16;
	word16 = (Head->Pseudoheader.Dest_IP[0]<<8) + Head->Pseudoheader.Dest_IP[1];
	sum += (uint32_t)word16;
	word16 = (Head->Pseudoheader.Dest_IP[2]<<8) + Head->Pseudoheader.Dest_IP[3];
	sum += (uint32_t)word16;
	word16 = Head->Pseudoheader.Protocoll_ID;
	sum += (uint32_t)word16;
	word16 = Head->Pseudoheader.UDP_Datagram_Length;
	sum += (uint32_t)word16;
	
	// If the length is odd then add the padding byte to make it even
	if (Head->Length%2)
	{
		padding=1;
		buff[Head->Length] = 0;
	}
	
	// make 16 bit words out of every two adjacent 8 bit words in the packet
	// and add them up
	for (i=0;i<Head->Length+padding;i=i+2)
	{
		word16 = (buff[i]<<8)+buff[i+1];
		sum += (uint32_t)word16;
	}
	// take only 16 bits out of the 32 bit sum and add up the carries
	while (sum>>16)
	{
		sum = (sum & 0xFFFF)+(sum >> 16);
	}
	// one's complement the result
	sum = ~sum;
	
	return sum & 0xffff;
}

uint8_t UDP_Send_Packet(struct UDP_Head *UDP_Header, struct IP_Head *IP_Header, struct ENC_Head *ENC_Header)					// Paket senden, UDP Header liegt als Pointer auf UDP_Header->Daten
{
	IP_Create_Header_in_Buffer(IP_Header);
	UDP_Create_Header_in_Buffer(UDP_Header,IP_Header);
	if (ENC_Send_Packet(&ENC_Header->Dest_MAC[0],ENC_Header->Type,0x1000, IP_Header->Totallength)==0)								// Sende das NTP Paket
	{
		#ifdef Stack_Debug
			USART_Write_String("UDP: Paket gesendet\r\n");
		#endif
		return 0;
	}
	else
	{
		#ifdef Stack_Debug
			USART_Write_String("UDP: Paket konnte nicht gesendet werden\r\n");
		#endif
		return 1;
	}
	return 1;
}

void TCP_Create_Header_in_Buffer(struct TCP_Head *TCP_Header, struct IP_Head *IP_Header)		// Erstellt einen TCP Header im Buffer
{
	uint8_t Dataoffset_in_Bytes = 20 + (TCP_Header->Dataoffset*4);						// Position im Buffer an der die TCP Daten anfangen
	
	gl_ENC_Write_Datapayload[20] = (TCP_Header->Sourceport>>8)&0xff;							// Highbyte des Sourceport
	gl_ENC_Write_Datapayload[21] = TCP_Header->Sourceport&0xff;									// Lowbyte des Sourceport
	gl_ENC_Write_Datapayload[22] = (TCP_Header->Destport>>8)&0xff;								// Highbyte des Destinationport
	gl_ENC_Write_Datapayload[23] = TCP_Header->Destport&0xff;									// Lowbyte des Destinationport
	gl_ENC_Write_Datapayload[24] = (TCP_Header->Sequencenumber>>24)&0xff;						// Highbyte der Sequenznummer
	gl_ENC_Write_Datapayload[25] = (TCP_Header->Sequencenumber>>16)&0xff;
	gl_ENC_Write_Datapayload[26] = (TCP_Header->Sequencenumber>>8)&0xff;
	gl_ENC_Write_Datapayload[27] = TCP_Header->Sequencenumber&0xff;								// Lowbyte der Sequenznummer
	gl_ENC_Write_Datapayload[28] = (TCP_Header->Acknumber>>24)&0xff;							// Highbyte der Acknummer
	gl_ENC_Write_Datapayload[29] = (TCP_Header->Acknumber>>16)&0xff;
	gl_ENC_Write_Datapayload[30] = (TCP_Header->Acknumber>>8)&0xff;
	gl_ENC_Write_Datapayload[31] = TCP_Header->Acknumber&0xff;									// Lowbyte der Acknummer
	gl_ENC_Write_Datapayload[32] = TCP_Header->Dataoffset<<4;									// Rotiere den Dataoffset an die richtige Stelle
	gl_ENC_Write_Datapayload[33] = TCP_Header->Flags;											// Flags
	gl_ENC_Write_Datapayload[34] = (TCP_Header->Window>>8)&0xff;								// Highbyte des Window
	gl_ENC_Write_Datapayload[35] = TCP_Header->Window&0xff;										// Lowbyte des Window
	gl_ENC_Write_Datapayload[38] = (TCP_Header->Urgentpointer>>8)&0xff;							// Highbyte des Urgentpointer
	gl_ENC_Write_Datapayload[39] = TCP_Header->Urgentpointer&0xff;								// Lowbyte des Urgentpointer
	
	memcpy(&gl_ENC_Write_Datapayload[40],&TCP_Header->Options[0],Dataoffset_in_Bytes-40);		// Optionen
	memcpy(&gl_ENC_Write_Datapayload[Dataoffset_in_Bytes],&TCP_Header->Data[0],IP_Header->Totallength-Dataoffset_in_Bytes);				// Daten

	// Pseudoheader kopieren und Checksumme generieren
	memcpy(&TCP_Header->Pseudoheader.Dest_IP[0],&IP_Header->Dest_IP[0],4);						// DestIP
	memcpy(&TCP_Header->Pseudoheader.Source_IP[0],&IP_Header->Source_IP[0],4);					// SourceIP
	TCP_Header->Pseudoheader.Protocoll_ID = 6;
	TCP_Header->Pseudoheader.TCP_Datagram_Length = IP_Header->Totallength - 20;					// Laenge des UDP Datagrams
	TCP_Header->Checksum = TCP_Calc_Checksum(TCP_Header,&gl_ENC_Write_Datapayload[20]);
	gl_ENC_Write_Datapayload[36] = (TCP_Header->Checksum>>8)&0xff;
	gl_ENC_Write_Datapayload[37] = TCP_Header->Checksum&0xff;
}

uint8_t TCP_Read_Header_from_Buffer(struct TCP_Head *TCP_Header, struct IP_Head *IP_Header)
{
	uint16_t Dataoffset_in_Bytes=0, Checksum=0;
	
	memset(&TCP_Header->Options[0],0x00,40);
	TCP_Header->Sourceport = (gl_ENC_Readbuffer[34]<<8)|gl_ENC_Readbuffer[35];					// Ursprungsport
	TCP_Header->Destport = (gl_ENC_Readbuffer[36]<<8)|gl_ENC_Readbuffer[37];					// Zielport
	TCP_Header->Sequencenumber = char_to_long_int(gl_ENC_Readbuffer[38],gl_ENC_Readbuffer[39],gl_ENC_Readbuffer[40],gl_ENC_Readbuffer[41]);	// Sequenznummer
	TCP_Header->Acknumber = char_to_long_int(gl_ENC_Readbuffer[42],gl_ENC_Readbuffer[43],gl_ENC_Readbuffer[44],gl_ENC_Readbuffer[45]);		// Acknummer
	TCP_Header->Dataoffset = (gl_ENC_Readbuffer[46]&0xf0)>>4;									// Dataoffset in 32 Bitbl�cken
	TCP_Header->Flags = gl_ENC_Readbuffer[47];													// Flags
	TCP_Header->Window = (gl_ENC_Readbuffer[48]<<8)|gl_ENC_Readbuffer[49];						// Windows
	TCP_Header->Checksum = (gl_ENC_Readbuffer[50]<<8)|gl_ENC_Readbuffer[51];					// Checksumme
	TCP_Header->Urgentpointer = (gl_ENC_Readbuffer[52]<<8)|gl_ENC_Readbuffer[53];				// Urgent Pointer
	
	gl_ENC_Readbuffer[50]=gl_ENC_Readbuffer[51]=0x00;											// Loesche die Checksum
	
	memcpy(&TCP_Header->Pseudoheader.Dest_IP[0],&IP_Header->Dest_IP[0],4);						// DestIP
	memcpy(&TCP_Header->Pseudoheader.Source_IP[0],&IP_Header->Source_IP[0],4);					// SourceIP
	TCP_Header->Pseudoheader.Protocoll_ID = 6;
	TCP_Header->Pseudoheader.TCP_Datagram_Length = IP_Header->Totallength - 20;					// Laenge des UDP Datagrams
	
	Dataoffset_in_Bytes = (TCP_Header->Dataoffset*4)+34;										// Offset in Bytes im ENC Buffer

	for(uint16_t g=54,z=0;g<Dataoffset_in_Bytes;g++,z++)									// Trage die Optionen ein
	{
		TCP_Header->Options[z] = gl_ENC_Readbuffer[g];
	}

	TCP_Header->Data = &gl_ENC_Readbuffer[Dataoffset_in_Bytes];									// Pointer auf erstes Datenbyte im Readbuffer zeigen
	TCP_Header->Datalength = IP_Header->Totallength - 20 - 20 - (Dataoffset_in_Bytes - 54);		// Anzahl der Datenbytes
	
	Checksum = TCP_Calc_Checksum(TCP_Header,&gl_ENC_Readbuffer[34]);							// Checksumme berechnen
	gl_ENC_Readbuffer[50] = (TCP_Header->Checksum >> 8) & 0xff;									// Checksumme wieder eintragen
	gl_ENC_Readbuffer[51] = TCP_Header->Checksum & 0xff;
	
	if (TCP_Header->Checksum == Checksum)			// Berechne die Checksum des erhaltenen Headers)
	{
		#ifdef Stack_Debug
			USART_Write_String("TCP: Checksumme korrekt, TCP Paket akzeptiert\r\n");
		#endif
		return 0;
	}
	//else if (TCP_Header->Checksum == 0)
	//{
	//	#ifdef Stack_Debug
	//		USART_Write_String("TCP: Checksumme = 0, TCP Paket trotzdem akzeptiert\r\n");
	//	#endif
	//	return 0;
	//}
	else
	{
		#ifdef Stack_Debug
			USART_Write_String("TCP: Checksum nicht korrekt!\r\n");
		#endif
		return 1;
	}
}

uint16_t TCP_Read_MSS_Option_from_Buffer(void)														// return 0=Error, return>0=ok
{
	for (uint8_t g=0;g<40;g=g+4)
	{
		if (gl_TCP_Head_read.Options[g]==0x02 && gl_TCP_Head_read.Options[g+1]==0x04)
		{
			return (gl_TCP_Head_read.Options[g+2]<<8)|gl_TCP_Head_read.Options[g+3];
		}
	}
	return 0;
}

uint8_t TCP_Read_WS_Option_from_Buffer(void)														// return<15=ok, return 15=Error
{
	for (uint8_t g=0;g<40;g=g+4)
	{
		if (gl_TCP_Head_read.Options[g]==0x01 && gl_TCP_Head_read.Options[g+1]==0x03)
		{
			return gl_TCP_Head_read.Options[g+3];
		}
	}
	return 15;
}

uint16_t TCP_Calc_Checksum(struct TCP_Head *Head, char *buff)
{
	uint16_t word16=0,i=0;
	uint32_t sum=0;
	uint8_t padding=0;
	
	word16 = (Head->Pseudoheader.Source_IP[0]<<8) + Head->Pseudoheader.Source_IP[1];
	sum += (unsigned long)word16;
	word16 = (Head->Pseudoheader.Source_IP[2]<<8) + Head->Pseudoheader.Source_IP[3];
	sum += (unsigned long)word16;
	word16 = (Head->Pseudoheader.Dest_IP[0]<<8) + Head->Pseudoheader.Dest_IP[1];
	sum += (unsigned long)word16;
	word16 = (Head->Pseudoheader.Dest_IP[2]<<8) + Head->Pseudoheader.Dest_IP[3];
	sum += (unsigned long)word16;
	word16 = Head->Pseudoheader.Protocoll_ID;
	sum += (unsigned long)word16;
	word16 = Head->Pseudoheader.TCP_Datagram_Length;
	sum += (unsigned long)word16;
	
	// If the length is odd then add the padding byte to make it even
	if (Head->Pseudoheader.TCP_Datagram_Length%2)
	{
		padding=1;
		buff[Head->Pseudoheader.TCP_Datagram_Length] = 0;
	}
	
	// make 16 bit words out of every two adjacent 8 bit words in the packet
	// and add them up
	for (i=0;i<Head->Pseudoheader.TCP_Datagram_Length+padding;i=i+2)
	{
		word16 = (buff[i]<<8)+buff[i+1];
		sum += (unsigned long)word16;
	}
	// take only 16 bits out of the 32 bit sum and add up the carries
	while (sum>>16)
	{
		sum = (sum & 0xFFFF)+(sum >> 16);
	}
	// one's complement the result
	sum = ~sum;
	
	return sum & 0xffff;
}

uint8_t TCP_Send_Packet(struct TCP_Head *TCP_Header, struct IP_Head *IP_Header, struct ENC_Head *ENC_Header)			// Paket senden, Daten liegen als Pointer auf TCP_Header->Daten 0=ok, 1=nok
{
	uint8_t Position_in_Table = TCP_Get_Position_from_Table(&IP_Header->Dest_IP[0], TCP_Header->Destport);			// IP_Header->Dest_IP = (*IP_Header).Dest_IP
	
	if (Position_in_Table==255)																								// Falls mal die Position nicht gefunden wird breche ab. Wegen Debug Server an Fritzbox, �bergabepointer stimmen manchmal nicht??
	{
		#ifdef Stack_Debug
			char Status[14];
			printf("IP: %u.%u.%u.%u Destport: %u\r\n",IP_Header->Dest_IP[0],IP_Header->Dest_IP[1],IP_Header->Dest_IP[2],IP_Header->Dest_IP[3],TCP_Header->Destport);
			for (uint8_t g=0;g<TCP_Max_Entries;g++)
			{
				printf("In Benutzung: %u IP: %u.%u.%u.%u",gl_TCP_Table[g].In_use, gl_TCP_Table[g].Client_IP[0], gl_TCP_Table[g].Client_IP[1], gl_TCP_Table[g].Client_IP[2], gl_TCP_Table[g].Client_IP[3]);
				printf(" Cl.port: %u  Ser.port: %u",gl_TCP_Table[g].Destport, gl_TCP_Table[g].Sourceport);
				strcpy(&Status[0],&gl_TCP_Status_Name_Table[gl_TCP_Table[g].Status][0]);
				printf(" Status: %s\r\n", Status);
			}
			
			USART_Write_String("TCP: Paket senden: Position nicht gefunden\r\n");
		#endif
		return 1;
	}
	
	IP_Create_Header_in_Buffer(IP_Header);
	TCP_Create_Header_in_Buffer(TCP_Header,IP_Header);
	
	gl_TCP_Table[Position_in_Table].Retransmission.Retransmitcount=0;															// Connectiontimer zur�cksetzen
	gl_TCP_Table[Position_in_Table].Retransmission.Length = IP_Header->Totallength;												// Paketl�nge sichern
	memcpy(&gl_TCP_Table[Position_in_Table].Retransmission.MAC[0],&ENC_Header->Dest_MAC[0],6);									// Mac sichern
	
	#ifdef Use_external_RAM
		SRAM_23LC1024_Write_Bytes(gl_TCP_Table[Position_in_Table].Retransmission.SRAM_Address,gl_TCP_Table[Position_in_Table].Retransmission.Length,&gl_ENC_Write_Datapayload[0]);	// Paket in Backup legen
	#endif
	#ifdef Use_internal_RAM
		memcpy(&gl_TCP_Retransmit_Packet_Backup[Position_in_Table][0],&gl_ENC_Write_Datapayload[0],gl_TCP_Table[Position_in_Table].Retransmission.Length);							// Paket in Backup legen
	#endif
	
	if (ENC_Send_Packet(&ENC_Header->Dest_MAC[0],ENC_Header->Type,0x1000, IP_Header->Totallength)==0)							// Sende das TCP Paket
	{
		#ifdef Stack_Debug
			USART_Write_String("TCP: Paket gesendet\r\n");
		#endif
		return 0;
	}
	else
	{
		#ifdef Stack_Debug
			USART_Write_String("TCP: Paket konnte nicht gesendet werden\r\n");
		#endif
		return 1;
	}
	return 1;
}

uint8_t TCP_Open_Connection(uint16_t Sourceport, uint16_t Destport, char *Dest_IP, uint8_t Active_Passive)
{
	struct TCP_Table Entrie;
	uint8_t Position_in_Table=0;
	char Options[4]={0x02,0x04,(TCP_MSS>>8)&0xff,TCP_MSS&0xff};									// MSS setzen

	struct TCP_Head TCP_Head_Write;
	struct IP_Head	IP_Head_Write;
	struct ENC_Head ENC_Head_Write;
	
	memset(&TCP_Head_Write,0,sizeof(TCP_Head_Write));											// Lokale Structs l�schen
	memset(&IP_Head_Write,0,sizeof(IP_Head_Write));
	memset(&ENC_Head_Write,0,sizeof(ENC_Head_Write));
	memset(&Entrie,0,sizeof(Entrie));															// memset(&Entrie.Retransmission.MAC[0],0,6);	// MAC Eintrag wird w�hrend Send Packet gef�llt
	
	if (ARP_Get_MAC_from_IP(&ENC_Head_Write.Dest_MAC[0],&Dest_IP[0]))							// Hole die Mac des TCP Senders
	{
		#ifdef Stack_Debug
			USART_Write_String("TCP: Verbindung konnte nicht geoeffnet werden, MAC konnte nicht aufgeloest werden\r\n");
		#endif
		return 1;
	}
		
	Entrie.In_use=1;																			// Eintrag in Benutzung
	if(Active_Passive==TCP_Connection_Active)
	{
		Entrie.Status=TCP_Syn_Sent;
	}
	else
	{
		Entrie.Status=TCP_Syn_Recd;
	}
	memcpy(&Entrie.Client_IP[0],&Dest_IP[0],4);													// ClientIP
	Entrie.Destport = Destport;																	// Destport
	Entrie.Sourceport = Sourceport;																// Sourceport
	Entrie.Sequencenumber = rand();																// Sequencenumber x
	if(Active_Passive==TCP_Connection_Active)
	{
		Entrie.Acknumber = 0;
	}
	else
	{
		Entrie.Acknumber = gl_TCP_Head_read.Sequencenumber+1;									// Acknumber 0
		Entrie.Client_MSS = TCP_Read_MSS_Option_from_Buffer();
		Entrie.Client_Windowscale = TCP_Read_WS_Option_from_Buffer();
		Entrie.Packetstatus=TCP_SYN_received;													// Syn empfangen
	}
	Entrie.Retransmission.Retransmitcount=0;													// Connectiontimer auf 0
	Entrie.Retransmission.Length=0;																// L�nge auf 0

	Position_in_Table = TCP_Set_Entry_in_Table(&Entrie);
	
	if (Position_in_Table == 255)
	{
		#ifdef Stack_Debug
			USART_Write_String("TCP: Verbindung �ffnen nicht m�glich, TCP Tabelle voll\r\n");
		#endif
		return 255;
	}
	
	#ifdef Use_external_RAM
		gl_TCP_Table[Position_in_Table].Retransmission.SRAM_Address = Position_in_Table * ENC_Buffsize;		// SRAM Adresse speichern, sobald Position in Tabelle bekannt
	#endif
	
	TCP_Head_Write.Sourceport = Sourceport;														// Sourceport
	TCP_Head_Write.Destport = Destport;															// Destport
	TCP_Head_Write.Sequencenumber = Entrie.Sequencenumber;										// Sequencenummer random
	TCP_Head_Write.Acknumber = Entrie.Acknumber;												// Acknummer read + 1
	TCP_Head_Write.Dataoffset=6;																// Offset 6*4
	if (Active_Passive == TCP_Connection_Active)
	{
		TCP_Head_Write.Flags = TCP_Syn;															// Syn
	}
	else
	{
		TCP_Head_Write.Flags = TCP_SynAck;														// Synack	
	}
	TCP_Head_Write.Window = TCP_Window;															// Max Fenstergr��e
	TCP_Head_Write.Checksum=0;																	// Checksummer erst 0
	TCP_Head_Write.Urgentpointer=0;																// Urgentpinter 0
	memcpy(&TCP_Head_Write.Options[0],&Options[0],4);											// MSS 1460, Windowscale 1
		
	IP_Head_Write.Totallength = 44;																// Totale Laenge IP + TCP
	//IP_Header.Identifikation = 0x0005;														// Wird generiert
	IP_Head_Write.Flags = 0x00;																	// Flags 0
	IP_Head_Write.Fragment_Offset = 0x0000;														// Fragmentoffset 0
	IP_Head_Write.TTL = 128;																	// Time to live
	IP_Head_Write.Protocol = 6;																	// TCP
	memcpy(&IP_Head_Write.Source_IP[0],&gl_Webserver.IP_address[0],4);							// Eigene IP addresse
	memcpy(&IP_Head_Write.Dest_IP[0],&gl_TCP_Table[Position_in_Table].Client_IP[0],4);			// DestIP read
	ENC_Head_Write.Type = 0x0800;																// Type IP
	
	gl_TCP_Table[Position_in_Table].Acknumber = TCP_Head_Write.Acknumber;						// Acknummer aus Sicht des Webserver
	gl_TCP_Table[Position_in_Table].Sequencenumber = TCP_Head_Write.Sequencenumber;				// Sequencenummer aus Sicht des Webservers

	#ifdef Stack_Debug
		USART_Write_String("TCP: Paket senden: 1\r\n");
		USART_Write_String("TCP: TCP_Head:");
		USART_Write_X_Bytes((char*)&TCP_Head_Write,0,sizeof(TCP_Head_Write));
		USART_Write_String("\r\nTCP: IP_Head:");
		USART_Write_X_Bytes((char*)&IP_Head_Write,0,sizeof(IP_Head_Write));
		USART_Write_String("\r\nTCP: ENC_Head:");
		USART_Write_X_Bytes((char*)&ENC_Head_Write,0,sizeof(ENC_Head_Write));
		USART_Write_String("\r\n");
	#endif

	if (TCP_Send_Packet(&TCP_Head_Write, &IP_Head_Write, &ENC_Head_Write)!=0)					// Paket senden
	{
		return 255;
	}
	
	if (Active_Passive == TCP_Connection_Active)
	{
		gl_TCP_Table[Position_in_Table].Packetstatus=TCP_SYN_sent;								// Paketstatus setzen
	}
	else
	{
		gl_TCP_Table[Position_in_Table].Packetstatus=TCP_SYNACK_sent;							// Paketstatus setzen
	}
	
	return Position_in_Table;
}

uint8_t TCP_Close_Connection(uint8_t Position_in_Table, uint8_t Active_Passive)
{
	struct TCP_Head TCP_Head_Write;
	struct IP_Head	IP_Head_Write;
	struct ENC_Head ENC_Head_Write;
	char MAC[6];
	
	memset(&TCP_Head_Write,0,sizeof(TCP_Head_Write));											// Lokale Structs l�schen
	memset(&IP_Head_Write,0,sizeof(IP_Head_Write));
	memset(&ENC_Head_Write,0,sizeof(ENC_Head_Write));
	

	if (ARP_Get_MAC_from_IP(&MAC[0],&gl_TCP_Table[Position_in_Table].Client_IP[0]))				// Hole die Mac des TCP Senders
	{
		#ifdef Stack_Debug
			USART_Write_String("TCP: Verbindung konnte nicht geschlossen werden, MAC konnte nicht aufgeloest werden\r\n");
		#endif
		return 1;
	}
	
	if(Active_Passive==TCP_Connection_Active)
	{
		TCP_Head_Write.Sourceport = gl_TCP_Table[Position_in_Table].Sourceport;						// Sourceport
		TCP_Head_Write.Destport = gl_TCP_Table[Position_in_Table].Destport;							// Destport
		TCP_Head_Write.Sequencenumber = gl_TCP_Table[Position_in_Table].Sequencenumber;				//
		TCP_Head_Write.Acknumber = gl_TCP_Table[Position_in_Table].Acknumber;						//
		TCP_Head_Write.Dataoffset=5;																// Offset 5*4
		TCP_Head_Write.Flags = TCP_FinAck;															// Finack
		TCP_Head_Write.Window = TCP_Window;																// Max Fenstergr��e
		TCP_Head_Write.Checksum=0;																	// Checksummer erst 0
		TCP_Head_Write.Urgentpointer=0;																// Urgentpinter 0
		TCP_Head_Write.Data = 0;																	// Data durchreichen
		
		IP_Head_Write.Totallength = 40;																// Totale Laenge IP + UDP + NTP
		//IP_Header.Identifikation = 0x0005;														// Wird generiert
		IP_Head_Write.Flags = 0x00;																	// Flags 0
		IP_Head_Write.Fragment_Offset = 0x0000;														// Fragmentoffset 0
		IP_Head_Write.TTL = 128;																	// Time to live
		IP_Head_Write.Protocol = 6;																	// TCP
		memcpy(&IP_Head_Write.Source_IP[0],&gl_Webserver.IP_address[0],4);							// Eigene IP addresse
		memcpy(&IP_Head_Write.Dest_IP[0],&gl_TCP_Table[Position_in_Table].Client_IP[0],4);			// DestIP read
		memcpy(&ENC_Head_Write.Dest_MAC[0],&MAC[0],6);												// Kopiere MAC in ENC Struct
		ENC_Head_Write.Type = 0x0800;																// Type IP
		
		gl_TCP_Table[Position_in_Table].Acknumber = TCP_Head_Write.Acknumber;						// Acknummer aus Sicht des Webserver
		gl_TCP_Table[Position_in_Table].Sequencenumber = TCP_Head_Write.Sequencenumber+1;			// Sequencenummer aus Sicht des Webservers
		gl_TCP_Table[Position_in_Table].Status = TCP_Fin_Wait1;
		
		#ifdef Stack_Debug
			USART_Write_String("TCP: Paket senden: 2\r\n");
			USART_Write_String("TCP: TCP_Head:");
			USART_Write_X_Bytes((char*)&TCP_Head_Write,0,sizeof(TCP_Head_Write));
			USART_Write_String("\r\nTCP: IP_Head:");
			USART_Write_X_Bytes((char*)&IP_Head_Write,0,sizeof(IP_Head_Write));
			USART_Write_String("\r\nTCP: ENC_Head:");
			USART_Write_X_Bytes((char*)&ENC_Head_Write,0,sizeof(ENC_Head_Write));
			USART_Write_String("\r\n");
		#endif
		if (TCP_Send_Packet(&TCP_Head_Write, &IP_Head_Write, &ENC_Head_Write)!=0)					// Paket senden
		{
			return 1;
		}
		gl_TCP_Table[Position_in_Table].Packetstatus=TCP_FINACK_sent;								// Status zuruecksezen
		return 0;
	}
	else if(Active_Passive==TCP_Connection_Passive)
	{
		TCP_Head_Write.Sourceport = gl_TCP_Table[Position_in_Table].Sourceport;						// Sourceport
		TCP_Head_Write.Destport = gl_TCP_Table[Position_in_Table].Destport;							// Destport
		TCP_Head_Write.Sequencenumber = gl_TCP_Table[Position_in_Table].Sequencenumber;				// 
		TCP_Head_Write.Acknumber = gl_TCP_Table[Position_in_Table].Acknumber+1;						// 
		TCP_Head_Write.Dataoffset=5;																// Offset 5*4
		TCP_Head_Write.Flags = TCP_FinAck;															// Finack
		TCP_Head_Write.Window = TCP_Window;															// Max Fenstergr��e
		TCP_Head_Write.Checksum=0;																	// Checksummer erst 0
		TCP_Head_Write.Urgentpointer=0;																// Urgentpinter 0
		TCP_Head_Write.Data=0;
		
		IP_Head_Write.Totallength = 40;																// Totale Laenge IP + UDP + NTP
		//IP_Header.Identifikation = 0x0005;														// Wird generiert
		IP_Head_Write.Flags = 0x00;																	// Flags 0
		IP_Head_Write.Fragment_Offset = 0x0000;														// Fragmentoffset 0
		IP_Head_Write.TTL = 128;																	// Time to live
		IP_Head_Write.Protocol = 6;																	// TCP
		memcpy(&IP_Head_Write.Source_IP[0],&gl_Webserver.IP_address[0],4);							// Eigene IP addresse
		memcpy(&IP_Head_Write.Dest_IP[0],&gl_TCP_Table[Position_in_Table].Client_IP[0],4);			// DestIP read
		memcpy(&ENC_Head_Write.Dest_MAC[0],&MAC[0],6);												// Kopiere MAC in ENC Struct
		ENC_Head_Write.Type = 0x0800;																// Type IP
		
		gl_TCP_Table[Position_in_Table].Acknumber = TCP_Head_Write.Acknumber;						// Acknummer aus Sicht des Webserver
		gl_TCP_Table[Position_in_Table].Sequencenumber = TCP_Head_Write.Sequencenumber;				// Sequencenummer aus Sicht des Webservers
		gl_TCP_Table[Position_in_Table].Status = TCP_Last_Ack;										// Last ACK
		
		#ifdef Stack_Debug
			USART_Write_String("TCP: Paket senden: 3\r\n");
			USART_Write_String("TCP: TCP_Head:");
			USART_Write_X_Bytes((char*)&TCP_Head_Write,0,sizeof(TCP_Head_Write));
			USART_Write_String("\r\nTCP: IP_Head:");
			USART_Write_X_Bytes((char*)&IP_Head_Write,0,sizeof(IP_Head_Write));
			USART_Write_String("\r\nTCP: ENC_Head:");
			USART_Write_X_Bytes((char*)&ENC_Head_Write,0,sizeof(ENC_Head_Write));
			USART_Write_String("\r\n");
		#endif
		if (TCP_Send_Packet(&TCP_Head_Write, &IP_Head_Write, &ENC_Head_Write)!=0)					// Paket senden
		{
			return 1;
		}
		gl_TCP_Table[Position_in_Table].Packetstatus=TCP_FINACK_sent;								// Status zuruecksezen
		return 0;
	}
	return 1;
}

uint8_t TCP_Reset_Connection(uint8_t Position_in_Table)
{
	struct TCP_Head TCP_Head_Write;
	struct IP_Head	IP_Head_Write;
	struct ENC_Head ENC_Head_Write;
	char MAC[6];
	
	memset(&TCP_Head_Write,0,sizeof(TCP_Head_Write));												// Lokale Structs l�schen
	memset(&IP_Head_Write,0,sizeof(IP_Head_Write));
	memset(&ENC_Head_Write,0,sizeof(ENC_Head_Write));
	
	if (ARP_Get_MAC_from_IP(&MAC[0],&gl_TCP_Table[Position_in_Table].Client_IP[0]))				// Hole die Mac des TCP Senders
	{
		#ifdef Stack_Debug
			USART_Write_String("TCP: Verbindung konnte nicht resetet werden, MAC konnte nicht aufgeloest werden\r\n");
		#endif
		return 1;
	}
	
	TCP_Head_Write.Sourceport = gl_TCP_Table[Position_in_Table].Sourceport;							// Sourceport
	TCP_Head_Write.Destport = gl_TCP_Table[Position_in_Table].Destport;								// Destport
	TCP_Head_Write.Sequencenumber = gl_TCP_Table[Position_in_Table].Sequencenumber+1;				// 
	TCP_Head_Write.Acknumber = gl_TCP_Table[Position_in_Table].Acknumber;							// 
	TCP_Head_Write.Dataoffset=5;																	// Offset 5*4
	TCP_Head_Write.Flags = TCP_Rst;																	// Rst
	TCP_Head_Write.Window = TCP_Window;																// Max Fenstergr��e
	TCP_Head_Write.Checksum=0;																		// Checksummer erst 0
	TCP_Head_Write.Urgentpointer=0;																	// Urgentpinter 0
	
	IP_Head_Write.Totallength = 40;																	// Totale Laenge IP + UDP + NTP
	//IP_Header.Identifikation = 0x0005;															// Wird generiert
	IP_Head_Write.Flags = 0x00;																		// Flags 0
	IP_Head_Write.Fragment_Offset = 0x0000;															// Fragmentoffset 0
	IP_Head_Write.TTL = 128;																		// Time to live
	IP_Head_Write.Protocol = 6;																		// TCP
	memcpy(&IP_Head_Write.Source_IP[0],&gl_Webserver.IP_address[0],4);								// Eigene IP addresse
	memcpy(&IP_Head_Write.Dest_IP[0],&gl_TCP_Table[Position_in_Table].Client_IP[0],4);				// DestIP read
	memcpy(&ENC_Head_Write.Dest_MAC[0],&MAC[0],6);													// Kopiere MAC in ENC Struct
	ENC_Head_Write.Type = 0x0800;																	// Type IP
	#ifdef Stack_Debug
		USART_Write_String("TCP: Paket senden: 4\r\n");
		USART_Write_String("TCP: TCP_Head:");
		USART_Write_X_Bytes((char*)&TCP_Head_Write,0,sizeof(TCP_Head_Write));
		USART_Write_String("\r\nTCP: IP_Head:");
		USART_Write_X_Bytes((char*)&IP_Head_Write,0,sizeof(IP_Head_Write));
		USART_Write_String("\r\nTCP: ENC_Head:");
		USART_Write_X_Bytes((char*)&ENC_Head_Write,0,sizeof(ENC_Head_Write));
		USART_Write_String("\r\n");
	#endif
	if (TCP_Send_Packet(&TCP_Head_Write, &IP_Head_Write, &ENC_Head_Write)!=0)						// Paket senden
	{
		return 1;
	}
	
	gl_TCP_Table[Position_in_Table].In_use=0;														// Nicht mehr benutzen
	gl_TCP_Table[Position_in_Table].Status = TCP_RST;												// RST Status anzeigen
	// Packetstatus hier nicht setzen, da sonst der Funktionsaufruf im Retransmit nach dem RST nicht funktioniert
	//gl_TCP_Table[Position_in_Table].Packetstatus = TCP_RST_sent;									// RST Paket gesendet

	#ifdef Stack_Debug
		USART_Write_String("TCP: Verbindung zurueckgesetzt\r\n");
	#endif
	return 0;
}

uint8_t TCP_Set_Entry_in_Table(struct TCP_Table *TCP_Table_Entrie)
{
	uint8_t g=TCP_Get_Position_from_Table(&TCP_Table_Entrie->Client_IP[0],TCP_Table_Entrie->Destport);
	if (g<255)																						// Mehrfacheintragungen verhindern aber als ok zur�ckgeben
	{
		#ifdef Stack_Debug
			USART_Write_String("TCP: Es existiert bereits ein aktiver Eintrag f�r diese IP und Port\r\n");
		#endif
		return g;
	}
	for (g=0;g<TCP_Max_Entries;g++)																	// Eintrag erstellen
	{
		if (gl_TCP_Table[g].In_use==0)
		{
			memcpy(&gl_TCP_Table[g],TCP_Table_Entrie,sizeof(gl_TCP_Table[0]));						// Neuen Eintrag kopieren
			#ifdef Stack_Debug
				USART_Write_String("TCP: Eintrag erstellt\r\n");
			#endif
			return g;
		}
	}
	return 255;
}

uint8_t TCP_Get_Position_from_Table(char *IP, uint16_t Destport)
{
	for (uint8_t g=0; g<TCP_Max_Entries; g++)
	{
		if (gl_TCP_Table[g].In_use==1 && memcmp(&IP[0],&gl_TCP_Table[g].Client_IP[0],4)==0 && gl_TCP_Table[g].Destport == Destport)
		{
			#ifdef Stack_Debug
				USART_Write_String("TCP: Position in Tabelle gefunden\r\n");
			#endif
			return g;
		}
	}
	#ifdef Stack_Debug
		USART_Write_String("TCP: Position in Tabelle nicht gefunden\r\n");
	#endif
	return 255;
}

uint8_t TCP_Get_Position_from_Table_Sp(char *IP, uint16_t Sourceport)
{
	for (uint8_t g=0; g<TCP_Max_Entries; g++)
	{
		if (gl_TCP_Table[g].In_use==1 && memcmp(&IP[0],&gl_TCP_Table[g].Client_IP[0],4)==0 && gl_TCP_Table[g].Sourceport == Sourceport)
		{
			#ifdef Stack_Debug
				USART_Write_String("TCP: Position in Tabelle gefunden\r\n");
			#endif
			return g;
		}
	}
	#ifdef Stack_Debug
		USART_Write_String("TCP: Position in Tabelle nicht gefunden\r\n");
	#endif
	return 255;
}

uint8_t TCP_Write_Data(uint8_t Position_in_Table, char *Data, uint16_t Len)
{
	struct TCP_Head TCP_Head_Write;
	struct IP_Head	IP_Head_Write;
	struct ENC_Head ENC_Head_Write;
	char MAC[6];
	
	memset(&TCP_Head_Write,0,sizeof(TCP_Head_Write));											// Lokale Structs l�schen
	memset(&IP_Head_Write,0,sizeof(IP_Head_Write));
	memset(&ENC_Head_Write,0,sizeof(ENC_Head_Write));
	
	
	if (ARP_Get_MAC_from_IP(&MAC[0],&gl_TCP_Table[Position_in_Table].Client_IP[0]))				// Hole die Mac des TCP Senders
	{
		#ifdef Stack_Debug
			USART_Write_String("TCP: Daten konnten nicht gesendet werden, MAC konnte nicht aufgeloest werden\r\n");
		#endif
		return 1;
	}
	
	TCP_Head_Write.Sourceport = gl_TCP_Table[Position_in_Table].Sourceport;						// Sourceport
	TCP_Head_Write.Destport = gl_TCP_Table[Position_in_Table].Destport;							// Destport
	TCP_Head_Write.Sequencenumber = gl_TCP_Table[Position_in_Table].Sequencenumber;				//
	TCP_Head_Write.Acknumber = gl_TCP_Table[Position_in_Table].Acknumber;						//
	TCP_Head_Write.Dataoffset=5;																// Offset 5*4
	TCP_Head_Write.Flags = TCP_PshAck;															// PSHack
	TCP_Head_Write.Window = TCP_Window;															// Max Fenstergr��e
	TCP_Head_Write.Checksum=0;																	// Checksummer erst 0
	TCP_Head_Write.Urgentpointer=0;																// Urgentpointer 0
	TCP_Head_Write.Data = Data;																	// Data durchreichen
		
	IP_Head_Write.Totallength = 40+Len;															// Totale Laenge IP + UDP + NTP
	//IP_Header.Identifikation = 0x0005;														// Wird generiert
	IP_Head_Write.Flags = 0x00;																	// Flags 0
	IP_Head_Write.Fragment_Offset = 0x0000;														// Fragmentoffset 0
	IP_Head_Write.TTL = 128;																	// Time to live
	IP_Head_Write.Protocol = 6;																	// TCP
	memcpy(&IP_Head_Write.Source_IP[0],&gl_Webserver.IP_address[0],4);							// Eigene IP addresse
	memcpy(&IP_Head_Write.Dest_IP[0],&gl_TCP_Table[Position_in_Table].Client_IP[0],4);			// DestIP read
	memcpy(&ENC_Head_Write.Dest_MAC[0],&MAC[0],6);												// Kopiere MAC in ENC Struct
	ENC_Head_Write.Type = 0x0800;																// Type IP
	
	gl_TCP_Table[Position_in_Table].Acknumber = TCP_Head_Write.Acknumber;						// Acknummer aus Sicht des Webserver
	gl_TCP_Table[Position_in_Table].Sequencenumber = TCP_Head_Write.Sequencenumber+Len;			// Sequencenummer aus Sicht des Webservers
	
	#ifdef Stack_Debug
		USART_Write_String("TCP: Paket senden: 5\r\n");
		USART_Write_String("TCP: TCP_Head:");
		USART_Write_X_Bytes((char*)&TCP_Head_Write,0,sizeof(TCP_Head_Write));
		USART_Write_String("\r\nTCP: IP_Head:");
		USART_Write_X_Bytes((char*)&IP_Head_Write,0,sizeof(IP_Head_Write));
		USART_Write_String("\r\nTCP: ENC_Head:");
		USART_Write_X_Bytes((char*)&ENC_Head_Write,0,sizeof(ENC_Head_Write));
		USART_Write_String("\r\n");
	#endif
	
	if (TCP_Send_Packet(&TCP_Head_Write, &IP_Head_Write, &ENC_Head_Write))						// Paket senden
	{
		return 1;
	}
	
	gl_TCP_Table[Position_in_Table].Packetstatus=TCP_PSHACK_sent;								// Status zuruecksezen
		
	return 0;
}

uint8_t TCP_Read_Data(uint8_t Position_in_Table, char **Data, uint16_t *Len)
{
	struct TCP_Head TCP_Head_Write;
	struct IP_Head	IP_Head_Write;
	struct ENC_Head ENC_Head_Write;
	char MAC[6];
	
	if (gl_TCP_Head_read.Datalength!=0)																// Wenn Daten da sind dann best�tige diese
	{
		*Data = gl_TCP_Head_read.Data;
		*Len = gl_TCP_Head_read.Datalength;
		
		memset(&TCP_Head_Write,0,sizeof(TCP_Head_Write));											// Lokale Structs l�schen
		memset(&IP_Head_Write,0,sizeof(IP_Head_Write));
		memset(&ENC_Head_Write,0,sizeof(ENC_Head_Write));
		
		if (ARP_Get_MAC_from_IP(&MAC[0],&gl_TCP_Table[Position_in_Table].Client_IP[0]))				// Hole die Mac des TCP Senders
		{
			#ifdef Stack_Debug
			USART_Write_String("TCP: Daten konnten nicht gelesen werden, MAC konnte nicht aufgeloest werden\r\n");
			#endif
			return 1;
		}
		
		TCP_Head_Write.Sourceport = gl_TCP_Table[Position_in_Table].Sourceport;						// Sourceport
		TCP_Head_Write.Destport = gl_TCP_Table[Position_in_Table].Destport;							// Destport
		TCP_Head_Write.Sequencenumber = gl_TCP_Table[Position_in_Table].Sequencenumber;				//
		TCP_Head_Write.Acknumber = gl_TCP_Table[Position_in_Table].Acknumber + gl_TCP_Head_read.Datalength; 	//
		TCP_Head_Write.Dataoffset=5;																// Offset 5*4
		TCP_Head_Write.Flags = TCP_Ack;																// Finack
		TCP_Head_Write.Window = TCP_Window;																// Max Fenstergr��e
		TCP_Head_Write.Checksum=0;																	// Checksummer erst 0
		TCP_Head_Write.Urgentpointer=0;																// Urgentpinter 0
		TCP_Head_Write.Data = 0;																	// Data durchreichen
		
		IP_Head_Write.Totallength = 40;																// Totale Laenge IP + TCP
		//IP_Header.Identifikation = 0x0005;														// Wird generiert
		IP_Head_Write.Flags = 0x00;																	// Flags 0
		IP_Head_Write.Fragment_Offset = 0x0000;														// Fragmentoffset 0
		IP_Head_Write.TTL = 128;																	// Time to live
		IP_Head_Write.Protocol = 6;																	// TCP
		memcpy(&IP_Head_Write.Source_IP[0],&gl_Webserver.IP_address[0],4);							// Eigene IP addresse
		memcpy(&IP_Head_Write.Dest_IP[0],&gl_TCP_Table[Position_in_Table].Client_IP[0],4);			// DestIP read
		memcpy(&ENC_Head_Write.Dest_MAC[0],&MAC[0],6);												// Kopiere MAC in ENC Struct
		ENC_Head_Write.Type = 0x0800;																// Type IP
		
		gl_TCP_Table[Position_in_Table].Acknumber = TCP_Head_Write.Acknumber;						// Acknummer aus Sicht des Webserver
		gl_TCP_Table[Position_in_Table].Sequencenumber = TCP_Head_Write.Sequencenumber;				// Sequencenummer aus Sicht des Webservers
		#ifdef Stack_Debug
			USART_Write_String("TCP: Paket senden: 6\r\n");
			USART_Write_String("TCP: TCP_Head:");
			USART_Write_X_Bytes((char*)&TCP_Head_Write,0,sizeof(TCP_Head_Write));
			USART_Write_String("\r\nTCP: IP_Head:");
			USART_Write_X_Bytes((char*)&IP_Head_Write,0,sizeof(IP_Head_Write));
			USART_Write_String("\r\nTCP: ENC_Head:");
			USART_Write_X_Bytes((char*)&ENC_Head_Write,0,sizeof(ENC_Head_Write));
			USART_Write_String("\r\n");
		#endif
		if (TCP_Send_Packet(&TCP_Head_Write, &IP_Head_Write, &ENC_Head_Write)!=0)					// Paket senden
		{
			return 1;
		}
		return 0;																					// Daten gelesen und best�tigt
	}
	return 2;																						// Wenn keine Daten da sind, gebe dies zur�ck
}

uint8_t TCP_Send_Ack(uint8_t Position_in_Table)
{
	struct TCP_Head TCP_Head_Write;
	struct IP_Head	IP_Head_Write;
	struct ENC_Head ENC_Head_Write;
	char MAC[6];
	
	if (ARP_Get_MAC_from_IP(&MAC[0],&gl_TCP_Table[Position_in_Table].Client_IP[0]))				// Hole die Mac des TCP Senders
	{
		#ifdef Stack_Debug
		USART_Write_String("TCP: Ack konnte nicht gesendet werden, MAC konnte nicht aufgeloest werden\r\n");
		#endif
		return 1;
	}
	
	memset(&TCP_Head_Write,0,sizeof(TCP_Head_Write));											// Lokale Structs l�schen
	memset(&IP_Head_Write,0,sizeof(IP_Head_Write));
	memset(&ENC_Head_Write,0,sizeof(ENC_Head_Write));
		
	TCP_Head_Write.Sourceport = gl_TCP_Table[Position_in_Table].Sourceport;						// Sourceport
	TCP_Head_Write.Destport = gl_TCP_Table[Position_in_Table].Destport;							// Destport
	TCP_Head_Write.Sequencenumber = gl_TCP_Table[Position_in_Table].Sequencenumber;				//
	TCP_Head_Write.Acknumber = gl_TCP_Table[Position_in_Table].Acknumber+gl_TCP_Head_read.Datalength; 	//
	TCP_Head_Write.Dataoffset=5;																// Offset 5*4
	TCP_Head_Write.Flags = TCP_Ack;																// Finack
	TCP_Head_Write.Window = TCP_Window;															// Max Fenstergr��e
	TCP_Head_Write.Checksum=0;																	// Checksummer erst 0
	TCP_Head_Write.Urgentpointer=0;																// Urgentpinter 0
	TCP_Head_Write.Data = 0;																	// Data durchreichen
	
	IP_Head_Write.Totallength = 40;																// Totale Laenge IP + UDP + NTP
	//IP_Header.Identifikation = 0x0005;														// Wird generiert
	IP_Head_Write.Flags = 0x00;																	// Flags 0
	IP_Head_Write.Fragment_Offset = 0x0000;														// Fragmentoffset 0
	IP_Head_Write.TTL = 128;																	// Time to live
	IP_Head_Write.Protocol = 6;																	// TCP
	memcpy(&IP_Head_Write.Source_IP[0],&gl_Webserver.IP_address[0],4);							// Eigene IP addresse
	memcpy(&IP_Head_Write.Dest_IP[0],&gl_TCP_Table[Position_in_Table].Client_IP[0],4);			// DestIP read
	memcpy(&ENC_Head_Write.Dest_MAC[0],&MAC[0],6);												// Kopiere MAC in ENC Struct
	ENC_Head_Write.Type = 0x0800;																// Type IP
	
	gl_TCP_Table[Position_in_Table].Acknumber = TCP_Head_Write.Acknumber;						// Acknummer aus Sicht des Webserver
	gl_TCP_Table[Position_in_Table].Sequencenumber = TCP_Head_Write.Sequencenumber;				// Sequencenummer aus Sicht des Webservers
	#ifdef Stack_Debug
		USART_Write_String("TCP: Paket senden: 7\r\n");
		USART_Write_String("TCP: TCP_Head:");
		USART_Write_X_Bytes((char*)&TCP_Head_Write,0,sizeof(TCP_Head_Write));
		USART_Write_String("\r\nTCP: IP_Head:");
		USART_Write_X_Bytes((char*)&IP_Head_Write,0,sizeof(IP_Head_Write));
		USART_Write_String("\r\nTCP: ENC_Head:");
		USART_Write_X_Bytes((char*)&ENC_Head_Write,0,sizeof(ENC_Head_Write));
		USART_Write_String("\r\n");
	#endif
	if (TCP_Send_Packet(&TCP_Head_Write, &IP_Head_Write, &ENC_Head_Write)!=0)					// Paket senden
	{
		return 1;
	}
	gl_TCP_Table[Position_in_Table].Packetstatus = TCP_ACK_sent;
	return 0;
}

void TCP_Porthandler(uint16_t Port, void (*FP)(uint8_t))
{
	uint8_t Position_in_Table = TCP_Get_Position_from_Table(&gl_IP_Head_read.Source_IP[0],gl_TCP_Head_read.Sourceport);				// Aktualisiere den TCP Eintrag
			
	if (Position_in_Table == 255 && gl_TCP_Head_read.Flags == TCP_Syn)																		// Falls neue TCP Verbindung aufgebaut werden soll �ffne diese im Passiven Modus
	{
		Position_in_Table = TCP_Open_Connection(Port,gl_TCP_Head_read.Sourceport,&gl_IP_Head_read.Source_IP[0],TCP_Connection_Passive);
		if(Position_in_Table == 255)																										// Verbindung kann nicht ge�ffnet werden, kein Platz in Tabelle
		{
			return;																															// Stell dich tot und ignoriere die Anfrage
		}
		if (gl_TCP_Table[Position_in_Table].Client_MSS >= TCP_MSS)																			// Wenn die MSS des Clients gr��er als unsere MSS ist, setze unsere MSS
		{
			gl_TCP_Table[Position_in_Table].Transaction_MSS = TCP_MSS;
		}
		else
		{
			gl_TCP_Table[Position_in_Table].Transaction_MSS = gl_TCP_Table[Position_in_Table].Client_MSS;									// Wenn die MSS des Clients kleiner als unsere MSS ist, setze dessen als unsere
		}
		return;
	}	
	
	if ((gl_TCP_Head_read.Flags == TCP_Rst)||(gl_TCP_Head_read.Flags == TCP_RstAck))														// Falls ein Reset von einer Eingetragenen Verbindung kommt, breche die Verbindung ab
	{
		if (Position_in_Table == 255)																										// Falls einer Resets rumschickt, ohne das ein Eintrag �ber die Verbindung herscht, ignoriere das
		{
			return;	
		}
		if (gl_TCP_Head_read.Flags == TCP_RstAck)
		{
			gl_TCP_Table[Position_in_Table].Packetstatus=TCP_RSTACK_received;																// RstAck Paket empfangen
		}
		else
		{
			gl_TCP_Table[Position_in_Table].Packetstatus=TCP_RST_received;																	// RST Paket empfangen
		}
		TCP_Reset_Connection(Position_in_Table);
		FP(Position_in_Table);																												// Nach einem Reset gebe der Funktion die M�glichkeit, seine Statemachine zur�ckzusetzen
		return;
	}
	
	switch(gl_TCP_Table[Position_in_Table].Status)																							// TCP State Machine
	{
		case TCP_Syn_Sent:
			if (gl_TCP_Head_read.Acknumber == (gl_TCP_Table[Position_in_Table].Sequencenumber+1))
			{
				if(gl_TCP_Head_read.Flags == TCP_SynAck)
				{
					gl_TCP_Table[Position_in_Table].Sequencenumber+=1;																		// Sequencenummer schonmal auf 1 setzen
					gl_TCP_Table[Position_in_Table].Acknumber = gl_TCP_Head_read.Sequencenumber+1;											// Sequencenummer aus Sicht des Webservers
					gl_TCP_Table[Position_in_Table].Retransmission.Retransmitcount=0;														// Connectiontimer zur�cksetzen
					gl_TCP_Table[Position_in_Table].Status = TCP_Established;																// Tatus �ndern
					gl_TCP_Table[Position_in_Table].Packetstatus = TCP_SYNACK_received;														// Ack auf Synack empfangen
					gl_TCP_FP_Table[Position_in_Table] = FP;																				// Trage den FP ein

					
					gl_TCP_Table[Position_in_Table].Client_MSS = TCP_Read_MSS_Option_from_Buffer();
					gl_TCP_Table[Position_in_Table].Client_Windowscale = TCP_Read_WS_Option_from_Buffer();
					if (gl_TCP_Table[Position_in_Table].Client_MSS >= TCP_MSS)																// Wenn die MSS des Clients gr��er als unsere MSS ist, setze unsere MSS
					{
						gl_TCP_Table[Position_in_Table].Transaction_MSS = TCP_MSS;
					}
					else
					{
						gl_TCP_Table[Position_in_Table].Transaction_MSS = gl_TCP_Table[Position_in_Table].Client_MSS;						// Wenn die MSS des Clients kleiner als unsere MSS ist, setze dessen als unsere
					}
					
					TCP_Send_Ack(Position_in_Table);
					
					#ifdef Stack_Debug
						USART_Write_String("TCP: SynAck erhalten\r\n");
					#endif
					return;
				}
			}
		break;
		
		case TCP_Syn_Recd:
			if ((gl_TCP_Head_read.Sequencenumber==gl_TCP_Table[Position_in_Table].Acknumber) && (gl_TCP_Head_read.Acknumber == (gl_TCP_Table[Position_in_Table].Sequencenumber+1)))
			{
				if(gl_TCP_Head_read.Flags == TCP_Ack)
				{
					gl_TCP_Table[Position_in_Table].Sequencenumber += 1 ;														// Sequencenummer aus Sicht des Webservers
					gl_TCP_Table[Position_in_Table].Retransmission.Retransmitcount=0;											// Connectiontimer zur�cksetzen
					gl_TCP_Table[Position_in_Table].Status = TCP_Established;													// Tatus �ndern
					gl_TCP_Table[Position_in_Table].Packetstatus = TCP_ACK_received;											// Ack auf Synack empfangen
					gl_TCP_FP_Table[Position_in_Table] = FP;																	// Trage den FP ein
					
					#ifdef Stack_Debug
						USART_Write_String("TCP: Verbindung geoeffnet\r\n");
					#endif
					
					FP(Position_in_Table);																						// Nach dem �ffnen der Verbindung hat der Server die erste M�glichkeit zu antworten
					
					return;
				}
			}
		break;
		
		case TCP_Established:
			if ((gl_TCP_Head_read.Sequencenumber == gl_TCP_Table[Position_in_Table].Acknumber) && (gl_TCP_Head_read.Acknumber == gl_TCP_Table[Position_in_Table].Sequencenumber))
			{	
				if(gl_TCP_Head_read.Flags == TCP_FinPshAck)
				{
					gl_TCP_Table[Position_in_Table].Retransmission.Retransmitcount=0;											// Counter Zur�cksetzen
					gl_TCP_Table[Position_in_Table].Packetstatus=TCP_PSHACK_received;											// pshack Paket empfangen

					FP(Position_in_Table);
									
					gl_TCP_Table[Position_in_Table].Status = TCP_Close_Wait;													// Close_Wait
					TCP_Close_Connection(Position_in_Table,TCP_Connection_Passive);
									
					FP(Position_in_Table);																						//Sobald die Verbindung schlie�t muss die Funktion keine Daten mehr senden
					return;
				}
				if(gl_TCP_Head_read.Flags == TCP_Ack || gl_TCP_Head_read.Flags == TCP_PshAck)									// ACK auf unser PSHACK empfangen
				{
					gl_TCP_Table[Position_in_Table].Retransmission.Retransmitcount=0;											// Counter Zur�cksetzen
					if (gl_TCP_Head_read.Flags == TCP_PshAck)
					{
						gl_TCP_Table[Position_in_Table].Packetstatus=TCP_PSHACK_received;										// pshack Paket empfangen
					}
					else
					{
						gl_TCP_Table[Position_in_Table].Packetstatus=TCP_ACK_received;											// psh Paket empfangen
					}

					FP(Position_in_Table);

					return;
				}
			if(gl_TCP_Head_read.Flags == TCP_FinAck || gl_TCP_Head_read.Flags == TCP_Fin)									// FIN oder FINACK
			{
				gl_TCP_Table[Position_in_Table].Retransmission.Retransmitcount=0;											// Counter Zur�cksetzen
				if (gl_TCP_Head_read.Flags == TCP_FinAck)
				{
					gl_TCP_Table[Position_in_Table].Packetstatus=TCP_FINACK_received;										// Finack Paket empfangen
				}
				else
				{
					gl_TCP_Table[Position_in_Table].Packetstatus=TCP_FIN_received;											// Fin Paket empfangen
				}

				gl_TCP_Table[Position_in_Table].Status = TCP_Close_Wait;													// Close_Wait
				TCP_Close_Connection(Position_in_Table,TCP_Connection_Passive);
				FP(Position_in_Table);																						// Nachdem die Verbindung vom Client getrennt wurde, gebe der Funktion die Chance sich zu selbst zu reseten
				return;
			}
		}
		break;

		// TCP_Closing macht genau das selbe wie Last_ACK
		case TCP_Closing:
		// Close Wait wird hier �bersprungen, da ich direkt das FINACK sende und nur noch auf das ACK warte
		case TCP_Last_Ack:
			if ((gl_TCP_Head_read.Sequencenumber == gl_TCP_Table[Position_in_Table].Acknumber) && (gl_TCP_Head_read.Acknumber == (gl_TCP_Table[Position_in_Table].Sequencenumber+1)))
			{
				if(gl_TCP_Head_read.Flags == TCP_Ack)
				{
					gl_TCP_Table[Position_in_Table].Retransmission.Retransmitcount=0;											// Counter Zur�cksetzen
					gl_TCP_Table[Position_in_Table].Packetstatus = TCP_ACK_received;											// Ack auf Finack empfangen
					gl_TCP_Table[Position_in_Table].In_use=0;																	// Verbindung nicht mehr benutzbar
					gl_TCP_Table[Position_in_Table].Status = TCP_Closed;														// Verbindung geschlossen
					#ifdef Stack_Debug
						USART_Write_String("TCP: Verbindung geschlossen\r\n");
					#endif
					return;
				}
			}
		break;
		
		case TCP_Fin_Wait1:
			if ((gl_TCP_Head_read.Sequencenumber==gl_TCP_Table[Position_in_Table].Acknumber) && (gl_TCP_Head_read.Acknumber == (gl_TCP_Table[Position_in_Table].Sequencenumber)))
			{
				if (gl_TCP_Head_read.Flags==TCP_Ack)
				{
					gl_TCP_Table[Position_in_Table].Retransmission.Retransmitcount=0;											// Counter Zur�cksetzen
					gl_TCP_Table[Position_in_Table].Packetstatus = TCP_ACK_received;											// Ack auf Finack empfangen
					gl_TCP_Table[Position_in_Table].Status = TCP_Fin_Wait2;
					return;
				}
				if(gl_TCP_Head_read.Flags==TCP_Fin)
				{
					gl_TCP_Table[Position_in_Table].Packetstatus=TCP_FIN_received;												// Fin Paket empfangen
					gl_TCP_Table[Position_in_Table].Retransmission.Retransmitcount=0;											// Counter Zur�cksetzen
					gl_TCP_Table[Position_in_Table].Acknumber++;																// Ack schon um 1 hochz�hlen das die Acknummer passt
					TCP_Send_Ack(Position_in_Table);																			// Paket best�tigen
					gl_TCP_Table[Position_in_Table].Status = TCP_Closing;
					return;
				}	
				if(gl_TCP_Head_read.Flags==TCP_FinAck)
				{
					gl_TCP_Table[Position_in_Table].Retransmission.Retransmitcount=0;											// Counter Zur�cksetzen
					gl_TCP_Table[Position_in_Table].Packetstatus=TCP_FINACK_received;											// Finack Paket empfangen
					gl_TCP_Table[Position_in_Table].Acknumber++;																// Ack schon um 1 hochz�hlen das die Acknummer passt
					TCP_Send_Ack(Position_in_Table);																			// Paket best�tigen
					gl_TCP_Table[Position_in_Table].In_use=0;																	// Verbindung nicht mehr benutzbar
					gl_TCP_Table[Position_in_Table].Status = TCP_Closed;														// Verbindung geschlossen
					#ifdef Stack_Debug
						USART_Write_String("TCP: Verbindung geschlossen\r\n");
					#endif
					return;
				}
			}
		break;
		
		case TCP_Fin_Wait2:
			if ((gl_TCP_Head_read.Sequencenumber==gl_TCP_Table[Position_in_Table].Acknumber) && (gl_TCP_Head_read.Acknumber == (gl_TCP_Table[Position_in_Table].Sequencenumber)))
			{
				if(gl_TCP_Head_read.Flags==TCP_FinAck)
				{
					gl_TCP_Table[Position_in_Table].Retransmission.Retransmitcount=0;											// Counter Zur�cksetzen
					gl_TCP_Table[Position_in_Table].Packetstatus=TCP_FINACK_received;											// Finack Paket empfangen
					gl_TCP_Table[Position_in_Table].Acknumber++;																// Ack schon um 1 hochz�hlen das die Acknummer passt
					TCP_Send_Ack(Position_in_Table);																			// Paket best�tigen
					gl_TCP_Table[Position_in_Table].In_use=0;																	// Verbindung nicht mehr benutzbar
					gl_TCP_Table[Position_in_Table].Status = TCP_Closed;														// Verbindung geschlossen
					#ifdef Stack_Debug
					USART_Write_String("TCP: Verbindung geschlossen\r\n");
					#endif
					return;
				}
				if(gl_TCP_Head_read.Flags==TCP_Fin)
				{
					gl_TCP_Table[Position_in_Table].Packetstatus=TCP_FIN_received;												// Fin Paket empfangen
					gl_TCP_Table[Position_in_Table].Retransmission.Retransmitcount=0;											// Counter Zur�cksetzen
					gl_TCP_Table[Position_in_Table].Acknumber++;																// Ack schon um 1 hochz�hlen das die Acknummer passt
					TCP_Send_Ack(Position_in_Table);																			// Paket best�tigen
					gl_TCP_Table[Position_in_Table].In_use=0;																	// Verbindung nicht mehr benutzbar
					gl_TCP_Table[Position_in_Table].Status = TCP_Closed;														// Verbindung geschlossen
					return;
				}
			}
		break;
		
		default:
		break;
	}
}

void TCP_Retransmittimer(void)
{
	for (uint8_t g=0;g<TCP_Max_Entries;g++)
	{
		if (gl_TCP_Table[g].In_use)
		{
			if (gl_TCP_Table[g].Retransmission.Retransmitcount == 0)																															// Warte erst mal die erste Runde ab, bevor nachgeschickt wird
			{
				gl_TCP_Table[g].Retransmission.Retransmitcount++;																																// Eine Sekunde warten
			}
			else
			{
				if (gl_TCP_Table[g].Retransmission.Retransmitcount < (TCP_Retransmission_Max+1))
				{
					if (gl_TCP_Table[g].Packetstatus == TCP_SYNACK_sent || gl_TCP_Table[g].Packetstatus == TCP_PSHACK_sent || gl_TCP_Table[g].Packetstatus == TCP_FINACK_sent || gl_TCP_Table[g].Packetstatus == TCP_SYN_sent)					// Wenn nach einer Sekunde keine Antwort kam sende erneut
					{
						#ifdef Use_external_RAM
							SRAM_23LC1024_Read_Bytes(gl_TCP_Table[g].Retransmission.SRAM_Address,gl_TCP_Table[g].Retransmission.Length,gl_ENC_Write_Datapayload);									// Lade Paket aus Backup
						#endif
						#ifdef Use_internal_RAM
							memcpy(&gl_ENC_Write_Datapayload[0],&gl_TCP_Retransmit_Packet_Backup[g][0],gl_TCP_Table[g].Retransmission.Length);													// Lade Paket aus Backup
						#endif
						ENC_Send_Packet(&gl_TCP_Table[g].Retransmission.MAC[0],0x0800,0x1000, gl_TCP_Table[g].Retransmission.Length);															// Sende das TCP Paket
						gl_TCP_Table[g].Retransmission.Retransmitcount++;																														// Erh�he Z�hler um 1
					}
				}
				else
				{
					TCP_Reset_Connection(g);																																					// Verbindung reseten
					if (gl_TCP_Table[g].Packetstatus == TCP_PSHACK_sent || gl_TCP_Table[g].Packetstatus == TCP_FINACK_sent)																		// Rufe die Funktion auf, wenn eine sicher existiert; Bei Syn_sent und Synack_sent, ist die Funktion noch nicht in der Tabelle hinterlegt worden
					{
						gl_TCP_FP_Table[g](g);																																					// Anwendung aufrufen, die soll noch ihren Kram aufr�umen
					}
				}
			}
		}
	}
}

void Stack_Packetloop(uint8_t ARP_Wait_Reply)
{
	Main_Stack_Packetloop_stuff();									// F�hre noch weiteres Zeug aus, dass in der Main definiert wurde

	if(ENC_Check_for_Packets())										// Wenn Paket zu lesen ist, dann mache das; L�st auch einen Neustart aus, wenn der Link unterbrochen wurde
	{
		if (ENC_Read_Next_Packet())									// Lesefehler fuehren zu Neustart
		{
			USART_Write_String("Stack: Paketlesefehler, Neustart!\r\n");
			IWDG->KR = 0x0000cccc;									// Watchdogtimer einschalten
			while(1);												// Endlosschleife loest Watchdog aus
			return;
		}

		#ifdef Stack_Debug
			USART_Write_String("Stack: Paket angekommen\r\n");
		#endif

		ENC_Read_Header_from_Frame(&gl_ENC_Head_read);				// Lese ENC Header aus
		
		if (ARP_Wait_Reply == 1 && gl_ENC_Head_read.Type == Byteswap16(0x0806))	// Bei ARP Reply wird nur der Teil ausgef�hrt, der Rest wird ignoriert bis wir unsere Antwort haben
		{
			ARP_Read_Header_from_Buffer(&gl_ARP_Head_Read);
			if(gl_ARP_Head_Read.Opcode == Byteswap16(0x0002) && memcmp(&gl_ARP_Head_Read.Target_IP[0],&gl_Webserver.IP_address[0],4)==0) // Wenn auf den Request eine Antwort kommt, trage diese ein
			{
				ARP_Set_Entry(&gl_ARP_Head_Read.Sender_MAC[0],&gl_ARP_Head_Read.Sender_IP[0]);
				#ifdef Stack_Debug
					USART_Write_String("ARP: Reply erhalten\r\n");
				#endif
			}
			else
			{
				#ifdef Stack_Debug
					USART_Write_String("ARP: Das Paket wurde nicht bearbeitet\r\n");
				#endif
			}
			return;
		}

		switch (gl_ENC_Head_read.Type)
		{
			case Byteswap16(0x0806):								// ARP
				ARP_Read_Header_from_Buffer(&gl_ARP_Head_Read);
				if (gl_ARP_Head_Read.Opcode == Byteswap16(0x0001) && memcmp(&gl_ARP_Head_Read.Target_IP[0],&gl_Webserver.IP_address[0],4)==0)	// Wenn ein Request auf die IP des �C kommt, gebe eine Antwort
				{
					struct ARP_Head ARP_Head_write;
					ARP_Head_write.Opcode = ARP_Answer;
					memcpy(&ARP_Head_write.Sender_MAC[0],&gl_Webserver.MAC[0],6);
					memcpy(&ARP_Head_write.Sender_IP[0],&gl_Webserver.IP_address[0],4);
					memcpy(&ARP_Head_write.Target_MAC[0],&gl_ARP_Head_Read.Sender_MAC[0],6);
					memcpy(&ARP_Head_write.Target_IP[0],&gl_ARP_Head_Read.Sender_IP[0],4);
					
					if (ARP_Send_Packet (&ARP_Head_write) == 0)
					{
						ARP_Set_Entry(&gl_ARP_Head_Read.Sender_MAC[0],&gl_ARP_Head_Read.Sender_IP[0]);
						#ifdef Stack_Debug
							USART_Write_String("ARP: Antwort auf Request gesendet\r\n");
						#endif
					}
					else
					{
						#ifdef Stack_Debug
							USART_Write_String("ARP: Antwort auf Request konnte nicht gesendet werden\r\n");
						#endif
					}
					break;
				}
				else
				{
					#ifdef Stack_Debug
						USART_Write_String("ARP: Das Paket wurde nicht bearbeitet\r\n");
					#endif
					break;
				}
			break;
			
			case Byteswap16(0x0800):																	//IP
				if(IP_Read_Header_from_Buffer(&gl_IP_Head_read))
				{
					break;
				}
				
				//**************************************************************************************************************************
				// Nur f�r DHCP, UDP Header wird hier zwar jedesmal mitgelesen ist aber egal. Unten wird er erneut eingelesen, falls er ben�tigt wird.
				// Ansonsten funktioniert DHCP nicht zuverl�ssig, da nicht jede Dest_IP auch die IP_address ist
				if (gl_IP_Head_read.Protocol == 0x11)													// Falls UDP
				{
					if(UDP_Read_Header_from_Buffer(&gl_UDP_Head_read,&gl_IP_Head_read))					// UDP
					{
						break;
					}
					if(gl_UDP_Head_read.Destport==DHCP_Destport)										// Portlistener
					{
						DHCP_Read_Header(&gl_DHCP_Head_Read,&gl_IP_Head_read);
						if ((DHCP_Read_Option(53,&gl_DHCP_Option,&gl_DHCP_Head_Read)==0) && (gl_DHCP_Head_Read.Transaction_ID == gl_DHCP_Cache.TransID) && (memcmp(&gl_Webserver.MAC[0],&gl_ENC_Head_read.Dest_MAC[0],6)==0))
						{
							gl_DHCP_Packet_received=1;
						}
						break;
					}
				}
				//***************************************************************************************************************************
				
				if (!(memcmp(&gl_IP_Head_read.Dest_IP[0],&gl_Webserver.IP_address[0],4)==0))		// Wenns nicht unsere IP ignoriere das Paket
				{
					break;
				}
				
				switch(gl_IP_Head_read.Protocol)
				{
					case 0x01:																		// ICMP
						if(ICMP_Read_Header_from_Buffer(&gl_ICMP_Head_read,&gl_IP_Head_read))
						{
							break;
						}
						switch(gl_ICMP_Head_read.Type)
						{
							case 0x08:														
								ICMP_Packethandler();												// Pingrequest
								#ifdef Stack_Debug
									USART_Write_String("ICMP: Pingantwort gegeben\r\n");
								#endif
							break;
							
							default:
								#ifdef Stack_Debug
									USART_Write_String("ICMP: Unbekannter Typ, Paket wird nicht bearbeitet\r\n");
								#endif
							break;
						}
					break;
					
					case 0x06:
						if (TCP_Read_Header_from_Buffer(&gl_TCP_Head_read,&gl_IP_Head_read))
						{
							break;
						}
						switch (gl_TCP_Head_read.Destport)
						{
							case HTTP_Serverport:																// Webserver Port
								TCP_Porthandler(HTTP_Serverport,&HTTP_Server);
							break;
							
							case FTP_Command_Port:
								TCP_Porthandler(FTP_Command_Port,(void*)&FTP_Command_Server);
							break;
							
							case FTP_Data_Port:
								TCP_Porthandler(FTP_Data_Port,(void*)&FTP_Data_Server);
							break;
							
							case Firmwareupdater_Firmwareport:
								TCP_Porthandler(Firmwareupdater_Firmwareport,(void*)&Firmware_Updater);
							break;


							default:
								#ifdef Stack_Debug
									USART_Write_String("TCP: Port nicht erreichbar\r\n");
								#endif	
							break;
						}
					break;
					
					case 0x11:
						if(UDP_Read_Header_from_Buffer(&gl_UDP_Head_read,&gl_IP_Head_read))		// UDP
						{
							break;
						}
						switch (gl_UDP_Head_read.Destport)										// Portlistener
						{
							case NTP_Sourceport:												//NTP
								NTP_Porthandler();
							break;
							
							default:
								#ifdef Stack_Debug
									USART_Write_String("UDP: Port nicht erreichbar\r\n");
								#endif
							break;
						}
					break;
					
					default:
						#ifdef Stack_Debug
							USART_Write_String("IP4: Protocol unbekannt und wird nicht verarbeitet\r\n");
						#endif
					break;
				}
			break;
			
			default:
				#ifdef Stack_Debug
					USART_Write_String("Paketloop: Unbekanter Pakettyp\r\n");
				#endif
			break;
		}
	}
}
