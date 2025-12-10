// Includes
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "time.h"
#include <stm32f4xx.h>
#include <stdint.h>
#include "USART.h"
#include "SoftwareI2C.h"
#include "SPI.h"
#include "DS1307.h"
#include "SD.h"
#include "FAT32.h"
#include "ENC28J60.h"
#include "Stack.h"
#include "Tasktimer.h"
#include "DHCP.h"
#include "NTP.h"
#include "HTTP.h"
#include "FTP.h"
#include "Geniedisplay.h"
#include "Debug.h"
#include "WAV.h"
#include "PWM.h"
#include "weather.h"
#include "ADC.h"
#include "Power.h"
#include "DS18B20.h"
#include "AT24C32.h"
#include "DS2482_100.h"
#include "INI_Parser.h"
#include "FirmwareUpdater.h"

/*

************************************************************************************************
* AquaconV2 auf dem STM32F407VGT6															   *
* 19.02.2019 � Frederinn															   *
************************************************************************************************

*/

#if TempLog_Temp_Filenumber < TCP_Max_Entries || TempLog_Temp_Filenumber > FAT32_Max_Entries
	#error "Die Temp-Datei darf nicht im Bereich der TCP Dateien liegen"
#endif
#if TempLog_Label_Filenumber < TCP_Max_Entries || TempLog_Label_Filenumber > FAT32_Max_Entries
	#error "Die Label-Datei darf nicht im Bereich der TCP Dateien liegen"
#endif
#if Weather_Filenumber < TCP_Max_Entries || Weather_Filenumber > FAT32_Max_Entries
	#error "Die Wetter-Datei darf nicht im Bereich der TCP Dateien liegen"
#endif
#if Weather_Temp_Filenumber < TCP_Max_Entries || Weather_Temp_Filenumber > FAT32_Max_Entries
	#error "Die Wettertemperatur-Datei darf nicht im Bereich der TCP Dateien liegen"
#endif
#if INI_Parser_Read_Filenumber < TCP_Max_Entries || INI_Parser_Read_Filenumber > FAT32_Max_Entries
	#error "Die INI-Parser-Datei darf nicht im Bereich der TCP Dateien liegen"
#endif
#if INI_Parser_Write_Filenumber < TCP_Max_Entries || INI_Parser_Write_Filenumber > FAT32_Max_Entries
	#error "Die INI-Parser-Datei darf nicht im Bereich der TCP Dateien liegen"
#endif
#if WAV_Filenumber < TCP_Max_Entries || WAV_Filenumber > FAT32_Max_Entries
	#error "Die WAV-Datei darf nicht im Bereich der TCP Dateien liegen"
#endif
#if FTP_Filenumber < TCP_Max_Entries || FTP_Filenumber > FAT32_Max_Entries
	#error "Die FTP Hilfsdatei darf nicht im Bereich der TCP Dateien liegen"
#endif
#if FTP_Max_Entries != TCP_Max_Entries || FTP_Max_Entries > FAT32_Max_Entries
	#error "FTP und TCP Tabelle muss gleich gross sein"
#endif
#if ARP_Max_Entries != TCP_Max_Entries
	#error "ARP und TCP Tabelle muss gleich gross sein"
#endif
#if HTTP_Max_Entries != TCP_Max_Entries || HTTP_Max_Entries > FAT32_Max_Entries
	#error "HTTP und TCP Tabelle muss gleich gross sein"
#endif
#if FAT32_Max_Entries < TCP_Max_Entries
	#error "FAT32 Tabelle muss groesser oder gleich TCP Tabelle sein"
#endif
#if TCP_MSS < 405
	#error "TCP_MSS muss groesser als 405 Bytes sein"
#endif
#if TCP_Window < 405
	#error "TCP_Window muss groesser als 405 Bytes sein"
#endif
#if Parser_min_MSS > TCP_MSS || Parser_min_MSS > TCP_Window
	#error "TCP_Window und TCP_MSS muessen groesser als Parser_min_MSS sein, sonst funktioniert das Parsen nicht"
#endif
#if ARP_Max_Entries >= 255
	#error "ARP Tabelle muss kleiner 255 Eintr�ge sein"
#endif
#if TCP_Max_Entries >= 255
	#error "TCP Tabelle muss kleiner 255 Eintr�ge sein"
#endif


// Variablen
struct Webserver gl_Webserver;													// Globale von Webserver

// Funktionen
int main(void)
{
	__enable_irq();																// IRQs nach Bootloader wieder aktivieren

	// PLL einstellen mit internem 16MHz Quarz auf 102MHz
	if(!((RCC->CFGR & 0x0000000c)==4)) 											// Wenn HSI16 nicht eingeschaltet ist, dann mach das mal
	{
		RCC->CR |= (1<<0);														// Schalte den HSI16 an
		while(!((RCC->CR & 0x00000002)==0b10));									// Warte bis die Clock stabil l�uft
		RCC->CFGR &= 0xfffffffc;												// Setze die HSI16 als Systemclock
		while(!((RCC->CFGR & 0x0000000c)==0b0000));								// Warte bis die HSI116 l�uft
		RCC->CR &= ~(1<<24);													// PLL auschalten
		while((RCC->CR & 0x02000000) == 0x02000000); 							// Warte bis die PLL steht
		FLASH->ACR |= FLASH_ACR_LATENCY_3WS;									// Latenz auf 3 (90 < F_CPU <= 120)
		RCC->PLLCFGR = 0b00000000000000000110011000100000; 						// PLLP auf 2, PLLN auf 408, PLLM auf 32 -> 16MHz HSI auf 102MHz
		RCC->CR &= ~(1<<16);													// PLLSRC auf 0 = HSI16 Source
		RCC->CR |= (1<<24);														// PLL anschalten
		while(!((RCC->CR & 0x02000000) == 0x02000000)); 						// Warte bis die PLL l�uft
		RCC->CFGR |= (1<<1);													// Setze die PLL
		while(!((RCC->CFGR & 0x0000000c)==0b1000));								// Warte bis die PLL als Sysclock l�uft
	}

	_delay_us_init(250000);														// Delay 250ms nach Systemstart

	USART_Init(256000UL);														// Nur hier um unten die FAT debuggen zu k�nnen
	Genie_Init();

	SD_Portinit();																// SD Portinit
	SPI_Portinit();																// SPI Portinit

	uint8_t SD_Init_result=0;
	if(SD_Card_mount()==0)														// SD Karte einbinden
	{
		SD_Init_result=1;
		Genie_Write_String_5_7(0,0,"SD Init OK");
	}
	else
	{
		Genie_Write_String_5_7(0,0,"SD Init FEHLER");
	}

	// PLL einstellen mit internem 16MHz Quarz auf 168MHz
	RCC->CR |= (1<<0);															// Schalte den HSI16 an
	while(!((RCC->CR & 0x00000002)==0b10));										// Warte bis die Clock stabil l�uft
	RCC->CFGR &= 0xfffffffc;													// Setze die HSI16 als Systemclock
	while(!((RCC->CFGR & 0x0000000c)==0b0000));									// Warte bis die HSI116 l�uft
	RCC->CR &= ~(1<<24);														// PLL auschalten
	while((RCC->CR & 0x02000000) == 0x02000000); 								// Warte bis die PLL steht
	FLASH->ACR |= FLASH_ACR_DCEN | FLASH_ACR_ICEN | FLASH_ACR_LATENCY_4WS;	// Latenz auf 4 und Caches an (F_CPU > 120MHz)
	RCC->PLLCFGR = 0b00000000000000000101010000010000; 							// PLLP auf 2, PLLN auf 336, PLLM auf 16 -> 16MHz HSI auf 168MHz
	RCC->CR &= ~(1<<16);														// PLLSRC auf 0 = HSI16 Source
	RCC->CR |= (1<<24);															// PLL anschalten
	while(!((RCC->CR & 0x02000000) == 0x02000000)); 							// Warte bis die PLL l�uft
	RCC->CFGR |= (1<<1);														// Setze die PLL
	while(!((RCC->CFGR & 0x0000000c)==0b1000));									// Warte bis die PLL als Sysclock l�uft

	NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);								// Priogruppe auf 4, 4 Bits preemption, 0 Sub

	SPI_Init(SPI_Clockdiv_default);												// SPI Defaultinit
	uint8_t FAT32_Init_result=0;												// Merker f�r sp�ter
	if(SD_Init_result==1)
	{
		if(FAT32_Init()==0)														// Muss zuerst initialisiert werden, dass die basic.INI gelesen werden kann
		{
			FAT32_Init_result=1;
		}
	}

	SoftwareI2C_Master_Init();													// I2C Master Init
	Debug_Command_Init();														// Debug Init, Baud aus I2C EEPROM
	ENC_Portinit();																// ENC Portinit
	PWM_Init();																	// PWM Init
	ADC1_Init();																// ADC1 Init 12Bit
	ADC2_Init();																// ADC2 Init 12Bit

	if(FAT32_Init_result==1)													// In der Debug_Comand_Init wird der Uart konfiguriert
	{
		Genie_Write_String_5_7(0,8,"FAT32 Init OK");
		USART_Write_String("Main: FAT32 Init erfolgreich\r\n");
	}
	else
	{
		Genie_Write_String_5_7(0,8,"FAT32 Init FEHLER");
	}

	if (DS2482_Reset()==0)														// One Wire I2C Bridge
	{
		Genie_Write_String_5_7(0,16,"DS2482-100 Init OK");
		USART_Write_String("Main: DS2482-100 Reset erfolgreich\r\n");
	}
	else
	{
		Genie_Write_String_5_7(0,16,"DS2482-100 Init FEHLER");
	}

	if (DS18B20_Init(&gl_DS18B20,DS18B20_12Bit_resolution,0.00) == 0)			// DS18B20 Init
	{
		Genie_Write_String_5_7(0,24,"DS18B20 Init OK");
		DS18B20_Start_Temperatureconvert(&gl_DS18B20,DS18B20_Match_ROM_true);	// Temperatur messen
	}
	else
	{
		Genie_Write_String_5_7(0,24,"DS18B20 Init FEHLER");
	}

	if (ENC_Init()==0)
	{
		Genie_Write_String_5_7(0,32,"ENC28J60 Init OK");
		USART_Write_String("Main: ENC28J60 Init erfolgreich\r\n");
	}
	else
	{
		Genie_Write_String_5_7(0,32,"ENC28J60 Init FEHLER");
	}

	WAV_Init();																	// WAV Init

	if(DS1307_Init()==0)
	{
		Genie_Write_String_5_7(0,40,"DS1307 Init OK");
		USART_Write_String("Main: DS1307 Init erfolgreich: ");
		USART_Write_String(&gl_Timestamp_String[0]);
	}
	else
	{
		Genie_Write_String_5_7(0,40,"DS1307 Init FEHLER");
	}

	if(DHCP_Init()==0)																			// Muss zuerst sein, fordert IPs von DHCP Server an, ansonsten werden die Werte aus dem EEPROM geladen
	{
		Genie_Write_String_5_7(0,48,"DHCP Init OK");
		USART_Write_String("Main: Netzwerkeinstellungen per DHCP bezogen. Einstellungen aus EEPROM gegebenfalls mitverwendet\r\n");
		printf("Main: DHCP Name: %s\r\n",&gl_DHCP_Cache.Name[0]);
	}
	else
	{
		Genie_Write_String_5_7(0,48,"Netzwerk aus EEPROM");
		USART_Write_String("Main: Netzwerkeinstellungen aus EEPROM geladen\r\n");
	}

	if(NTP_Init()==0)																			// Lade Sommerzeit, Zeitzone und fordere Zeitstempel an; IP wird in der DHCP_Init() ermittelt, Uhrzeit in DS1307 gesetzt
	{
		USART_Write_String("Main: Uhrzeit aus Netzwerk bezogen\r\n");
		Genie_Write_String_5_7(0,56,"NTP Uhrzeit bezogen");
	}
	else
	{
		USART_Write_String("Main: NTP Server nicht erreichbar\r\n");
		Genie_Write_String_5_7(0,56,"NTP nicht erreichbar");
	}

	Main_Init_Tasktimer();																		// Tasktimer wird ganz zum Schluss initialisiert

	HTTP_Init();																				// HTTP Init
	Power_Init();																				// Leistungserfassung der PWM Kan�le anschalten
	Weather_Init();																				// Wettersimulation Init

	FAT32_Directory_Change("/");																// Wechsle zur Root
	WAV_Play_File(WAV_Filenumber,"startup.wav");												// Spiele Startup-Sound
	USART_Write_String("Main: Sytem Hochgefahren\r\n\r\n");

	while (1)
	{
		Stack_Packetloop(0);																	// Paketschleife, Kein ARP Reply Filter
	}
}

void Main_Init_Tasktimer(void)	// 5 und 8 sind wieder frei
{
	DS1307_Read_Timestamp(&gl_Time);																		// Hole einmalig die Systemzeit
	Tasktimer_Add_Task(0,Time_Timestamp_to_UTC(&gl_Time),1,&Main_Read_Timestamp,"RTC");						// Clock lesen

	struct Timestamp temp={gl_Time.Day,gl_Time.Month,gl_Time.Year,gl_Time.Hour,00,00,gl_Time.Weekday};		// Zeitstempel f�r einmal die Stunde erstellen
	Tasktimer_Add_Task(1,Time_Timestamp_to_UTC(&temp)+3600,3600,&NTP_Request_Func_for_Tasktimer,"NTPR");	// NTP Request einmal die Stunde

	if (gl_Tasktimer_Tasks[2].Is_used)																		// Falls der Task in DHCP eingeschaltet wird, muss die Zeit angepasst werden
	{
		Tasktimer_Restart_Task(2,Time_Timestamp_to_UTC(&gl_Time)+gl_DHCP_Cache.Lease,gl_DHCP_Cache.Lease);	// Stelle den Task mit der richtigen Zeit ein
	}

	Tasktimer_Add_Task(3,Time_Timestamp_to_UTC(&gl_Time),TCP_Retransmission_Frequency,&TCP_Retransmittimer,"TCPR");	// TCP Retransmission ausf�hren
	Tasktimer_Add_Task(4,Time_Timestamp_to_UTC(&gl_Time),3600,&Tasktimer_Sync_Clock,"Tsyn");				// Einmal die Stunde die Tasktimerclock syncen
	Tasktimer_Add_Task(5,Time_Timestamp_to_UTC(&gl_Time),3600,&Weather_Sync_Tick,"WTck");					// Einmal die Stunde den Weathertick syncen
	Tasktimer_Add_Task(6,Time_Timestamp_to_UTC(&gl_Time),600,&ARP_Clear_Table,"ARPT");						// ARP Tabelle alle 10 Minuten loeschen
	Tasktimer_Add_Task(7,Time_Timestamp_to_UTC(&gl_Time),30,&Main_Genie_refresh,"LCD");						// LCD Text refresh

	Tasktimer_Add_Task(9,Time_Timestamp_to_UTC(&gl_Time),20,&Main_Temperaturelogger,"Temp");				// Beckentemperatur messen

	Tasktimer_Init();																						// Tasktimer Init
}



void Main_Genie_refresh(void)
{
	char Text[22];
	Genie_Clear_Display();
	sprintf(&Text[0],"IP:%u.%u.%u.%u",gl_Webserver.IP_address[0],gl_Webserver.IP_address[1],gl_Webserver.IP_address[2],gl_Webserver.IP_address[3]);
	Genie_Write_String_5_7(0,0,&Text[0]);
	sprintf(&Text[0],"Mask:%u.%u.%u.%u",gl_Webserver.Subnetmask[0],gl_Webserver.Subnetmask[1],gl_Webserver.Subnetmask[2],gl_Webserver.Subnetmask[3]);
	Genie_Write_String_5_7(0,9,&Text[0]);
	sprintf(&Text[0],"Gate:%u.%u.%u.%u",gl_Webserver.Gateway_IP[0],gl_Webserver.Gateway_IP[1],gl_Webserver.Gateway_IP[2],gl_Webserver.Gateway_IP[3]);
	Genie_Write_String_5_7(0,18,Text);
	sprintf(&Text[0],"Becken:%2.2f degC",DS18B20_Read_Temperature(&gl_DS18B20));
	Genie_Write_String_5_7(0,27,Text);
	//sprintf(&Text[0],"NTP:%u.%u.%u.%u",gl_Webserver.NTP_IP[0],gl_Webserver.NTP_IP[1],gl_Webserver.NTP_IP[2],gl_Webserver.NTP_IP[3]);
	//Genie_Write_String_5_7(0,27,Text);
	//Genie_Write_String_5_7(0,36,"DHCP Name:");
	//Genie_Write_String_5_7(0,45,&gl_DHCP_Cache.Name[0]);
	sprintf(&Text[0],"%02u.%02u.%04d %02u:%02u",gl_Time.Day,gl_Time.Month,gl_Time.Year,gl_Time.Hour,gl_Time.Minute);
	Genie_Write_String_5_7(18,56,&Text[0]);
	strcpy(&Text[0],gl_Weekday[gl_Time.Weekday]);
	Text[2] = 0;	// Nur die ersten zwei Buchstaben des Tages verwenden
	Genie_Write_String_5_7(0,56,&Text[0]);
}

void Main_Read_Timestamp(void)
{
	DS1307_Read_Timestamp(&gl_Time);
}

void Main_Temperaturelogger(void)
{
	char buffer[22]={0};
	static uint8_t Day = 1;																// Byte ist statisch

	gl_DS18B20.Temperature = DS18B20_Read_Temperature(&gl_DS18B20);						// Temperatur auslesen
	/*
	FAT32_Directory_Change("/webserv/meassure/");										// Wechsle ins Verzeichnis meassure

	if (gl_Time.Day != Day)
	{
		Day = gl_Time.Day;																// Um 00:00 werden die alten Daten gel�scht
		FAT32_File_Delete("labels.txt");												// Dateien l�schen
		FAT32_File_Delete("temp.txt");

		FAT32_File_Create("labels.txt",0);												// und neu anlegen
		FAT32_File_Create("temp.txt",0);
	}

	FAT32_File_Open(TempLog_Label_Filenumber, "labels.txt",FAT32_Write);				// �ffne die Datei labels.txt
	FAT32_File_Open(TempLog_Temp_Filenumber, "temp.txt",FAT32_Write);					// �ffne die Datei temp.txt

	sprintf(&buffer[0],"%02u:%02u:%02u,",gl_Time.Hour,gl_Time.Minute,gl_Time.Seconds);	// Schreibe den String
	FAT32_File_Write(TempLog_Label_Filenumber,&buffer[0],strlen(buffer));

	sprintf(&buffer[0],"%2.4f,",gl_DS18B20.Temperature);								// Schreibe den String
	FAT32_File_Write(TempLog_Temp_Filenumber,&buffer[0],strlen(buffer));

	FAT32_File_Close(TempLog_Label_Filenumber);
	FAT32_File_Close(TempLog_Temp_Filenumber);
	*/

	DS18B20_Start_Temperatureconvert(&gl_DS18B20,DS18B20_Match_ROM_true);
}

void Main_Stack_Packetloop_stuff(void)
{
	char buffer[5];
	// USART bearbeitet
	if (gl_USART.Command_arrived)																// Behandle den UART
	{
		gl_USART.Command_arrived=0;
		Debug_Commands();
		USART_Write_String("\r\n\r\n");
		USART_Clear_RX_Buffer();
	}

	// Sommer- & Winterzeit einstellen falls n�tig
	if (gl_Time.Day >= 25 && gl_Time.Day <= 31 && gl_Time.Month == 3 && gl_Time.Weekday == 6 && gl_Time.Hour == 2 && gl_NTP_Summertime==0)							// Letzer Sonntag im M�rz ist beginn der Sommerzeit um 2 Uhr
	{
		gl_NTP_Summertime=1;
		FAT32_Directory_Change("/");
		sprintf(&buffer[0],"%hhu",gl_NTP_Summertime);																												// Drucke die Variable in den String
		INI_Write_Key_String("weather.ini","Cooling","Used",&buffer[0]);																							// Schreibe den Wert; // Falls nicht per NTP die Zeit bezogen wird muss man den Zeitstempel nicht nach jedem Neustart einstellen
		gl_Time.Hour++;
		DS1307_Set_Timestamp(&gl_Time);
		Main_Init_Tasktimer();
	}
	if (gl_Time.Day >= 25 && gl_Time.Day <= 31 && gl_Time.Month == 10 && gl_Time.Weekday == 6 && gl_Time.Hour == 3 && gl_NTP_Summertime==1)							// Letzer Sonntag im Oktober ist beginn der Winterzeit um 3 Uhr
	{
		gl_NTP_Summertime=0;
		FAT32_Directory_Change("/");
		sprintf(&buffer[0],"%hhu",gl_NTP_Summertime);																												// Drucke die Variable in den String
		INI_Write_Key_String("weather.ini","Cooling","Used",&buffer[0]);																							// Schreibe den Wert; // Falls nicht per NTP die Zeit bezogen wird muss man den Zeitstempel nicht nach jedem Neustart einstellen
		gl_Time.Hour--;
		DS1307_Set_Timestamp(&gl_Time);
		Main_Init_Tasktimer();
	}

	WAV_Read_Filedata();																																			// Falls ben�tigt, lese die n�chsten Daten der WAV-Datei
	Weather_Init_Trigger();																																			// Falls ben�tigt, triggere die Weather_Init neu

	// Ein Sekunden Task Timer
	Tasktimer_Do_Tasks();																																			// Fuehre die Tasks aus
}
