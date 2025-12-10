#ifndef HTTP_H_
#define HTTP_H_

// Includes
#include "Stack.h"

/*

************************************************************************************************
* HTTP Webserver Handling																	   *
* 2019 � Frederinn															 	  	   *
************************************************************************************************

*/

// Defines
//#define				 	HTTP_Debug
#define					HTTP_Max_Entries	5								// Maximale Anzahl an gleichzeitigen Verbindungen
#define					HTTP_Cache_Age		"604800"						// in Sekunden als String, 7 Tage
#define					HTTP_Serverport		80								// HTTP Serverport

// Structures
struct HTTP_Head
{
	char *Method;															// Methode POST, GET usw.
	char *Filepath;															// Dateipfad der aufzurufenden Datei
	uint16_t Filepath_length;												// L�nge des Dateipfades
	char *Argument;															// Argumente
	char *Data;																// Daten die bei POST mitgegeben werden
};

struct HTTP_Table
{
	struct pars
	{
		uint8_t Use;														// Parser verwenden 1=ja, 0=nein
		unsigned int Leftdata;												// >0 =Daten aus Puffer verwenden,0=neue laden
		unsigned int Bytes_in_Buffer;										// Anzahl von Max Bytes im Buffer (Entweder 512, wenn kleiner wurde nur die Leftdata geladen
		char Buffer[512];													// Puffer zum schnelleren lesen des Parserbuffers
	}Parser;
	uint8_t Content_Length_used;											// Wird die Content-Length ben�tigt 1=ja, 0=nein
	uint8_t Statemachine;													// Aktueller Zustand der Statemachine 0=Erstaufruf, 1=Es gib noch was zu tun
	uint32_t Leftdata;														// Anzahl der noch zu senden Bytes
};

// Globale Variablen
extern char gl_Default_Dir[128];											// Standarddir
extern char gl_Default_Page[13];											// Startseite des Servers in den RAM geladen
extern char gl_Default_404[13];												// Standard 404
extern char gl_HTTP_Default_String[200];									// Nachricht fuer die Umleitung von 192.168.178.14 auf 192.168.178.14/home.htm
extern struct HTTP_Head gl_HTTP_Head_read;									// HTTP Header
extern struct HTTP_Table gl_HTTP_Table[HTTP_Max_Entries];					// Http Tabelle

//Funktionen
extern void HTTP_Init(void);												// Init
extern void HTTP_Read_Header(char *Head, uint16_t Length);					// Lese Header
extern void HTTP_Server(uint8_t Position_in_Table);							// Server






#endif /* HTTP_H_ */
