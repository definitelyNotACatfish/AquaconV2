#ifndef SD_H_
#define SD_H_

// Includes
#include <stm32f4xx.h>
#include <string.h>

/*

************************************************************************************************
* Header fuer das Ansteuern einer SD-Karte ueber den SPI auf dem STM32F401RET6				   *
* 2019 - 2020 � Frederinn													 	  	   *
************************************************************************************************

*/

// Defines
//#define SD_Debug													// Nur fuer Debug notwendig
//#define Use_Carddetect											// Wenn definiert wird der Carddetectpin verwendet

#define SD_RCC_IOPENR		RCC_AHB1ENR_GPIOEEN						// Clock enable f�r GPIO
#define SD_GPIO				GPIOE									// An welchem Port sind die Pins angeschlossen
#define SD_DIR				SD_GPIO->MODER							// Datadirection Port an dem alle SD Pins ausser MOSI MISO SCK angeschlossen sind
#define SD_OUT				SD_GPIO->ODR							// Output
#define SD_IN				SD_GPIO->IDR							// Input
#define SD_CS				5										// Chipselect Karte
#ifdef Use_Carddetect
	#define CARD_SD			3										// Carddetect
#endif
#define SD_MBR_address		0x00000000								// Addresse der MBR
#define SD_SPI_Max_Clock	25000000UL								// Maximale SPI Clock

// Globales Struct
struct SD_Card
{
	uint8_t Initstate;												// 1=Initialisiert, 0=nicht
	uint8_t isHCXD;													// 1=HCXD, 0=SD
	char RWbuffer[512];												// Schreib-/Lesespeicher 1 Sektor gross
};

struct MBR
{
	struct Partition
	{
		uint16_t Partition_Entryaddress;							// In der ersten Partition 0x1be
		uint8_t Active_Partition;									// 0x80 = aktiv, 0x00 = inaktiv
		uint8_t Partition_Type;										// 0x0b = FAT32, 0x0c = FAT32
		uint32_t Partition_Startaddress;							// Anzahl an Sektoren zwischen MBR und Partion * 512 Bytes
		uint32_t Sectors_in_Partition;								// Groesse der FAT in Sektoren
		uint16_t Boot_Record_Signature;								// Immer 0x55AA
	}Partition1;
};

// Globale Variablen
extern struct SD_Card gl_SD_Card;									// Struct fuer alle globalen Variablen der SD Karte
extern struct MBR gl_SD_MBR;										// MBR einer SD Karte in FAT32 Format

// Funktionen
extern void SD_Portinit(void);										// Ausgaenge setzen
#ifdef Use_Carddetect
extern uint8_t SD_Card_Check_if_present(void);						// Prueft ob die Karte noch eingesteckt ist
#endif
extern uint8_t SD_Card_mount(void);									// Bindet die SD Karte ein 0=erfolgreich, 1=nicht
extern uint8_t SD_Card_read_MBR(void);								// Lese den MBR aus, bei Lesefehler 1 zurueck, sonst 0
extern uint8_t SD_Card_CMD24(uint32_t Address, char *Buffer);		// Schreibe einen Block; return 1=fehlerhaft, 0=erfolgreich; Buffer = Buffer oder Array von das geschrieben werden soll
extern uint8_t SD_Card_CMD25(uint32_t Address, uint32_t Sectorcount, char *Buffer); // Schreibt mehrere Bl�cke auf die SD Karte, return 1=fehlerhaft, 0=erfolgreich; Sectorcount = Anzahl der Sektoren; Buffer = Buffer oder Array von dem geschrieben werden soll
extern uint8_t SD_Card_CMD17(uint32_t Address, char *Buffer);		// Lese einen Block; return 1=fehler, 0=erfolgreich; Buffer = Buffer oder Array in das geschrieben werden soll
extern uint8_t SD_Card_CMD18(uint32_t Address,uint8_t Sectorcount, char *Buffer);// Liest mehrere Sektoren der SD-Karte aus; return 1=Fehler, 0=erfolgreich

extern uint8_t SD_Card_CMD58(void);									// Liest OCR Register aus, 1=Fehler, 0=Keine HCXD Karte, 2=HCXD Karte
extern uint8_t SD_Card_CMD0(void);									// SD Karte Reseten 0=erfolgreich,1=fehler
extern uint8_t SD_Card_CMD8(void);									// Frage an SD Karte, ob diese mit der Versorgungsspannung arbeiten kann, 0=ok, 1=Keine Antwort, oder SDV1
extern uint8_t SD_Card_CMD55(void);									// Wird f�r die ACMDs ben�tigt, 0=erfolgreich,1=Fehler
extern uint8_t SD_Card_ACMD41(void);								// SD Karte initialisieren 0=erfolgreich,1=fehler
extern uint8_t SD_Card_CMD1(void);									// Intialisiert die SD-Karte 5 Versuche fuer die Init. 1=Erfolglos, 0=Erfolgreich
extern uint8_t SD_Card_CMD16 (uint32_t Blocklength);				// Blockl�nge setzen, geht nicht bei XD/HC Karten return 1=fehlgeschlagen, 0=erfolgreich

// Inlined Funktionen
static inline void SD_Card_clear_RWbuffer(void) __attribute__((always_inline));				// SD Readbuffer leeren

void SD_Card_clear_RWbuffer(void)
{
	memset(&gl_SD_Card.RWbuffer[0],0x00,512);												// Readbuffer loeschen
}

#endif /* SD_H_ */
