#ifndef WAV_H_
#define WAV_H_

/*

***************************************************************************************************
* Kleiner WAV Player, spielt Mono wie Stero Sounds �ber einen DAC oder PWM DAC	 				  *
* �ber den Tiefpass an PB6 wird das PWM Signal erzeugt und gefiltert. 							  *
* TIM4 als 8Bit PWM und TIM9 als Sampler								  						  *
* 2019 � Frederinn																  		  *
***************************************************************************************************

*/

// Defines
//#define WAV_Debug											// Debugoutput f�r WAV

//#define WAV_PWM											// Wenn definiert wird die 8Bit PWM verwendet
#define WAV_DAC												// Wenn definiert wird der interne DAC verwendet
#define WAV_Filenumber			6							// Filenumber f�r die WAV Datei in der FAT32 Tabelle
#define WAV_Buffsize			16384						// Buffergr��e

// Structures
struct WAV
{
	volatile uint8_t Position_in_Table;						// Position in Table
	volatile uint32_t Bytecounter;							// Bytecounter
	char DATA_Buffer[2][WAV_Buffsize];						// Doppelbuffer f�r Data
	volatile uint8_t Togglebit;								// Togglebit f�r Buffer
	volatile uint8_t Data_read;								// Datenpaket aus Datei gelesen
	volatile uint16_t Counter;								// Counter f�r Bufferladen
	volatile float Volume;									// Lautst�rke in %
	struct Fileformat
	{
		uint32_t ChunkID;									// Chunk ID "RIFF" (little-endian), "RIFX" (big-endian)
		uint32_t ChunkSize;									// 36 + Subchunk2ID
		uint32_t Format;									// "WAVE"
		uint32_t Subchunk1ID;								// "fmt "
		uint32_t Subchunk1Size;								// 16 f�r PCM
		uint16_t AudioFormat;								// 1=PCM
		uint16_t NumChannels;								// 1=Mono, 2=Stero
		uint32_t Samplerate;								// 8000, 16000, 32000
		uint32_t ByteRate;									// SampleRate * NumChannels * BitsPerSample/8
		uint16_t BlockAlign;								// Anzahl an Bytes die f�r ein Sample f�r alle Kan�le ben�tigt wird
		uint16_t BitsPerSample;								// 8bits = 8, 16bits = 16, usw.
		uint32_t Subchunk2ID;								// "data"
		uint32_t Subchunk2Size;								// NumSamples * NumChannels * BitsPerSample/8
		//char *Data;										// Sounddaten
	}File;
};
	
//Globale Variablen
extern struct WAV gl_WAV;									// Globale Struct f�r WAV

//Funktionen
extern void WAV_Init(void);														// Init WAV
extern uint8_t WAV_Play_File(uint8_t Position_in_Table, char *Filename);		// Gibt eine wav Datei wieder, return 1=Datei nicht gefunden, 2=Falsches Format, 0=OK
extern void WAV_Stop(void);														// Stopt die Wiedergabe
extern void WAV_Pause(void);													// Pausiert die Wiedergabe
extern void WAV_Continue(void);													// Setzt die Wiedergabe fort
extern void WAV_Volume(float Volume);											// Lautst�rke setzen
extern void WAV_Read_Filedata(void);											// Liest die n�chsten Byte der WAV Datei aus, muss in der Main-Loop aufgerufen werden

#endif
