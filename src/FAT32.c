// Includes
#include "main.h"
#include <string.h>
#include "SD.h"
#include "FAT32.h"
#include "Allerlei.h"
#include "USART.h"
#include "stdio.h"
#include "time.h"
#ifdef FAT32_Debug
	#include <stdio.h>
#endif
#ifdef FAT32_USE_RTC
	#include "DS1307.h"
#endif

/*

Es wird nur eine Partition unterstuetzt und die Sektorgroesse muss 512 Bytes betragen
Dateinamen muessen sich im 8.3 Format befinden. Kann mit "Test.txt" oder "TEST    TXT" aufgerufen werden
Die SD Karte muss eine Blockgr��e von 512 Bytes haben, �ltere Karten die kleinere Blockgr��en besitzen k�nnen, werden nicht unterst�tzt

************************************************************************************************
* FAT32 Bibliothek fuer SD-Karte ueber den SPI auf dem STM32F401RET6						   *
* 2019 - 2020 � Frederinn														   	   *
************************************************************************************************

*/

//Globale Variablen
struct SD_FILE gl_FAT32_File[FAT32_Max_Entries];													// Globale fuer Datei
struct SD_FILE gl_FAT32_Directory;																	// Globale fuer Verzeichnis
struct FAT32 gl_FAT32;																				// Globale fuer FAT32

// Funktionen
uint8_t FAT32_Init(void)
{
	if (SD_Card_read_MBR())																				// Lese den MBR aus
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: MBR konnte nicht gelesen werden\r\n");
		#endif
		gl_FAT32.Mounted=0;
		return 1;
	}
	if (!(gl_SD_MBR.Partition1.Partition_Type == 0x0b || gl_SD_MBR.Partition1.Partition_Type == 0x0c))
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Karte ist nicht in FAT32 formatiert\r\n");
			USART_Write_Byte(gl_SD_MBR.Partition1.Partition_Type);
		#endif
		gl_FAT32.Mounted=0;
		return 1;
	}

	SD_Card_CMD17(gl_SD_MBR.Partition1.Partition_Startaddress, &gl_SD_Card.RWbuffer[0]);						// Lese den Bootsektor der FAT aus

	gl_FAT32.Bytes_per_Sector = (gl_SD_Card.RWbuffer[0x0c]<<8)|gl_SD_Card.RWbuffer[0x0b];						// Bytes per Sector
	gl_FAT32.Sectors_per_Cluster = gl_SD_Card.RWbuffer[0x0d];													// Anzahl Sektoren pro Cluster
	gl_FAT32.Bytes_times_Cluster = gl_FAT32.Sectors_per_Cluster * gl_FAT32.Bytes_per_Sector;						// Hilfsvariable um Rechenzeit zu sparen
	gl_FAT32.Reserved_Sectors = (gl_SD_Card.RWbuffer[0x0f]<<8)|gl_SD_Card.RWbuffer[0x0e];						// Anzahl der reservierten Sektoren ausgeben
	gl_FAT32.Coppies_of_FAT = gl_SD_Card.RWbuffer[0x10];														// Kopien der FAT
	gl_FAT32.Sectors_per_FAT = (gl_SD_Card.RWbuffer[0x25]<<8)|gl_SD_Card.RWbuffer[0x24];						// Sektoren pro FAT
	gl_FAT32.Root_First_Cluster = Get_uint32_from_Little_Endian_Buffer(&gl_SD_Card.RWbuffer[0x2c]);				// Gibt den Startcluster fuer die Root zurueck

	gl_FAT32.First_FAT_Address = (unsigned long)((gl_FAT32.Reserved_Sectors) * gl_FAT32.Bytes_per_Sector) + gl_SD_MBR.Partition1.Partition_Startaddress;	// Ersten Sektor der FAT-Tabelle ausrechnen
	gl_FAT32.Root_Startaddress = gl_FAT32.First_FAT_Address + (2 * gl_FAT32.Sectors_per_FAT * gl_FAT32.Bytes_per_Sector);									//Startaddresse der Root auf der Karte

	SD_Card_CMD17(gl_FAT32.First_FAT_Address,&gl_FAT32.FAT_Buffer[0]);											// Lade den ersten FAT Sektor in den Buffer
	gl_FAT32.Loaded_FAT_Sector = gl_FAT32.First_FAT_Address;													// Speichere die aktuell geladene addresse
	gl_FAT32.Mounted=1;																							// Fat ist eingebunden
	gl_FAT32_Directory.First_Clustersector = gl_FAT32.Root_First_Cluster;										// Weise der Directory die Root zu
	gl_FAT32.Last_Found_Clustercounter = 0;																		// Initwert der letzten gefunden Adresse mit freien Sektoren

	#ifdef FAT32_Debug
		USART_Write_String("FAT32: FAT ist eingebunden\r\n");
	#endif
	return 0;
}

uint8_t FAT32_Get_Data_from_Sector(uint32_t Cluster, uint8_t Sector, char *Buffer)
{
	SD_Card_CMD17(gl_FAT32.Root_Startaddress + ((Cluster - gl_FAT32.Root_First_Cluster) * gl_FAT32.Bytes_times_Cluster) + (Sector * gl_FAT32.Bytes_per_Sector),&Buffer[0]);// Lese den Sector aus
	Sector++;
	return Sector < gl_FAT32.Sectors_per_Cluster ? Sector : 0;													// Wenn der letzte Sektor der Cluster gelesen wurde, gebe 0=letzter Sektor der Cluster gelesen, oder >0 = N�chster zu lesender Sektor
}

uint32_t FAT32_Get_Next_Cluster(uint32_t Cluster)
{
	uint32_t FAT_Offset_Address=0;																				// Offsetadresse der FAT, Da Sektor 512 Bytes gro� ist

	FAT_Offset_Address = ((Cluster/128) * gl_FAT32.Bytes_per_Sector) + gl_FAT32.First_FAT_Address;				// Hier liegt der FAT Offset + First_FAT_Adress zur Vereinfachung
	Cluster &= 127;																								// Hier kommt die Position im FAT Sektor raus mithilfe des Clusters

	if (FAT_Offset_Address != gl_FAT32.Loaded_FAT_Sector)														// Lade den FAT Sektor nur falls dieser nicht im FAT Buffer liegt
	{
		SD_Card_CMD17(FAT_Offset_Address, &gl_FAT32.FAT_Buffer[0]);												// Lade den aktuell benoetigen FAT Sektor
		gl_FAT32.Loaded_FAT_Sector = FAT_Offset_Address;														// Setze die neue FAT Tabellenaddresse
	}

	return Get_uint32_from_Little_Endian_Buffer(&gl_FAT32.FAT_Buffer[Cluster*4]);								// Gebe den nachfolgenden Cluster zurueck
}

uint8_t FAT32_Write_Data_to_Sector(uint32_t Cluster, uint8_t Sector, char *Buffer)
{
	if (SD_Card_CMD24(gl_FAT32.Root_Startaddress + ((Cluster - gl_FAT32.Root_First_Cluster) * gl_FAT32.Bytes_times_Cluster) + (Sector * gl_FAT32.Bytes_per_Sector),&Buffer[0]) == 0)	// Beschreibe den Sektor
	{
		return 0;
	}
	return 1;
}
uint8_t FAT32_File_Open(uint8_t Position_in_Table, char *Filename, uint8_t Read_Write)
{
	char Filename_converted[12];																			// Konvertierter Dateiname
	uint32_t Temp_Dir_Cluster = gl_FAT32_Directory.First_Clustersector, Backup_Cluster=0;					// Aktuelle Cluster der Dir, Backup der Dir
	uint16_t Next_Date=0;																					// Naeste Datei im Array +=32
	uint8_t Temp_Dir_Sector=0, Backup_Sector;																// Tempor�rer Dir Sector, Backup vom Dir Sector

	if (gl_FAT32_File[Position_in_Table].Is_Open)
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Es wurde bereits eine Datei auf diese Position_in_Table geoeffnet\r\n");
		#endif
		return 1;
	}

	Convert_String_to_8_3(Filename_converted, Filename);													// Konvertiere Dateistring

	while(1)																								// Endlosschleife bis der Dateiname gefunden wurde
	{
		if((Temp_Dir_Cluster > 0x0ffffff7) && (Temp_Dir_Cluster < 0x10000000))								// Pr�fe od das Ende der Dir erreicht wurde
		{
			#ifdef FAT32_Debug
				printf("FAT32: Datei \"%s\" wurde nicht gefunden\r\n",Filename_converted);
			#endif
			return 2;
		}

		Backup_Cluster = Temp_Dir_Cluster;																	// Speichere den letzten Cluster
		Backup_Sector = Temp_Dir_Sector;																	// Speichere den letzten Sektor
		Temp_Dir_Sector = FAT32_Get_Data_from_Sector(Temp_Dir_Cluster,Temp_Dir_Sector,&gl_SD_Card.RWbuffer[0]);		// Lade einen Sektor der Cluster
		if(Temp_Dir_Sector==0) Temp_Dir_Cluster = FAT32_Get_Next_Cluster(Temp_Dir_Cluster);							// Wenn der letzte Sektor der Cluster geladen wurde, lade die n�chste Cluster

		Next_Date = 0;																																	// Next Date zuruecksetzen
		for (uint8_t g=0;g<16;g++,Next_Date+=32)																										// 16 Eintraege pro FAT32 Sektor
		{
			if (memcmp(&gl_SD_Card.RWbuffer[Next_Date],&Filename_converted[0],11)==0)
			{
				strcpy(gl_FAT32_File[Position_in_Table].Name,Filename_converted);																		// Filename nochmal global Speichern
				gl_FAT32_File[Position_in_Table].Attributes = gl_SD_Card.RWbuffer[Next_Date+11];														// Dateiattribute speichern
				gl_FAT32_File[Position_in_Table].Size = Get_uint32_from_Little_Endian_Buffer(&gl_SD_Card.RWbuffer[Next_Date+28]);						// Dateigroesse
				gl_FAT32_File[Position_in_Table].First_Clustersector =  char_to_long_int(gl_SD_Card.RWbuffer[Next_Date+21],gl_SD_Card.RWbuffer[Next_Date+20],gl_SD_Card.RWbuffer[Next_Date+27],gl_SD_Card.RWbuffer[Next_Date+26]);		// Erster Datasektor
				gl_FAT32_File[Position_in_Table].Creation_Time = (gl_SD_Card.RWbuffer[Next_Date+0x0f]<<8)|(gl_SD_Card.RWbuffer[Next_Date+0x0e]);		// Erstellzeit komprimiert
				gl_FAT32_File[Position_in_Table].Creation_Date = (gl_SD_Card.RWbuffer[Next_Date+0x11]<<8)|(gl_SD_Card.RWbuffer[Next_Date+0x10]);		// Erstelldatum komprimiert
				gl_FAT32_File[Position_in_Table].Last_Access_Date = (gl_SD_Card.RWbuffer[Next_Date+0x13]<<8)|(gl_SD_Card.RWbuffer[Next_Date+0x12]);		// Letzer Zugriff komprimiert
				gl_FAT32_File[Position_in_Table].Changetime = (gl_SD_Card.RWbuffer[Next_Date+0x17]<<8)|(gl_SD_Card.RWbuffer[Next_Date+0x16]);			// Modifikationszeit komprimiert
				gl_FAT32_File[Position_in_Table].Changedate = (gl_SD_Card.RWbuffer[Next_Date+0x19]<<8)|(gl_SD_Card.RWbuffer[Next_Date+0x18]);			// Modifikationsdatum komprimiert
				
				if (gl_FAT32_File[Position_in_Table].Attributes == 0x10 && gl_FAT32_File[Position_in_Table].Size == 0)									// Falls ein Verzeichnis mit dieser Funktion ge�ffnet wurde
				{
					#ifdef FAT32_Debug
						printf("FAT32: \"%s\" ist ein Verzeichnis und keine Datei\r\nFAT32: Datei konnte nicht geoffent werden\r\n",Filename_converted);
					#endif
					return 1;
				}

				switch(Read_Write)
				{
					default:
					case FAT32_Read:
						gl_FAT32_File[Position_in_Table].Next_Byte_Read = 0;																				// Naechstes zu lesendes Byte
						gl_FAT32_File[Position_in_Table].Next_Byte_Read_Cluster = gl_FAT32_File[Position_in_Table].First_Clustersector;						// Cluster in dem sich dieses Byte befindet
						gl_FAT32_File[Position_in_Table].Next_Byte_Read_Sector = 0;																			// Fange im 0. Sektor der Cluster an zu lesen
						gl_FAT32_File[Position_in_Table].Read_Write = FAT32_Read;																			// Datei im Lesemodus �ffnen
						gl_FAT32_File[Position_in_Table].Readcache.Pos_in_Buffer=0;   																		// Position an den Anfang des Puffers stellen
						gl_FAT32_File[Position_in_Table].Readcache.Count = 0;																				// Es wurden nock keine Bytes geladen
						memset(gl_FAT32_File[Position_in_Table].Readcache.Buffer,0,sizeof(gl_FAT32_File[Position_in_Table].Readcache.Buffer));				// Readcache leeren
					break;
					case FAT32_Write:
						gl_FAT32_File[Position_in_Table].Dir_Clustersector = Backup_Cluster;																// Speichere den Cluster des Verzeichnisses in dem die Datei steht
						gl_FAT32_File[Position_in_Table].Dir_Sector = Backup_Sector;																		// Speichere den Sektor des Verzeichnisses in dem die Datei steht
						gl_FAT32_File[Position_in_Table].Dir_Entrypos = Next_Date;																			// Speichere die Byteoffsetaddresse des Dateieintrags

						gl_FAT32_File[Position_in_Table].Next_Byte_Write = gl_FAT32_File[Position_in_Table].Size;											// Naechstes zu schreibende Byte
						gl_FAT32_File[Position_in_Table].Next_Byte_Write_Cluster = FAT32_Get_Last_Cluster(gl_FAT32_File[Position_in_Table].First_Clustersector);	// Addresse des ersten Sektors in den geschrieben werden koennte
						//gl_FAT32_File[Position_in_Table].Next_Byte_Write_Sector = (gl_FAT32_File[Position_in_Table].Size - 1 - (((gl_FAT32_File[Position_in_Table].Size - 1) / (gl_FAT32.Bytes_times_Cluster)) * gl_FAT32.Bytes_times_Cluster)) / gl_FAT32.Bytes_per_Sector;
						gl_FAT32_File[Position_in_Table].Read_Write = FAT32_Write;																			// Schreibzugriff auf die Datei

						uint32_t Startsector = 0;
						uint32_t Sectors_to_Read =0;

						if(gl_FAT32_File[Position_in_Table].Size>0)																							// Wenn Daten in Datei vorhanden sind
						{
							if(gl_FAT32.Bytes_times_Cluster <= FAT32_Writecachesize)																		// Bsp: Cache ist 4096, bytes per Cluster ist 2048; Cache ist 4096, Cluster ist 4096
							{
								// Anzahl der Bytes im Cache
								gl_FAT32_File[Position_in_Table].Writecache.Count = gl_FAT32.Bytes_times_Cluster;
								memset(gl_FAT32_File[Position_in_Table].Writecache.Buffer,0,gl_FAT32_File[Position_in_Table].Writecache.Count);

								// Lade die komplette letzte Cluster in den Buffer
								// gl_FAT32.Root_Startaddress + ((Cluster - gl_FAT32.Root_First_Cluster) * gl_FAT32.Bytes_times_Cluster) + (Sector * gl_FAT32.Bytes_per_Sector)
								SD_Card_CMD18(gl_FAT32.Root_Startaddress + ((gl_FAT32_File[Position_in_Table].Next_Byte_Write_Cluster - gl_FAT32.Root_First_Cluster) * gl_FAT32.Bytes_times_Cluster),gl_FAT32.Sectors_per_Cluster,gl_FAT32_File[Position_in_Table].Writecache.Buffer);

								// Ermittle die Position des Writecachezeigers
								gl_FAT32_File[Position_in_Table].Writecache.Pos_in_Buffer = ((gl_FAT32_File[Position_in_Table].Size - 1) % gl_FAT32_File[Position_in_Table].Writecache.Count) + 1;		// Pos in Buffer ausrechnen

								gl_FAT32_File[Position_in_Table].Next_Byte_Write_Sector = 0;																// Da ich den kompletten Cluster laden konnte, ist der Sektor 0
							}
							else																															// BSP Cache ist 512, Cluster ist 4096
							{
								// Anzahl der Bytes im Cache
								gl_FAT32_File[Position_in_Table].Writecache.Count = FAT32_Writecachesize;
								memset(gl_FAT32_File[Position_in_Table].Writecache.Buffer,0,gl_FAT32_File[Position_in_Table].Writecache.Count);

								// Lade den letzen Sektor in der Datei der noch Daten enth�lt
								// gl_FAT32.Root_Startaddress + ((Cluster - gl_FAT32.Root_First_Cluster) * gl_FAT32.Bytes_times_Cluster) + (Sector * gl_FAT32.Bytes_per_Sector)
								Sectors_to_Read = FAT32_Writecachesize/gl_FAT32.Bytes_per_Sector;													// Errechne anhand der Writecachegr��e, die Anzahl zu lesender Sektoren
								// (((size-1)%bytes_per_Cluster)/Writecachesize)*(writecache/bytes_per_sector)
								Startsector = (((gl_FAT32_File[Position_in_Table].Size-1)%gl_FAT32.Bytes_times_Cluster)/FAT32_Writecachesize)*FAT32_Writecachebuffercount;			// Der Startsektor ist der Offset im Cluster ab dem die Daten in den Cache gelesen werden
								SD_Card_CMD18(gl_FAT32.Root_Startaddress + ((gl_FAT32_File[Position_in_Table].Next_Byte_Write_Cluster - gl_FAT32.Root_First_Cluster) * gl_FAT32.Bytes_times_Cluster) + (Startsector * gl_FAT32.Bytes_per_Sector),Sectors_to_Read,gl_FAT32_File[Position_in_Table].Writecache.Buffer);

								// Ermittle die Position des Writecachezeigers
								gl_FAT32_File[Position_in_Table].Writecache.Pos_in_Buffer = ((gl_FAT32_File[Position_in_Table].Size - 1) % gl_FAT32_File[Position_in_Table].Writecache.Count) + 1;		// Pos in Buffer ausrechnen

								gl_FAT32_File[Position_in_Table].Next_Byte_Write_Sector = Startsector;														// Da ich von diesem Startsector geladen habe, muss ich auch dahin wieder schreiben
							}
						}
						else	// Wenn die Dateigr��e 0 ist, wei�e den richtigen Count zu um bei FAT32_File_Write, einen ersten Cluster anh�ngen zu k�nnen
						{
							gl_FAT32_File[Position_in_Table].Writecache.Pos_in_Buffer=0;
							gl_FAT32_File[Position_in_Table].Next_Byte_Write_Sector=0;
							if(gl_FAT32.Bytes_times_Cluster <= FAT32_Writecachesize)																		// Bsp: Cache ist 4096, bytes per Cluster ist 2048; Cache ist 4096, Cluster ist 4096
							{
								// Anzahl der Bytes im Cache
								gl_FAT32_File[Position_in_Table].Writecache.Count = gl_FAT32.Bytes_times_Cluster;
							}
							else																															// BSP Cache ist 512, Cluster ist 4096
							{
								// Anzahl der Bytes im Cache
								gl_FAT32_File[Position_in_Table].Writecache.Count = FAT32_Writecachesize;
							}
						}
						#ifdef FAT32_Debug
							printf("next_bytes_write_sector: %u\r\nbytes_times_cluster: %lu\r\ncount: %lu\r\nPos_in_Buffer: %lu\r\nStartsector: %lu\r\n",gl_FAT32_File[Position_in_Table].Next_Byte_Write_Sector,gl_FAT32.Bytes_times_Cluster,gl_FAT32_File[Position_in_Table].Writecache.Count,gl_FAT32_File[Position_in_Table].Writecache.Pos_in_Buffer, Startsector);
							USART_Write_X_Bytes(gl_FAT32_File[Position_in_Table].Writecache.Buffer,0,FAT32_Writecachesize);
						#endif
					break;
				}

				gl_FAT32_File[Position_in_Table].Is_Open = 1;																								// Datei ist geoeffnet

				#ifdef FAT32_Debug
					printf("Datei geoeffnet: %s\r\nAttribute: %u\r\nDateigroesse: %lu\r\nErster Sektor: %lu\r\n",gl_FAT32_File[Position_in_Table].Name,gl_FAT32_File[Position_in_Table].Attributes,gl_FAT32_File[Position_in_Table].Size,gl_FAT32_File[Position_in_Table].First_Clustersector);
				#endif
				return 0;
			}
		}
	}
}

uint8_t FAT32_File_Check_if_Exist(char *Filename)
{
	char Filename_converted[12];																			// Konvertierter Dateiname
	uint32_t Temp_Dir_Cluster = gl_FAT32_Directory.First_Clustersector;										// Aktueller Cluster der Dir
	uint16_t Next_Date=0;																					// Naechste Datei im Array +=32
	uint8_t Temp_Dir_Sector=0;																				// Tempor�rer Sektor der Dir

	Convert_String_to_8_3(Filename_converted, Filename);													// Konvertiere Dateistring
	while(1)																								// Endlosschleife bis der Dateiname gefunden wurde
	{
		if((Temp_Dir_Cluster > 0x0ffffff7) && (Temp_Dir_Cluster < 0x10000000))								// Pr�fe ob ende der Dir Cluster erreicht wurde
		{
			#ifdef FAT32_Debug
			printf("FAT32: Datei \"%s\" wurde nicht gefunden\r\n",Filename_converted);
			#endif
			return 1;
		}

		Temp_Dir_Sector = FAT32_Get_Data_from_Sector(Temp_Dir_Cluster,Temp_Dir_Sector,&gl_SD_Card.RWbuffer[0]);		// Lade einen Sektor der Cluster
		if(Temp_Dir_Sector==0) Temp_Dir_Cluster = FAT32_Get_Next_Cluster(Temp_Dir_Cluster);							// Wenn der letzte Sektor der Cluster geladen wurde, lade die n�chste Cluster

		Next_Date = 0;																								// Next Date zuruecksetzen
		for (uint8_t g=0;g<16;g++,Next_Date+=32)																	// 16 Eintraege pro FAT32 Sektor
		{
			if (memcmp(&gl_SD_Card.RWbuffer[Next_Date],&Filename_converted[0],11)==0)
			{
				#ifdef FAT32_Debug
					USART_Write_String("FAT32: Dateieintrag gefunden, Datei existiert\r\n");
				#endif
				return 0;
			}
		}
	}
}

uint32_t FAT32_Get_Next_Free_Cluster(uint32_t Clear_Cluster)
{
	uint32_t Reserved_Sectors = gl_FAT32.Reserved_Sectors, Clustercounter, Clustercounter_old;					// Lokalisierung Variable, Anzahl der durchsuchten Sektoren
	uint32_t Returnvalue, FAT_Offset_Address = gl_FAT32.Last_Found_Clustercounter_times_Bytes_per_Sector + gl_FAT32.First_FAT_Address;	// Returnsector, FAT Offset + Erste FAT Adresse
	uint32_t Bytes_per_Sector = gl_FAT32.Bytes_per_Sector;														// Bytes per Sektor als lokale Variable

	Clustercounter = gl_FAT32.Last_Found_Clustercounter;														// Ab dem wird gesucht
	Clustercounter_old = Clustercounter;																		// Speichere den Clustercounter in das Backup

	do
	{
		if (FAT_Offset_Address != gl_FAT32.Loaded_FAT_Sector)													// Lade den Sektor nur falls dieser nicht im FAT Buffer liegt
		{
			SD_Card_CMD17(FAT_Offset_Address, &gl_FAT32.FAT_Buffer[0]);											// Lade den aktuell benoetigen FAT Sektor
			gl_FAT32.Loaded_FAT_Sector = FAT_Offset_Address;													// Setze die neue FAT Tabellenaddresse
		}

		for (uint8_t g=0;g<128;g++)
		{
			Returnvalue = Get_uint32_from_Little_Endian_Buffer(&gl_FAT32.FAT_Buffer[4*g]);						// Lese Cluster aus FAT Sektor aus
			if (Returnvalue == 0)
			{
				Write_uint32_to_Little_Endian_Buffer(&gl_FAT32.FAT_Buffer[4*g],0x0fffffff);						// Markiere den Cluster als in Benutzung
				SD_Card_CMD24(FAT_Offset_Address,&gl_FAT32.FAT_Buffer[0]);										// Schreibe den buffer in auf die Karte

				Returnvalue = Clustercounter * 128 + g;															// Clustereintrag ist 4 Byte breit

				#ifdef FAT32_Debug
					printf("FAT32: Freier Sektor: 0x%08lx wurde gefunden\r\n",Returnvalue);
				#endif

				if (Clear_Cluster)
				{
					SD_Card_clear_RWbuffer();																		// Readbuffer loeschen
					for(uint8_t i=0; i<gl_FAT32.Sectors_per_Cluster;i++)											// Alten Clusterinhalt loeschen
					{
						FAT32_Write_Data_to_Sector(Returnvalue,i,&gl_SD_Card.RWbuffer[0]);
					}
				}

				gl_FAT32.Last_Found_Clustercounter = Clustercounter;											// Speichere den neuen Sektor ab
				gl_FAT32.Last_Found_Clustercounter_times_Bytes_per_Sector = Clustercounter * Bytes_per_Sector;	// Hilfsvariable f�r  get next free sector

				return Returnvalue;																				// Naechster freier Sektor zurueck
			}
		}

		FAT_Offset_Address+=Bytes_per_Sector;																	// Z�hle einen Sektor weiter
		Clustercounter++;
		if(Clustercounter == Reserved_Sectors)																	// Wenn im hinteren Bereich der FAT nichts mehr frei ist, fange von vorne an zu suchen
		{
			Clustercounter = 0;
			FAT_Offset_Address = gl_FAT32.First_FAT_Address;													// Setze die Offsetadresse auf den Anfang der FAT
		}
	}while(Clustercounter != Clustercounter_old);																// Suche ab dem letzten freien Sektor, bis du wieder bei diesem ankommst


	#ifdef FAT32_Debug
		USART_Write_String("FAT32: FAT-Tabelle ist voll! Es konnte kein freier Sektor gefunden werden\r\n");
	#endif

	gl_FAT32.Last_Found_Clustercounter = Reserved_Sectors;														// Speichere die Last Adresse ab
	gl_FAT32.Last_Found_Clustercounter_times_Bytes_per_Sector = Reserved_Sectors * Bytes_per_Sector;			// Hilfsvariable f�r  get next free sector
	return 0x0fffffff;																							// Gebe zurueck das der letze Sektor erreicht wurde
}

void FAT32_Change_Clusterentrie_in_FAT(uint32_t Cluster, uint32_t Value)
{
	uint32_t FAT_Offset_Address=0;																				// Offsetadresse der FAT, Da Sektor 512 Bytes gro� ist

	FAT_Offset_Address = ((Cluster/128) * gl_FAT32.Bytes_per_Sector) + gl_FAT32.First_FAT_Address;				// Hier liegt der FAT Offset
	Cluster = (Cluster & 127);																					// Hier kommt die Position im Sector raus mithilfe der Cluster

	if (FAT_Offset_Address != gl_FAT32.Loaded_FAT_Sector)														// Lade den Sektor nur falls dieser nicht im FAT Buffer liegt
	{
		SD_Card_CMD17(FAT_Offset_Address, &gl_FAT32.FAT_Buffer[0]);												// Lade den aktuell benoetigen FAT Sektor
		gl_FAT32.Loaded_FAT_Sector = FAT_Offset_Address;														// Setze die neue FAT Tabellenaddresse
	}
	
	Write_uint32_to_Little_Endian_Buffer(&gl_FAT32.FAT_Buffer[Cluster*4],Value);								// Trage an die Position des Clustereintrags den Wert ein
	SD_Card_CMD24(FAT_Offset_Address,&gl_FAT32.FAT_Buffer[0]);													// Schreibe den aktuellen veraenderten FAT Sektor
}

uint32_t FAT32_Connect_Empty_Cluster_to_Filescluster(uint32_t Filecluster, uint32_t Clear_Cluster)
{
	uint32_t Empty_Cluster = FAT32_Get_Next_Free_Cluster(Clear_Cluster);

	if (Empty_Cluster != 0x0fffffff)
	{
		//FAT32_Change_Clusterentrie_in_FAT(Empty_Cluster,0x0fffffff);									// Wird schon in FAT32_Get_Next_Free_Cluster() erledigt
		FAT32_Change_Clusterentrie_in_FAT(Filecluster,Empty_Cluster);
	}

	return Empty_Cluster;
}

uint32_t FAT32_Connect_First_Cluster_to_File(uint8_t Position_in_Table)
{
	uint32_t Empy_Cluster = FAT32_Get_Next_Free_Cluster(0);												// Hole freien Cluster

	if (Empy_Cluster != 0x0fffffff)																		// Wenn g�ltig
	{
		//FAT32_Change_Clusterentrie_in_FAT(Empy_Cluster,0x0fffffff);									// Trage in der FAT den Cluster als Dateiende ein, Wird schon in FAT32_Get_Next_Free_Cluster() erledigt

		FAT32_Get_Data_from_Sector(gl_FAT32_File[Position_in_Table].Dir_Clustersector,gl_FAT32_File[Position_in_Table].Dir_Sector,&gl_SD_Card.RWbuffer[0]);	// Lade den Sektor aus der Dircluster, in dem der Eintrag der Datei steht

		gl_SD_Card.RWbuffer[gl_FAT32_File[Position_in_Table].Dir_Entrypos+26]=Empy_Cluster & 0xff;		// Trage den Cluster ein
		gl_SD_Card.RWbuffer[gl_FAT32_File[Position_in_Table].Dir_Entrypos+27]=(Empy_Cluster>>8) & 0xff;
		gl_SD_Card.RWbuffer[gl_FAT32_File[Position_in_Table].Dir_Entrypos+20]=(Empy_Cluster>>16) & 0xff;
		gl_SD_Card.RWbuffer[gl_FAT32_File[Position_in_Table].Dir_Entrypos+21]=(Empy_Cluster>>24) & 0xff;

		FAT32_Write_Data_to_Sector(gl_FAT32_File[Position_in_Table].Dir_Clustersector,gl_FAT32_File[Position_in_Table].Dir_Sector,&gl_SD_Card.RWbuffer[0]);	// Schreibe den Sektor in die Dircluster zur�ck

		gl_FAT32_File[Position_in_Table].First_Clustersector = Empy_Cluster;							// Trage den Cluster in die Datei ein
	}
	return Empy_Cluster;																				// Gebe den gefunden Cluster zur�ck
}

uint32_t FAT32_Get_Last_Cluster(uint32_t Filecluster)
{
	uint32_t FAT_Offset_Address=0, Back_Cluster=0;														// Offsetadresse der FAT, Da Sektor 512 Bytes gro� ist, Speichere den Sektor fuer spaeteres lesen ab

	while(1)
	{																									// Setze den FAT Offset zurueck und den Sectorcounter
		Back_Cluster = Filecluster;																		// Speichere den aktuellen Cluster in das Backup

		FAT_Offset_Address = ((Filecluster/128) * gl_FAT32.Bytes_per_Sector) + gl_FAT32.First_FAT_Address;	// Hier liegt der FAT Offset + First_FAT_Adress zur Vereinfachung
		Filecluster &= 127;																					// Hier kommt die Position im FAT Sector raus mithilfe des Clusters

		if (FAT_Offset_Address != gl_FAT32.Loaded_FAT_Sector)											// Lade den Sektor nur falls dieser nicht im FAT Buffer liegt
		{
			SD_Card_CMD17(FAT_Offset_Address, &gl_FAT32.FAT_Buffer[0]);									// Lade den aktuell benoetigen FAT Sektor
			gl_FAT32.Loaded_FAT_Sector = FAT_Offset_Address;											// Setze die neue FAT Tabellenaddresse
		}
		
		Filecluster = Get_uint32_from_Little_Endian_Buffer(&gl_FAT32.FAT_Buffer[Filecluster*4]);		// Gebe den nachfolgenden Cluster zurueck

		if ((Filecluster > 0x0ffffff7) && (Filecluster < 0x10000000))									// Breche ab wenn der naechste Cluster der EOF 0x0fffffff waere
		{
			return Back_Cluster;
		}
	}
}

uint8_t FAT32_File_Create(char *Filename, uint8_t Attribute)
{
	char Filename_converted[12];																		// Konvertierter Dateiname
	uint32_t Temp_Dir_Cluster = gl_FAT32_Directory.First_Clustersector, Backup_Cluster=0;				// Aktueller Cluster der Dir, Backup des letzten Cluster
	uint16_t Next_Date=0, Date=0, Time=0;																// Naeste Datei im Array +=32, Datum, Uhrzeit
	uint8_t cYear=0, Temp_Dir_Sector=0, Backup_Sector=0;												// Aktueller Sektor der Dir, Backup des letzten Sektor

	if(FAT32_File_Check_if_Exist(Filename)==0)
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Datei konnte nicht erstellt werden. Datei existiert bereits\r\n");
		#endif
		return 1;
	}

	Convert_String_to_8_3(Filename_converted, Filename);												// Konvertiere Dateistring

	while(1)																							// Endlosschleife bis der Dateiname gefunden wurde
	{
		Backup_Cluster = Temp_Dir_Cluster;																// Speichere den letzten Cluster
		Backup_Sector = Temp_Dir_Sector;																// Speichere den letzten Sektor
		Temp_Dir_Sector = FAT32_Get_Data_from_Sector(Temp_Dir_Cluster,Temp_Dir_Sector,&gl_SD_Card.RWbuffer[0]);		// Lade einen Sektor der Cluster
		if(Temp_Dir_Sector==0) Temp_Dir_Cluster = FAT32_Get_Next_Cluster(Temp_Dir_Cluster);				// Wenn der letzte Sektor der Cluster geladen wurde, lade die n�chste Cluster

		Next_Date = 0;																					// Next Date zuruecksetzen
		for (uint8_t g=0;g<16;g++,Next_Date+=32)														// 16 Eintraege pro FAT32 Sektor
		{
			if (gl_SD_Card.RWbuffer[Next_Date]==0 || gl_SD_Card.RWbuffer[Next_Date]==0xe5)
			{
				Temp_Dir_Sector = FAT32_Get_Data_from_Sector(Backup_Cluster,Backup_Sector,&gl_SD_Card.RWbuffer[0]);	// Lade wieder den letzen Sektor der Dir in den Readbuffer

				#ifdef FAT32_USE_RTC
					DS1307_Read_Timestamp(&gl_Time);														// Lese Timestamp
				#endif
				strcpy(&gl_SD_Card.RWbuffer[Next_Date], Filename_converted);
				cYear = (uint8_t)(gl_Time.Year - 1980);													// Errechne den Charwert aus dem Year

				Date = cYear; Date = Date << 4; Date |= gl_Time.Month; Date = Date << 5; Date |= gl_Time.Day;					// Quetsche Year, Month und Day in eine Integer
				Time = gl_Time.Hour; Time = Time << 6; Time |= gl_Time.Minute; Time = Time << 5; Time |= (gl_Time.Seconds/2);	// Das selbe mit Stunde, Minute und Sekunde

				gl_SD_Card.RWbuffer[Next_Date+11]=Attribute;
				gl_SD_Card.RWbuffer[Next_Date+12]=0;
				gl_SD_Card.RWbuffer[Next_Date+13]=0;
				gl_SD_Card.RWbuffer[Next_Date+14]=Time & 0xff;										// Erstellzeit
				gl_SD_Card.RWbuffer[Next_Date+15]=Time>>8;											// Erstellzeit
				gl_SD_Card.RWbuffer[Next_Date+16]=Date & 0xff;										// Erstelldatum
				gl_SD_Card.RWbuffer[Next_Date+17]=Date>>8;											// Erstelldatum
				gl_SD_Card.RWbuffer[Next_Date+18]=Date & 0xff;										// Letzer Zugriff, nur Datum
				gl_SD_Card.RWbuffer[Next_Date+19]=Date>>8;											// Letzer Zugriff, nur Datum
				gl_SD_Card.RWbuffer[Next_Date+22]=Time & 0xff;										// Aenderungszeit
				gl_SD_Card.RWbuffer[Next_Date+23]=Time>>8;											// Aenderungszeit
				gl_SD_Card.RWbuffer[Next_Date+24]=Date & 0xff;										// Aenderungsdatum
				gl_SD_Card.RWbuffer[Next_Date+25]=Date>>8;											// Aenderungsdatum

				// Erster Freier Sektor wird hier nicht erstellt, erfolgt in der FAT32_Write_File()
				gl_SD_Card.RWbuffer[Next_Date+26]=0;
				gl_SD_Card.RWbuffer[Next_Date+27]=0;
				gl_SD_Card.RWbuffer[Next_Date+20]=0;
				gl_SD_Card.RWbuffer[Next_Date+21]=0;

				gl_SD_Card.RWbuffer[Next_Date+28]=0x00;												// Dateigroesse wird auf 0 gesetzt
				gl_SD_Card.RWbuffer[Next_Date+29]=0x00;
				gl_SD_Card.RWbuffer[Next_Date+30]=0x00;
				gl_SD_Card.RWbuffer[Next_Date+31]=0x00;

				FAT32_Write_Data_to_Sector(Backup_Cluster, Backup_Sector,&gl_SD_Card.RWbuffer[0]);	// Schreibe den geaenderten Readbuffer auf die Karte zurueck

				#ifdef FAT32_Debug
					printf("FAT32: Datei %s wurde angelegt\r\n",Filename_converted);
				#endif
				return 0;
			}
		}

		if((Temp_Dir_Cluster > 0x0ffffff7) && (Temp_Dir_Cluster < 0x10000000))							// Wenn das Ende der Dir erreicht wurde, versuche einen neuen Sektor zuzuordnen, falls nicht breche ab
		{
			Temp_Dir_Cluster = FAT32_Connect_Empty_Cluster_to_Filescluster(Backup_Cluster,1);
			Temp_Dir_Sector = 0;
			if(Temp_Dir_Cluster == 0x0fffffff)
			{
				#ifdef FAT32_Debug
					printf("FAT32: Datei \"%s\" konnte nicht angelegt werden\r\n",Filename_converted);
				#endif
				return 1;
			}
		}
	}
}

uint8_t FAT32_Directory_Create(char *Filename, uint8_t Attribute)
{
	char Filename_converted[12];																		// Konvertierter Dateiname
	uint32_t Temp_Dir_Cluster = gl_FAT32_Directory.First_Clustersector, Backup_Cluster=0, Free_Cluster=0;// Aktueller Cluster der Dir, Backup des letzten Cluster, Freier Cluster
	uint16_t Next_Date=0, Date=0, Time=0;																// Naeste Datei im Array +=32, Datum, Uhrzeit
	uint8_t cYear=0, Temp_Dir_Sector=0, Backup_Sector=0;												// Aktueller Sektor der Dir, Backup des letzten Sektor

	if(FAT32_File_Check_if_Exist(Filename)==0)
	{
		#ifdef FAT32_Debug
		USART_Write_String("FAT32: Verzeichnis konnte nicht erstellt werden. Verzeichnis existiert bereits\r\n");
		#endif
		return 1;
	}

	Convert_String_to_8_3(Filename_converted, Filename);												// Konvertiere Dateistring

	while(1)																							// Endlosschleife bis der Dateiname gefunden wurde
	{
		Backup_Cluster = Temp_Dir_Cluster;																// Speichere den letzten Cluster
		Backup_Sector = Temp_Dir_Sector;																// Speichere den letzten Sektor
		Temp_Dir_Sector = FAT32_Get_Data_from_Sector(Temp_Dir_Cluster,Temp_Dir_Sector,&gl_SD_Card.RWbuffer[0]);		// Lade einen Sektor der Cluster
		if(Temp_Dir_Sector==0) Temp_Dir_Cluster = FAT32_Get_Next_Cluster(Temp_Dir_Cluster);				// Wenn der letzte Sektor der Cluster geladen wurde, lade die n�chste Cluster

		Next_Date = 0;																					// Next Date zuruecksetzen
		for (uint8_t g=0;g<16;g++,Next_Date+=32)														// 16 Eintraege pro FAT32 Sektor
		{
			if (gl_SD_Card.RWbuffer[Next_Date]==0 || gl_SD_Card.RWbuffer[Next_Date]==0xe5)
			{
				Free_Cluster = FAT32_Get_Next_Free_Cluster(1);											// Besorge den naechsten freien Sektor

				Temp_Dir_Sector = FAT32_Get_Data_from_Sector(Backup_Cluster,Backup_Sector,&gl_SD_Card.RWbuffer[0]);	// Lade wieder den letzen Sektor der aktuellen Cluster Dir in den Readbuffer

				#ifdef FAT32_USE_RTC
					DS1307_Read_Timestamp(&gl_Time);													// Lese Timestamp
				#endif
				strcpy(&gl_SD_Card.RWbuffer[Next_Date], Filename_converted);
				cYear = (uint8_t)(gl_Time.Year - 1980);													// Errechne den Charwert aus dem Year

				Date = cYear; Date = Date << 4; Date |= gl_Time.Month; Date = Date << 5; Date |= gl_Time.Day;				// Quetsche Year, Month und Day in eine Integer
				Time = gl_Time.Hour; Time = Time << 6; Time |= gl_Time.Minute; Time = Time << 5; Time |= gl_Time.Seconds;	// Das selbe mit Stunde, Minute und Sekunde

				gl_SD_Card.RWbuffer[Next_Date+11]=Attribute | 0b00010000;							// Setze auf jedenfall das Directory Bit
				gl_SD_Card.RWbuffer[Next_Date+12]=0;
				gl_SD_Card.RWbuffer[Next_Date+13]=0;
				gl_SD_Card.RWbuffer[Next_Date+14]=Time & 0xff;										// Erstellzeit
				gl_SD_Card.RWbuffer[Next_Date+15]=Time>>8;											// Erstellzeit
				gl_SD_Card.RWbuffer[Next_Date+16]=Date & 0xff;										// Erstelldatum
				gl_SD_Card.RWbuffer[Next_Date+17]=Date>>8;											// Erstelldatum
				gl_SD_Card.RWbuffer[Next_Date+18]=Date & 0xff;										// Letzer Zugriff, nur Datum
				gl_SD_Card.RWbuffer[Next_Date+19]=Date>>8;											// Letzer Zugriff, nur Datum
				gl_SD_Card.RWbuffer[Next_Date+22]=Time & 0xff;										// aenderungszeit
				gl_SD_Card.RWbuffer[Next_Date+23]=Time>>8;											// aenderungszeit
				gl_SD_Card.RWbuffer[Next_Date+24]=Date & 0xff;										// aenderungsdatum
				gl_SD_Card.RWbuffer[Next_Date+25]=Date>>8;											// aenderungsdatum

				gl_SD_Card.RWbuffer[Next_Date+26]=Free_Cluster & 0xff;								// Erster Datensektor wird beim Erstellen eines leeren Verzeichnis mit angelegt
				gl_SD_Card.RWbuffer[Next_Date+27]=(Free_Cluster>>8) & 0xff;
				gl_SD_Card.RWbuffer[Next_Date+20]=(Free_Cluster>>16) & 0xff;
				gl_SD_Card.RWbuffer[Next_Date+21]=(Free_Cluster>>24) & 0xff;

				gl_SD_Card.RWbuffer[Next_Date+28]=0x00;												// Dateigroesse wird auf 0 gesetzt
				gl_SD_Card.RWbuffer[Next_Date+29]=0x00;
				gl_SD_Card.RWbuffer[Next_Date+30]=0x00;
				gl_SD_Card.RWbuffer[Next_Date+31]=0x00;

				FAT32_Write_Data_to_Sector(Backup_Cluster,Backup_Sector,&gl_SD_Card.RWbuffer[0]);	// Schreibe den geaenderten Readbuffer auf die Karte zurueck

				//FAT32_Change_Clusterentrie_in_FAT(Free_Cluster,0x0fffffff);							// Trage in der FAT in den ersten Datensektor der Datei das EOF ein, Wird schon in FAT32_Get_Next_Free_Cluster() erledigt

				#ifdef FAT32_Debug
					printf("FAT32: Verzeichnis %s wurde angelegt\r\n",Filename_converted);
				#endif
				return 0;
			}
		}
		if((Temp_Dir_Cluster > 0x0ffffff7) && (Temp_Dir_Cluster < 0x10000000))							// Wenn das Ende der Dir erreicht wurde, versuche einen neuen Sektor zuzuordnen, falls nicht breche ab
		{
			Temp_Dir_Cluster = FAT32_Connect_Empty_Cluster_to_Filescluster(Backup_Cluster,1);
			Temp_Dir_Sector = 0;
			#ifdef FAT32_Debug
				USART_Write_String("FAT32: H�nge weitere Cluster an das Verzeichnis\r\n");
			#endif
			if(Temp_Dir_Cluster == 0x0fffffff)
			{
				#ifdef FAT32_Debug
					printf("FAT32: Datei \"%s\" konnte nicht angelegt werden\r\n",Filename_converted);
				#endif
				return 1;
			}
		}
	}
}

uint8_t FAT32_File_Close(uint8_t Position_in_Table)
{
	uint8_t returnvalue=0;
	if (gl_FAT32_File[Position_in_Table].Read_Write==FAT32_Write)															// Wenn auf die Karte geschrieben wurde aktuallisiere den Eintrag
	{
		uint32_t Temp_Cluster = gl_FAT32_File[Position_in_Table].Next_Byte_Write_Cluster;

		if (gl_FAT32_File[Position_in_Table].First_Clustersector == 0)													// Falls noch kein Sektor der Datei zugewiesen wurde mache das
		{
			Temp_Cluster = FAT32_Connect_First_Cluster_to_File(Position_in_Table);										// Binde den ersten Sektor an die Datei und gebe den Sektor zur�ck
		}

		if (Temp_Cluster == 0x0fffffff)																					// Falls das anbinden nicht geklappt hat, breche ab
		{
			#ifdef FAT32_Debug
				USART_Write_String("FAT32: Kein Platz in der FAT. Es konnten nicht alle Bytes geschrieben werden\r\n");
			#endif
			return 1;
		}

		// Schreibe den geladenen Teil einer CLuster in die Karte zur�ck
		uint32_t Sectors_to_Write = gl_FAT32_File[Position_in_Table].Writecache.Count/gl_FAT32.Bytes_per_Sector;			// Errechne anhand der Writecachegr��e, die Anzahl zu lesender Sektoren
		SD_Card_CMD25(gl_FAT32.Root_Startaddress + ((Temp_Cluster - gl_FAT32.Root_First_Cluster) * gl_FAT32.Bytes_times_Cluster) + (gl_FAT32_File[Position_in_Table].Next_Byte_Write_Sector * gl_FAT32.Bytes_per_Sector),Sectors_to_Write,gl_FAT32_File[Position_in_Table].Writecache.Buffer);

		if (FAT32_Update_Entrie(Position_in_Table,gl_FAT32_File[Position_in_Table].Attributes, gl_FAT32_File[Position_in_Table].Next_Byte_Write))		// Update die Dateigroesse auf der SD Karte
		{
			returnvalue=1;
		}
	}
	memset(&gl_FAT32_File[Position_in_Table], 0, sizeof(gl_FAT32_File[Position_in_Table]));									// Setze Struct auf 0

	#ifdef FAT32_Debug
		USART_Write_String("FAT32: Datei geschlossen\r\n");
	#endif
	return returnvalue;
}

uint8_t FAT32_Directory_Change(const char *Directorystring)
{
	char Filename_converted[12],Directorystringcopy[255];														// Konvertierter Dateiname, 
	char *ptr=0;																								// Pointer zum arbeiten
	uint32_t Temp_Dir_Cluster = gl_FAT32.Root_First_Cluster;													// Aktueller Cluster der Root
	uint16_t Next_Date=0;																						// Naeste Datei im Array +=32
	uint8_t Next_Sub=1, Temp_Dir_Sector=0;																		// Naechster Unterordner, N�chste zu ladender Sektor der Cluster

	memset(&Directorystringcopy[0],0,sizeof(Directorystringcopy));
	strcpy(&Directorystringcopy[0],&Directorystring[0]);														// strtok veraendert den String, daher muss dieser in einer kopie gebuffert werden
	
	if ((strlen(&Directorystringcopy[0])==1) && (memcmp(&Directorystringcopy[0],"/",1)==0))
	{
		strcpy(&gl_FAT32_Directory.Name[0],"Root");																// Dateinamen leeren
		gl_FAT32_Directory.Attributes = 0;																		// Dateiattribute speichern
		gl_FAT32_Directory.Size = 0;																			// Dateigroesse
		gl_FAT32_Directory.Creation_Time = 0;																	// Erstellzeit komprimiert
		gl_FAT32_Directory.Creation_Date = 0;																	// Erstelldatum komprimiert
		gl_FAT32_Directory.Last_Access_Date = 0;																// Erstellzeit komprimiert
		gl_FAT32_Directory.Changetime = 0;																		// Modifikationszeit komprimiert
		gl_FAT32_Directory.Changedate = 0;																		// Erstellzeit komprimiert
		gl_FAT32_Directory.First_Clustersector =  gl_FAT32.Root_First_Cluster;									// Erster Datasektor

		#ifdef FAT32_Debug
			printf("Verzeichnis: %s\r\nAttribute: %u\r\nDateigroesse: %lu\r\nErster Sektor: %lu\r\n",gl_FAT32_Directory.Name,gl_FAT32_Directory.Attributes,gl_FAT32_Directory.Size,gl_FAT32_Directory.First_Clustersector);
		#endif
		return 0;
	}

	ptr = strstr(&Directorystringcopy[0],"/") != 0 ? strtok(Directorystringcopy, "/") : Directorystringcopy;	// Pruefe ob im Filename ein "/" vorkommt

	while(1)
	{
		Next_Sub=1;
		Convert_String_to_8_3(Filename_converted, ptr);																							// Konvertiere Dateistring
		
		while(Next_Sub)																															// Solange bis der letze Sektor durchgeschaut wurde suche weiter
		{
			if((Temp_Dir_Cluster > 0x0ffffff7) && (Temp_Dir_Cluster < 0x10000000))
			{
				#ifdef FAT32_Debug
					printf("FAT32: Verzeichnis \"%s\" wurde nicht gefunden\r\n",Filename_converted);
				#endif
				return 1;
			}
			Temp_Dir_Sector = FAT32_Get_Data_from_Sector(Temp_Dir_Cluster,Temp_Dir_Sector,&gl_SD_Card.RWbuffer[0]);								// Lade einen Sektor der Cluster
			if(Temp_Dir_Sector==0) Temp_Dir_Cluster = FAT32_Get_Next_Cluster(Temp_Dir_Cluster);													// Wenn der letzte Sektor der Cluster geladen wurde, lade die n�chste Cluster

			Next_Date = 0;																														// Next Date zuruecksetzen
			for (uint8_t g=0;g<16;g++,Next_Date+=32)																							// 16 Eintraege pro FAT32 Sektor
			{
				if (memcmp(&gl_SD_Card.RWbuffer[Next_Date],&Filename_converted[0],11)==0)
				{
					gl_FAT32_Directory.First_Clustersector =  char_to_long_int(gl_SD_Card.RWbuffer[Next_Date+21],gl_SD_Card.RWbuffer[Next_Date+20],gl_SD_Card.RWbuffer[Next_Date+27],gl_SD_Card.RWbuffer[Next_Date+26]);		// Erster Datasektor

					Temp_Dir_Sector = 0;																										// Temp Sektor auf 0. zur�cksetzen
					Temp_Dir_Cluster = gl_FAT32_Directory.First_Clustersector;																	// Wenn noch nichts gefunden wurde lade den naechsten Sektor
					ptr = strtok(NULL, "/");
					Next_Sub=0;																													// Beende die Whileschleife um den naechsten Dateinamen laden zu koennen

					if (*ptr == 0)																												// Wenn kein weiterer Unterordner mehr aufzurufen ist, sind wir am Ziel
					{
						strcpy(gl_FAT32_Directory.Name,Filename_converted);																			// Filename nochmal global Speichern
						gl_FAT32_Directory.Attributes = gl_SD_Card.RWbuffer[Next_Date+11];															// Dateiattribute speichern
						gl_FAT32_Directory.Size = Get_uint32_from_Little_Endian_Buffer(&gl_SD_Card.RWbuffer[Next_Date+28]);							// Dateigroesse
						gl_FAT32_Directory.Creation_Time = (gl_SD_Card.RWbuffer[Next_Date+0x0f]<<8)|(gl_SD_Card.RWbuffer[Next_Date+0x0e]);			// Erstellzeit komprimiert
						gl_FAT32_Directory.Creation_Date = (gl_SD_Card.RWbuffer[Next_Date+0x11]<<8)|(gl_SD_Card.RWbuffer[Next_Date+0x10]);			// Erstelldatum komprimiert
						gl_FAT32_Directory.Last_Access_Date = (gl_SD_Card.RWbuffer[Next_Date+0x13]<<8)|(gl_SD_Card.RWbuffer[Next_Date+0x12]);		// Erstellzeit komprimiert
						gl_FAT32_Directory.Changetime = (gl_SD_Card.RWbuffer[Next_Date+0x17]<<8)|(gl_SD_Card.RWbuffer[Next_Date+0x16]);				// Modifikationszeit komprimiert
						gl_FAT32_Directory.Changedate = (gl_SD_Card.RWbuffer[Next_Date+0x19]<<8)|(gl_SD_Card.RWbuffer[Next_Date+0x18]);				// Erstellzeit komprimiert

						#ifdef FAT32_Debug
							printf("Verzeichnis: %s\r\nAttribute: %u\r\nDateigroesse: %lu\r\nErste Cluster: %lu\r\n",gl_FAT32_Directory.Name,gl_FAT32_Directory.Attributes,gl_FAT32_Directory.Size,gl_FAT32_Directory.First_Clustersector);
						#endif
						return 0;
					}
					break;																														// Wenn der Unterordner gefunden werde, breche die FOR ab
				}
			}
		}
	}
}

uint8_t FAT32_File_Delete(char *Filename)
{
	char Filename_converted[12];																			// Konvertierter Dateiname
	uint32_t Temp_Dir_Cluster = gl_FAT32_Directory.First_Clustersector, Backup_Cluster=0, Delete_Cluster=0;	// Aktueller Cluster der Dir, Backupcluster um den richtigen zurueckzuschreiben, Zu l�schender Cluster
	uint16_t Next_Date=0;																					// Naeste Datei im Array +=32
	uint8_t Temp_Dir_Sector=0, Backup_Sector=0;																// Aktueller Sector der Dir, Backupsector der Dir

	Convert_String_to_8_3(Filename_converted, Filename);													// Konvertiere Dateistring

	if(FAT32_File_Check_If_Open(&Filename[0])==0)
	{
		#ifdef FAT32_Debug
			printf("FAT32: Datei \"%s\" konnte nicht geloescht werden. Datei wird momentan verwendet.\r\n",Filename_converted);
		#endif
		return 2;
	}

	while(1)																								// Endlosschleife bis der Dateiname gefunden wurde
	{
		if((Temp_Dir_Cluster > 0x0ffffff7) && (Temp_Dir_Cluster < 0x10000000))
		{
			#ifdef FAT32_Debug
				printf("FAT32: Datei \"%s\" konnte nicht geloescht werden. Datei nicht gefunden.\r\n",Filename_converted);
			#endif
			return 1;
		}

		Backup_Cluster = Temp_Dir_Cluster;																	// Speichere den letzten Cluster
		Backup_Sector = Temp_Dir_Sector;																	// Speichere den letzten Sektor
		Temp_Dir_Sector = FAT32_Get_Data_from_Sector(Temp_Dir_Cluster,Temp_Dir_Sector,&gl_SD_Card.RWbuffer[0]);		// Lade einen Sektor der Cluster
		if(Temp_Dir_Sector==0) Temp_Dir_Cluster = FAT32_Get_Next_Cluster(Temp_Dir_Cluster);							// Wenn der letzte Sektor der Cluster geladen wurde, lade die n�chste Cluster

		Next_Date = 0;																						// Next Date zuruecksetzen
		for (uint8_t g=0;g<16;g++,Next_Date+=32)															// 16 Eintraege pro FAT32 Sektor
		{
			if (memcmp(&gl_SD_Card.RWbuffer[Next_Date],&Filename_converted[0],11)==0)
			{
				gl_SD_Card.RWbuffer[Next_Date] = 0xe5;														// Loesche die Datei
				FAT32_Write_Data_to_Sector(Backup_Cluster,Backup_Sector,&gl_SD_Card.RWbuffer[0]);			// Schreibe ge�nderten Inhalt auf SD Karte

				Delete_Cluster = char_to_long_int(gl_SD_Card.RWbuffer[Next_Date+21],gl_SD_Card.RWbuffer[Next_Date+20],gl_SD_Card.RWbuffer[Next_Date+27],gl_SD_Card.RWbuffer[Next_Date+26]);		// Erster Datasektor

				if (Delete_Cluster != 0)																	// Wenn der Datei keine Sektoren zugeordnet waren, muss auch nichts geloescht werden
				{
					while(1)																				// Loesche alle FAT Eintraege der Datei
					{
						if ((Delete_Cluster > 0x0ffffff7) && (Delete_Cluster < 0x10000000))					// Wenn der Letzte Sektor gel�scht wurde, breche ab
						{
							break;
						}
						Backup_Cluster = FAT32_Get_Next_Cluster(Delete_Cluster);							// Lade den n�chsten Cluster
						FAT32_Change_Clusterentrie_in_FAT(Delete_Cluster,0);								// L�sche den alten Cluster
						Delete_Cluster = Backup_Cluster;													// Trage den n�chsten Cluster als zu l�schend ein
					}
				}

				#ifdef FAT32_Debug
					USART_Write_String("FAT32: Datei geloescht\r\n");
				#endif
				return 0;																					// Datei wurde gel�scht
			}
		}
	}
}

uint8_t FAT32_Get_Data_from_Sector_File_read(uint8_t Position_in_Table, uint32_t Cluster, uint8_t Sector, uint32_t *Read_Bytes)
{
	uint8_t Sectors_per_Cluster = gl_FAT32.Sectors_per_Cluster;
	if(Sectors_per_Cluster == 1)
	{
		SD_Card_CMD17(gl_FAT32.Root_Startaddress + ((Cluster - gl_FAT32.Root_First_Cluster) * gl_FAT32.Bytes_times_Cluster) + (Sector * gl_FAT32.Bytes_per_Sector),&gl_FAT32_File[Position_in_Table].Readcache.Buffer[0]);// Lese den Sector aus
		Sector++;
		*Read_Bytes = gl_FAT32.Bytes_per_Sector;
	}
	else
	{
		// Wenn die Sektors_per_Cluster < FAT32_Readcachebuffercount sind, wird der Cache nur bis maximal Sektors_per_Cluster*gl_FAT32.Bytes_per_Sector verwendet
		if(Sectors_per_Cluster > FAT32_Readcachebuffercount){Sectors_per_Cluster = FAT32_Readcachebuffercount;}	// Wenn der Readstreambuffercount kleiner als der Sector_per_Cluster ist, passe es an, Buffer ist 512,1024,2048,4096
		SD_Card_CMD18(gl_FAT32.Root_Startaddress + ((Cluster - gl_FAT32.Root_First_Cluster) * gl_FAT32.Bytes_times_Cluster) + (Sector * gl_FAT32.Bytes_per_Sector),Sectors_per_Cluster,&gl_FAT32_File[Position_in_Table].Readcache.Buffer[0]);// Lese den Sector aus
		Sector+=Sectors_per_Cluster;																				// Z�hle um die geladenen Sektoren weiter
		*Read_Bytes = Sectors_per_Cluster*gl_FAT32.Bytes_per_Sector;
	}
	//return Sector < gl_FAT32.Sectors_per_Cluster ? Sector : 0;													// Wenn der letzte Sektor der Cluster gelesen wurde, gebe 0=letzter Sektor der Cluster gelesen, oder >0 = N�chster zu lesender Sektor
	if (Sector < gl_FAT32.Sectors_per_Cluster) {return Sector;}														// Ist schneller
	else {return 0;}
}

uint8_t FAT32_File_Read(uint8_t Position_in_Table, char *Array, uint32_t Length)
{
	uint8_t Next_Sector=0;																							// N�chster Sektor
	uint32_t Next_Cluster=0, Pos_in_Buffer=0, Bytecount=0;																// N�chster Cluster, Readstream.Pos_in_Buffer, Readstream.Count

	if (gl_FAT32_File[Position_in_Table].Is_Open!=1)
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Lesen nicht moeglich, da die Datei nicht geoeffnet ist\r\n");
		#endif
		return 1;
	}
	
	if (gl_FAT32_File[Position_in_Table].Read_Write == FAT32_Write)
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Lesen nicht moeglich, da die Datei im Schreibmodus geoeffnet ist\r\n");
		#endif
		return 1;
	}

	if((gl_FAT32_File[Position_in_Table].Size < (Length + gl_FAT32_File[Position_in_Table].Next_Byte_Read)) || (gl_FAT32_File[Position_in_Table].Size == 0))	// Hiermit wird diese Pruefung if ((Next_Sector > 0x0ffffff7) && (Next_Sector < 0x10000000)) ueberfluessig
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Laenge des zu lesenden Bereiches ueberschreitet die Dateigroesse. Abbruch\r\n");
		#endif
		return 1;
	}

	if(Length == 0)																									// Wenn L�nge 0 dann breche ab
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Laenge = 0\r\n");
		#endif
		return 0;
	}

	// Wenn noch gen�gend Bytes im Readstrembuffer sind, dann gebe diese einfach zur�ck
	if((gl_FAT32_File[Position_in_Table].Readcache.Count-gl_FAT32_File[Position_in_Table].Readcache.Pos_in_Buffer+1) > Length)	// Pos_in_Buffer ist 0 basierend
	{
		memcpy(&Array[0],&gl_FAT32_File[Position_in_Table].Readcache.Buffer[gl_FAT32_File[Position_in_Table].Readcache.Pos_in_Buffer],Length);						// Kopiere die vorhandenen Daten in das Array
		gl_FAT32_File[Position_in_Table].Readcache.Pos_in_Buffer += Length;												// Pufferzeiger um die gelesene L�nge hochsetzen
	}
	else // Wenn nicht, lade neue Daten nach
	{
		Pos_in_Buffer = gl_FAT32_File[Position_in_Table].Readcache.Pos_in_Buffer;										// Variable lokal speichern
		Bytecount = gl_FAT32_File[Position_in_Table].Readcache.Count;													// Variable lokal speichern
		Next_Sector = gl_FAT32_File[Position_in_Table].Next_Byte_Read_Sector;											// Speichere den aktuellen Sektor in der Cluster
		Next_Cluster = gl_FAT32_File[Position_in_Table].Next_Byte_Read_Cluster;											// Speichere die aktuelle Cluster

		for (uint32_t g=0; g<Length; g++)
		{
			if (Pos_in_Buffer == Bytecount)																				// Wenn Sektor ausgelesen, muss ein neuer her
			{
				Next_Sector = FAT32_Get_Data_from_Sector_File_read(Position_in_Table,Next_Cluster,Next_Sector, &Bytecount);			// Cluster und Sector hat er beim ersten durchlauf von oben

				if(Next_Sector==0)																						// Wenn der letzte Sektor der Cluster erreicht wurde, lade eine neue Cluster
				{
					Next_Cluster = FAT32_Get_Next_Cluster(Next_Cluster);												// Hole den n�chsten Cluster
				}
				Pos_in_Buffer=0;																						// Position auf 0 setzen
			}
			Array[g] = gl_FAT32_File[Position_in_Table].Readcache.Buffer[Pos_in_Buffer++];								// Lese das Array ein
		}

		gl_FAT32_File[Position_in_Table].Readcache.Count = Bytecount;													// Speichere den Bytecount wieder ab
		gl_FAT32_File[Position_in_Table].Next_Byte_Read_Sector = Next_Sector;											// Speichere den letzen Sektor ab
		gl_FAT32_File[Position_in_Table].Next_Byte_Read_Cluster = Next_Cluster;											// Speichere die Cluster ab
		gl_FAT32_File[Position_in_Table].Readcache.Pos_in_Buffer = Pos_in_Buffer;										// Speichere Pos_in_Buffer in globale zur�ck
	}

	gl_FAT32_File[Position_in_Table].Next_Byte_Read += Length;														// Setze den Zaehler weiter hoeher

	#ifdef FAT32_Debug
		printf("FAT32: NextByteReadSector: %u NextByteReadCluster: %lu\r\nFAT32: Bytes gelesen\r\n",gl_FAT32_File[Position_in_Table].Next_Byte_Read_Sector,gl_FAT32_File[Position_in_Table].Next_Byte_Read_Cluster);
	#endif
	return 0;
}

uint8_t FAT32_File_Write(uint8_t Position_in_Table, char *Array, uint32_t Length)
{
	uint32_t Temp_Cluster=gl_FAT32_File[Position_in_Table].Next_Byte_Write_Cluster;											// Temp Cluster
	uint8_t Temp_Sector=gl_FAT32_File[Position_in_Table].Next_Byte_Write_Sector;											// Temp Sector
	uint32_t Pos_in_Buffer = gl_FAT32_File[Position_in_Table].Writecache.Pos_in_Buffer;										// Pos in Buffer als lokale Variable

	if (gl_FAT32_File[Position_in_Table].Is_Open != 1)
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Schreiben nicht moeglich, da die Datei nicht geoeffnet ist\r\n");
		#endif
		return 1;
	}

	if (gl_FAT32_File[Position_in_Table].Read_Write == FAT32_Read)
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Schreiben nicht moeglich, da die Datei im Lesemodus geoeffnet ist\r\n");
		#endif
		return 1;
	}

	for (uint32_t g=0; g<Length; g++, Pos_in_Buffer++)
	{
		if (Pos_in_Buffer == gl_FAT32_File[Position_in_Table].Writecache.Count)												// Wenn der Chace voll ist, wird es zeit die Daten wegzuschreiben
		{
			if (gl_FAT32_File[Position_in_Table].First_Clustersector == 0)													// Falls noch kein Sektor der Datei zugewiesen wurde mache das
			{
				Temp_Cluster = FAT32_Connect_First_Cluster_to_File(Position_in_Table);										// Binde den ersten Sektor an die Datei und gebe den Sektor zur�ck
				Temp_Sector = 0;																							// Fange im 0. Sektor an
			}

			uint32_t Sectors_to_Write = gl_FAT32_File[Position_in_Table].Writecache.Count/gl_FAT32.Bytes_per_Sector;		// Errechne anhand der Writecachegr��e, die Anzahl zu schreibenders Sektoren
			SD_Card_CMD25(gl_FAT32.Root_Startaddress + ((Temp_Cluster - gl_FAT32.Root_First_Cluster) * gl_FAT32.Bytes_times_Cluster) + (Temp_Sector * gl_FAT32.Bytes_per_Sector),Sectors_to_Write,gl_FAT32_File[Position_in_Table].Writecache.Buffer);

			Temp_Sector+=Sectors_to_Write;																					// Z�hle den Sektorcounter um 1 hoch
			if (Temp_Sector==gl_FAT32.Sectors_per_Cluster)																	// Wenn ein neuer Sektor geschrieben werden muss, pr�fe ob das der letzte Sektor der Cluster war
			{
				Temp_Cluster = FAT32_Connect_Empty_Cluster_to_Filescluster(Temp_Cluster,0);									// Binde eine neue Cluster an wenn es sein muss
				Temp_Sector = 0;
			}

			if (Temp_Cluster == 0x0fffffff)																					// Falls das anbinden nicht geklappt hat, breche ab
			{
				#ifdef FAT32_Debug
					USART_Write_String("FAT32: Kein Platz in der FAT. Es konnten nicht alle Bytes geschrieben werden\r\n");
				#endif
				return 1;
			}
			Pos_in_Buffer = 0;																								// Zeiger in Buffer zur�cksetzen
		}
		gl_FAT32_File[Position_in_Table].Writecache.Buffer[Pos_in_Buffer] = Array[g];										// Schreibe in die Datei
	}

	gl_FAT32_File[Position_in_Table].Next_Byte_Write += Length;																// Setze den Zaehler weiter hoeher, wird in FAT32_File_Close ben�tigt
	gl_FAT32_File[Position_in_Table].Next_Byte_Write_Cluster = Temp_Cluster;												// Speichere den letzen Sektor
	gl_FAT32_File[Position_in_Table].Next_Byte_Write_Sector = Temp_Sector;													// Speichere den letzen Sektor
	gl_FAT32_File[Position_in_Table].Writecache.Pos_in_Buffer = Pos_in_Buffer;												// Pos in Buffer zur�ckschreiben

	#ifdef FAT32_Debug
		USART_Write_String("FAT32: Bytes geschrieben\r\n");
	#endif
	return 0;
}


/*uint8_t FAT32_File_Write(uint8_t Position_in_Table, char *Array, uint32_t Length)
{
	uint16_t Bytes_per_Sector = gl_FAT32.Bytes_per_Sector,Buffercounter=0;													// Lokale Variable, Buffercounter fuer SD Karte
	uint32_t Temp_Cluster=0;																								// Temp Cluster
	uint8_t Temp_Sector=0;																									// Temp Sector

	if (gl_FAT32_File[Position_in_Table].Is_Open != 1)
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Schreiben nicht moeglich, da die Datei nicht geoeffnet ist\r\n");
		#endif
		return 1;
	}

	if (gl_FAT32_File[Position_in_Table].Read_Write == FAT32_Read)
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Schreiben nicht moeglich, da die Datei im Lesemodus geoeffnet ist\r\n");
		#endif
		return 1;
	}
	
	Temp_Cluster = gl_FAT32_File[Position_in_Table].Next_Byte_Write_Cluster;												// Temp_Cluster festlegen
	Temp_Sector = gl_FAT32_File[Position_in_Table].Next_Byte_Write_Sector;													// Temp Sector festlegen
	Buffercounter = gl_FAT32_File[Position_in_Table].Next_Byte_Write & (Bytes_per_Sector-1);								// Errechne die Startposition im Buffer, Fuege dies zusammen um die richtige Position im Writebuffer zu finden

	if (Buffercounter == 0)																									// Wenn gleich zu Beginn ein leerer Sektor benoetigt wird, dann weise einen zu
	{
		if (gl_FAT32_File[Position_in_Table].First_Clustersector == 0)														// Falls noch kein Sektor der Datei zugewiesen wurde mache das
		{
			Temp_Cluster = FAT32_Connect_First_Cluster_to_File(Position_in_Table);											// Binde den ersten Sektor an die Datei und gebe den Sektor zur�ck
			Temp_Sector = 0;																								// Fange im 0. Sektor an
		}
		else
		{
			Temp_Sector++;																									// Z�hle den Sektorcounter um 1 hoch
			if (Temp_Sector==gl_FAT32.Sectors_per_Cluster)																	// Wenn ein neuer Sektor geschrieben werden muss, pr�fe ob das der letzte Sektor der Cluster war
			{
				Temp_Cluster = FAT32_Connect_Empty_Cluster_to_Filescluster(Temp_Cluster,0);									// Binde eine neue Cluster an wenn es sein muss
				Temp_Sector = 0;
			}
		}
	}
	else																													// Ansonsten lade den letzten Sektor um diesen weiter zu f�llen
	{
		FAT32_Get_Data_from_Sector(Temp_Cluster,Temp_Sector,&gl_SD_Card.RWbuffer[0]);										// Lade den letzten Sektor um diesen weiter zu f�llen
	}

	for (uint32_t g=0; g<Length; g++, Buffercounter++)
	{
		if (Buffercounter == Bytes_per_Sector)																				// Wenn Sektor voll
		{
			FAT32_Write_Data_to_Sector(Temp_Cluster,Temp_Sector,&gl_SD_Card.RWbuffer[0]);									// Schreibe die Daten in den Sektor
			Temp_Sector++;																									// Z�hle den Sektorcounter um 1 hoch
			if (Temp_Sector==gl_FAT32.Sectors_per_Cluster)																	// Wenn ein neuer Sektor geschrieben werden muss, pr�fe ob das der letzte Sektor der Cluster war
			{
				Temp_Cluster = FAT32_Connect_Empty_Cluster_to_Filescluster(Temp_Cluster,0);									// Binde eine neue Cluster an wenn es sein muss
				Temp_Sector = 0;
			}

			if (Temp_Cluster == 0x0fffffff)																					// Falls das anbinden nicht geklappt hat, breche ab
			{
				#ifdef FAT32_Debug
					USART_Write_String("FAT32: Kein Platz in der FAT. Es konnten nicht alle Bytes geschrieben werden\r\n");
				#endif
				return 1;
			}

			Buffercounter = 0;
		}
		gl_SD_Card.RWbuffer[Buffercounter] = Array[g];																		// Schreibe in die Datei
	}

	FAT32_Write_Data_to_Sector(Temp_Cluster,Temp_Sector,&gl_SD_Card.RWbuffer[0]);											// Schreibe die Daten in den Sektor

	gl_FAT32_File[Position_in_Table].Next_Byte_Write += Length;																// Setze den Zaehler weiter hoeher
	gl_FAT32_File[Position_in_Table].Next_Byte_Write_Cluster = Temp_Cluster;												// Speichere den letzen Sektor
	gl_FAT32_File[Position_in_Table].Next_Byte_Write_Sector = Temp_Sector;													// Speichere den letzen Sektor

	#ifdef FAT32_Debug
		USART_Write_String("FAT32: Bytes geschrieben\r\n");
	#endif
	return 0;
}*/

uint8_t FAT32_Update_Entrie(uint8_t Position_in_Table,uint8_t Attribute, uint32_t Filelength)
{
	uint16_t Entrypos=0,Date=0, Time=0;																			// Byteoffset des Dateieintrags im Sektor, Datum, Zeit
	uint8_t cYear=0;																							// Jahr als char

	FAT32_Get_Data_from_Sector(gl_FAT32_File[Position_in_Table].Dir_Clustersector,gl_FAT32_File[Position_in_Table].Dir_Sector,&gl_SD_Card.RWbuffer[0]);	// Lade den Sektor der Dir Cluster
	Entrypos = gl_FAT32_File[Position_in_Table].Dir_Entrypos;													// Byteoffset in lokaler Var speichern

	#ifdef FAT32_USE_RTC
		DS1307_Read_Timestamp(&gl_Time);														// Lese Timestamp
	#endif
	cYear = (uint8_t)(gl_Time.Year - 1980);																		// Errechne den Charwert aus dem Year
	Date = cYear; Date = Date << 4; Date |= gl_Time.Month; Date = Date << 5; Date |= gl_Time.Day;				// Quetsche Year, Month und Day in eine Integer
	Time = gl_Time.Hour; Time = Time << 6; Time |= gl_Time.Minute; Time = Time << 5; Time |= (gl_Time.Seconds/2);// Das selbe mit Stunde, Minute und Sekunde

	gl_SD_Card.RWbuffer[Entrypos+11] = Attribute;																// Dateiattribute speichern
	Write_uint32_to_Little_Endian_Buffer(&gl_SD_Card.RWbuffer[Entrypos + 28],Filelength);						// Passe die Dateigroesse an
	gl_SD_Card.RWbuffer[Entrypos+18]=Date & 0xff;																// Letzer Zugriff, nur Datum
	gl_SD_Card.RWbuffer[Entrypos+19]=Date>>8;																	// Letzer Zugriff, nur Datum
	gl_SD_Card.RWbuffer[Entrypos+22]=Time & 0xff;																// aenderungszeit
	gl_SD_Card.RWbuffer[Entrypos+23]=Time>>8;																	// aenderungszeit
	gl_SD_Card.RWbuffer[Entrypos+24]=Date & 0xff;																// aenderungsdatum
	gl_SD_Card.RWbuffer[Entrypos+25]=Date>>8;																	// aenderungsdatum

	if(FAT32_Write_Data_to_Sector(gl_FAT32_File[Position_in_Table].Dir_Clustersector,gl_FAT32_File[Position_in_Table].Dir_Sector,&gl_SD_Card.RWbuffer[0])==1)	// Schreibe den aktualisierten Sektor zurueck
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Fileupdate nicht m�glich, Sektor nicht beschreibbar\r\n");
		#endif
		return 1;																								// Bei Fehler gebe 1 zur�ck
	}

	gl_FAT32_File[Position_in_Table].Size = Filelength;															// Aktualisiere auch die aktuell geladenen Datei
	gl_FAT32_File[Position_in_Table].Attributes = Attribute;													// Attribute
	gl_FAT32_File[Position_in_Table].Last_Access_Date = Date;													// Letztes Zugriffsdatum
	gl_FAT32_File[Position_in_Table].Changetime = Time;															// aenderungszeit
	gl_FAT32_File[Position_in_Table].Changedate = Date;															// aenderungsdatum

	#ifdef FAT32_Debug
		printf("Datei geupdated: %s\r\nAttribute: %u\r\nDateigroesse: %lu\r\nDatei geupdated\r\n",gl_FAT32_File[Position_in_Table].Name,gl_FAT32_File[Position_in_Table].Attributes,gl_FAT32_File[Position_in_Table].Size);
	#endif
	return 0;

}

uint8_t FAT32_Directory_List(void)
{
	uint32_t Temp_Dir_Cluster = gl_FAT32_Directory.First_Clustersector;										// Aktueller Cluster der Root, Backup des Temp Sektors
	uint16_t Next_Date=0;																					// Naeste Datei im Array +=32
	uint8_t Temp_Dir_Sector=0;																				// Aktueller Sektor der Root

	printf("FAT32: Verzeichnis %s Auflistung:\r\n",gl_FAT32_Directory.Name);
	while(1)																								// Endlosschleife bis alle Dateinamen gelesen wurden
	{
		if((Temp_Dir_Cluster > 0x0ffffff7) && (Temp_Dir_Cluster < 0x10000000))
		{
			#ifdef FAT32_Debug
			USART_Write_String("FAT32: Ordner ausgelesen\r\n");
			#endif
			USART_Write_String("\r\n");
			return 0;
		}

		Temp_Dir_Sector = FAT32_Get_Data_from_Sector(Temp_Dir_Cluster,Temp_Dir_Sector,&gl_SD_Card.RWbuffer[0]);	// Lade einen Sektor der Cluster
		if(Temp_Dir_Sector==0) Temp_Dir_Cluster = FAT32_Get_Next_Cluster(Temp_Dir_Cluster);					// Wenn der letzte Sektor der Cluster geladen wurde, lade die n�chste Cluster

		Next_Date = 0;																						// Next Date zuruecksetzen
		for (uint8_t g=0;g<16;g++,Next_Date+=32)															// 16 Eintraege pro FAT32 Sektor
		{
			if (gl_SD_Card.RWbuffer[Next_Date] != 0 && gl_SD_Card.RWbuffer[Next_Date] != 0xe5 && gl_SD_Card.RWbuffer[Next_Date] != '.' && gl_SD_Card.RWbuffer[Next_Date+11] != 0x0f)
			{
				USART_Write_String("Dateiname: ");
				USART_Write_X_Bytes(&gl_SD_Card.RWbuffer[Next_Date],0,11);
				printf(" Groesse: %lu\r\n",Get_uint32_from_Little_Endian_Buffer(&gl_SD_Card.RWbuffer[Next_Date+28]));
			}
		}
	}
}

uint8_t FAT32_Directory_List_Entry_from_Position(char *Filename, uint32_t Position_of_Entry)
{
	char temp_filename[13];
	uint32_t PoE = 0;																						// Kopiere Nummer
	uint32_t Temp_Dir_Cluster = gl_FAT32_Directory.First_Clustersector;										// Aktueller Cluster der D�r
	uint16_t Next_Date=0;																					// Naeste Datei im Array +=32
	uint8_t Temp_Dir_Sector=0;																				// Aktueller Sektor der Dir

	while(1)																								// Endlosschleife bis alle Dateinamen gelesen wurden
	{
		if((Temp_Dir_Cluster > 0x0ffffff7) && (Temp_Dir_Cluster < 0x10000000))
		{
			#ifdef FAT32_Debug
				USART_Write_String("FAT32: Ordner ausgelesen\r\n");
			#endif
			return 1;
		}

		Temp_Dir_Sector = FAT32_Get_Data_from_Sector(Temp_Dir_Cluster,Temp_Dir_Sector,&gl_SD_Card.RWbuffer[0]);	// Lade einen Sektor der Cluster
		if(Temp_Dir_Sector==0) Temp_Dir_Cluster = FAT32_Get_Next_Cluster(Temp_Dir_Cluster);					// Wenn der letzte Sektor der Cluster geladen wurde, lade die n�chste Cluster

		Next_Date = 0;																						// Next Date zuruecksetzen
		for (uint8_t g=0;g<16;g++,Next_Date+=32)															// 16 Eintraege pro FAT32 Sektor
		{
			if (gl_SD_Card.RWbuffer[Next_Date] != 0 && gl_SD_Card.RWbuffer[Next_Date] != 0xe5 && gl_SD_Card.RWbuffer[Next_Date] != '.' && gl_SD_Card.RWbuffer[Next_Date+11] != 0x0f)
			{
				if (PoE == Position_of_Entry)																// Wenn die Position stimmt schreibe diesen in Filename
				{
					memcpy(&temp_filename[0],&gl_SD_Card.RWbuffer[Next_Date],11);
					temp_filename[11]=0;																	// Stringterminator
					Convert_8_3_to_string(&Filename[0],&temp_filename[0]);									// Wandle 8_3 in Dateiname mit . um
					return 0;
				}
				PoE++;
			}
		}
	}
}

uint8_t FAT32_File_Check_If_Open(char *Filename)
{
	char Filename_converted[12];
	Convert_String_to_8_3(&Filename_converted[0], &Filename[0]);
	for (uint8_t g=0;g<FAT32_Max_Entries;g++)
	{
		if ((memcmp(&gl_FAT32_File[g].Name[0],&Filename_converted[0],11)==0) && gl_FAT32_File[g].Is_Open)
		{
			return 0;
		}
	}
	return 1;
}

uint8_t FAT32_File_Directory_Rename(char *Filename_old, char *Filename_new)
{
	char Filename_converted_old[12], Filename_converted_new[12];											// Konvertierter Dateiname alt und neu
	uint32_t Temp_Dir_Cluster = gl_FAT32_Directory.First_Clustersector, Backup_Cluster=0;					// Aktueller Cluster der Dir, Backup
	uint16_t Next_Date=0;																					// Naeste Datei im Array +=32
	uint8_t Temp_Dir_Sector=0, Backup_Sector;																// Aktueller Sektor der Dir, Backup

	Convert_String_to_8_3(Filename_converted_old, Filename_old);											// Konvertiere Dateistring
	Convert_String_to_8_3(Filename_converted_new, Filename_new);											// Konvertiere Dateistring

	while(1)																								// Endlosschleife bis der Dateiname gefunden wurde
	{
		if((Temp_Dir_Cluster > 0x0ffffff7) && (Temp_Dir_Cluster < 0x10000000))
		{
			#ifdef FAT32_Debug
				printf("FAT32: Datei \"%s\" wurde nicht gefunden\r\n",Filename_converted_old);
			#endif
			return 1;
		}
		Backup_Cluster = Temp_Dir_Cluster;																	// Speichere den letzten Cluster zwischen
		Backup_Sector = Temp_Dir_Sector;																	// Speichere den letzten Sektor zwischen
		Temp_Dir_Sector = FAT32_Get_Data_from_Sector(Temp_Dir_Cluster,Temp_Dir_Sector,&gl_SD_Card.RWbuffer[0]);	// Lade einen Sektor der Cluster
		if(Temp_Dir_Sector==0) Temp_Dir_Cluster = FAT32_Get_Next_Cluster(Temp_Dir_Cluster);					// Wenn der letzte Sektor der Cluster geladen wurde, lade die n�chste Cluster

		Next_Date = 0;																						// Next Date zuruecksetzen
		for (uint8_t g=0;g<16;g++,Next_Date+=32)															// 16 Eintraege pro FAT32 Sektor
		{
			if (memcmp(&gl_SD_Card.RWbuffer[Next_Date],&Filename_converted_old[0],11)==0)
			{
				memcpy(&gl_SD_Card.RWbuffer[Next_Date],&Filename_converted_new[0],11);						// Neuen Dateinamen eintragen
				if(FAT32_Write_Data_to_Sector(Backup_Cluster,Backup_Sector,&gl_SD_Card.RWbuffer[0])==0)		// Schreibe den aktualisierten Sector zurueck
				{
					#ifdef FAT32_Debug
						printf("FAT32: Datei von %s nach %s umbenannt\r\n",Filename_old,Filename_new);
					#endif
					return 0;
				}
				else
				{
					#ifdef FAT32_Debug
						printf("FAT32: Datei %s konnte nicht nach %s umbenannt werden\r\n",Filename_old,Filename_new);
					#endif
					return 1;
				}
			}
		}
	}
}

void FAT32_Convert_Filedate(uint8_t Position_in_Table, struct Timestamp *Timestamp_file)
{
	uint16_t Time = gl_FAT32_File[Position_in_Table].Changetime;
	uint16_t Date = gl_FAT32_File[Position_in_Table].Changedate;
	
	Timestamp_file->Seconds = (Time & 0b00011111)*2;
	Time = Time >> 5;
	Timestamp_file->Minute = Time & 0b00111111;
	Time = Time >> 6;
	Timestamp_file->Hour = Time & 0b00011111;
	
	Timestamp_file->Day = Date & 0b00011111;
	Date = Date >> 5;
	Timestamp_file->Month = Date & 0b00001111;
	Date = Date >> 4;
	Timestamp_file->Year = (Date & 0b01111111)+1980;
}

uint8_t FAT32_File_Read_Line(uint8_t Position_in_Table, char *Array, uint32_t Max)
{
	uint8_t Next_Sector=0;																							// N�chster Sektor
	uint32_t Next_Cluster=0,Bytecounter=0;																			// N�chster Cluster, Anzahl der gelesenen Bytes
	uint32_t Bytecount=0, Pos_in_Buffer=0;																			// Buffercounter fuer SD Karte, Zeiger f�r Readcache

	if (gl_FAT32_File[Position_in_Table].Is_Open!=1)
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Lesen nicht moeglich, da die Datei nicht geoeffnet ist\r\n");
		#endif
		return 1;
	}

	if (gl_FAT32_File[Position_in_Table].Read_Write == FAT32_Write)
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Lesen nicht moeglich, da die Datei im Schreibmodus geoeffnet ist\r\n");
		#endif
		return 1;
	}

	if((gl_FAT32_File[Position_in_Table].Size == gl_FAT32_File[Position_in_Table].Next_Byte_Read) || (gl_FAT32_File[Position_in_Table].Size == 0))					// Hiermit wird diese Pruefung if ((Next_Sector > 0x0ffffff7) && (Next_Sector < 0x10000000)) ueberfluessig
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Dateiende erreicht, Zeilenlesen nicht moeglich. Abbruch\r\n");
		#endif
		return 1;
	}


	Pos_in_Buffer = gl_FAT32_File[Position_in_Table].Readcache.Pos_in_Buffer;										// Variable lokal speichern
	Bytecount = gl_FAT32_File[Position_in_Table].Readcache.Count;													// Variable lokal speichern
	Next_Sector = gl_FAT32_File[Position_in_Table].Next_Byte_Read_Sector;											// Speichere den aktuellen Sektor in der Cluster
	Next_Cluster = gl_FAT32_File[Position_in_Table].Next_Byte_Read_Cluster;											// Speichere die aktuelle Cluster

	uint32_t g=0;
	for (; g<(Max-1); g++)
	{
		if (Pos_in_Buffer == Bytecount)																				// Wenn Sektor ausgelesen, muss ein neuer her
		{
			Next_Sector = FAT32_Get_Data_from_Sector_File_read(Position_in_Table,Next_Cluster,Next_Sector, &Bytecount);			// Cluster und Sector hat er beim ersten durchlauf von oben

			if(Next_Sector==0)																						// Wenn der letzte Sektor der Cluster erreicht wurde, lade eine neue Cluster
			{
				Next_Cluster = FAT32_Get_Next_Cluster(Next_Cluster);												// Hole den n�chsten Cluster
			}
			Pos_in_Buffer=0;																						// Position auf 0 setzen
		}
		Array[g] = gl_FAT32_File[Position_in_Table].Readcache.Buffer[Pos_in_Buffer++];								// Lese das Array ein
		Bytecounter++;																								// Anzahl der gelesenen Bytes hochz�hlen

		if (Array[g] == '\n')																						// Wenn der Zeilenumbruch gefunden wurde breche die Schleife ab
		{
			if (Array[g-1] == '\r')																					// Bei \r\n wird bei \r abgeschnitten sonst \n
			{
				Array[g-1]=0;																						// Stringterminierung eintragen
			}
			else
			{
				Array[g]=0;																							// Stringterminierung eintragen
			}
			break;
		}

		if(gl_FAT32_File[Position_in_Table].Size == Bytecounter)													// Dateiende erreicht
		{
			#ifdef FAT32_Debug
				USART_Write_String("FAT32: Zeile lesen abgebrochen, Dateiende erreicht\r\n");
			#endif
			Array[0]=0;																								// String zur�cksetzen
			gl_FAT32_File[Position_in_Table].Next_Byte_Read = gl_FAT32_File[Position_in_Table].Size;				// Dateiende erreicht
			return 2;
		}
	}

	gl_FAT32_File[Position_in_Table].Readcache.Count = Bytecount;													// Speichere den Bytecount wieder ab
	gl_FAT32_File[Position_in_Table].Next_Byte_Read_Sector = Next_Sector;											// Speichere den letzen Sektor ab
	gl_FAT32_File[Position_in_Table].Next_Byte_Read_Cluster = Next_Cluster;											// Speichere die Cluster ab
	gl_FAT32_File[Position_in_Table].Readcache.Pos_in_Buffer = Pos_in_Buffer;										// Speichere Pos_in_Buffer in globale zur�ck

	gl_FAT32_File[Position_in_Table].Next_Byte_Read += Bytecounter;													// Trage die lokale wieder in die globale ein

	if(g==(Max-1))
	{
		#ifdef FAT32_Debug
			USART_Write_String("FAT32: Maximale Zeichen bei Zeilenlesen erreicht\r\n");
		#endif
		Array[0]=0;																									// String zur�cksetzen
		return 2;
	}

	#ifdef FAT32_Debug
		USART_Write_String("FAT32: Zeile gelesen\r\n");
	#endif
	return 0;
}
