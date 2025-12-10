#ifndef HTTP_UC_H_
#define HTTP_UC_H_
// Includes


/*

************************************************************************************************
* Erweiterung des HTTP Servers um per GET direkt Informationen vom �C zu bekommen			   *
* 07.06.2019 � Frederinn															   *
************************************************************************************************

*/

// Defines
#define HTTP_uC_Maxlen		128												// Maximale Stringl�nge

// Structs


// Variablen


// Funktionen
extern void HTTP_uC_GET_requests(uint8_t Position_in_Table,char *Action);	// F�hrt verschiedene GET Anfragen auf �C Ressourcen aus (ADC, ...)

#endif /* HTTP_UC_H_ */
