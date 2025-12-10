// Includes
#include "main.h"
#include "FirmwareUpdater.h"
#include "Stack.h"
#include "USART.h"
#include "FAT32.h"
#include "Allerlei.h"
#include "Stack.h"
#include <stdio.h>

/*

************************************************************************************************
* Firmware Update per TCP 5555  															   *
* 2017 - 2019 � Frederinn															   *
************************************************************************************************

*/

// Variablen
struct Firmwareupdate gl_Firmwareupdate;																		// Variablen f�r Firmwareupdate

// Funktionen
unsigned char Firmware_Updater(unsigned char Position_in_Table)
{
	char *G;																									// Zeiger auf die Daten
	uint16_t g=0;																								// Lenge des gelesenen Paketes in Bytes
	
	if (gl_Firmwareupdate.Position_in_Table_connected != Position_in_Table && gl_Firmwareupdate.Statemachine != 0)	// Lasse den ersten Funktionsaufruf durch, aber die n�chsten von falschen IP und Port werden ignoriert
	{
		#ifdef Firmware_Debug
			USART_Write_String("Firmwareupdate: Daten von falscher IP und/oder Port wurden erkannt. Keine Antwort gesendet\r\n");
		#endif
		return 1;
	}
	
	if(gl_TCP_Table[Position_in_Table].Status == TCP_RST)														// Wenn die Verbindung resetet wurde, L�sche die fehlerhafte Datei 
	{
		gl_Firmwareupdate.Statemachine = 0;																		// Appstatus auf 0
		FAT32_File_Close(Position_in_Table);
		
		if(gl_Firmwareupdate.Command=='F')
		{
			FAT32_File_Delete("firmware.bin");																	// L�sche die unvollst�ndige Datei, falls vorhanden
		}
		return 1;
	}
	
	switch (gl_Firmwareupdate.Statemachine)
	{
		case 0:																									// Wenn der erste Aufruf kommt schaue was zu tun ist
			if(TCP_Read_Data(Position_in_Table,&G,&g)==0)														// Wenn Daten im TCP Paket, lese dies aus und mache weiter
			{
				gl_Firmwareupdate.Position_in_Table_connected = Position_in_Table;								// Speichere die Position
				gl_Firmwareupdate.Command = G[0];																// Sage welches Speichermedium verwendet werden soll
				sscanf(&G[2], "%lu", &gl_Firmwareupdate.Filelength_received);									// Gesammte Dateil�nge
	
				switch(gl_Firmwareupdate.Command)
				{
					case 'F':
						FAT32_Directory_Change("/");															// Wechsle in die Root
						FAT32_File_Delete("firmware.bin");														// L�sche die alte Firmware.bin
						if (FAT32_File_Create("firmware.bin",0x00))												// Erstelle die Datei neu
						{
							#ifdef Firmware_Debug
								USART_Write_String("Firmwareupdate: firmware.bin konnte nicht auf SD Karte angelegt werden. Karte ist voll.\r\n");
							#endif
							return 1;
						}
						FAT32_File_Open(Position_in_Table,"firmware.bin",FAT32_Write);							// �ffne die Datei zum Schreiben
						gl_Firmwareupdate.Statemachine=1;														// Appstatus setzen
						#ifdef Firmware_Debug
							USART_Write_String("Firmwareupdate: firmware.bin erstellt und geoeffnet\r\n");
						#endif
					break;
					
					case 'R':
						TCP_Close_Connection(Position_in_Table,TCP_Connection_Active);							// Schlie�e die Verbindung aktiv
						USART_Write_String("Firmwareupdate: TCP Remoteneustart!\r\n");							// Gebe die Meldung raus

						IWDG->KR = 0x00005555;																	// Schreibzugriff auf Watchdogregister
						IWDG->PR = 0;																			// Teiler auf /4
						IWDG->RLR = 0x01;																		// Wert auf 1
						IWDG->KR = 0x0000cccc;																	// Watchdogtimer einschalten
						while(1);																				// Endlosschleife loest Watchdog auswhile(1);																				// Endlosschleife loest Watchdog aus
					break;
					
					default:
						return 1;																				// Wenn der Command nicht stimmt, gebe Fehler zur�ck
					break;
				}
				return 0;
			}
		break;
		
		case 1:
			if(TCP_Read_Data(Position_in_Table,&G,&g)==0)														// Wenn Daten im TCP Paket, lese dies aus und mache weiter
			{
				switch(gl_Firmwareupdate.Command)
				{
					case 'F':
						if (FAT32_File_Write(Position_in_Table,&G[0],g))											// Versuche den Dateiinhalt zu schreiben
						{
							#ifdef Firmware_Debug
								USART_Write_String("Firmwareupdate: In firmware.bin konnte nicht geschrieben werden. Karte ist voll.\r\n");
							#endif
							FAT32_File_Close(Position_in_Table);
							FAT32_Directory_Change("/");															// Wechsle in die Root
							FAT32_File_Delete("firmware.bin");														// L�sche die unvollst�ndige Datei
							TCP_Close_Connection(Position_in_Table,TCP_Connection_Active);
							return 1;
						}
						gl_Firmwareupdate.Filelength_received -= g;													// Ziehe die gesendeten Bytes ab
						if (gl_Firmwareupdate.Filelength_received == 0)												// Wenn alle Bytes empfangen wurden schlie�e Datei
						{
							FAT32_File_Close(Position_in_Table);
							#ifdef Firmware_Debug
								USART_Write_String("Firmwareupdate: firmware.bin geschlossen\r\n");
							#endif
							gl_Firmwareupdate.Statemachine = 0;														// Appstatus auf 0
						}
					break;

					default:
						return 1;
					break;
				}
				return 0;
			}
		break;
	}
	return 1;
}
