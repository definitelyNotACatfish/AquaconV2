// Includes
#include "main.h"
#include "FTP.h"
#include "Stack.h"
#include "Allerlei.h"
#include "FAT32.h"
#include "time.h"
#include <stdio.h>
#include <string.h>
#include "usart.h"

/*

************************************************************************************************
* FTP Server auf STM32F407VGT6																   *
* 2019-2020 � Frederinn															 	   *
************************************************************************************************

*/

// Variablen
struct FTP_Table gl_FTP_Table;																						// FTP Tabelle mit Infos �ber die Verbindungen
const char gl_FTP_Months[][4]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};			// Monate als String
char gl_FTP_Buffer[250];																							// Sendepuffer

// Funktionen
void FTP_Command_Server(uint8_t Position_in_Table)
{
	if (gl_TCP_Table[Position_in_Table].Status!=TCP_Established)													// Falls die Verbindung nicht mehr als "Aufgebaut" gilt, setze die Statemachine zur�ck
	{
		gl_FTP_Table.Statemachine_Command=0;
		FAT32_File_Close(FTP_Filenumber);
		#ifdef FTP_Debug
			USART_Write_String("FTP: Commandverbindung wurde vom Client beendet\r\n");
		#endif
		return;
	}
	
	switch (gl_FTP_Table.Statemachine_Command)
	{
		char *G;
		uint16_t g;
		
		case 0:																																					
			gl_FTP_Table.Statemachine_Command = 1;														// Funktion das erste mal aufrufen direkt nach dem Ack vom SynAck von 4 auf 1 umstellen
			sprintf(&gl_FTP_Table.Workingpath[0],"/");													// Startverzeichnis ist root
			gl_FTP_Table.Client_Command_PiT = Position_in_Table;										// Speichere den Kommandoport des Client ab
			TCP_Write_Data(Position_in_Table,"220 aquacon_ftp\r\n",17);									// Meldung das Verbindung steht
		break;
		
		case 1:
			if (TCP_Read_Data(Position_in_Table,&G,&g)!=0)												// Im Packet gab es nichts zu lesen, breche ab
			{
				#ifdef FTP_Debug
					USART_Write_String("FTP: Im angekommenen Paket gab es nichts zu lesen\r\n");
				#endif
				return;
			}
			
			memcpy(&gl_FTP_Table.Last_Command[0],&G[0],g);												// Kopiere den letzen Befehl

			#ifdef FTP_Debug
				USART_Write_String("FTP:");
				USART_Write_X_Bytes(&gl_FTP_Table.Last_Command[0],0,g);
				USART_Write_String("\r\n");
			#endif
		
			if (STRCMP_ALT("USER",&G[0])==0)
			{
				sprintf(&gl_FTP_Buffer[0],"331 Please specify the password.\r\n");						// Meldung das Verbindung steht
			}
			else if (STRCMP_ALT("PASS",&G[0])==0)
			{
				sprintf(&gl_FTP_Buffer[0],"230 Login successful.\r\n");									// Meldung das Verbindung steht
			}
			else if (STRCMP_ALT("SYST",&G[0])==0)
			{
				sprintf(&gl_FTP_Buffer[0],"215 STM32F407VGT6\r\n");										// Meldung welches System es ist
			}
			else if (STRCMP_ALT("FEAT",&G[0])==0)
			{
				sprintf(&gl_FTP_Buffer[0],"211-Features:\r\nPASV\r\n211 END.\r\n");						// Meldung welche Kommandos vom Server bearbeitet werden
			}
			else if (STRCMP_ALT("OPTS UTF8 ON",&G[0])==0)
			{
				sprintf(&gl_FTP_Buffer[0],"504 Command not implemented for that parameter.\r\n");		// Meldung UTF-8 wird nicht unterst�tzt
			}
			else if (STRCMP_ALT("PWD",&G[0])==0)
			{
				sprintf(&gl_FTP_Buffer[0],"257 \"");
				strcat(&gl_FTP_Buffer[0],&gl_FTP_Table.Workingpath[0]);									// Aktuelles Verzeichnis ausgeben
				strcat(&gl_FTP_Buffer[0],"\"\r\n");
			}
			else if (STRCMP_ALT("PASV",&G[0])==0)															// Passiver Modus
			{
				sprintf(&gl_FTP_Buffer[0],"227 Entering Passive Mode (%u,%u,%u,%u,%u,%u).\r\n",gl_Webserver.IP_address[0],gl_Webserver.IP_address[1],gl_Webserver.IP_address[2],gl_Webserver.IP_address[3],(FTP_Data_Port>>8)&0xff,FTP_Data_Port&0xff); // Gehe in den passiven Modus
			}
			else if (STRCMP_ALT("TYPE",&G[0])==0)															// Verbindungstyp
			{
				switch (G[5])
				{
					case 'A':
						sprintf(&gl_FTP_Buffer[0],"200 Switching to ASCII mode.\r\n");
					break;
					case 'I':
						sprintf(&gl_FTP_Buffer[0],"200 Switching to Binary mode.\r\n");
					break;
					default:
						sprintf(&gl_FTP_Buffer[0],"504 Command not implemented for that parameter.\r\n");
					break;
				}
			}
			else if (STRCMP_ALT("LIST",&G[0])==0)															// List Verzeichnis auflisten
			{
				uint8_t Client_Data_PiT = TCP_Get_Position_from_Table_Sp(&gl_TCP_Table[Position_in_Table].Client_IP[0],FTP_Data_Port);				// Lade die Verbindungsid die mit unserem Datenport verbunden ist
				
				sprintf(&gl_FTP_Buffer[0],"150 Here comes the directory listing.\r\n");
				TCP_Write_Data(Position_in_Table,&gl_FTP_Buffer[0],strlen(&gl_FTP_Buffer[0]));				// Paket schicken
				
				gl_FTP_Table.LIST_Filecounter=0;															// Der 0.te Eintrag ist der Name der SD Karte
				
				if (STRCMP_ALT(&gl_FTP_Table.Workingpath[0],"/")==0)										// In der Root ignoriere den ersten Eintrag, denn der ist der Kartenname
				{
					gl_FTP_Table.LIST_Filecounter++;
				}
				
				gl_FTP_Table.LIST_Statemachine=1;
				
				if (Client_Data_PiT!=255)																	// Wenn die Verbindung gefunden wurde, schicke den ersten Eintrag
				{
					FTP_List_Send_Entry(Client_Data_PiT);
				}
				#ifdef FTP_Debug
					USART_Write_String(&gl_FTP_Buffer[0]);
				#endif
				return;
			}
			else if (STRCMP_ALT("CDUP",&G[0])==0)															// CDUP Ein Verzeichnis nach oben
			{
				uint8_t length = strlen(&gl_FTP_Table.Workingpath[0]);										// Ermittle L�nge des Strings
				char * pch;
				gl_FTP_Table.Workingpath[length-1]=0;														// Setze letztes Zeichen des Strings 0
				pch=strrchr(&gl_FTP_Table.Workingpath[0],'/');												// Schneide den letzten Verzeichnisteil ab nach dem /
				pch++;
				*pch = 0;

				if (FAT32_Directory_Change(&gl_FTP_Table.Workingpath[0]))
				{
					sprintf(&gl_FTP_Buffer[0],"550 Directory not found.\r\n");								// Meldung Pfad nicht gewechselt
				}
				else
				{
					sprintf(&gl_FTP_Buffer[0],"250 Directory successfully changed.\r\n");					// Meldung Pfad gewechselt
				}
			}
			else if (STRCMP_ALT("CWD",&G[0])==0)															// CWD Pfad wechseln
			{
				char path[12];
				uint8_t strlength;
				sscanf(&G[3],"%s",&path[0]);																// Pfad lesen
				if(path[0]=='/')																			// Der Pfad ist kein Ordner sondern ein Verzeichnissprung
				{
					strcpy(&gl_FTP_Table.Workingpath[0],&path[0]);
					strlength = strlen(&gl_FTP_Table.Workingpath[0]);
					gl_FTP_Table.Workingpath[strlength-1]='/';												// Filezilla macht /pfad nicht /pfad/ wie TotalComander
				}
				else
				{
					strcat(&gl_FTP_Table.Workingpath[0],&path[0]);
					strcat(&gl_FTP_Table.Workingpath[0],"/");
				}
				
				if (FAT32_Directory_Change(&gl_FTP_Table.Workingpath[0]))
				{
					sprintf(&gl_FTP_Buffer[0],"550 Directory not found.\r\n");								// Meldung Pfad nicht gewechselt
				}
				else
				{
					sprintf(&gl_FTP_Buffer[0],"250 Directory successfully changed.\r\n");					// Meldung Pfad gewechselt
				}
			}
			else if (STRCMP_ALT("QUIT",&G[0])==0)																									// QUID Verbindung schlie�en
			{
				sprintf(&gl_FTP_Buffer[0],"226 Goodbye.\r\n");																						// Verabschiedung
				gl_FTP_Table.Statemachine_Command=0;
			}
			else if (STRCMP_ALT("RETR",&G[0])==0)																									// RETR FILENAME TXT
			{
				uint8_t Client_Data_PiT = TCP_Get_Position_from_Table_Sp(&gl_TCP_Table[Position_in_Table].Client_IP[0],FTP_Data_Port);				// Lade die Verbindungsid die mit unserem Datenport verbunden ist
				
				if (FAT32_Directory_Change(&gl_FTP_Table.Workingpath[0]))
				{
					sprintf(&gl_FTP_Buffer[0],"550 Directory not found.\r\n");																		// Meldung Pfad nicht gewechselt
					#ifdef FTP_Debug
						USART_Write_String(&gl_FTP_Buffer[0]);
					#endif
					TCP_Write_Data(Position_in_Table,&gl_FTP_Buffer[0],strlen(&gl_FTP_Buffer[0]));															// Paket schicken
					gl_FTP_Table.STOR_Statemachine=0;																										// Statemachine zur�cksetzen
					return;
				}
				if (FAT32_File_Open(FTP_Filenumber,&G[5],FAT32_Read)==2)																					// Datei �ffnen, hingewixxe aber FAT32_file_open �ffnet auch ohne richtigen string
				{
					sprintf(&gl_FTP_Buffer[0],"550 File not found.\r\n");
					TCP_Write_Data(Position_in_Table,&gl_FTP_Buffer[0],strlen(&gl_FTP_Buffer[0]));															// Paket schicken
				}
				else
				{
					sprintf(&gl_FTP_Buffer[0],"150 Sending File.\r\n");

					TCP_Write_Data(Position_in_Table,&gl_FTP_Buffer[0],strlen(&gl_FTP_Buffer[0]));															// Paket schicken
					gl_FTP_Table.RETR_Dataleft=gl_FAT32_File[FTP_Filenumber].Size;																			// Dateil�nge eintragen
					
					if (Client_Data_PiT!=255)																												// Wenn die Verbindung gefunden wurde, schicke den ersten Eintrag
					{
						FTP_RETR_Send_File(Client_Data_PiT);
					}
				}
				#ifdef FTP_Debug
					USART_Write_String(&gl_FTP_Buffer[0]);
				#endif
				return;
			}
			else if (STRCMP_ALT("STOR",&G[0])==0)																											// STOR FILENAME.TXT
			{
				uint8_t Result=0;
				char Filename[13]; 
				
				for(uint8_t i=0;i<13;i++)
				{
					Filename[i] = G[5+i];
					if(G[5+i]=='\r')
					{
						Filename[i]=0;
						break;
					}
				}
				
				if (FAT32_Directory_Change(&gl_FTP_Table.Workingpath[0]))
				{
					sprintf(&gl_FTP_Buffer[0],"550 Directory not found.\r\n");																				// Meldung Pfad nicht gewechselt
					#ifdef FTP_Debug
						USART_Write_String(&gl_FTP_Buffer[0]);
					#endif
					TCP_Write_Data(Position_in_Table,&gl_FTP_Buffer[0],strlen(&gl_FTP_Buffer[0]));															// Paket schicken
					gl_FTP_Table.STOR_Statemachine=0;																										// Statemachine zur�cksetzen
					return;
				}
				
				if (FAT32_File_Check_if_Exist(&Filename[0])==0)																								// Wenn die Datei existiert, l�sche die alte
				{
					Result = FAT32_File_Delete(&Filename[0]);
					if(Result==1)																															// Versuche die Datei zu l�schen
					{
						sprintf(&gl_FTP_Buffer[0],"550 File not found.\r\n");																				// Datei konnte nicht gel�scht werden
						#ifdef FTP_Debug
							USART_Write_String(&gl_FTP_Buffer[0]);
						#endif
						TCP_Write_Data(Position_in_Table,&gl_FTP_Buffer[0],strlen(&gl_FTP_Buffer[0]));														// Paket schicken
						gl_FTP_Table.STOR_Statemachine=0;																									// Statemachine zur�cksetzen
						return;																																// Breche ab
					}
					else if (Result==2)
					{
						sprintf(&gl_FTP_Buffer[0],"450 Requested file action not taken. File in use.\r\n");													// Datei konnte nicht gel�scht werden
						#ifdef FTP_Debug
							USART_Write_String(&gl_FTP_Buffer[0]);
						#endif
						TCP_Write_Data(Position_in_Table,&gl_FTP_Buffer[0],strlen(&gl_FTP_Buffer[0]));														// Paket schicken
						gl_FTP_Table.STOR_Statemachine=0;																									// Statemachine zur�cksetzen
						return;																																// Breche ab
					}
				}
				
				if(FAT32_File_Create(&Filename[0],0x00))																									// Versuche die Datei zu erstellen
				{
					sprintf(&gl_FTP_Buffer[0],"550 Filecreation not possible.\r\n");																		// Datei konnte nicht ge�ffnet werden
					#ifdef FTP_Debug
						USART_Write_String(&gl_FTP_Buffer[0]);
					#endif
					TCP_Write_Data(Position_in_Table,&gl_FTP_Buffer[0],strlen(&gl_FTP_Buffer[0]));															// Paket schicken
					gl_FTP_Table.STOR_Statemachine=0;																										// Statemachine zur�cksetzen
					return;
				}
				
				Result = FAT32_File_Open(FTP_Filenumber,&Filename[0],FAT32_Write);
				if(Result == 1)
				{
					sprintf(&gl_FTP_Buffer[0],"550 File allready open.\r\n");																				// Datei konnte nicht ge�ffnet werden
					gl_FTP_Table.STOR_Statemachine=0;																										// Statemachine zur�cksetzen
				}
				else if(Result == 2)
				{
					sprintf(&gl_FTP_Buffer[0],"550 File not found.\r\n");																					// Datei konnte nicht ge�ffnet werden
					gl_FTP_Table.STOR_Statemachine=0;																										// Statemachine zur�cksetzen
				}
				else
				{
					sprintf(&gl_FTP_Buffer[0],"125 Connection open, waiting for file.\r\n");
					gl_FTP_Table.STOR_Statemachine=1;																										// Statemachine starten
				}
				#ifdef FTP_Debug
					USART_Write_String(&gl_FTP_Buffer[0]);
				#endif
				TCP_Write_Data(Position_in_Table,&gl_FTP_Buffer[0],strlen(&gl_FTP_Buffer[0]));																// Paket schicken
				return;
			}
			else if (STRCMP_ALT("DELE",&G[0])==0)
			{
				char Filename[13];
				
				for(uint8_t i=0;i<13;i++)
				{
					Filename[i] = G[5+i];
					if(G[5+i]=='\r')
					{
						Filename[i]=0;
						break;
					}
				}

				if (FAT32_Directory_Change(&gl_FTP_Table.Workingpath[0]))
				{
					sprintf(&gl_FTP_Buffer[0],"550 Directory not found.\r\n");																				// Meldung Pfad nicht gewechselt
					#ifdef FTP_Debug
						USART_Write_String(&gl_FTP_Buffer[0]);
					#endif
					TCP_Write_Data(Position_in_Table,&gl_FTP_Buffer[0],strlen(&gl_FTP_Buffer[0]));															// Paket schicken
					return;
				}
				
				uint8_t Result = FAT32_File_Delete(&Filename[0]);																							// L�sche die Datei
				if(Result==1)																																// Versuche die Datei zu l�schen
				{
					sprintf(&gl_FTP_Buffer[0],"550 File not found.\r\n");																					// Datei konnte nicht gel�scht werden
				}
				else if (Result==2)
				{
					sprintf(&gl_FTP_Buffer[0],"450 Requested file action not taken. File in use.\r\n");														// Datei konnte nicht gel�scht werden
				}
				else
				{
					sprintf(&gl_FTP_Buffer[0],"250 DELE command successful.\r\n");
				}
				#ifdef FTP_Debug
					USART_Write_String(&gl_FTP_Buffer[0]);
				#endif
			}
			else if (STRCMP_ALT("MKD",&G[0])==0)																											// MKD Ordner
			{
				uint8_t Result=0;
				char Filename[13];
				
				for(uint8_t i=0;i<13;i++)
				{
					Filename[i] = G[4+i];
					if(G[4+i]=='\r')
					{
						Filename[i]=0;
						break;
					}
				}

				if (FAT32_Directory_Change(&gl_FTP_Table.Workingpath[0]))
				{
					sprintf(&gl_FTP_Buffer[0],"550 Directory not found.\r\n");																				// Meldung Pfad nicht gewechselt
					#ifdef FTP_Debug
						USART_Write_String(&gl_FTP_Buffer[0]);
					#endif
					TCP_Write_Data(Position_in_Table,&gl_FTP_Buffer[0],strlen(&gl_FTP_Buffer[0]));															// Paket schicken
					return;
				}

				Result = FAT32_Directory_Create(&Filename[0],0x00);																							// L�sche die Datei
				if(Result==1)																																// Versuche die Datei zu l�schen
				{
					sprintf(&gl_FTP_Buffer[0],"550 MKD not successful.\r\n");																				// Datei konnte nicht gel�scht werden
				}
				else
				{
					sprintf(&gl_FTP_Buffer[0],"257 %s created.\r\n", &Filename[0]);
				}
			}
			else if (STRCMP_ALT("HELP SITE",&G[0])==0)
			{
				sprintf(&gl_FTP_Buffer[0],"214-The following commands are recognize.\r\nCDUP\r\nCWD\r\nDELE\r\nHELP\r\nLIST\r\nMKD\r\nOPTS\r\nPASS\r\nPASV\r\nPWD\r\nQUIT\r\nSITE\r\nSTOR\r\nSYST\r\nTYPE\r\nUSER\r\n214 Help OK.\r\n");			// Meldung welches Kommandos vom Server erkannt werden
			}
			else
			{
				sprintf(&gl_FTP_Buffer[0],"502 Command not implemented.\r\n");																// Meldung Kommando nicht implementiert
			}
			#ifdef FTP_Debug
				USART_Write_String(&gl_FTP_Buffer[0]);
			#endif
			TCP_Write_Data(Position_in_Table,&gl_FTP_Buffer[0],strlen(&gl_FTP_Buffer[0]));													// Paket schicken
		break;
	}
	
}

void FTP_Data_Server(uint8_t Position_in_Table)
{
	#ifdef FTP_Debug
		USART_Write_String("FTP: Daten Server\r\n");
	#endif
	/*Wird in den Unterfunktionen verwaltet
	if (gl_TCP_Table[Position_in_Table].Status!=TCP_Established)																			// Falls die Verbindung nicht mehr als "Aufgebaut" gilt, setze die Statemachine zur�ck
	{
		FAT32_File_Close(FTP_Filenumber);
		#ifdef FTP_Debug
			USART_Write_String("FTP: Datenverbindung wurde vom Client beendet\r\n");
		#endif
		return;
	}*/

	if (STRCMP_ALT("LIST",&gl_FTP_Table.Last_Command[0])==0)																				// RETR FILENAME TXT
	{
		if (gl_FTP_Table.LIST_Statemachine!=0)																								// �bertrage die Daten f�r List
		{
			FTP_List_Send_Entry(Position_in_Table);
			return;
		}
	}
	else if (STRCMP_ALT("RETR",&gl_FTP_Table.Last_Command[0])==0)																			// RETR FILENAME TXT
	{
		FTP_RETR_Send_File(Position_in_Table);
		return;
	}
	else if (STRCMP_ALT("STOR",&gl_FTP_Table.Last_Command[0])==0)																			// STOR FILENAME TXT
	{
		if (gl_FTP_Table.STOR_Statemachine!=0)																								// �bertrage die Daten f�r List
		{
			FTP_STOR_Receive_File(Position_in_Table);
			return;
		}
	}
}

void FTP_List_Send_Entry(uint8_t Position_in_Table)
{
	char Filename[13];
	char Month[4];
	struct Timestamp Times;
	
	#ifdef FTP_Debug
		USART_Write_String("FTP: Sende Listeneintraege\r\n");
	#endif

	FAT32_Directory_Change(&gl_FTP_Table.Workingpath[0]);																					// Wechsle in den aktuellen Arbeitspfad
	
	if (FAT32_Directory_List_Entry_from_Position(&Filename[0],gl_FTP_Table.LIST_Filecounter))												// Wenn an der Stelle keine Datei mehr ist, sende, dass das Verzeichnis gelesen wurde
	{
		TCP_Close_Connection(Position_in_Table,TCP_Connection_Active);																		// Verbindung schlie�en
		sprintf(&gl_FTP_Buffer[0],"226 Directory send OK.\r\n");
		#ifdef FTP_Debug
			USART_Write_String(&gl_FTP_Buffer[0]);
		#endif
		TCP_Write_Data(gl_FTP_Table.Client_Command_PiT,&gl_FTP_Buffer[0],strlen(&gl_FTP_Buffer[0]));										// Paket schicken
		gl_FTP_Table.LIST_Filecounter=0;
		gl_FTP_Table.LIST_Statemachine=0;
		return;
	}
	FAT32_File_Open(FTP_Filenumber,&Filename[0],FAT32_Read);																				// Datei �ffnen
	
	FAT32_Convert_Filedate(FTP_Filenumber,&Times);

 	strncpy(&Month[0],gl_FTP_Months[Times.Month-1],4);

	if (gl_FAT32_File[FTP_Filenumber].Attributes & 0x10)																					// Wenn Datei ein Verzeichnis ist drucke einen anderen String
	{
		sprintf(&gl_FTP_Buffer[0],"drwxr-xr-x 1 0 0 0 %s %02u %4u %02u:%02u %s\r\n",&Month[0],Times.Day,Times.Year,Times.Hour,Times.Minute,&Filename[0]);				// Dir
	}
	else
	{
		sprintf(&gl_FTP_Buffer[0],"-rwxr-xr-x 1 0 0 %lu %s %02u %4u %02u:%02u %s\r\n",gl_FAT32_File[FTP_Filenumber].Size,&Month[0],Times.Day,Times.Year,Times.Hour,Times.Minute,&Filename[0]);	// File
	}
	#ifdef FTP_Debug
		USART_Write_String(&gl_FTP_Buffer[0]);
	#endif
	FAT32_File_Close(FTP_Filenumber);																										// Datei schlie�en

	TCP_Write_Data(Position_in_Table,&gl_FTP_Buffer[0],strlen(&gl_FTP_Buffer[0]));															// Paket schicken
	gl_FTP_Table.LIST_Filecounter++;
}

void FTP_RETR_Send_File(uint8_t Position_in_Table)
{
	char Buff[TCP_MSS];																														// Buffer f�r Server

	#ifdef FTP_Debug
		USART_Write_String("FTP: Uebertrage Daten aus Datei\r\n");
	#endif

	if (gl_FTP_Table.RETR_Dataleft==0)																										// Wenn keine Daten mehr gesendet werden m�ssen, schlie�e die Verbindung
	{
		TCP_Close_Connection(Position_in_Table,TCP_Connection_Active);																		// Verbindung schlie�en
		sprintf(&Buff[0],"226 File sent.\r\n");
		#ifdef FTP_Debug
			USART_Write_String(&Buff[0]);
		#endif
		TCP_Write_Data(gl_FTP_Table.Client_Command_PiT,&Buff[0],strlen(&Buff[0]));															// Paket schicken
		FAT32_File_Close(FTP_Filenumber);																									// Datei schlie�en
		return;
	}
	
	if (gl_FTP_Table.RETR_Dataleft > gl_TCP_Table[Position_in_Table].Transaction_MSS)
	{
		FAT32_File_Read(FTP_Filenumber,&Buff[0],gl_TCP_Table[Position_in_Table].Transaction_MSS);
		TCP_Write_Data(Position_in_Table,&Buff[0],gl_TCP_Table[Position_in_Table].Transaction_MSS);											// Paket schicken
		gl_FTP_Table.RETR_Dataleft-=gl_TCP_Table[Position_in_Table].Transaction_MSS;
	}
	else
	{
		FAT32_File_Read(FTP_Filenumber,&Buff[0],gl_FTP_Table.RETR_Dataleft);
		TCP_Write_Data(Position_in_Table,&Buff[0],gl_FTP_Table.RETR_Dataleft);																// Paket schicken
		gl_FTP_Table.RETR_Dataleft=0;
	}
}

void FTP_STOR_Receive_File(uint8_t Position_in_Table)
{
	char *G;
	uint16_t g=0;
	static uint32_t size=0;
	
	#ifdef FTP_Debug
		USART_Write_String("FTP: Schreibe Daten in Datei\r\n");
	#endif

	if (gl_TCP_Table[Position_in_Table].Status==TCP_Last_Ack)														// Falls die Command Verbindung geschlossen wird, schlie�e den Datentransfer
	{
		gl_FTP_Table.STOR_Statemachine=0;
		sprintf(&gl_FTP_Buffer[0],"226 Transfer complete. %lu Bytes transfered.\r\n",size);
		#ifdef FTP_Debug
			USART_Write_String(&gl_FTP_Buffer[0]);
		#endif
		if (FAT32_File_Close(FTP_Filenumber))
		{
			sprintf(&gl_FTP_Buffer[0],"550 Fileupdate not possible.\r\n");
			#ifdef FTP_Debug
				USART_Write_String(&gl_FTP_Buffer[0]);
			#endif
		}
		TCP_Write_Data(gl_FTP_Table.Client_Command_PiT,&gl_FTP_Buffer[0],strlen(&gl_FTP_Buffer[0]));				// Paket schicken
		return;
	}
	
	if (TCP_Read_Data(Position_in_Table,&G,&g)==0)																	// Falls Daten vorhanden, schreibe sie in die Datei
	{
		FAT32_File_Write(FTP_Filenumber,&G[0],g);																	// Schreibe den Inhalt in die Datei
		size += g;
	}	
}
