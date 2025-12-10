#ifndef STACK_H_
#define STACK_H_

// Includes
#include "ENC28J60.h"

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

// Defines
//#define Stack_Debug																// Debug ein/aus
#define Byteswap16(x) ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))					// Byteswap

// RAM Usage entwerder oder einbinden
#define Use_internal_RAM															// Wenn der interne RAM verwendet werden soll, nutze diese Define
//#define Use_external_RAM															// Bei externem RAM muss dieses Define einkommentiert werden

// ARP
#define ARP_Max_Entries					5											// Maximale Anzahl an ARP Eintraegen<255
#define ARP_Request						0x01
#define ARP_Answer						0x02

// TCP
#define TCP_Max_Entries					5											// Maximale TCP Eintraege <255
#define TCP_Retransmission_Frequency	2											// TCP Wiederholungen senden in Sekunden 1
#define TCP_Retransmission_Max			5											// Max TCP Wiederholungen
#define TCP_MSS							1460										// MSS des Servers max 1460 Bytes
#define TCP_Window						1460										// Normalerweise so gro� wie der Buffer, aber niemals gr��er 1460 Bytes
#define TCP_Connection_Active			0x01										// Verbindung aktiv �ffnen
#define TCP_Connection_Passive			0x00										// Verbindung Passiv �ffnen
// TCP States																		// TCP Verbindungsstatus
#define TCP_Closed						0x00
#define TCP_Listen						0x01
#define TCP_Syn_Sent					0x02
#define TCP_Syn_Recd					0x03
#define TCP_Established					0x04
#define TCP_Fin_Wait1					0x05
#define TCP_Fin_Wait2					0x06
#define TCP_Close_Wait					0x07
#define TCP_Last_Ack					0x08
#define TCP_Time_Wait					0x09
#define TCP_RST							0x0a
#define TCP_Closing						0x0b
// TCP Sendstats																	// Sendestatus der Pakete f�r die Handhabung des Sendewiederholung einzelner Pakete
#define TCP_RST_sent					0x00										// Rst senden
#define TCP_SYNACK_sent					0x01										// Synack wurde gesendet
#define TCP_PSHACK_sent					0x02										// PSHACK gesendet
#define TCP_FINACK_sent					0x03										// Finack gesendet
#define TCP_SYN_sent					0x04										// Syn wurde gesendet
#define TCP_ACK_received				0x05										// Ack empfangen
#define TCP_SYNACK_received				0x06										// Synack auf Syn wurde empfangen
#define TCP_PSHACK_received				0x07										// Ack auf PSHACK empfangen
#define TCP_FIN_received				0x08										// Fin empfangen
#define TCP_FINACK_received				0x09										// FinAck empfangen
#define TCP_RST_received				0x0A										// Rst empfangen
#define TCP_RSTACK_received				0x0B										// RstAck empfangen
#define TCP_SYN_received				0x0C										// Syn empfangen
#define TCP_ACK_sent					0x0d										// Ack gesendet
// TCP Headerflags
#define TCP_PshAck						0x18										// TCP PSH ACK
#define TCP_Psh							0x08										// TCP PSH
#define TCP_Syn							0x02										// TCP Syn
#define TCP_SynAck						0x12										// TCP SynAck
#define TCP_Ack							0x10										// TCP Ack
#define TCP_Fin							0x01										// TCP Fin
#define TCP_FinAck						0x11										// TCP FinAck
#define TCP_FinPshAck					0x19										// TCP FinPshAck
#define TCP_Rst							0x04										// TCP Rst
#define TCP_RstAck						0x14										// TCP RstAck

// Structures
struct ARP_Table
{
	char IP[4];																		// IP
	char MAC[6];																	// MAC
	uint8_t Used;																	// Zeigt ob der Eintrag belegt ist
};

struct ARP_Head
{
	uint16_t Hardwaretype;
	uint16_t Protocoltype;
	uint8_t Hardwaresize;
	uint8_t Protocolsize;
	uint16_t Opcode;
	char Sender_MAC[6];
	char Sender_IP[4];
	char Target_MAC[6];
	char Target_IP[4];
};

struct IP_Head
{
	uint16_t Totallength;															// Wird von der hoeheren Anwendung berechnet
	uint16_t Identifikation;														// Identifikation fuer das Packet
	uint8_t Flags;																	// Fragmentierungsflags
	uint16_t Fragment_Offset;														// Fragmentierungsoffset
	uint8_t TTL;																	// TTL
	uint8_t Protocol;																// Ob UDP, TCP
	uint16_t Checksum;																// Checksum
	char Dest_IP[4];																// Die empfaenger IP
	char Source_IP[4];																// Quell_IP; Wird nur fuer den Readhader benoetigt
};

struct UDP_Head
{
	struct PSHD_UDP
	{
		char Source_IP[4];															// Quell IP
		char Dest_IP[4];															// Ziel IP
		uint16_t Protocoll_ID;														// Null und 17
		uint16_t UDP_Datagram_Length;												// laenge des UDP Paketes
	}Pseudoheader;
	uint16_t Sourceport;															// Quellport
	uint16_t Destport;																// Zielport
	uint16_t Length;																// Laenge
	uint16_t Checksum;																// Checksumme Paket + Pseudoheader
	char *Data;																		// Data
};

struct TCP_Head																		// Sowohl fuer write und read
{
	struct PSHD_TCP
	{
		char Source_IP[4];															// Quell IP
		char Dest_IP[4];															// Ziel IP
		uint16_t Protocoll_ID;														// Null und 17
		uint16_t TCP_Datagram_Length;												// laenge des UDP Paketes
	}Pseudoheader;
	uint16_t Sourceport;															// Sourceport
	uint16_t Destport;																// Destport
	uint32_t Sequencenumber;														// Sequencenummer aus Sicht des Webservers
	uint32_t Acknumber;																// Acknummer aus Sicht des Webservers
	uint16_t Dataoffset;															// Dataoffset
	uint8_t Flags;																	// Flags
	uint16_t Window;																// Window
	uint16_t Checksum;																// Checksumme
	uint16_t Urgentpointer;															// Urgentpointer
	char Options[40];																// Optionen
	uint16_t Datalength;															// Datalength
	char *Data;																		// Zeiger auf das erste Datenbyte
};

struct TCP_Table																	// TCP Tabelle aller Verbindungen
{
	struct Retrans
	{
		uint8_t Retransmitcount;													// Timer um tote Verbindungen zu schlie�en
		char MAC[6];																// MAC Adresse als Backup
		uint16_t Length;															// L�nge des Paketes
		uint16_t SRAM_Address;														// SRAM Adresse
	}Retransmission;
	uint8_t Packetstatus;															// Zur Steuerung des Retransmittes
	uint8_t In_use;																	// In Benutzung 0=nein, 1=ja
	uint8_t Status;																	// Status der Verbindung
	uint16_t Transaction_MSS;														// MSS der Verbindung
	uint8_t Client_Windowscale;														// Windowscale des Clients
	uint16_t Client_MSS;															// Max Segment Size Client
	char Client_IP[4];																// IP des Client
	uint16_t Destport;																// Port	des Client
	uint16_t Sourceport;															// Port des Servers
	uint32_t Sequencenumber;														// Letzte Sequencenummer schreiben
	uint32_t Acknumber;																// Letzte Acknummer schreiben
	//void (*FP)(uint8_t);															// Zeiger auf Funktion	// Wegen Pointeradresse im Struct muss gerade sein, wegen -fpack-struct ist dies nicht gew�hrleistet und zieht in "gl_TCP_FP_Table" Array aus
};

// Globale Variablen
#ifdef Use_internal_RAM
	extern char gl_TCP_Retransmit_Packet_Backup[TCP_Max_Entries][ENC_Buffsize];											// TCP Retransmit Packet Backup
#endif
extern void (*gl_TCP_FP_Table[TCP_Max_Entries])(uint8_t);																// FP Array f�r TCP Retransmission
extern uint16_t gl_IP_Identification;																					// Identifikationsnummer fuer die gesendeten Pakete
extern const char gl_TCP_Status_Name_Table[11][13];																		// TCP Verbindungsstatus als String
extern struct ARP_Table		gl_ARP_Table[ARP_Max_Entries];																// ARP Tabelle
extern struct ARP_Head		gl_ARP_Head_Read;																			// ARP Leseheader
extern struct IP_Head		gl_IP_Head_read;																			// IP Leseheader
extern struct TCP_Head		gl_TCP_Head_read;																			// TCP Leseheader
extern struct TCP_Table		gl_TCP_Table[TCP_Max_Entries];																// TCP Tabelle
extern struct UDP_Head		gl_UDP_Head_read;																			// UDP Leseheader

//Funktionen
extern void ARP_Read_Header_from_Buffer(struct ARP_Head *ARP_Header);													// Liest den ARP Header aus dem Readbuffer
extern void ARP_Create_Header_in_Buffer(struct ARP_Head *ARP_Header);													// Erstellen einen ARP Header im Buffer
extern uint8_t ARP_Send_Packet (struct ARP_Head *ARP_Header);															// return 0=ok, 1=nok; MAC ist optional und wird nur bei der Answer benoetigt
extern void ARP_Clear_Table(void);																						// Komplette ARP Tabelle loeschen
extern uint8_t ARP_Position_in_Table(char *IP);																			// Gebe die Position im Array zurueck an welcher stelle in der Tabelle sich die IP befindet
extern uint8_t ARP_Set_Entry(char *MAC,char *IP);																		// Eintrag mit Zeitstempel; return 0=erstellt, 1=bereits vorhanden
extern uint8_t ARP_Clear_Entry(char *IP);																				// Loescht einen Eintrag aus der ARP Tabelle
extern uint8_t ARP_Get_MAC_from_IP(char *Return_MAC,char *IP);															// Prueft ob die IP in der Tabelle steht und aktuell ist, ansonsten fordert die funktion die aktuelle IP an
extern void ARP_Read_MAC_from_Table(char *Return_MAC, char *IP);														// Gibt bei Fehler 0-String zurueck

extern void IP_Create_Header_in_Buffer(struct IP_Head *Head);															// Erstelle einen IP Header
extern uint8_t IP_Read_Header_from_Buffer(struct IP_Head *Head);														// Lese IP Header aus dem Buffer 0=ok, 1=nok
extern uint16_t IP_Calc_Checksum(uint16_t len_ip_header, char *buff);													// Errechne die Checksum fuer den IP Header

extern void TCP_Create_Header_in_Buffer(struct TCP_Head *TCP_Header, struct IP_Head *IP_Header);						// Erstellt einen TCP Header im Buffer
extern uint8_t TCP_Read_Header_from_Buffer(struct TCP_Head *TCP_Header, struct IP_Head *IP_Header);						// Lese TCP Header aus Buffer 0=ok, 1=nok
extern uint16_t TCP_Calc_Checksum(struct TCP_Head *Head, char *buff);													// Berechne die Checksumme eines TCP Headers
extern uint8_t TCP_Send_Packet(struct TCP_Head *TCP_Header, struct IP_Head *IP_Header, struct ENC_Head *ENC_Header);	// Paket senden, Daten liegen als Pointer auf UDP_Header->Daten 0=ok, 1=nok
extern uint8_t TCP_Set_Entry_in_Table(struct TCP_Table *TCP_Table_Entrie);												// Trage Eintrag in TCP Tabelle ein, <255 Position, 255=nok
extern uint8_t TCP_Get_Position_from_Table(char *IP, uint16_t Destport);												// Lade Position von IP <255=ok, 255=nok
extern uint8_t TCP_Get_Position_from_Table_Sp(char *IP, uint16_t Sourceport);											// Lade Position von IP <255=ok, 255=nok
extern uint16_t TCP_Read_MSS_Option_from_Buffer(void);																	// return 0=Error, return>0=ok
extern uint8_t TCP_Read_WS_Option_from_Buffer(void);																	// return<15=ok, return 15=Error
extern uint8_t TCP_Send_Ack(uint8_t Position_in_Table);																	// Sendet ein Ack auf ein Paket; 0=ok,1=nok

extern void TCP_Retransmittimer(void);																					// �berwacht und beendet halbtote Verbindungen
extern void TCP_Porthandler(uint16_t Port, void (*FP)(uint8_t));														// Packethandler
extern uint8_t TCP_Open_Connection(uint16_t Sourceport, uint16_t Destport, char *Dest_IP, uint8_t Active_Passive); 		// �ffnet eine Verbindung aktiv oder Passiv, 255=Error, <255 Position in TCP Tabelle
extern uint8_t TCP_Reset_Connection(uint8_t Position_in_Table);															// Resete Verbindung, 0=Paket gesendet, 1=nicht m�glich
extern uint8_t TCP_Close_Connection(uint8_t Position_in_Table, uint8_t Active_Passive);									// Schlie�t eine Verbindung aktiv oder Passiv, 0=ok, 1=Error
extern uint8_t TCP_Write_Data(uint8_t Position_in_Table, char *Data, uint16_t Len);										// Sende Daten �ber Verbindung
extern uint8_t TCP_Read_Data(uint8_t Position_in_Table, char **Data, uint16_t *Len);									// Best�tige Empfangenes Paket und gibt Datenpointer mit L�nge zur�ck

extern uint8_t UDP_Read_Header_from_Buffer(struct UDP_Head *UDP_Header, struct IP_Head *IP_Header);						// Lese Daten in Header, ueberpruefe Checksumme 0=ok, 1=nok
extern void UDP_Create_Header_in_Buffer(struct UDP_Head *UDP_Header,struct IP_Head *IP_Header);							// Schreibe Daten in Buffer, generiere Checksumme
extern uint16_t UDP_Calc_Checksum(struct UDP_Head *Head, char *buff);													// UDP Checksum calc
extern uint8_t UDP_Send_Packet(struct UDP_Head *UDP_Header, struct IP_Head *IP_Header, struct ENC_Head *ENC_Header);	// Paket senden, Daten liegen als Pointer auf UDP_Header->Daten 0=ok, 1=nok

extern void Stack_Packetloop(uint8_t ARP_Wait_Reply);																	// Schleife in der auf ein Packet gewartet und bearbeitet wird

#endif /* STACK_H_ */
