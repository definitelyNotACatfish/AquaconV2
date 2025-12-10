#ifndef NTP_H_
#define NTP_H_

// Includes
#include "ENC28J60.h"
#include "Stack.h"
#include "time.h"

/*

************************************************************************************************
* NTP Protokoll											  									   *
* 2019 � Frederinn															 		   *
************************************************************************************************

*/

// Defines
//#define NTP_Debug
#define NTP_Transmission_Frequency	1000													// Wiederholgeschwindigkeit Pakete in ms, ca.
#define NTP_Retransmission_Max 		5														// Maximale Wiederholungen
#define NTP_Sourceport				1338
#define NTP_Destport				123

// Structs
struct NTP_Head
{
	uint8_t Flags;
	uint8_t Peer_Clock_Stratum;
	uint8_t Peer_Polling_Interval;
	uint8_t Peer_Clock_Precision;
	uint32_t Root_Delay;
	uint32_t Root_Dispersion;
	uint32_t Reference_ID;
	uint64_t Reference_Timestamp;
	uint64_t Origin_Timestamp;
	uint64_t Receive_Timestamp;
	uint64_t Transmit_Timestamp;
};

// Variablen
extern struct NTP_Head gl_NTP_Head_Read;																			// Speicher fuer Paketdaten
extern float gl_NTP_Timezone;																						// Standard Zeitzone
extern uint8_t gl_NTP_Summertime;																					// Sommer Winterzeit
uint8_t gl_NTP_Paket_received;																						// 0=Anfrage gesendet, 1=Antwort erhalten

// Funktionen
extern void NTP_Read_Header_from_Buffer(struct NTP_Head *Head);														// Lie�t Daten aus dem Header
extern uint8_t NTP_Request_Time_from_Server(char *IP_Serv);															// Macht einen Request auf die gl_Webserver.NTP_IP auf Port 123
extern uint8_t NTP_Init(void);																						// Fordere Zeit von Server an, IP wird in DHCP_Init() ermittelt
extern void NTP_Convert_UTC_Seconds_to_Time(uint32_t Time, float Timezone, struct Timestamp *Timestamp_read);		// NTP UTC Convert mit Sommer- bzw. Winterzeitberechnung
extern uint8_t NTP_Monthlen(uint8_t isleapyear,uint8_t month);														// Gibt die Monatslaenge in Tage zurueck, abhaenging von Schaltjahr
extern void NTP_Request_Func_for_Tasktimer(void);																	// Funktion f�r Tasktimer
extern void NTP_Porthandler(void);																					// NTP Porthandler Function



#endif /* NTP_H_ */
