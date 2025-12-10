// Includes
#include "main.h"
#include "HTTP.h"
#include <stdio.h>
#include <string.h>
#include "Allerlei.h"
#include "Stack.h"
#include "FAT32.h"
#include "DHCP.h"
#include "HTTP_Parser.h"
#include "HTTP_uC.h"
#ifdef HTTP_Debug
	#include "USART.h"
#endif
#include "INI_Parser.h"

/*

************************************************************************************************
* HTTP Webserver Handling																	   *
* 2019 � Frederinn															 	  	   *
************************************************************************************************

*/

// Globale Variablen
char gl_Default_Dir[128];																							// Standarddir
char gl_Default_Page[13];																							// Startseite des Servers in den RAM geladen
char gl_Default_404[13];																							// Standard 404
char gl_HTTP_Default_String[200];																					// Nachricht fuer die Umleitung von 192.168.178.14 auf 192.168.178.14/home.htm
struct HTTP_Head gl_HTTP_Head_read;																					// HTTP Header
struct HTTP_Table gl_HTTP_Table[HTTP_Max_Entries];																	// Http Tabelle																	// Http Tabelle

void HTTP_Init(void)
{
	FAT32_Directory_Change("/");
	INI_Read_Key_String("http.ini","HTTP","Default_Dir",&gl_Default_Dir[0]);
	INI_Read_Key_String("http.ini","HTTP","Default_Page",&gl_Default_Page[0]);
	INI_Read_Key_String("http.ini","HTTP","Default_404",&gl_Default_404[0]);

	strcpy(&gl_HTTP_Default_String[0],"HTTP/1.1 302 Found\r\nConnection: close\r\nLocation: ");
	strncat(&gl_HTTP_Default_String[0],&gl_Default_Dir[0],128);
	strncat(&gl_HTTP_Default_String[0],&gl_Default_Page[0],13);
	strcat(&gl_HTTP_Default_String[0],"\r\n\r\n\r\n");
	#ifdef HTTP_Debug
		USART_Write_String("HTTP: Init erfolgreich\r\n");
	#endif
}

void HTTP_Read_Header(char *Head, uint16_t Length)
{
	uint16_t g=0;
	
	gl_HTTP_Head_read.Filepath_length=0;
	
	gl_HTTP_Head_read.Method = &Head[0];
	for (;Head[g]!=' ';g++);																											// HTTP Anfragemethode ermitteln
	Head[g] = '\0';
	
	g++;
	gl_HTTP_Head_read.Filepath = &Head[g];																								// Dateipfad ermitteln
	for (;Head[g]!=' ';g++)
	{
		if (g==255)
		{
			Head[g-1] = '\0';
			#ifdef HTTP_Debug
				USART_Write_String("HTTP: Dateipfad zu lang beim Lesen\r\n");
			#endif
			return;
		}
		gl_HTTP_Head_read.Filepath_length++;																							// Z�hle die Dateil�nge mit
	}
	Head[g] = '\0';
	g++;
	gl_HTTP_Head_read.Argument = &Head[g];																								// Argumentezeiger

	while(1)
	{
		if (Head[g] == '\r')
		{
			g++;
			if (Head[g] == '\n')
			{	
				g++;
				if (Head[g] == '\r')
				{	
					g++;
					if (Head[g] == '\n')
					{
						g++;
						Head[g-1]=0;																									// Trennung von Data und Argumente jetzt steht am Ende \r\n\r0
						gl_HTTP_Head_read.Data = &Head[g];																				// Datenzeiger
						Head[Length]=0;																									// Trennung von Data und RAM Bereich, somit sind die Variablen als String besser verarbeitbar. M�glich, da der ENC Readbuffer gr��er als 1540Bytes sind
						#ifdef HTTP_Debug
							USART_Write_String("HTTP: Header gelesen\r\n");
						#endif
						return;
					}
				}
			}
		}
		if (g==Length)																													// Breche die Schleife ab
		{
			#ifdef HTTP_Debug
				USART_Write_String("HTTP: Ende des Headers erreicht\r\n");
			#endif
			return;
		}
		g++;
	}
}
void HTTP_Server(uint8_t Position_in_Table)
{
	char HTTP_Buff[TCP_MSS];																											// Buffer f�r Server
	memset(&HTTP_Buff[0],0,TCP_MSS);																									// Setze den Buffer so gro� wie unsere MSS max gl_TCP_Table[Position_in_Table].Transaction_MSS Bytes
	
	if (gl_TCP_Table[Position_in_Table].Status!=TCP_Established)																		// Falls die Verbindung nicht mehr als "Aufgebaut" gilt, setze die Statemachine zur�ck
	{
		gl_HTTP_Table[Position_in_Table].Statemachine=0;
		FAT32_File_Close(Position_in_Table);
		#ifdef HTTP_Debug
			USART_Write_String("HTTP: Verbindung wurde vom Client beendet\r\n");
		#endif
		return;
	}
	
	if (gl_HTTP_Table[Position_in_Table].Statemachine==0)																				// Haeader lesen bei Erstaufruf
	{
		char *G;
		char Dir[128], Filename[15], Filelength[11], str[250];
		int Pos;
		uint16_t g=0;
		
		if(TCP_Read_Data(Position_in_Table,&G,&g)!=0)																					// Wenn keine Daten zum �bermittelt wurden breche ab
		{
			return;
		}
		
		HTTP_Read_Header(&G[0],g);																										// Daten aus Header auswerten
		
		if (strcmp(&gl_HTTP_Head_read.Method[0],"GET")==0)
		{		
			strcpy(&str[0],"HTTP/1.1 200 OK\r\nServer: STM32 Webserver 1.0\r\nContent-Language: de\r\n");								// String aus Flash laden
			memset(&gl_HTTP_Table[Position_in_Table],0,sizeof(gl_HTTP_Table[Position_in_Table]));
			memset(&Dir[0],0,sizeof(Dir));
			memset(&Filename[0],0,sizeof(Filename));
			Pos=String_Search_Reverse(&gl_HTTP_Head_read.Filepath[0], '/', gl_HTTP_Head_read.Filepath_length);							// Suche nach '/'
			memcpy(&Dir[0],&gl_HTTP_Head_read.Filepath[0],Pos+1);
			memcpy(&Filename[0],&gl_HTTP_Head_read.Filepath[Pos+1],strlen(&gl_HTTP_Head_read.Filepath[0])-Pos);
			
			#ifdef HTTP_Debug
				printf("HTTP: %s %s%s\r\n",gl_HTTP_Head_read.Method,Dir,Filename);
				USART_Write_String("HTTP ARGUMENT:");
				USART_Write_String(&gl_HTTP_Head_read.Argument[0]);
				USART_Write_String("HTTP DATA:");
				USART_Write_String(&gl_HTTP_Head_read.Data[0]);
			#endif
			FAT32_File_Close(Position_in_Table);																						// Da wir nur lesen, brauchen wir das Verzeichnis nicht zu kennen. Die alte Datei an der Stelle schlie�en

			if ((strcmp(&Dir[0],"/")==0) && (strcmp(&Filename[0],"")==0))
			{
				TCP_Write_Data(Position_in_Table,&gl_HTTP_Default_String[0],strlen(&gl_HTTP_Default_String[0]));						// 302 Moved!
				gl_HTTP_Table[Position_in_Table].Statemachine=0;
				return;
			}

			if (strcmp(&Dir[0],"/uc/")==0)
			{
				gl_HTTP_Table[Position_in_Table].Leftdata=0;																			// Keine Daten mehr vorhanden
				gl_HTTP_Table[Position_in_Table].Content_Length_used = 0;																// Content-Length wird nicht benutzt
				gl_HTTP_Table[Position_in_Table].Statemachine=1;																		// wichtig das unten die Verbindung geschlossen wird
				HTTP_uC_GET_requests(Position_in_Table, &Filename[0]);
				return;
			}

			if (FAT32_Directory_Change(&Dir[0])==0)
			{
				if (FAT32_File_Open(Position_in_Table,&Filename[0],FAT32_Read))															// Wenn Datei nicht gefunden werden kann lade die 404 Meldung
				{
					FAT32_Directory_Change(&gl_Default_Dir[0]);
					FAT32_File_Open(Position_in_Table,&gl_Default_404[0],FAT32_Read);
				}
			}
			else
			{
				#ifdef HTTP_Debug
					USART_Write_String("HTTP: Dieser Pfad wurde nicht gefunden: ");
					USART_Write_String(&Dir[0]);
					USART_Write_String("\r\n");
				#endif
				FAT32_Directory_Change(&gl_Default_Dir[0]);
				FAT32_File_Open(Position_in_Table,&gl_Default_404[0],FAT32_Read);
			}

			sprintf(&Filelength[0],"%lu",gl_FAT32_File[Position_in_Table].Size);														// Dateil�nge in String wandeln
			
			if (strcmp(&gl_FAT32_File[Position_in_Table].Name[8],"JPG")==0)																// Dateizuordnung und falls n�tig Dateil�nge anh�ngen
			{
				strcat(&str[0],"Content-Length: ");
				strcat(&str[0],Filelength);
				strcat(&str[0],"\r\nContent-Type: image/jpg");
				strcat(&str[0],"\r\nCache-Control: max-age="HTTP_Cache_Age);
				gl_HTTP_Table[Position_in_Table].Content_Length_used = 1;																// Content-Length wird benutzt
			}
			else if (strcmp(&gl_FAT32_File[Position_in_Table].Name[8],"PNG")==0)
			{
				strcat(&str[0],"Content-Length: ");
				strcat(&str[0],Filelength);
				strcat(&str[0],"\r\nContent-Type: image/png");
				strcat(&str[0],"\r\nCache-Control: max-age="HTTP_Cache_Age);
				gl_HTTP_Table[Position_in_Table].Content_Length_used = 1;																// Content-Length wird benutzt
			}
			else if (strcmp(&gl_FAT32_File[Position_in_Table].Name[8],"PDF")==0)
			{
				strcat(&str[0],"Content-Length: ");
				strcat(&str[0],Filelength);
				strcat(&str[0],"\r\nContent-Type: application/pdf");
				gl_HTTP_Table[Position_in_Table].Content_Length_used = 1;																// Content-Length wird benutzt
			}
			else if (strcmp(&gl_FAT32_File[Position_in_Table].Name[8],"GIF")==0)
			{
				strcat(&str[0],"Content-Length: ");
				strcat(&str[0],Filelength);
				strcat(&str[0],"\r\nContent-Type: image/gif");
				strcat(&str[0],"\r\nCache-Control: max-age="HTTP_Cache_Age);
				gl_HTTP_Table[Position_in_Table].Content_Length_used = 1;																// Content-Length wird benutzt
			}
			else if (strcmp(&gl_FAT32_File[Position_in_Table].Name[8],"ICO")==0)
			{
				strcat(&str[0],"Content-Length: ");
				strcat(&str[0],Filelength);
				strcat(&str[0],"\r\nContent-Type: image/ico");
				strcat(&str[0],"\r\nCache-Control: max-age="HTTP_Cache_Age);
				gl_HTTP_Table[Position_in_Table].Content_Length_used = 1;																// Content-Length wird benutzt
			}
			else if(strcmp(&gl_FAT32_File[Position_in_Table].Name[8],"MP3")==0)
			{
				strcat(&str[0],"Content-Length: ");
				strcat(&str[0],Filelength);
				strcat(&str[0],"\r\nContent-Type: audio/mp3");
				strcat(&str[0],"\r\nCache-Control: max-age="HTTP_Cache_Age);
				gl_HTTP_Table[Position_in_Table].Content_Length_used = 1;																// Content-Length wird benutzt
			}
			else if(strcmp(&gl_FAT32_File[Position_in_Table].Name[8],"MP4")==0)
			{
				strcat(&str[0],"Content-Length: ");
				strcat(&str[0],Filelength);
				strcat(&str[0],"\r\nContent-Type: video/mp4");
				strcat(&str[0],"\r\nCache-Control: max-age="HTTP_Cache_Age);
				gl_HTTP_Table[Position_in_Table].Content_Length_used = 1;																// Content-Length wird benutzt
			}
			else if(strcmp(&gl_FAT32_File[Position_in_Table].Name[8],"CSS")==0)
			{
				strcat(&str[0],"Content-Length: ");
				strcat(&str[0],Filelength);
				strcat(&str[0],"\r\nContent-Type: text/css");
				strcat(&str[0],"\r\nCache-Control: max-age="HTTP_Cache_Age);
				gl_HTTP_Table[Position_in_Table].Content_Length_used = 1;																// Content-Length wird benutzt
			}
			else if(strcmp(&gl_FAT32_File[Position_in_Table].Name[8],"JSS")==0)
			{
				strcat(&str[0],"Content-Length: ");
				strcat(&str[0],Filelength);
				strcat(&str[0],"\r\nContent-Type: text/javascript");
				strcat(&str[0],"\r\nCache-Control: max-age="HTTP_Cache_Age);
				gl_HTTP_Table[Position_in_Table].Content_Length_used = 1;																// Content-Length wird benutzt
			}
			else if(strcmp(&gl_FAT32_File[Position_in_Table].Name[8],"TXT")==0)
			{
				strcat(&str[0],"Content-Length: ");
				strcat(&str[0],Filelength);
				strcat(&str[0],"\r\nContent-Type: text/txt");
				gl_HTTP_Table[Position_in_Table].Content_Length_used = 1;																// Content-Length wird benutzt
			}
			else if(strcmp(&gl_FAT32_File[Position_in_Table].Name[8],"HTM")==0)
			{
				strcat(&str[0],"Content-Type: text/html");
				strcat(&str[0],"\r\nCache-Control: max-age="HTTP_Cache_Age);
			}
			else if(strcmp(&gl_FAT32_File[Position_in_Table].Name[8],"PSF")==0)
			{
				strcat(&str[0],"Content-Type: text/html");

				gl_HTTP_Table[Position_in_Table].Parser.Use = 0;																		// Neue Daten in den Parser einlesen
				gl_HTTP_Table[Position_in_Table].Parser.Use = 1;																		// Parser wird verwendet
			}
			else if(strcmp(&gl_FAT32_File[Position_in_Table].Name[8],"TTF")==0)															// True Type File
			{
				strcat(&str[0],"Content-Length: ");
				strcat(&str[0],Filelength);
				strcat(&str[0],"\r\nContent-Type: application/octet-stream");
				strcat(&str[0],"\r\nCache-Control: max-age="HTTP_Cache_Age);
				gl_HTTP_Table[Position_in_Table].Content_Length_used = 1;																// Content-Length wird benutzt
			}
			else
			{
				strcat(&str[0],"Content-Type: text/html");
				gl_HTTP_Table[Position_in_Table].Content_Length_used = 0;																// Content-Length wird nicht benutzt
			}
			strcat(&str[0],"\r\nConnection: close\r\n\r\n");
			g=strlen(&str[0]);																											// L�nge des TCP Paketes
			TCP_Write_Data(Position_in_Table,&str[0],g);																				// Paket senden
			gl_HTTP_Table[Position_in_Table].Leftdata = gl_FAT32_File[Position_in_Table].Size;											// Anzahl der zu senden Bytes
			gl_HTTP_Table[Position_in_Table].Statemachine = 1;																			// App wurde das erste mal aufgerufen
			return;
		}
		else if (strcmp(&gl_HTTP_Head_read.Method[0],"POST")==0)
		{
			Parser_do_parse_for_POST();

			strcpy(&str[0],"HTTP/1.1 201 Created\r\nServer: STM32 Webserver 1.0\r\nContent-Length: 94\r\nConnection: close\r\n\r\n\r\n");
			TCP_Write_Data(Position_in_Table,&str[0],strlen(&str[0]));																		// 501 Not Implemented
			gl_HTTP_Table[Position_in_Table].Content_Length_used=0;
			gl_HTTP_Table[Position_in_Table].Statemachine=1;
			gl_HTTP_Table[Position_in_Table].Leftdata=0;
			#ifdef HTTP_Debug
				USART_Write_String("HTTP: Parsen f�r POST ausgef�hrt\r\n");																	// Parse for POST
			#endif
			return;
		}
		else
		{
			sprintf(&str[0],"HTTP/1.1 501 Not Implemented\r\nAllow: GET,POST\r\nContent-Length: %d\r\nConnection: close\r\n\r\n\r\n<html>Error 501<br>Method: %s not implemented</html>",strlen(gl_HTTP_Head_read.Method)+54,&gl_HTTP_Head_read.Method[0]);
			TCP_Write_Data(Position_in_Table,&str[0],strlen(&str[0]));																		// 501 Not Implemented
			gl_HTTP_Table[Position_in_Table].Content_Length_used=0;
			gl_HTTP_Table[Position_in_Table].Statemachine=1;
			gl_HTTP_Table[Position_in_Table].Leftdata=0;
			#ifdef HTTP_Debug
				USART_Write_String("HTTP: Methode nicht implementiert\r\n");																// Hinweis
			#endif
			return;
		}
	}
	else
	{
		if(gl_HTTP_Table[Position_in_Table].Parser.Use==1 && gl_TCP_Table[Position_in_Table].Transaction_MSS >= Parser_min_MSS)									// Standard 512 Bytes
		{
			#ifdef HTTP_Debug
				USART_Write_String("HTTP: Hello Parser my old friend...\r\n");
			#endif
			
			if (gl_HTTP_Table[Position_in_Table].Leftdata==0)																									// Falls keine Bytes mehr gesendet werden m�ssen, kann die Verbindung und Datei geschlossen werden
			{
				#ifdef HTTP_Debug
					printf("HTTP: PiT: %u, Gibt nix mehr zu parsen. Datei schliessen\r\n",Position_in_Table);
				#endif
				
				FAT32_File_Close(Position_in_Table);
				TCP_Close_Connection(Position_in_Table,TCP_Connection_Active);
				gl_HTTP_Table[Position_in_Table].Statemachine=0;
				return;
			}
			
			uint16_t g=0;
			for (g=0;g<(gl_TCP_Table[Position_in_Table].Transaction_MSS-Parser_String_len);g++)																	// Loope ein neues Paket zusammen
			{
				if(gl_HTTP_Table[Position_in_Table].Parser.Leftdata==0)																							// Falls Daten zum Parsen gebraucht werden, lade diese nach
				{
					if(gl_HTTP_Table[Position_in_Table].Leftdata>511)
					{
						FAT32_File_Read(Position_in_Table,&gl_HTTP_Table[Position_in_Table].Parser.Buffer[0],512);												// Lese ein Byte in den Buffer
						gl_HTTP_Table[Position_in_Table].Parser.Leftdata=512;																					// Lade 512 Bytes in den Parserbuffer
						gl_HTTP_Table[Position_in_Table].Parser.Bytes_in_Buffer = 512;																			// Maximale Byteanzahl im Parserbuffer
					}
					else
					{
						FAT32_File_Read(Position_in_Table,&gl_HTTP_Table[Position_in_Table].Parser.Buffer[0],gl_HTTP_Table[Position_in_Table].Leftdata);		// Lese ein Byte in den Buffer
						gl_HTTP_Table[Position_in_Table].Parser.Leftdata=gl_HTTP_Table[Position_in_Table].Leftdata;												// Lade den Rest in den Parserbuffer
						gl_HTTP_Table[Position_in_Table].Parser.Bytes_in_Buffer = gl_HTTP_Table[Position_in_Table].Leftdata;									// Maximale Byteanzahl im Parserbuffer
					}
				}

				if(gl_HTTP_Table[Position_in_Table].Parser.Buffer[gl_HTTP_Table[Position_in_Table].Parser.Bytes_in_Buffer-gl_HTTP_Table[Position_in_Table].Parser.Leftdata] == Parser_Ident_char)					// Wenn im Parserbuffer ein Parser_Ident_Char gefunden wurde, dann fange an den Teil rauszukopieren
				{
					memset(&gl_Parser.Buffer[0],0,Parser_String_len);																							// Parserbuffer leeren

					gl_Parser.Buffer[0] = gl_HTTP_Table[Position_in_Table].Parser.Buffer[gl_HTTP_Table[Position_in_Table].Parser.Bytes_in_Buffer-gl_HTTP_Table[Position_in_Table].Parser.Leftdata];					// Kopiere Parser_Ident_char auf 0 Position

					gl_HTTP_Table[Position_in_Table].Parser.Leftdata--;																							// Z�hle den Parserbuffer um eins runter
					gl_HTTP_Table[Position_in_Table].Leftdata--;																								// Anzahl der bereits gelesenen Zeichen abziehen

					for(uint8_t i=1;i<Parser_String_len;i++)																								// Nicht endlos Zeichen nachladen																												// Lade den Befehl in den String
					{
						if(gl_HTTP_Table[Position_in_Table].Parser.Leftdata==0)																					// Falls Daten zum Parsen gebraucht werden, lade diese nach
						{
							if(gl_HTTP_Table[Position_in_Table].Leftdata>511)
							{
								FAT32_File_Read(Position_in_Table,&gl_HTTP_Table[Position_in_Table].Parser.Buffer[0],512);										// Lese ein Byte in den Buffer
								gl_HTTP_Table[Position_in_Table].Parser.Leftdata=512;																			// Lade 512 Bytes in den Parserbuffer
								gl_HTTP_Table[Position_in_Table].Parser.Bytes_in_Buffer = 512;																	// Maximale Byteanzahl im Parserbuffer
							}
							else
							{
								FAT32_File_Read(Position_in_Table,&gl_HTTP_Table[Position_in_Table].Parser.Buffer[0],gl_HTTP_Table[Position_in_Table].Leftdata);// Lese ein Byte in den Buffer
								gl_HTTP_Table[Position_in_Table].Parser.Leftdata=gl_HTTP_Table[Position_in_Table].Leftdata;										// Lade den Rest in den Parserbuffer
								gl_HTTP_Table[Position_in_Table].Parser.Bytes_in_Buffer = gl_HTTP_Table[Position_in_Table].Leftdata;							// Maximale Byteanzahl im Parserbuffer
							}
						}

						gl_Parser.Buffer[i] = gl_HTTP_Table[Position_in_Table].Parser.Buffer[gl_HTTP_Table[Position_in_Table].Parser.Bytes_in_Buffer-gl_HTTP_Table[Position_in_Table].Parser.Leftdata];															// Lade ein Byte aus dem Parserbuffer

						gl_HTTP_Table[Position_in_Table].Parser.Leftdata--;																						// Z�hle den Parserbuffer um eins runter
						gl_HTTP_Table[Position_in_Table].Leftdata--;																							// Anzahl der bereits gelesenen Zeichen abziehen

						if(gl_Parser.Buffer[i] == Parser_Ident_char)																							// Wenn zweites zeichen gefunden wurde dann breche die for-loop ab
						{
							gl_Parser.Buffer[i+1]=0;																											// Das nach dem zweite Parser_Ident_char Byte auf 0 setzen
							break;
						}
						if(gl_HTTP_Table[Position_in_Table].Leftdata==0)																						// Falls keine Daten mehr in der Datei sind, breche das Parsen ab
						{
							gl_Parser.Buffer[i+1]=0;																											// Falls das zweite Parser_Ident_char Byte nicht gefunden wurde setze trotzdem das n�chste Zeichen auf 0
							break;
						}
					}
					Parser_do_parse_for_GET();
					memcpy(&HTTP_Buff[g],&gl_Parser.Buffer[0],strlen(&gl_Parser.Buffer[0]));																	// Kopiere den String in den HTTP Buffer
					g += strlen(&gl_Parser.Buffer[0])-1;																										// Z�hle die Stringl�nge dazu und 1 ab, da g in der for oben um 1 dazugez�hlt wird
				}
				else																																			// ansonsten kopiere das Byte in den HTTP_Buff und z�hle ein Byte runter
				{
					HTTP_Buff[g] = gl_HTTP_Table[Position_in_Table].Parser.Buffer[gl_HTTP_Table[Position_in_Table].Parser.Bytes_in_Buffer-gl_HTTP_Table[Position_in_Table].Parser.Leftdata];
					gl_HTTP_Table[Position_in_Table].Parser.Leftdata--;
					gl_HTTP_Table[Position_in_Table].Leftdata--;
				}
				
				if(gl_HTTP_Table[Position_in_Table].Leftdata==0)																								// Falls Ende der Schleife erreicht wurde, breche ab
				{
					g++;																																		// Am Ende der Datei z�hle g um eins hoch das das 0 Byte zum Abschluss richtig gesetzt wird
					break;																																		// Beende die For-Schleife
				}
			}
			TCP_Write_Data(Position_in_Table,&HTTP_Buff[0],g);																								
			return;
		}
		else
		{
			#ifdef HTTP_Debug
				printf("HTTP: PiT: %u, Lade restliche Daten nach\r\n",Position_in_Table);
			#endif
			
			if (gl_HTTP_Table[Position_in_Table].Leftdata>=gl_TCP_Table[Position_in_Table].Transaction_MSS)														// Solange die Bytez�hler >512 Bytes ist ziehe immer 512 Bytes ab
			{
				#ifdef HTTP_Debug
					printf("HTTP: PiT: %u, Lese vollen MSS Datenblock\r\n",Position_in_Table);
				#endif
				FAT32_File_Read(Position_in_Table,&HTTP_Buff[0],gl_TCP_Table[Position_in_Table].Transaction_MSS);
				TCP_Write_Data(Position_in_Table,&HTTP_Buff[0],gl_TCP_Table[Position_in_Table].Transaction_MSS);
				gl_HTTP_Table[Position_in_Table].Leftdata-=gl_TCP_Table[Position_in_Table].Transaction_MSS;
				return;
			}
			if (gl_HTTP_Table[Position_in_Table].Leftdata<gl_TCP_Table[Position_in_Table].Transaction_MSS && gl_HTTP_Table[Position_in_Table].Leftdata!=0)		// Letzter Sektor der Datei wurde erreicht, beende nun
			{
				#ifdef HTTP_Debug
					printf("HTTP: PiT: %u, Lese restlichen Datenblock\r\n",Position_in_Table);
				#endif
				FAT32_File_Read(Position_in_Table,&HTTP_Buff[0],gl_HTTP_Table[Position_in_Table].Leftdata);
				TCP_Write_Data(Position_in_Table,&HTTP_Buff[0],gl_HTTP_Table[Position_in_Table].Leftdata);
				gl_HTTP_Table[Position_in_Table].Leftdata=0;
				return;
			}
			if (gl_HTTP_Table[Position_in_Table].Leftdata==0)																									// Falls keine Bytes mehr gesendet werden m�ssen, kann die Verbindung und Datei geschlossen werden
			{
				#ifdef HTTP_Debug
					printf("HTTP: PiT: %u, Datei schliessen\r\n",Position_in_Table);
				#endif
				FAT32_File_Close(Position_in_Table);
				if (gl_HTTP_Table[Position_in_Table].Content_Length_used==0)																					// Wenn keine L�nge mit �bergeben wurde, dann schlie�t der Server die Verbindung
				{
					TCP_Close_Connection(Position_in_Table,TCP_Connection_Active);
				}
				gl_HTTP_Table[Position_in_Table].Statemachine=0;
				return;
			}
		}
	}
}
