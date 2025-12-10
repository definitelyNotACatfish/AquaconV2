#ifndef FIRMWAREUPDATER_H_
#define FIRMWAREUPDATER_H_

/*

************************************************************************************************
* Firmware Update per TCP 5555  															   *
* 2017 - 2019 � Frederinn															   *
************************************************************************************************

*/

// Defines
#define Firmwareupdater_Firmwareport 5555																	// Port f�r Firmwareupdater_Firmwareport
//#define Firmware_Debug																					// Firmwaredebug

// Structs
struct Firmwareupdate
{
	unsigned long int Filelength_received;																	// Empfangene Dateil�nge nach Instruktion
	unsigned char Position_in_Table_connected;																// Lasse nur eine Verbindung zu, von dem die Daten kommen
	unsigned char Command;																					// Variable ob SD ode RAM zum speichern verwendet wird
	unsigned char Statemachine;																				// Statemachine des Firmwareupdaters
};

// Variablen
extern struct Firmwareupdate gl_Firmwareupdate;																// Variablen f�r Firmwareupdate

// Funktionen
extern unsigned char Firmware_Updater(unsigned char Position_in_Table);										// Firmwareupdatefunktion










#endif
