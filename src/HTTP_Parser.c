// Includes
#include "main.h"
#include "HTTP_Parser.h"
#include "Allerlei.h"
#include "DS1307.h"
#include "DHCP.h"
#include "HTTP.h"
#include "FAT32.h"
#include "Debug.h"
#include "WAV.h"
#include "PWM.h"
#include "weather.h"
#include "Tasktimer.h"
#include "ADC.h"
#include "Power.h"
#include <string.h>
#include <stdio.h>
#include <stm32f4xx.h>
#include "DS18B20.h"
#include "INI_Parser.h"

/*

************************************************************************************************
* HTTP Parser um Platzhalter in Textdatein zu ersetzen  									   *
* 2019 � Frederinn														 		   	   *
************************************************************************************************

*/

// Variablen
struct Parser gl_Parser;																									// Globale Verwaltung der Parservariablen

// Funktionen
void Parser_do_parse_for_GET(void)
{
	if (STRCMP_ALT("Date",&gl_Parser.Buffer[1])==0)																			// Ersetze Buffer mit Datum
	{
		char buff[11];
		sprintf(&gl_Parser.Buffer[0],"%s, %02u.%02u.%04u",strcpy(&buff[0],gl_Weekday[gl_Time.Weekday]),gl_Time.Day,gl_Time.Month,gl_Time.Year);							// Drucke String
		return;
	}
	else if (STRCMP_ALT("Time",&gl_Parser.Buffer[1])==0)																	// Ersetze Buffer mit Zeit
	{ 
		sprintf(&gl_Parser.Buffer[0],"%02u:%02u:%02u",gl_Time.Hour,gl_Time.Minute,gl_Time.Seconds);							// Drucke String
		return;
	}
	else if (STRCMP_ALT("Debugbaud",&gl_Parser.Buffer[1])==0)																// Ersetze Debugbaud mit der Baud der Seriellen Debugschnittstelle
	{
		sprintf(&gl_Parser.Buffer[0],"%lu",gl_USART_Baud);																	// Drucke String
		return;
	}
	else if (STRCMP_ALT("IP",&gl_Parser.Buffer[1])==0)																		// Ersetze IP mit Webserver IP
	{
		sprintf(&gl_Parser.Buffer[0],"%u.%u.%u.%u",gl_Webserver.IP_address[0],gl_Webserver.IP_address[1],gl_Webserver.IP_address[2],gl_Webserver.IP_address[3]);	// Drucke String
		return;
	}
	else if (STRCMP_ALT("NTP",&gl_Parser.Buffer[1])==0)																		// Ersetze NTP mit Webserver NTP Adresse
	{
		sprintf(&gl_Parser.Buffer[0],"%u.%u.%u.%u",gl_Webserver.NTP_IP[0],gl_Webserver.NTP_IP[1],gl_Webserver.NTP_IP[2],gl_Webserver.NTP_IP[3]);	// Drucke String
		return;
	}
	else if (STRCMP_ALT("Subnetmask",&gl_Parser.Buffer[1])==0)																// Ersetze Subnetmask mit Webserver Subnetzmaske
	{
		sprintf(&gl_Parser.Buffer[0],"%u.%u.%u.%u",gl_Webserver.Subnetmask[0],gl_Webserver.Subnetmask[1],gl_Webserver.Subnetmask[2],gl_Webserver.Subnetmask[3]);	// Drucke String
		return;
	}
	else if (STRCMP_ALT("MAC",&gl_Parser.Buffer[1])==0)																		// Ersetze MAC mit MAC des Webservers
	{
		sprintf(&gl_Parser.Buffer[0],"%02X:%02X:%02X:%02X:%02X:%02X",gl_Webserver.MAC[0],gl_Webserver.MAC[1],gl_Webserver.MAC[2],gl_Webserver.MAC[3],gl_Webserver.MAC[4],gl_Webserver.MAC[5]);	// Drucke String
		return;
	}
	else if (STRCMP_ALT("DHCP",&gl_Parser.Buffer[1])==0)																	// Ersetze DHCP durch den DHCP Namen
	{
		sprintf(&gl_Parser.Buffer[0],"%s",gl_DHCP_Cache.Name);																// Schreibe den String
		return;
	}
	else if (STRCMP_ALT("Gateway",&gl_Parser.Buffer[1])==0)																	// Ersetze Gateway durch die Gateway IP Namen
	{
		sprintf(&gl_Parser.Buffer[0],"%u.%u.%u.%u",gl_Webserver.Gateway_IP[0],gl_Webserver.Gateway_IP[1],gl_Webserver.Gateway_IP[2],gl_Webserver.Gateway_IP[3]);	// Drucke String
		return;
	}
	else if (STRCMP_ALT("Uptime",&gl_Parser.Buffer[1])==0)																	// Ersetze Uptime mit gl_Tasktimer_Uptime
	{
		uint32_t Tasktimer = gl_Tasktimer_Uptime;																			// Lokale Variable
		
		uint32_t days = Tasktimer / 86400UL;																				// Tage
		Tasktimer -= days * 86400UL;
		
		uint32_t hours = Tasktimer / 3600;																					// Stunden
		Tasktimer -= hours * 3600;
		
		uint32_t minutes = Tasktimer / 60;																					// Minuten
		Tasktimer -= minutes * 60;
		
		uint32_t seconds = Tasktimer;																						// Sekunden
		
		sprintf(&gl_Parser.Buffer[0],"%lud %luh %lum %lus\r\n",days,hours,minutes,seconds);									// Drucke String
		return;
	}
	else if (STRCMP_ALT("WAV_Volume",&gl_Parser.Buffer[1])==0)																// WAV_Volume
	{
		sprintf(&gl_Parser.Buffer[0],"%3.2f",gl_WAV.Volume*100.0);															// Drucke String
		return;
	}
	else if (STRCMP_ALT("WAV_File",&gl_Parser.Buffer[1])==0)																// WAV_File1, WAV_File2, ...
	{
		uint8_t File;
		char Filename[13];

		sscanf(&gl_Parser.Buffer[9],"%hhu",&File);																			// Wert holen

		FAT32_Directory_Change("/webserv/music/");
		if(FAT32_Directory_List_Entry_from_Position(&Filename[0],File))
		{
			sprintf(&Filename[0],"Leer");
		}
		sprintf(&gl_Parser.Buffer[0],"%s",&Filename[0]);																	// Drucke String
		return;

	}
	else if (STRCMP_ALT("PWM",&gl_Parser.Buffer[1])==0)																		// PWM0-6, PWM0, PWM0_I
	{
		uint8_t Channel = gl_Parser.Buffer[4]-0x30;																			// Channel

		/*if (STRCMP_ALT_P(PSTR("_I"),&gl_Parser.Buffer[5])==0)																// Lade die Kanaleinstellung
		{
			sprintf_P(&gl_Parser.Buffer[0],PSTR("%1.3fA"),gl_Power.PWM_Channel[Channel].Current);							// Drucke den Strom
		}*/
		//else
		//{
			sprintf(&gl_Parser.Buffer[0],"%3.2f",gl_Weather.Channel[Channel].Duty);											// Drucke aktuellen Wert in String
		//}
		return;
	}
	else if (STRCMP_ALT("Wartungszustand",&gl_Parser.Buffer[1])==0)															// Wartungszustand
	{
		sprintf(&gl_Parser.Buffer[0],"%u",gl_Weather.Maintenance);															// Drucke String
		return;
	}
	else if (STRCMP_ALT("Systemp",&gl_Parser.Buffer[1])==0)																	// Systemp
	{
		sprintf(&gl_Parser.Buffer[0],"%2.3f",ADC1_Read_internal_Temperaturesensor());										// Drucke String
		return;
	}
	else if (STRCMP_ALT("Supplyvoltage",&gl_Parser.Buffer[1])==0)															// Ersetze Supplyvoltage mit der Eingangsspannung
	{
		sprintf(&gl_Parser.Buffer[0],"%2.2f V",Power_Read_Supplyvoltage());													// Drucke String
		return;
	}
	else if (STRCMP_ALT("DS18B20_temp",&gl_Parser.Buffer[1])==0)															// DS18B20_temp
	{
		sprintf(&gl_Parser.Buffer[0],"%2.4f",DS18B20_Read_Temperature(&gl_DS18B20));										// Drucke String
		return;
	}
	else if (STRCMP_ALT("Setting_Chan",&gl_Parser.Buffer[1])==0)															// Setting_Chan0 ... Setting_Chan5
	{
		sprintf(&gl_Parser.Buffer[0],"%u",gl_Weather.Channel[gl_Parser.Buffer[13]-0x30].Setting);							// Drucke String
		return;
	}
	else if (STRCMP_ALT("Tempcontrol_in_use",&gl_Parser.Buffer[1])==0)														// Tempcontrol_in_use
	{
		sprintf(&gl_Parser.Buffer[0],"%u",gl_Weather.Cooling.used);															// Drucke String
		return;
	}
	else if (STRCMP_ALT("Tempcontrol_trig",&gl_Parser.Buffer[1])==0)														// Tempcontrol_trig
	{
		sprintf(&gl_Parser.Buffer[0],"%2.2f",gl_Weather.Cooling.Treshold);													// Drucke String
		return;
	}
	else if (STRCMP_ALT("Tempcontrol_fan",&gl_Parser.Buffer[1])==0)															// Tempcontrol_fan
	{
		sprintf(&gl_Parser.Buffer[0],"%u",gl_Weather.Cooling.Fan_Channel);													// Drucke String
		return;
	}
	else if (gl_Parser.Buffer[1]==Parser_Ident_char && strlen(gl_Parser.Buffer)==2)											// Um den Parseridentchar normal nutzen zu k�nnen muss dieser als Variable eingef�gt werden. Bsp.: %% => %
	{
		gl_Parser.Buffer[0]=Parser_Ident_char;
		gl_Parser.Buffer[1]=0;
		return;
	}
	else																													// Alle Strings die nicht konvertiert werden k�nnen werden zur�ck gegeben
	{
		uint16_t buflen = strlen(&gl_Parser.Buffer[0]);
		if (gl_Parser.Buffer[buflen]==Parser_Ident_char)
		{
			gl_Parser.Buffer[buflen+1] = 0;
		} 
		else
		{
			gl_Parser.Buffer[buflen+1] = Parser_Ident_char;
			gl_Parser.Buffer[buflen+2] = 0;
		}
	}
}

void Parser_do_parse_for_POST(void)
{	
	if (STRCMP_ALT("Wartung",&gl_HTTP_Head_read.Data[0])==0)							// Wartung=True, Wartung=False
	{
		if (gl_HTTP_Head_read.Data[8]=='F')												// Wenn Wartung=False, dann mache eine Neuinitialisierung der Wettersimu
		{
			Weather_Init();																// Wetter neustarten
			gl_Weather.Maintenance=0;													// Keine Wartung
		}
		else
		{
			Weather_Pause();															// Pausiert die Wettersimulation, wenn Wartung=True
			gl_Weather.Maintenance=1;													// Wartung an
		}
		return;
	}
	else if (STRCMP_ALT("WeatherInit",&gl_HTTP_Head_read.Data[0])==0)					// WeatherInit=1
	{
		Weather_Pause();
		Weather_Init();
		return;
	}
	else if (STRCMP_ALT("WAV",&gl_HTTP_Head_read.Data[0])==0)							// WAV=Play V 50, WAV=Play firestar.wav
	{
		char Text[13];
		float Volume;

		sscanf(&gl_HTTP_Head_read.Data[9],"%s",&Text[0]);

		if(strstr(&gl_HTTP_Head_read.Data[9],"WAV") || strstr(&gl_HTTP_Head_read.Data[9],".wav"))
		{
			FAT32_Directory_Change("/webserv/music/");
			WAV_Play_File(WAV_Filenumber,&Text[0]);
			return;
		}
		else if(gl_HTTP_Head_read.Data[9] == 'S')
		{
			WAV_Stop();
			return;
		}
		else if(gl_HTTP_Head_read.Data[9] == 'P')
		{
			WAV_Pause();
			return;
		}
		else if(gl_HTTP_Head_read.Data[9] == 'C')
		{
			WAV_Continue();
			return;
		}
		else if(gl_HTTP_Head_read.Data[9] == 'V')
		{
			sscanf(&gl_HTTP_Head_read.Data[10],"%f",&Volume);
			WAV_Volume(Volume);
			return;
		}
		return;
	}
	else if(STRCMP_ALT("PWM",&gl_HTTP_Head_read.Data[0])==0)									// PWM0_0=100
	{
		uint8_t Channel = gl_HTTP_Head_read.Data[3]-0x30;										// Channel
		float Duty = 0;

		if (STRCMP_ALT("_0",&gl_HTTP_Head_read.Data[4])==0)										// Setze Channel
		{
			sscanf(&gl_HTTP_Head_read.Data[7],"%f",&Duty);										// Wert holen

			switch(gl_Weather.Channel[Channel].Setting)
			{
				case 1:			// Growx5
				case 2:			// Sunset
				case 3:			// SKY
				case 4:			// DAY
				case 5:			// TROPIC
					PWM_Channel_set(Channel,(pow(10.0,(Duty/96.0))-1) * 10.0 * (PWM_Max / 100.0));		// Nur f�r LED
				break;

				case 6:			// L�fter
				case 7:			// CO2
					PWM_Channel_set(Channel,Duty*(PWM_Max / 100.0));
				break;

				default:		// Bei 0 und unbekannten Einstellungen setze den Kanal auf 0
					PWM_Channel_set(Channel,0);
				break;
			}
		}
		return;
	}
	else if(STRCMP_ALT("chanset",&gl_HTTP_Head_read.Data[0])==0)								// chanset=0,0,0,0,0,0
	{
		sscanf(&gl_HTTP_Head_read.Data[8],"%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",&gl_Weather.Channel[0].Setting,&gl_Weather.Channel[1].Setting,&gl_Weather.Channel[2].Setting,&gl_Weather.Channel[3].Setting,&gl_Weather.Channel[4].Setting,&gl_Weather.Channel[5].Setting);
		FAT32_Directory_Change("/webserv/planning/");
		INI_Write_Key_String("weather.ini","Channelsetting","Channel",&gl_HTTP_Head_read.Data[8]);
		return;
	}
	if (STRCMP_ALT("Tempcontrol",&gl_HTTP_Head_read.Data[0])==0)								// Tempcontrol=True, Tempcontrol=False
	{
		if (gl_HTTP_Head_read.Data[12]=='F')													// Wenn Tempcontrol=False, dann schalte die Temperaturcontrolle aus, sonst an
		{
			Weather_Tempcontrol(Weather_Tempcontrol_Off);										// Temperatursteuerung aus
		}
		else
		{
			Weather_Tempcontrol(Weather_Tempcontrol_On);										// Temperatursteuerung aus
		}
		return;
	}
	else if (STRCMP_ALT("tempconsetting",&gl_HTTP_Head_read.Data[0])==0)						// tempconsetting=23.00,2
	{
		gl_HTTP_Head_read.Data[14] = 0;															// Mache nach tmp.txt eine Nullterminierung des Strings hin
		Weather_Save_Tempcontrol(&gl_HTTP_Head_read.Data[0],&gl_HTTP_Head_read.Data[15]);		// speichere die Temperatursteuerungseinstellungen ab
		return;
	}
	else if (STRCMP_ALT(".txt",&gl_HTTP_Head_read.Data[3])==0)									// day.txt=,
	{
		gl_HTTP_Head_read.Data[7] = 0;															// Mache nach mon.txt eine Nullterminierung des Strings hin
		Weather_Save_Day(&gl_HTTP_Head_read.Data[0],&gl_HTTP_Head_read.Data[8]);
		return;
	}
	else if (STRCMP_ALT("NTP",&gl_HTTP_Head_read.Data[0])==0)									// NTP?255.255.255.255
	{
		sscanf(&gl_HTTP_Head_read.Data[4],"%hhu.%hhu.%hhu.%hhu",&gl_Webserver.NTP_IP[0],&gl_Webserver.NTP_IP[1],&gl_Webserver.NTP_IP[2],&gl_Webserver.NTP_IP[3]);
		FAT32_Directory_Change("/");
		INI_Write_Key_String("basic.ini","Netz","NTP",&gl_HTTP_Head_read.Data[4]);
		return;
	}
	else if (STRCMP_ALT("IP",&gl_HTTP_Head_read.Data[0])==0)									// IP=255.255.255.255
	{
		sscanf(&gl_HTTP_Head_read.Data[3],"%hhu.%hhu.%hhu.%hhu",&gl_Webserver.IP_address[0],&gl_Webserver.IP_address[1],&gl_Webserver.IP_address[2],&gl_Webserver.IP_address[3]);
		FAT32_Directory_Change("/");
		INI_Write_Key_String("basic.ini","Netz","IP",&gl_HTTP_Head_read.Data[3]);
		return;
	}
	else if (STRCMP_ALT("MAC",&gl_HTTP_Head_read.Data[0])==0)									// MAC=ff:ff:ff:ff:ff:ff
	{
		sscanf(&gl_HTTP_Head_read.Data[4],"%02hx:%02hx:%02hx:%02hx:%02hx:%02hx",(uint16_t *)&gl_Webserver.MAC[0],(uint16_t *)&gl_Webserver.MAC[1],(uint16_t *)&gl_Webserver.MAC[2],(uint16_t *)&gl_Webserver.MAC[3],(uint16_t *)&gl_Webserver.MAC[4],(uint16_t *)&gl_Webserver.MAC[5]);	// Scan hex address
		FAT32_Directory_Change("/");
		INI_Write_Key_String("basic.ini","Netz","MAC",&gl_HTTP_Head_read.Data[4]);
		return;
	}
	else if (STRCMP_ALT("Subnetmask",&gl_HTTP_Head_read.Data[0])==0)							// Subnetmask=255.255.255.255
	{
		sscanf(&gl_HTTP_Head_read.Data[11],"%hhu.%hhu.%hhu.%hhu",&gl_Webserver.Subnetmask[0],&gl_Webserver.Subnetmask[1],&gl_Webserver.Subnetmask[2],&gl_Webserver.Subnetmask[3]);
		FAT32_Directory_Change("/");
		INI_Write_Key_String("basic.ini","Netz","Sub",&gl_HTTP_Head_read.Data[11]);
		return;
	}
	else if(STRCMP_ALT("Gateway",&gl_HTTP_Head_read.Data[0])==0)								// Gateway=192.168.178.001
	{
		sscanf(&gl_HTTP_Head_read.Data[8],"%hhu.%hhu.%hhu.%hhu",&gl_Webserver.Gateway_IP[0],&gl_Webserver.Gateway_IP[1],&gl_Webserver.Gateway_IP[2],&gl_Webserver.Gateway_IP[3]);
		FAT32_Directory_Change("/");
		INI_Write_Key_String("basic.ini","Netz","Gate",&gl_HTTP_Head_read.Data[8]);
		return;
	}
}
