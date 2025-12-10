#ifndef DHCP_H_
#define DHCP_H_

// Includes
#include "Stack.h"
#include "ENC28J60.h"

/*

************************************************************************************************
* DHCP Client											  									   *
* 2019 � Frederinn															  		   *
************************************************************************************************

*/

// Defines
//#define	DHCP_Debug

#define DHCP_Transmission_Frequency 2000												// Wiederholgeschwindigkeit Pakete in ms, ca.
#define DHCP_Retransmission_Max 5														// Maximale Wiederholungen
#define DHCP_Destport 68																// Destport
#define DHCP_Sourceport 67																// Sourceport

#define DHCP_Idle		0																// warten
#define DHCP_Discover	1																// Defines f�r Argumente
#define DHCP_Offer		2
#define DHCP_Request	3
#define DHCP_ACK		5
#define DHCP_Inform		8																//

// Structs
struct DHCP_Head
{
	uint8_t Message_Type;																// 1=Request, 2=Reply
	uint8_t Hardware_Type;																// Normal 1 ; Ethernet
	uint8_t Hardware_address_length;													// Normal 6 ; MAC Laenge
	uint8_t Hops;																		// Normal 0
	uint32_t Transaction_ID;															// ID fuer Transaktion, aehnlich IP Header
	uint16_t Seconds_elapsed;															// Normal 0
	uint16_t Bootp_flags;																// Unwichtig 0
	char Client_IP_address[4];															// Meine aktuelle IP addresse
	char Your_IP_address[4];															// IP addresse die der DHCP Server mir zuordnet
	char Next_server_IP_address[4];														// Gateway addresse
	char Relay_agent_IP_address[4];														// Unwichtig 0
	char Client_MAC_address[16];														// Ersten 6 Bytes fuer MAC
	char Server_Name[64];																// Server Name, unwichtig
	char File[128];																		// Boot Filename, unwichtig
	uint32_t Magic_cookie;																// Magic Cookie
	char Options[308];																	// Pointer zu den Optionen, nur f�r Read
};

struct DHCP_Cache
{
	char Name[30];																		// DHCP Name
	char Own_IP[4];																		// Ermittelte IP
	char Gateway_IP[4];																	// Gateway IP
	char Subnetmask[4];																	// Subnetmask
	char DHCP_Server_IP[4];																// DHCP Server IP
	char NTP[4];																		// NTP
	uint8_t Status;																		// Status Verbindung
	uint32_t Lease;																		// Leasetime in s
	uint32_t TransID;																	// TransID
};

struct DHCP_Option
{
	uint8_t Length;																		// L�nge
	char *Data;																			// Datapointer	
};

// Variablen
extern struct DHCP_Head gl_DHCP_Head_Read;												// Globale Variablen
extern struct DHCP_Cache gl_DHCP_Cache;													// DHCP Cache
extern struct DHCP_Option gl_DHCP_Option;												// DHCP Option aus Header
extern uint8_t gl_DHCP_Packet_received;													// Paket empfangen

// Fvunktionen
extern uint8_t DHCP_Init(void);															// DHCP Server IP aus EEPROM laden, und IP fuer IP Stack anfordern
extern uint8_t DHCP_Request_IP(void);													// Fordert eine IP �ber DHCP an
extern void DHCP_Read_Header(struct DHCP_Head *Head, struct IP_Head *IP_Header);		// DHCP Header auslesen
extern uint8_t DHCP_Read_Option(uint8_t Option, struct DHCP_Option *Return, struct DHCP_Head *Head); // Liest Option aus Header und setzt Pointer drauf
extern uint8_t DHCP_Send_Packet(uint8_t Type, uint32_t Transaction_ID);					// Sende DHCP Paket


#endif






