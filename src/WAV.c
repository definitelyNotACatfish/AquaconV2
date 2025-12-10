// Includes
#include "main.h"
#include <stm32f4xx.h>
#include "WAV.h"
#include "FAT32.h"
#include "spi.h"
#include "allerlei.h"
#ifdef WAV_Debug
	#include <stdio.h>
	#include "USART.h"
#endif

/*

***************************************************************************************************
* Kleiner WAV Player, spielt Mono wie Stero Sounds �ber einen DAC oder PWM DAC	 				  *
* �ber den Tiefpass an PB6 wird das PWM Signal erzeugt und gefiltert. 							  *
* TIM4 als 8Bit PWM und TIM9 als Sampler								  						  *
* 2019 � Frederinn																  		  *
***************************************************************************************************

*/

//Globale Variablen
struct WAV gl_WAV;																	// Globale Struct f�r WAV

// Funktionen
void WAV_Init(void)
{
	gl_WAV.Volume = 0.15;															// 15%
	gl_WAV.Data_read=1;																// Auf 1, damit in der main nicht spontan der Loop zum Laden loslegt
	
	#ifdef WAV_PWM
		RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;											// TIM4 Clock an
	#endif
	RCC->APB2ENR |= RCC_APB2ENR_TIM9EN;												// TIM9 Clock an

	// Starte den Timer f�r die 32000Hz Samplerate (TIM9)
	TIM9->CR1 = TIM_CR1_DIR;														// Downcounter
	TIM9->PSC = 0;																	// Teiler = PSC + 1
	//TIM9->ARR = 2624;																// 32000Hz (16000 Hz = 5248)
	TIM9->DIER = TIM_DIER_UIE;														// Updateinterrupt an

	#ifdef WAV_PWM
		// 	Starte den Timer f�r die Ton-PWM (TIM4)
		RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;										// Port A Clock an
		GPIOB->MODER = (GPIOB->MODER & ~(0b11 << (6*2))) | (0b10 << (6*2));			// Maskiere die alte Pinfunktion raus und setze auf alternate function
		GPIOB->OSPEEDR = (GPIOB->OSPEEDR & ~(0b11 << (6*2))) | (0b10 << (6*2));		// High Speed
		GPIOB->AFR[0] |= (2 << (6*4));												// Alternate function im AFR Register pro Pin eintragen
		TIM4->CR1 = 0;																// Upcounter, Edge Aligned
		TIM4->PSC = 0;																// Teiler = PSC + 1
		TIM4->ARR = 255;															// 330kHz	R:333, C:0.47�F
		TIM4->CCMR1 |= (0b110 << 4)|(1<<3);											// PWM1 Mode, Preload enable
		TIM4->CCER |= TIM_CCER_CC1E;												// Output f�r PWM an PB6 an
		TIM4->EGR |= TIM_EGR_UG;													// Preload der Register erzwingen
	#endif

	#ifdef WAV_DAC
		// Konfigurieren den internen 12 Bit DAC
		RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;										// Port A Clock an
		GPIOA->MODER = (GPIOA->MODER & ~(0b11 << (4*2))) | (0b11 << (4*2));			// Maskiere die alte Pinfunktion raus und setze auf Analogmodus
		RCC->APB1ENR |= RCC_APB1ENR_DACEN;											// DAC Clock an
		DAC->CR |= DAC_CR_EN1;														// DAC 1 anschalten
	#endif

	NVIC_SetPriority(TIM1_BRK_TIM9_IRQn,NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));				// Prio einstellen
	NVIC_EnableIRQ(TIM1_BRK_TIM9_IRQn);												// IRQ f�r TIM9 aktivieren
}

uint8_t WAV_Play_File(uint8_t Position_in_Table, char *Filename)
{
	char Fileheader[36];

	FAT32_File_Close(gl_WAV.Position_in_Table);										// Falls vorher schon eine Datei gespielt wurde, schlie�e sie
	if(FAT32_File_Open(Position_in_Table,Filename,FAT32_Read))						// �ffne die WAV Datei
	{
		#ifdef WAV_Debug
			printf("WAV: Datei %s nicht gefunden",Filename);
		#endif
		WAV_Stop();
		return 1;
	}

	FAT32_File_Read(Position_in_Table,&Fileheader[0],36);														// Lese den Dateiheader aus, ARM verwendet wie AVR Little Endian anscheinend
	gl_WAV.File.ChunkID = char_to_long_int(Fileheader[3],Fileheader[2],Fileheader[1],Fileheader[0]); 			// 0x52494646 = RIFF
	gl_WAV.File.ChunkSize = char_to_long_int(Fileheader[7],Fileheader[6],Fileheader[5],Fileheader[4]);  		// Little Endian
	gl_WAV.File.Format = char_to_long_int(Fileheader[11],Fileheader[10],Fileheader[9],Fileheader[8]); 			// 0x57415645 = WAVE
	gl_WAV.File.Subchunk1ID = char_to_long_int(Fileheader[15],Fileheader[14],Fileheader[13],Fileheader[12]); 	// 0x666d7420 = "fmt "
	gl_WAV.File.Subchunk1Size = char_to_long_int(Fileheader[19],Fileheader[18],Fileheader[17],Fileheader[16]); 	// Little Endian, 16 = PCM
	gl_WAV.File.AudioFormat = (Fileheader[21]<<8) | Fileheader[20];												// 1 = PCM
	gl_WAV.File.NumChannels = (Fileheader[23]<<8) | Fileheader[22];												// 1 = Mono, 2 = Stereo
	gl_WAV.File.Samplerate = char_to_long_int(Fileheader[27],Fileheader[26],Fileheader[25],Fileheader[24]);		// 8000, 16000, 32000 usw..
	gl_WAV.File.ByteRate = char_to_long_int(Fileheader[31],Fileheader[30],Fileheader[29],Fileheader[28]);			//
	gl_WAV.File.BlockAlign = (Fileheader[33]<<8) | Fileheader[32];												//
	gl_WAV.File.BitsPerSample = (Fileheader[35]<<8) | Fileheader[34];												// 8 = 8Bits, 16 = 16 Bits

	#ifdef WAV_Debug
		USART_Write_String("WAV: Fileheader\r\n");
		USART_Write_X_Bytes(&Fileheader[0],0,36);
		USART_Write_String("\r\nChunkID: ");
		gl_char_to_uint32.Value = gl_WAV.File.ChunkID;
		USART_Write_X_Bytes(&gl_char_to_uint32.arr[0],0,4); USART_Write_String("\r\n");
		printf("Chunksize: %lu\r\n",gl_WAV.File.ChunkSize);
		USART_Write_String("Format: ");
		gl_char_to_uint32.Value = gl_WAV.File.Format;
		USART_Write_X_Bytes(&gl_char_to_uint32.arr[0],0,4); USART_Write_String("\r\n");
		USART_Write_String("Subchunk1ID: ");
		gl_char_to_uint32.Value = gl_WAV.File.Subchunk1ID;
		USART_Write_X_Bytes(&gl_char_to_uint32.arr[0],0,4); USART_Write_String("\r\n");
		printf("Subchunk1Size: %lu\r\n",gl_WAV.File.Subchunk1Size);
		printf("Audioformat: %u\r\n",gl_WAV.File.AudioFormat);
		printf("NumChannels: %u\r\n",gl_WAV.File.NumChannels);
		printf("Samplerate: %lu\r\n",gl_WAV.File.Samplerate);
		printf("BlockAlign: %u\r\n",gl_WAV.File.BlockAlign);
		printf("BitsPerSample: %u\r\n\r\n",gl_WAV.File.BitsPerSample);

		printf("WAV: Datei %s wird wiedergegeben\r\n",Filename);
	#endif

	if((gl_WAV.File.NumChannels > 2) || (gl_WAV.File.BitsPerSample > 16))
	{
		#ifdef WAV_Debug
			USART_Write_String("WAV: Die Datei kann nicht abgespielt werden, da sie mehr als 2 Audiokanaele hat, oder mehr als 16Bit Aufloesung\r\n");
		#endif
		return 2;
	}

	gl_WAV.Position_in_Table = Position_in_Table;									// Speichern
	gl_WAV.Bytecounter = gl_FAT32_File[gl_WAV.Position_in_Table].Size-36;			// Bytecounter laden - 36 Byte Header

	FAT32_File_Read(Position_in_Table,&gl_WAV.DATA_Buffer[0][0],WAV_Buffsize);		// Lade den ersten Buffer voll
	
	#ifdef WAV_PWM
		TIM4->CR1 |= TIM_CR1_CEN;													// Counter an
	#endif

	gl_WAV.Counter=0;																// Variablen zur�cksetzen
	gl_WAV.Data_read=0;
	gl_WAV.Togglebit=0;

	TIM9->ARR = F_CPU/gl_WAV.File.Samplerate;										// (16000 Hz = 5248)
	TIM9->CR1 |= TIM_CR1_CEN;														// Counter an
	return 0;
}

void WAV_Stop(void)
{
	FAT32_File_Close(gl_WAV.Position_in_Table);												// Falls vorher schon eine Datei gespielt wurde, schlie�e sie
	#ifdef WAV_PWM
	TIM4->CCR1 = 0;																			// Duty auf 0
	TIM4->CR1 &= ~TIM_CR1_CEN;																// Counter aus
	#endif
	TIM9->CR1 &= ~TIM_CR1_CEN;																// Counter aus
}

void WAV_Pause(void)
{
	#ifdef WAV_PWM
	TIM4->CR1 &= ~TIM_CR1_CEN;																// Counter aus
	#endif
	TIM9->CR1 &= ~TIM_CR1_CEN;																// Counter aus
}

void WAV_Continue(void)
{
	#ifdef WAV_PWM
	TIM4->CR1 |= TIM_CR1_CEN;																// Counter an
	#endif
	TIM9->CR1 |= TIM_CR1_CEN;																// Counter an
}

void WAV_Volume(float Volume)
{
	if(Volume > 100.0 || Volume < 0) return;												// Wenn die Volume nicht zwischen 0-100 liegt breche ab
	gl_WAV.Volume = Volume/100.0;															// �nderung der Lautst�rke
}

void WAV_Read_Filedata(void)
{
	if (gl_WAV.Data_read==0)
	{
		gl_WAV.Data_read = 1;
		if (gl_WAV.Togglebit)
		{
			if(gl_WAV.Bytecounter < WAV_Buffsize)													// Wenn der Buffer nicht mehr ganz gef�llt werden kann, lade nur den Rest
			{
				FAT32_File_Read(gl_WAV.Position_in_Table,&gl_WAV.DATA_Buffer[0][0],gl_WAV.Bytecounter);	// dies der 2., 4., 6., usw.
				gl_WAV.Bytecounter = 0;																// Setze danach auf 0
				FAT32_File_Close(gl_WAV.Position_in_Table);											// Falls vorher schon eine Datei gespielt wurde, schlie�e sie
			}
			else
			{
				FAT32_File_Read(gl_WAV.Position_in_Table,&gl_WAV.DATA_Buffer[0][0],WAV_Buffsize);	// dies der 2., 4., 6., usw.
			}
		}
		else
		{
			if(gl_WAV.Bytecounter < WAV_Buffsize)													// Wenn der Buffer nicht mehr ganz gef�llt werden kann, lade nur den Rest
			{
				FAT32_File_Read(gl_WAV.Position_in_Table,&gl_WAV.DATA_Buffer[1][0],gl_WAV.Bytecounter);	// dies ist der 1., 3., 5., geladene Buffer
				gl_WAV.Bytecounter = 0;																// Setze danach auf 0
				FAT32_File_Close(gl_WAV.Position_in_Table);											// Falls vorher schon eine Datei gespielt wurde, schlie�e sie
			}
			else
			{
				FAT32_File_Read(gl_WAV.Position_in_Table,&gl_WAV.DATA_Buffer[1][0],WAV_Buffsize);	// dies ist der 1., 3., 5., geladene Buffer
			}
		}
	}
}

void TIM1_BRK_TIM9_IRQHandler(void)
{
	int16_t Left, Right;
	uint8_t Left8, Right8;

	if(TIM9->SR & TIM_SR_UIF)																	// Pr�fe ob Interruptbit gesetzt
	{
		TIM9->SR = 0;																			// Interruptflag l�schen

		if(gl_WAV.Bytecounter)
		{
			if (gl_WAV.Counter > (WAV_Buffsize-1))												// Wenn >= WAV_Buffsize
			{
				gl_WAV.Counter=0;
				gl_WAV.Togglebit = gl_WAV.Togglebit ? 0:1;
				gl_WAV.Data_read = 0;
			}
		}
		else
		{
			#ifdef WAV_DAC
				DAC->DHR12R1 = 0;																// DAC 1 ausschalten
			#endif
			#ifdef WAV_PWM
				TIM4->CCR1 = 0;																	// Duty auf 0
				TIM4->CR1 &= ~TIM_CR1_CEN;														// Counter aus
			#endif
			TIM9->CR1 &= ~TIM_CR1_CEN;															// Counter aus
			#ifdef WAV_Debug
				USART_Write_String("WAV: Datei wiedergegeben\r\n");
			#endif
			return;
		}

		if (gl_WAV.File.BitsPerSample == 16)																// 16 Bit werden als int16_t aus der Datei geladen (Nur bei RiFF)
		{
			if (gl_WAV.File.NumChannels == 2)																// Da wir nur einen Lautsprecher haben, mach aus Stereo Mono
			{
				Right = (int16_t)((gl_WAV.DATA_Buffer[gl_WAV.Togglebit][gl_WAV.Counter+1]<<8)|(gl_WAV.DATA_Buffer[gl_WAV.Togglebit][gl_WAV.Counter]));
				Left = (int16_t)((gl_WAV.DATA_Buffer[gl_WAV.Togglebit][gl_WAV.Counter+3]<<8)|(gl_WAV.DATA_Buffer[gl_WAV.Togglebit][gl_WAV.Counter+2]));

				#ifdef WAV_DAC
					DAC->DHR12R1 = ((uint16_t)(((Left+32767)/2) + ((Right+32767)/2))/16) * gl_WAV.Volume; 	// Setze aus Left, Right und Lautst�rke den DAC wert zusammen; 32767 macht aus -32767 = 0 und 32767 = 65534
				#endif
				gl_WAV.Counter+=4;
				gl_WAV.Bytecounter-=4;
			}
			else if (gl_WAV.File.NumChannels == 1)
			{
				#ifdef WAV_DAC
					DAC->DHR12R1 = ((int16_t)(gl_WAV.DATA_Buffer[gl_WAV.Togglebit][gl_WAV.Counter+1]<<8) | (gl_WAV.DATA_Buffer[gl_WAV.Togglebit][gl_WAV.Counter])/16) * gl_WAV.Volume ; // Mach aus 2 Byte ein little Endian int16_t und daraus uint16_t
				#endif
				gl_WAV.Counter+=2;
				gl_WAV.Bytecounter-=2;
			}
		}
		else if (gl_WAV.File.BitsPerSample == 8)															// 8 Bit sind standardm��ig als uint8_t in der Datei (Nur bei RIFF)
		{
			if (gl_WAV.File.NumChannels == 2)
			{
				Right8 = gl_WAV.DATA_Buffer[gl_WAV.Togglebit][gl_WAV.Counter];
				Left8 = gl_WAV.DATA_Buffer[gl_WAV.Togglebit][gl_WAV.Counter+1];

				#ifdef WAV_DAC
					DAC->DHR12R1 = (((Left8/2) + (Right8/2))*16) * gl_WAV.Volume;							// Setze aus Left, Right und Lautst�rke den DAC wert zusammen
				#endif
				#ifdef WAV_PWM
					TIM4->CCR1 = ((Left8/2) + (Right8/2)) * gl_WAV.Volume * 257);							// Duty setzen, CCA muss immer kleiner PER sein!!
				#endif

				gl_WAV.Counter+=2;
				gl_WAV.Bytecounter-=2;
			}
			else if (gl_WAV.File.NumChannels == 1)
			{
				#ifdef WAV_DAC
					DAC->DHR12R1 = (gl_WAV.DATA_Buffer[gl_WAV.Togglebit][gl_WAV.Counter] * 16) * gl_WAV.Volume;
				#endif
				#ifdef WAV_PWM
					TIM4->CCR1 = (uint16_t)(gl_WAV.DATA_Buffer[gl_WAV.Togglebit][gl_WAV.Counter] * gl_WAV.Volume);	// Duty setzen, CCA muss immer kleiner PER sein!!
				#endif
				gl_WAV.Counter++;
				gl_WAV.Bytecounter--;
			}
		}
	}
}
