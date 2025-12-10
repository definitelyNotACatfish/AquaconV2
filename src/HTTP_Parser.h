#ifndef HTTP_PARSER_H_
#define HTTP_PARSER_H_
// Includes

/*

************************************************************************************************
* HTTP Parser um Platzhalter in Textdatein zu ersetzen  									   *
* 2019 � Frederinn														 		   	   *
************************************************************************************************

*/

// Defines
#define		Parser_min_MSS		512																							// Der MSS der Verbindung muss mindestens so gro� in Bytes sein um das Parsen zu erm�glichen
#define		Parser_String_len	80																							// Maximale Stringl�nge zum Austauschen, maximum 64 Byte
#define		Parser_Ident_char   '%'																							// Identifizierung f�r die Variablen %Date%, %Time% usw.

// Structs
struct Parser
{
	char Buffer[Parser_String_len];																							// Hier wird das zu parsende und geparste Text gespeichert
};

// Variablen
extern struct Parser gl_Parser;																								// Globale Verwaltung der Parservariablen

// Funktionen
extern void Parser_do_parse_for_GET(void);																					// Parsing f�r GET durchf�hren
extern void Parser_do_parse_for_POST(void);																					// Parsing in POST durchf�hren













#endif
