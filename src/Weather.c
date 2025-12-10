// Includes
#include "main.h"
#include "Weather.h"
#include "stm32f4xx.h"
#include "Allerlei.h"
#include "DS1307.h"
#include "PWM.h"
#include "time.h"
#include "SPI.h"
#include "FAT32.h"
#include "Tasktimer.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "INI_Parser.h"
#include "DS18B20.h"
#ifdef Weather_Debug
	#include "USART.h"
#endif

/*

************************************************************************************************
* Wettersimulation an den 6 PWM-Ausg�ngen der Stuerungssoftware auf dem STM32F401RET6		   *
* TIM5 als 32Bit Ticker																		   *
* 2019 � Frederinn													 		   		   *
************************************************************************************************

*/

// Variablen
const char gl_Weather_Days[][4] ={"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};						// Dateinamen der Tage
volatile struct Weather gl_Weather;																	// Struct f�r die Wettersimulation

// Funktionen
void Weather_Init(void)
{
	struct Timestamp tempstamp;
	char Day0[8],Day1[8],Day2[8],buffer[20];														// Mon.txt

	memset((void*)&gl_Weather,0,sizeof(gl_Weather));												// Reset array

	FAT32_Directory_Change("/webserv/planning/");
	INI_Read_Key_String("weather.ini","Channelsetting","Channel",&buffer[0]);						// Lese den Channel0 Key aus der Ini
	sscanf(&buffer[0],"%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",&gl_Weather.Channel[0].Setting,&gl_Weather.Channel[1].Setting,&gl_Weather.Channel[2].Setting,&gl_Weather.Channel[3].Setting,&gl_Weather.Channel[4].Setting,&gl_Weather.Channel[5].Setting);	// wandle string in uint8
	INI_Read_Key_String("weather.ini","Cooling","Used",&buffer[0]);									// Lese den Used Key aus der Ini
	sscanf(&buffer[0],"%hhu",&gl_Weather.Cooling.used);												// wandle string in uint8
	INI_Read_Key_String("weather.ini","Cooling","Fan_Channel",&buffer[0]);							// Lese den Fan_Channel Key aus der Ini
	sscanf(&buffer[0],"%hhu",&gl_Weather.Cooling.Fan_Channel);										// wandle string in uint8
	INI_Read_Key_String("weather.ini","Cooling","Temp_Treshold",&buffer[0]);						// Lese den Used Key aus der Ini
	sscanf(&buffer[0],"%f",&gl_Weather.Cooling.Treshold);											// wandle string in uint8

	switch(gl_Time.Weekday)																			//Lade den gestrigen, heutigen und morgigen Tag aus dem Textfile in die Arrays //gl_Time.Weekday: 1=Mo,2=Di,3=Mi,4=Do,5=Fr,6=Sa,7=So
	{
		case 0:
			strcpy(&Day0[0],&gl_Weather_Days[6][0]);												// Dateinamenstring erstellen
			strcat(&Day0[0],".txt");
			strcpy(&Day1[0],&gl_Weather_Days[0][0]);												// Dateinamenstring erstellen // gl_Weather_Days f�ng bei 0="Montag" an
			strcat(&Day1[0],".txt");
			strcpy(&Day2[0],&gl_Weather_Days[1][0]);												// Dateinamenstring erstellen
			strcat(&Day2[0],".txt");
			break;
	
		case 6:
			strcpy(&Day0[0],&gl_Weather_Days[5][0]);												// Dateinamenstring erstellen
			strcat(&Day0[0],".txt");
			strcpy(&Day1[0],&gl_Weather_Days[6][0]);												// Dateinamenstring erstellen // gl_Weather_Days f�ng bei 0="Montag" an
			strcat(&Day1[0],".txt");
			strcpy(&Day2[0],&gl_Weather_Days[0][0]);												// Dateinamenstring erstellen
			strcat(&Day2[0],".txt");
			break;
		
		default:
			strcpy(&Day0[0],&gl_Weather_Days[gl_Time.Weekday-1][0]);								// Dateinamenstring erstellen
			strcat(&Day0[0],".txt");
			strcpy(&Day1[0],&gl_Weather_Days[gl_Time.Weekday][0]);									// Dateinamenstring erstellen // gl_Weather_Days f�ng bei 0="Montag" an
			strcat(&Day1[0],".txt");
			strcpy(&Day2[0],&gl_Weather_Days[gl_Time.Weekday+1][0]);								// Dateinamenstring erstellen
			strcat(&Day2[0],".txt");
		break;
	}
	Weather_Load_Day(&Day0[0],Weather_Yesterday);													// Gestrigen Tag in Structure laden, Fehlerhandler n�tig?
	Weather_Load_Day(&Day1[0],Weather_Today);														// Heutigen Tag in Structure laden, Fehlerhandler n�tig?
	Weather_Load_Day(&Day2[0],Weather_Tommorow);													// Morgigen Tag in Structure laden, Fehlerhandler n�tig?

	DS1307_Read_Timestamp(&tempstamp);
	gl_Weather.Systick = (Time_Timestamp_to_UTC(&tempstamp) % 86400UL)*1000;						// gl_Tasktimer_Timestamp wird selbst einmal die Stunde gestellt

	RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;																// Clock f�r TIM5 an
	if((TIM5->CR1 & TIM_CR1_CEN)==0)																// Wenn der Timer noch nicht l�uft, stelle ihn ein
	{
		TIM5->CR1 = TIM_CR1_DIR;																	// Downcounter
		TIM5->PSC = (F_CPU/65536/10)-1;																// Teiler = PSC + 1
		TIM5->ARR = 0xffff;																			// 10 Hz
		TIM5->DIER = TIM_DIER_UIE;																	// Updateinterrupt an
		NVIC_SetPriority(TIM1_UP_TIM10_IRQn,NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 2, 0));	// Timer ist eins niedriger als WAV Player
		NVIC_EnableIRQ(TIM5_IRQn);																	// IRQ f�r TIM6 aktivieren
		TIM5->CR1 |= TIM_CR1_CEN;																	// Counter an
	}
	gl_Weather.Maintenance=0;																		// Wartung aus
	Weather_Tempcontrol(gl_Weather.Cooling.used);													// Setze die K�hlfunktion an oder aus
	#ifdef Weather_Debug
		USART_Write_String("Weather: Init Erfolgreich\r\n");
	#endif
}

void Weather_Init_Trigger(void)
{
	if(gl_Weather.Trigger_Init==1)																	// Wenn der Trigger kommt, starte die Weather_Init neu
	{
		gl_Weather.Trigger_Init=0;
		Weather_Pause();
		Weather_Init();
	}
}

void Weather_Pause(void)
{
	TIM5->CR1 &= ~TIM_CR1_CEN;																		// Counter aus
	TIM5->DIER &= ~TIM_DIER_UIE;																	// Updateinterrupt aus
	NVIC_DisableIRQ(TIM5_IRQn);																		// Interrupt trennen
}

void Weather_Sync_Tick(void)																		// Einmal die Stunde wird die Clock mit der Systemzeit synchronisiert
{
	gl_Weather.Systick = (gl_Tasktimer_Timestamp % 86400UL)*1000;									// gl_Tasktimer_Timestamp wird selbst einmal die Stunde gestellt
}

void TIM5_IRQHandler(void)																			// Tickt im 100ms Takt
{
	static uint8_t flip=1;																			// Togglevariable

	if(TIM5->SR & TIM_SR_UIF)																		// Pr�fe ob Interruptbit gesetzt
	{
		TIM5->SR = 0;																				// Interruptflag l�schen
		//flip = 1;
		gl_Weather.Systick += 100;																	// Weathertick um 100ms hochz�hlen

		if (gl_Weather.Systick < 10000UL)															// Systick ist hier in 1ms
		{
			if (flip==1)																			// F�hre die Init nur einmal aus und nur wenn die FAT gerade nicht benutzt wird
			{
				gl_Weather.Trigger_Init=1;															// Triggere die Init in der Main
				flip = 0;
			}
		}
		else
		{
			if (gl_Weather.Systick > 10000UL){flip = 1;}											// Variable zur�cksetzen
		}
		Weather_Set_Channels(gl_Weather.Systick);													//Setze die PWM Ausg�nge, Systick hier in ms
	}
}

void Weather_Set_Channels(uint32_t Systick)															// Systick hier in 100ms Schritten
{
	uint8_t Last_Point_used=0, First_Point_used=0, Previous_Point_used=0;							// Letzten, ersten oder verherigen benutzbaren Punkt
	float Tick_ms=0.0;																				// Tick PWM Schritt pro 100ms von Punkt zu Punkt
	float fDuty=0.0;																				// Duty als float

	if(gl_Weather.Cooling.used==1 && gl_Weather.Cooling.Fan_Channel<6)								// Wenn die K�hlung aktiv und der L�fterkanal definiert ist, dann setze diesen
	{
		if(gl_Weather.Channel[gl_Weather.Cooling.Fan_Channel].Setting==6)							// Wenn f�r den Kanal kein L�fter eingestellt wurde, setze keine PWM
		{
			if(gl_Weather.Cooling.Treshold <= gl_DS18B20.Temperature)								// Wenn der Treshold �berschritten wurde, schalte den L�fter ein. Temperatur kommt per Tasktimer Temp_Datenlogger
			{
			  gl_Weather.Channel[gl_Weather.Cooling.Fan_Channel].Duty = 100;
				PWM_Channel_set(gl_Weather.Cooling.Fan_Channel,PWM_Max);
			}
			if((gl_Weather.Cooling.Treshold-1.0) >= gl_DS18B20.Temperature)							// Wenn der Treshold-1K unterschritten wurde, schalte den L�fter aus
			{
			    gl_Weather.Channel[gl_Weather.Cooling.Fan_Channel].Duty = 0;
				PWM_Channel_set(gl_Weather.Cooling.Fan_Channel,PWM_Min);
			}
		}
		else
		{
		    gl_Weather.Channel[gl_Weather.Cooling.Fan_Channel].Duty = 0;
		    PWM_Channel_set(gl_Weather.Cooling.Fan_Channel,PWM_Min);
		}
	}

	for (uint8_t j=0;j<6;j++)																		// Mache die Berechnung f�r jeden Kanal einzeln
	{
		if (gl_Weather.Channel[j].Manual == 1) continue;											// Wenn der Kanal auf manueller Steuerung steht, �berspringe diesen

		fDuty=0.0;																					// Duty zur�cksetzen
		Last_Point_used = Weather_Find_last_used_Point(j,Weather_Today);							// Suche letzten benutzbaren heutigen Punkt in der Weather Struct
		First_Point_used = Weather_Find_first_used_Point(j,Weather_Today);							// Ersten benutzbaren heutigen Punkt in der Weather Struct

		if(First_Point_used == 255)																	// Wenn f�r den Kanal kein Punkt gefunden wurde, setze ihn auf 0 und springe zum N�chsten
		{
			PWM_Channel_set(j, 0);
			continue;
		}

		if((gl_Weather.Days[Weather_Today][First_Point_used].Minutestamp*60000UL) > Systick)		// Wenn der erste Punkt heute noch nicht erreicht wurde
		{
			#ifdef Weather_Debug
				printf("Weather:1, Kanal:%u\r\n",j);
			#endif
			// Gilt nur wenn der aktuelle Tick ab 00:00:00 und kleiner gleich dem ersten Punkt heute liegt
			Last_Point_used = Weather_Find_last_used_Point(j,Weather_Yesterday);					// Suche letzten benutzbaren gestrigen Punkt in der Weather Struct
			First_Point_used = Weather_Find_first_used_Point(j,Weather_Today);						// Ersten benutzbaren heutigen Punkt in der Weather Struct
			Tick_ms = (gl_Weather.Days[Weather_Today][First_Point_used].PWM_Percent - gl_Weather.Days[Weather_Yesterday][Last_Point_used].PWM_Percent)/((1440.0 - gl_Weather.Days[Weather_Yesterday][Last_Point_used].Minutestamp + gl_Weather.Days[Weather_Today][First_Point_used].Minutestamp)*60000.0); //

			switch(gl_Weather.Channel[j].Setting)
			{
				case 1:			// Growx5
				case 2:			// Sunset
				case 3:			// SKY
				case 4:			// DAY
				case 5:			// TROPIC
					fDuty = (pow(10.0,(gl_Weather.Days[Weather_Today][First_Point_used].PWM_Percent-(((gl_Weather.Days[Weather_Today][First_Point_used].Minutestamp*60000.0)-Systick)*Tick_ms))/96.0)-1)*10.0*(PWM_Max/100.0);	// Errechne die aktuelle PWM Duty
					gl_Weather.Channel[j].Duty = (gl_Weather.Days[Weather_Today][First_Point_used].PWM_Percent-(((gl_Weather.Days[Weather_Today][First_Point_used].Minutestamp*60000.0)-Systick)*Tick_ms));
				break;

				case 6:			// L�fter
				case 7:			// CO2
					fDuty = (gl_Weather.Days[Weather_Today][First_Point_used].PWM_Percent-(((gl_Weather.Days[Weather_Today][First_Point_used].Minutestamp*60000.0)-Systick)*Tick_ms))*(PWM_Max/100.0);	// Errechne die aktuelle PWM Duty
					gl_Weather.Channel[j].Duty = (gl_Weather.Days[Weather_Today][First_Point_used].PWM_Percent-(((gl_Weather.Days[Weather_Today][First_Point_used].Minutestamp*60000.0)-Systick)*Tick_ms));
				break;

				default:		// Bei 0 und unbekannten Einstellungen setze den Kanal auf 0
					fDuty = 0;
					gl_Weather.Channel[j].Duty = 0;
				break;
			}
		}
		else if ((Systick >= (gl_Weather.Days[Weather_Today][Last_Point_used].Minutestamp*60000UL)) && Systick < 86400000UL)	// Wenn der letzte heutige Punkt schon erreicht wurde
		{
			#ifdef Weather_Debug
				printf("Weather:2, Kanal:%u\r\n",j);
			#endif
			// Gilt nur wenn der aktuelle Tick ab Last_Point heute und kleiner 00:00:00 Uhr ist
			Last_Point_used = Weather_Find_last_used_Point(j,Weather_Today);						// Suche letzten benutzbaren gestrigen Punkt in der Weather Struct
			First_Point_used = Weather_Find_first_used_Point(j,Weather_Tommorow);					// Ersten benutzbaren morgigen Punkt in der Weather Struct
			Tick_ms = (gl_Weather.Days[Weather_Tommorow][First_Point_used].PWM_Percent - gl_Weather.Days[Weather_Today][Last_Point_used].PWM_Percent)/((1440.0 - gl_Weather.Days[Weather_Today][Last_Point_used].Minutestamp + gl_Weather.Days[Weather_Tommorow][First_Point_used].Minutestamp)*60000.0); //

			switch(gl_Weather.Channel[j].Setting)
			{
				case 1:			// Growx5
				case 2:			// Sunset
				case 3:			// SKY
				case 4:			// DAY
				case 5:			// TROPIC
					fDuty = (pow(10.0,(((Systick-(gl_Weather.Days[Weather_Today][Last_Point_used].Minutestamp*60000.0))*Tick_ms)+gl_Weather.Days[Weather_Today][Last_Point_used].PWM_Percent)/96.0)-1)*10.0*(PWM_Max/100.0);	// Errechne die aktuelle PWM Duty
					gl_Weather.Channel[j].Duty = (((Systick-(gl_Weather.Days[Weather_Today][Last_Point_used].Minutestamp*60000.0))*Tick_ms)+gl_Weather.Days[Weather_Today][Last_Point_used].PWM_Percent);
				break;

				case 6:			// L�fter
				case 7:			// CO2
					fDuty = (((Systick-(gl_Weather.Days[Weather_Today][Last_Point_used].Minutestamp*60000.0))*Tick_ms)+gl_Weather.Days[Weather_Today][Last_Point_used].PWM_Percent)*(PWM_Max/100.0);	// Errechne die aktuelle PWM Duty
					gl_Weather.Channel[j].Duty = (((Systick-(gl_Weather.Days[Weather_Today][Last_Point_used].Minutestamp*60000.0))*Tick_ms)+gl_Weather.Days[Weather_Today][Last_Point_used].PWM_Percent);
				break;

				default:		// Bei 0 und unbekannten Einstellungen setze den Kanal auf 0
					fDuty = 0;
					gl_Weather.Channel[j].Duty = 0;
				break;
			}
		}
		else
		{
			#ifdef Weather_Debug
				printf("Weather:3, Kanal:%u\r\n",j);
			#endif
			First_Point_used = Weather_Find_first_used_Point(j,Weather_Today);						// Ersten benutzbaren heutigen Punkt in der Weather Struct
			for(uint8_t g=(First_Point_used+1);g<Weather_Max_Points;g++)							// Errechne an welcher Stelle die Simulation sein sollte, Fange ab dem 2. Punkt an
			{
				if ((gl_Weather.Days[Weather_Today][g].Point_Used==1) && (gl_Weather.Days[Weather_Today][g].Channel==j)) // Suche in den heutigen benutzen Punkten
				{
					if((gl_Weather.Days[Weather_Today][g].Minutestamp*60000UL) > Systick)			// Suche den ersten Punkt der sp�ter als der Tick liegt und nehme die Werte dessen Vorg�ngers
					{
						// Errechne den Tick und die Duty zwischen zwei Punkten, mindestens zwischen Last_Point heute und Point 0 heute, oder P2 heute und P1 heute usw..
						Previous_Point_used = Weather_Find_previous_used_Point(j,Weather_Today,g);	// Suche den vorherigen Punkt des Kanals im heutigen Tag
						Tick_ms = (gl_Weather.Days[Weather_Today][g].PWM_Percent - gl_Weather.Days[Weather_Today][Previous_Point_used].PWM_Percent)/((gl_Weather.Days[Weather_Today][g].Minutestamp - gl_Weather.Days[Weather_Today][Previous_Point_used].Minutestamp)*60000.0); //PWM pro ms

						switch(gl_Weather.Channel[j].Setting)
						{
							case 1:			// Growx5
							case 2:			// Sunset
							case 3:			// SKY
							case 4:			// DAY
							case 5:			// TROPIC
								fDuty = (pow(10.0,(gl_Weather.Days[Weather_Today][Previous_Point_used].PWM_Percent + ((Systick - (gl_Weather.Days[Weather_Today][Previous_Point_used].Minutestamp*60000))*Tick_ms))/96.0)-1)*10.0*(PWM_Max/100.0);	// Errechne die aktuelle PWM Duty
								gl_Weather.Channel[j].Duty = (gl_Weather.Days[Weather_Today][Previous_Point_used].PWM_Percent + ((Systick - (gl_Weather.Days[Weather_Today][Previous_Point_used].Minutestamp*60000))*Tick_ms));
							break;

							case 6:			// L�fter
							case 7:			// CO2
								fDuty = (gl_Weather.Days[Weather_Today][Previous_Point_used].PWM_Percent + ((Systick - (gl_Weather.Days[Weather_Today][Previous_Point_used].Minutestamp*60000))*Tick_ms)) * (PWM_Max / 100.0);
								gl_Weather.Channel[j].Duty = (gl_Weather.Days[Weather_Today][Previous_Point_used].PWM_Percent + ((Systick - (gl_Weather.Days[Weather_Today][Previous_Point_used].Minutestamp*60000))*Tick_ms));
							break;

							default:		// Bei 0 und unbekannten Einstellungen setze den Kanal auf 0
								fDuty = 0;
								gl_Weather.Channel[j].Duty = 0;
							break;
						}
						break;
					}
				}
				else
				{
					break;
				}
			}
		}
		PWM_Channel_set(j, (uint16_t)lround(fDuty));												// Setze den neuen Duty als gerundeter uint16
	}
}

void Weather_Save_Day(char *Filename, char *Filedata)
{
	FAT32_Directory_Change("/webserv/planning/");													// Wechsle ins Verzeichnis mit den Tagen
	FAT32_File_Delete(&Filename[0]);																// Alte Datei l�schen
	FAT32_File_Create(&Filename[0],0x00);															// Neue erstellen
	FAT32_File_Open(Weather_Filenumber,&Filename[0],FAT32_Write);									// Datei im Schreibmodus �ffnen
	FAT32_File_Write(Weather_Filenumber,&Filedata[0],strlen(&Filedata[0]));							// Inhalt in Datei schreiben
	FAT32_File_Close(Weather_Filenumber);															// Datei schlie�en
}

void Weather_Load_Day(char *Filename, uint8_t Position)
{
	char Line[20];
	memset((char*)&gl_Weather.Days[Position],0,sizeof(gl_Weather.Days[Position]));					// L�sche das Array
	FAT32_Directory_Change("/webserv/planning/");													// Wechsle ins Verzeichnis mit den Tagen
	FAT32_File_Open(Weather_Filenumber,&Filename[0],FAT32_Read);									// Datei im Lesemodus �ffnen

	for (uint8_t g=0;g<Weather_Max_Points;g++)
	{
		if (FAT32_File_Read_Line(Weather_Filenumber,&Line[0],20))
		{
			break;
		}
		sscanf(&Line[0],"%f,%f,%hhu",&gl_Weather.Days[Position][g].Minutestamp,&gl_Weather.Days[Position][g].PWM_Percent,&gl_Weather.Days[Position][g].Channel);	// 1260,57.10,0
		gl_Weather.Days[Position][g].Minutestamp *=2;												// Zahl muss mal zwei genommen werden
		gl_Weather.Days[Position][g].Point_Used = 1;												// Punkt im Array wird verwendet
	}
	FAT32_File_Close(Weather_Filenumber);															// Datei schlie�en
}

uint8_t Weather_Find_previous_used_Point(uint8_t Channel, uint8_t Day,uint8_t Point)
{
	for (int16_t g=(Point-1);g>-1;g--)
	{
		if ((gl_Weather.Days[Day][g].Point_Used == 1) && (gl_Weather.Days[Day][g].Channel == Channel))
		{
			return g;
		}
	}
	return 255;
}

uint8_t Weather_Find_last_used_Point(uint8_t Channel, uint8_t Day)
{
	for (int16_t g=(Weather_Max_Points-1);g>-1;g--)
	{
		if ((gl_Weather.Days[Day][g].Point_Used == 1) && (gl_Weather.Days[Day][g].Channel == Channel))
		{
			return g;
		}
	}
	return 255;
}

uint8_t Weather_Find_first_used_Point(uint8_t Channel, uint8_t Day)
{
	for (int16_t g=0;g<Weather_Max_Points;g++)
	{
		if ((gl_Weather.Days[Day][g].Point_Used == 1) && (gl_Weather.Days[Day][g].Channel == Channel))
		{
			return g;
		}
	}
	return 255;
}

void Weather_Save_Tempcontrol(char *Filename, char *Filedata)
{
	char buffer[10];
	sscanf(&Filedata[0],"%f,%hhu",&gl_Weather.Cooling.Treshold,&gl_Weather.Cooling.Fan_Channel);	// Filedata in Variablen aufnehmen
	FAT32_Directory_Change("/webserv/planning/");													// Wechsle ins Verzeichnis mit den Tagen
	sprintf(&buffer[0],"%2.3f",gl_Weather.Cooling.Treshold);
	INI_Write_Key_String("weather.ini","Cooling","Temp_Treshold",&buffer[0]);
	sprintf(&buffer[0],"%hhu",gl_Weather.Cooling.Fan_Channel);
	INI_Write_Key_String("weather.ini","Cooling","Fan_Channel",&buffer[0]);
}

void Weather_Tempcontrol(uint8_t On_Off)
{
	char Buffer[10];
	gl_Weather.Cooling.used = On_Off;																// �ndere das Controlbit f�r die Temperatur�berwachung
	FAT32_Directory_Change("/webserv/planning/");
	sprintf(&Buffer[0],"%hhu",gl_Weather.Cooling.used);												// Drucke die Variable in den String
	INI_Write_Key_String("weather.ini","Cooling","Used",&Buffer[0]);								// Schreibe den Wert

	if(On_Off == Weather_Tempcontrol_Off)
	{
		gl_Weather.Channel[gl_Weather.Cooling.Fan_Channel].Manual=0;								// Manuelle L�ftersteuerung aus
		return;																						// Wenn die �berwachung ausgeschaltet ist, breche hier ab
	}

	FAT32_Directory_Change("/webserv/planning/");													// Wechsle ins Verzeichnis mit den Tagen
	INI_Read_Key_String("weather.ini","Cooling","Fan_Channel",&Buffer[0]);							// Lese den L�fterkanal aus
	sscanf(&Buffer[0],"%hhu",&gl_Weather.Cooling.Fan_Channel);
	INI_Read_Key_String("weather.ini","Cooling","Temp_Treshold",&Buffer[0]);						// Lese den Temperatur Treshold aus
	sscanf(&Buffer[0],"%f",&gl_Weather.Cooling.Treshold);

	if(gl_Weather.Cooling.Fan_Channel<6)															// Falls der Kanal g�ltig ist, setze diesen PWM Kanal auf manuell
	{
		gl_Weather.Channel[gl_Weather.Cooling.Fan_Channel].Manual=1;								// Setze den Kanal auf manuelle Steuerung
	}
}










