#ifndef FAT32_H_
#define FAT32_H_

// Includes
#include "time.h"

/*

Es wird nur eine Partition unterstuetzt und die Sektorgroesse muss 512 Bytes betragen
Dateinamen muessen sich im 8.3 Format befinden. Kann mit "Test.txt" oder "TEST    TXT" aufgerufen werden
Die SD Karte muss eine Blockgr��e von 512 Bytes haben, �ltere Karten die kleinere Blockgr��en besitzen k�nnen, werden nicht unterst�tzt

************************************************************************************************
* FAT32 Bibliothek fuer SD-Karte ueber den SPI auf dem STM32F401RET6						   *
* 2019 - 2020 � Frederinn														   	   *
************************************************************************************************

*/

// Defines
//#define		FAT32_Debug											// Nur fuer Debug
//#define		FAT32_USE_RTC										// Wenn an, wird aktiv die Zeit aus der RTC ausgelesen; Wenn auskommentiert, wird der letzte Zeitstempel aus gl_Time verwendet
#define			FAT32_Max_Entries			13						// Maximale Dateieintr�ge

// Readcache Einstellungen
#define			FAT32_Readcachesize 		2048					// 512,1024,2048,4096
#define			FAT32_Readcachebuffercount  4						// FAT32_Readstreambuffersize/512

// Writecache Einstellungen
#define			FAT32_Writecachesize	 	512						// 512,1024,2048,4096
#define			FAT32_Writecachebuffercount 1						// FAT32_Writecachesize/512


// Nicht �ndern
#define			FAT32_Read					0x00					// Read
#define			FAT32_Write					0x01					// Write

// Structures
struct SD_FILE
{
	char Name[12];													// Dateiname
	uint8_t Attributes;												// Dateiattribute
	uint16_t Creation_Time;											// Erstellzeit
	uint16_t Creation_Date;											// Erstelldatum
	uint16_t Last_Access_Date;										// Letztes Zugriffsdatum, gleiches Datum wie in Changedate
	uint16_t Changetime;											// Modifikationszeit
	uint16_t Changedate;											// Modifikationsdatum
	uint32_t Size;													// Dateigroesse
	uint32_t First_Clustersector;									// Addresse des ersten Datensector der ersten Cluster der Datei in der FAT
	uint32_t Dir_Clustersector;										// Cluster der Directory in der die Datei steht
	uint8_t Dir_Sector;												// Sektor der Directory in der die Datei steht
	uint16_t Dir_Entrypos;											// Byteoffset der Eintragsposition der Datei in Sector_Dir
	uint32_t Next_Byte_Read;										// Naechstes zu lesendes Byte
	uint32_t Next_Byte_Read_Cluster;								// Cluster in dem sich der Sektor des zu lesenden Bytes befindet
	uint8_t Next_Byte_Read_Sector;									// Sektor in dem sich dieses Byte befindet
	uint32_t Next_Byte_Write;										// Naechstes zu schreibende Byte
	uint32_t Next_Byte_Write_Cluster;								// Cluster in dem sich der Sektor des zu schreiben Bytes befindet
	uint8_t Next_Byte_Write_Sector;									// Sektor des zu naechsten schreiben Bytes
	uint8_t Is_Open;												// Datei ist geoeffnet
	uint8_t Read_Write;												// Lese oder Schreibzugriff
	struct R_Cache													// Der Lesecache beschleunigt das lesen der Dateien von der SD Karte; Optimale Ausnutzung des Buffers, wenn die Cluster auf der Karte == R_Cache.Buffer ist
	{
		char Buffer[FAT32_Readcachesize];							// 512,1024,2048,4096,8192; FAT interner Puffer f�r lesedaten um diese nicht immer wieder laden zu m�ssen, nicht f�r FAT32_File_Read verwenden
		uint32_t Pos_in_Buffer;										// Position des n�chsten zu lesenden Bytes
		uint32_t Count;												// Anzahl der gelesen Bytes im Puffer
	}Readcache;

	struct W_Cache													// Der Writecache beschleunigt das schreiben der Dateien auf die SD Karte; Optimale Ausnutzung des Buffers, wenn die Cluster auf der Karte == W_Cache.Buffer ist
	{
		char Buffer[FAT32_Writecachesize];							// 512,1024,2048,4096,8192; FAT interner Puffer f�r Schreibdaten um diese nicht immer einzeln schreiben zu m�ssen, nicht f�r FAT32_File_Write verwenden
		uint32_t Pos_in_Buffer;										// Position des n�chsten zu schreibenden Bytes
		uint32_t Count;												// Anzahl der zur schreibenden Bytes im Puffer
	}Writecache;
};

struct FAT32
{
	uint16_t Bytes_per_Sector;										// Bytes per Sector, normalerweise 512
	uint8_t Sectors_per_Cluster;									// Anzahl von Sektoren pro Cluster
	uint32_t Bytes_times_Cluster;									// Hilfsvariable um Rechenzeit zu sparen
	uint16_t Reserved_Sectors;										// Anzahl der reservierten Sectoren
	uint8_t Coppies_of_FAT;											// Anzahl der Kopien dieser FAT
	uint16_t Sectors_per_FAT;										// Anzahl der Sektoren pro Fat
	uint32_t Root_First_Cluster;									// Clusternummer in der die Root steht
	uint32_t Root_Startaddress;										// Startaddresse der Root auf der Karte
	uint32_t First_FAT_Address;										// Adresse des ersten FAT Sektors auf der Karte

	char FAT_Buffer[512];											// Geladener Sektor der aktuell geladenen FAT
	uint32_t Loaded_FAT_Sector;										// Geladene Fat Sektoraddresse
	uint32_t Last_Found_Clustercounter;								// Z�hler f�r die Ermittlung des zuletzt gefundenen freien Cluster
	uint32_t Last_Found_Clustercounter_times_Bytes_per_Sector;		// Last_Found_Clustercounter * Bytes_per_Sector;
	uint8_t Mounted;												// Fat ist eingebunden
};

//Globale Variablen
extern struct SD_FILE gl_FAT32_File[FAT32_Max_Entries];				// Globale fuer eine Datei
extern struct SD_FILE gl_Directory;									// Globale fuer Verzeichnis
extern struct FAT32 gl_FAT32;										// Globale fuer FAT32

//Funktionen
extern uint8_t FAT32_Init(void);																			// Bindet die FAT32 ein, liest diesen in den gl_SD_Card.RWbuffer; Wenn erfolgreich eingebunden gibts 0 zurueck, sonst 1
extern uint8_t FAT32_Directory_Create(char *Filename, uint8_t Attribute);									// Erstelle ein Verzeichnis unter dem aktuellen Pfad, beim Attribut wird immer das Dir-Bit gesetzt, gibt 1 bei Fehler zurueck sonst 0
extern uint8_t FAT32_Directory_Change(const char *Directorystring);											// Wechselt zu einem bestimmten Verzeichnis. Bei Erfolg gibts 0 zurueck sonst 1. Mit einem "/" kommt man in die Root, sonst "/hallo/welt/das/ist/ein/test/" wobei "test" der aufzurufende Ordner ist
extern uint8_t FAT32_Directory_List(void);																	// Listet die Dateien des aktuellen Ordners ueber die Serielle Schnittstelle auf; 1 = Fehler, 0 = Erfolgreich
extern uint8_t FAT32_File_Delete(char *Filename);															// Loesche die Datei. 0=erfolgreich, 1=nicht gefunden, 2=wird noch verwendet
extern uint8_t FAT32_File_Close(uint8_t Position_in_Table);													// Datei schliessen
extern uint8_t FAT32_File_Create(char *Filename, uint8_t Attribute);										// Erstellt eine Datei im 8.3 Format; Bei Erfolg 0 zurueck, sonst 1; Attribute 0b Reserved Device Archive Subdir Volume System Hidden ReadOnly
extern uint8_t FAT32_File_Open(uint8_t Position_in_Table, char *Filename, uint8_t Read_Write);				// Oeffnet eine Datei im 8.3 Format; Bei Erfolg 0 zurueck, 1=es ist breits eine Datei auf die Position ge�ffnet, 2=Datei nicht gefunden
extern uint8_t FAT32_File_Read(uint8_t Position_in_Table, char *Array, uint32_t Length);					// Lese X Bytes in das angegebene Array
extern uint8_t FAT32_File_Write(uint8_t Position_in_Table, char *Array, uint32_t Length);					// Schreibt X Bytes in das angegebene Array
extern uint8_t FAT32_Update_Entrie(uint8_t Position_in_Table, uint8_t Attribute, uint32_t Filelength);		// Updated den Eintrag in der Directory Table
extern uint8_t FAT32_File_Check_if_Exist(char *Filename);													// 0=gefunden, 1=nicht
extern uint8_t FAT32_File_Check_If_Open(char *Filename);													// 0=Datei ist ge�ffnet, 1=Datei ist geschlossen
extern uint8_t FAT32_File_Directory_Rename(char *Filename_old, char *Filename_new);							// 0=Datei erfolgreich umgenannt, 1=Datei nicht gefunden
extern uint8_t FAT32_Directory_List_Entry_from_Position(char *Filename, uint32_t Position_of_Entry);		// Gibt den Dateiname einer bestimmen Datei im aktuellen Verzeichnis aus; 0=Position gefunden, 1=Position nicht gefunden
extern uint8_t FAT32_File_Read_Line(uint8_t Position_in_Table, char *Array, uint32_t Max);					// Liest eine Zeile aus der Datei aus, return 0=Zeile gelesen; 1=Dateiende  erreicht, oder im schreibmodus ge�ffnet; 2=Zeilenende oder Dateiende erreicht bevor \r\n gefunden wurde


extern uint8_t FAT32_Get_Data_from_Sector(uint32_t Cluster, uint8_t Sector, char *Buffer);					// Liest den Sektor aus dem Cluster in den Buffer und gibt den naechsten Sektor zurueck
extern uint8_t FAT32_Write_Data_to_Sector(uint32_t Cluster, uint8_t Sector, char *Buffer);					// Schreibt Daten in einen Sektor einer Cluster aus dem Buffer. Gibt 0 bei Erfolg zurueck sonst 1
extern uint32_t FAT32_Get_Next_Cluster(uint32_t Cluster);													// Gebe den naechsten Sektor der Datei wieder ohne die Daten zu lesen
extern uint32_t FAT32_Get_Next_Free_Cluster(uint32_t Clear_Cluster);										// Sucht die n�chste frei Cluster in der FAT und markiert diesen mit 0x0fffffff, return = Addresse des naechsten freien Cluster zurueck, ansonsten 0x0fffffff
extern void FAT32_Change_Clusterentrie_in_FAT(uint32_t Sector, uint32_t Value);								// Aendert den Wert eines Cluster in der FAT Tabelle
extern uint32_t FAT32_Connect_Empty_Cluster_to_Filescluster(uint32_t Filecluster, uint32_t Clear_Cluster);	// Haengt einen leeren CLuster an den letzen Dateicluster dran, gibt bei Erfolg den leeren Cluster zurueck sonst 0x0fffffff
extern uint32_t FAT32_Connect_First_Cluster_to_File(uint8_t Position_in_Table);								// Verbindet den ersten Cluster an die Datei
extern uint32_t FAT32_Get_Last_Cluster(uint32_t Filecluster);												// Gibt den letzten Cluster einer Datei oder Directory wieder
extern void FAT32_Convert_Filedate(uint8_t Position_in_Table, struct Timestamp *Timestamp_file);			// Wandelt das komprimierte �nderungsdatum in einen Zeitstempel um

extern uint8_t FAT32_Get_Data_from_Sector_File_read(uint8_t Position_in_Table, uint32_t Cluster, uint8_t Sector, uint32_t *Read_Bytes);// Testversion der urspr�nglichen Funktion im Update f�r den Readstreambuffer; Read_Bytes ist der R�ckgabewert der Anzahl gelesener Bytes







#endif /* FAT32_H_ */
