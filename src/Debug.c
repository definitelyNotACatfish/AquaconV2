// Includes
#include "main.h"
#include "Debug.h"
//#include "delay.h"		// Ist in main.h als Inline
#include <stm32f4xx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "USART.h"
#include "DS1307.h"
#include "Stack.h"
#include "ENC28J60.h"
#include "HTTP.h"
#include "Allerlei.h"
#include "FAT32.h"
#include "NTP.h"
#include "DHCP.h"
#include "Tasktimer.h"
#include "HTTP_Parser.h"
#include "Geniedisplay.h"
#include "SPI.h"
#include "wav.h"
#include "ADC.h"
#include "Power.h"
#include "INI_Parser.h"

/*

************************************************************************************************
* Command Interface fuer den Webserver ueber den USART auf dem STM32F401RET6				   *
* 2019 � Frederinn															 	   	   *
************************************************************************************************

*/

// Globale Variablen
uint32_t gl_USART_Baud=0;																// Globale Baud fuer den USART, 256000UL

void Debug_Command_Init(void)
{
	char baud[8];
	FAT32_Directory_Change("/");
	INI_Read_Key_String("basic.ini","Debug","Baud",&baud[0]);							// Lese den Baud Key aus der Ini
	sscanf(&baud[0],"%lu",&gl_USART_Baud);												// wandle string in uint32
	USART_Init(gl_USART_Baud);															// USART Init
	USART_Clear_RX_Buffer();
	USART_Write_String("Debugconsole: gestaret\r\n");
}

void Debug_Commands(void)
{
	USART_Write_X_Bytes(&gl_USART.Buffer_rx[0],0,gl_USART.U0rx-1);
	USART_Write_String(":\r\n");
	
	if (STRCMP_ALT("Neustart",gl_USART.Buffer_rx)==0)
	{
		Genie_Clear_Display();
		Genie_Write_String_5_7(0,0,"USART Neustart");
		USART_Write_String("System startet neu!\r\n");
		_delay_us(2000000);
		IWDG->KR = 0x00005555;																	// Schreibzugriff auf Watchdogregister
		IWDG->PR = 0;																			// Teiler auf /4
		IWDG->RLR = 0x01;																		// Wert auf 1
		IWDG->KR = 0x0000cccc;																	// Watchdogtimer einschalten
		while(1);																				// Endlosschleife loest Watchdog auswhile(1);
		return;
	}
	
	else if (STRCMP_ALT("Info",gl_USART.Buffer_rx)==0)
	{	
		uint8_t Version;
		USART_Write_String("STM32 trifft World Wide Web.\r\n");
		USART_Write_String("Geschrieben von Frederik  2019\r\n");
		USART_Write_String("Built am "__TIMESTAMP__"\r\n");
		USART_Write_String("GCC Version: "__VERSION__ "\r\n\r\n");
		
		USART_Write_String("CPU: STM32F407VGT6\r\n");
		USART_Write_String("Systemtakt: 168MHz\r\n");
		USART_Write_String("Lan: ENC28J60\r\n");
		Version = ENC_Read_Hardwareversion();
		USART_Write_String("ENC Version: B");
		switch(Version)
		{
			case 0b00000010:
				USART_Write_String("1");
			break;
			case 0b00000100:
				USART_Write_String("4");
			break;
			case 0b00000101:
				USART_Write_String("5");
			break;
			case 0b00000110:
				USART_Write_String("7");
			break;
			default:
				USART_Write_String("?");
			break;
		}
		USART_Write_String("\r\n");
		USART_Write_String("Speicher: Micro SD / SDXD Card\r\n");
		USART_Write_String("RTC: DS1307\r\n");
		printf("Baud Debug USART: %lu 8N1\r\n",gl_USART_Baud);
		DS1307_Read_Timestamp(&gl_Time);
		USART_Write_String("Aktuelles Datum: ");
		USART_Write_String(gl_Timestamp_String);
		USART_Write_String("Hinweis: Server arbeitet mit Sommer- und Winterzeit!\r\n\r\n");
		
		printf("HTTP:\r\nStartseite: %s\r\n",gl_Default_Page);
		printf("Standardordner: %s\r\n",gl_Default_Dir);
		printf("404 Datei: %s\r\n\r\n",gl_Default_404);
		
		printf("DHCP Cache:\r\nName: %s\r\n",gl_DHCP_Cache.Name);
		printf("Erhaltene IP: %u.%u.%u.%u\r\n",gl_DHCP_Cache.Own_IP[0],gl_DHCP_Cache.Own_IP[1],gl_DHCP_Cache.Own_IP[2],gl_DHCP_Cache.Own_IP[3]);
		printf("Subnetzmaske: %u.%u.%u.%u\r\n",gl_DHCP_Cache.Subnetmask[0],gl_DHCP_Cache.Subnetmask[1],gl_DHCP_Cache.Subnetmask[2],gl_DHCP_Cache.Subnetmask[3]);
		printf("NTP: %u.%u.%u.%u\r\n",gl_DHCP_Cache.NTP[0],gl_DHCP_Cache.NTP[1],gl_DHCP_Cache.NTP[2],gl_DHCP_Cache.NTP[3]);
		printf("Gateway: %u.%u.%u.%u\r\n",gl_DHCP_Cache.Gateway_IP[0],gl_DHCP_Cache.Gateway_IP[1],gl_DHCP_Cache.Gateway_IP[2],gl_DHCP_Cache.Gateway_IP[3]);
		printf("Leasing Time: %lu\r\n\r\n",gl_DHCP_Cache.Lease);
		
		printf("Netzwerkeinstellungen:\r\nMAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",gl_Webserver.MAC[0],gl_Webserver.MAC[1],gl_Webserver.MAC[2],gl_Webserver.MAC[3],gl_Webserver.MAC[4],gl_Webserver.MAC[5]);
		printf("IP: %u.%u.%u.%u\r\n",gl_Webserver.IP_address[0],gl_Webserver.IP_address[1],gl_Webserver.IP_address[2],gl_Webserver.IP_address[3]);
		printf("Subnetzmaske: %u.%u.%u.%u\r\n",gl_Webserver.Subnetmask[0],gl_Webserver.Subnetmask[1],gl_Webserver.Subnetmask[2],gl_Webserver.Subnetmask[3]);
		printf("Gateway: %u.%u.%u.%u\r\n",gl_Webserver.Gateway_IP[0],gl_Webserver.Gateway_IP[1],gl_Webserver.Gateway_IP[2],gl_Webserver.Gateway_IP[3]);
		printf("Standard NTP Server: %u.%u.%u.%u\r\n",gl_Webserver.NTP_IP[0],gl_Webserver.NTP_IP[1],gl_Webserver.NTP_IP[2],gl_Webserver.NTP_IP[3]);
		printf("Zeitzone Server: %1.2f\r\n",gl_NTP_Timezone);
		printf("Sommer-/Winterzeit: %u\r\n",gl_NTP_Summertime);
		printf("Uptime des Servers: %lu Sekunden\r\n",gl_Tasktimer_Uptime);
		ENC_Bankjump(ENC_Bank1);
		printf("Pakete im Buffer: %u\r\n",ENC_Read_ETH_Register(EPKTCNT_B1));
		return;
	}
	else if (STRCMP_ALT("IP",gl_USART.Buffer_rx)==0)					// IP 255.255.255.255
	{
		if (gl_USART.Buffer_rx[3]!='\n')
		{
			sscanf(&gl_USART.Buffer_rx[2],"%hhu.%hhu.%hhu.%hhu",&gl_Webserver.IP_address[0],&gl_Webserver.IP_address[1],&gl_Webserver.IP_address[2],&gl_Webserver.IP_address[3]);
			char *end = strstr(&gl_USART.Buffer_rx[3],"\r");
			*end=0;
			FAT32_Directory_Change("/");
			INI_Write_Key_String("basic.ini","Netz","IP",&gl_USART.Buffer_rx[3]);		// Schreibe den Wert
			printf("Neue IP: %u.%u.%u.%u\r\n",gl_Webserver.IP_address[0],gl_Webserver.IP_address[1],gl_Webserver.IP_address[2],gl_Webserver.IP_address[3]);
		}
		else
		{
			printf("IP: %u.%u.%u.%u\r\n",gl_Webserver.IP_address[0],gl_Webserver.IP_address[1],gl_Webserver.IP_address[2],gl_Webserver.IP_address[3]);
		}
		return;
	}
	else if (STRCMP_ALT("MAC",gl_USART.Buffer_rx)==0)					// Mac ff:ff:ff:ff:ff:ff
	{
		if (gl_USART.Buffer_rx[4]!='\n')
		{
			sscanf(&gl_USART.Buffer_rx[3],"%02hx:%02hx:%02hx:%02hx:%02hx:%02hx",(uint16_t *)&gl_Webserver.MAC[0],(uint16_t *)&gl_Webserver.MAC[1],(uint16_t *)&gl_Webserver.MAC[2],(uint16_t *)&gl_Webserver.MAC[3],(uint16_t *)&gl_Webserver.MAC[4],(uint16_t *)&gl_Webserver.MAC[5]);	// Scan hex address
			char *end = strstr(&gl_USART.Buffer_rx[4],"\r");
			*end=0;
			FAT32_Directory_Change("/");
			INI_Write_Key_String("basic.ini","Netz","MAC",&gl_USART.Buffer_rx[4]);		// Schreibe den Wert
			printf("Neue MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",gl_Webserver.MAC[0],gl_Webserver.MAC[1],gl_Webserver.MAC[2],gl_Webserver.MAC[3],gl_Webserver.MAC[4],gl_Webserver.MAC[5]);
		}
		else
		{
			printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",gl_Webserver.MAC[0],gl_Webserver.MAC[1],gl_Webserver.MAC[2],gl_Webserver.MAC[3],gl_Webserver.MAC[4],gl_Webserver.MAC[5]);
		}
		return;
	}
	else if (STRCMP_ALT("Subnetzmaske",gl_USART.Buffer_rx)==0)							// Subnetzmaske 255.255.255.255
	{
		if (gl_USART.Buffer_rx[13]!='\n')
		{
			sscanf(&gl_USART.Buffer_rx[12],"%hhu.%hhu.%hhu.%hhu",&gl_Webserver.Subnetmask[0],&gl_Webserver.Subnetmask[1],&gl_Webserver.Subnetmask[2],&gl_Webserver.Subnetmask[3]);
			char *end = strstr(&gl_USART.Buffer_rx[13],"\r");
			*end=0;
			FAT32_Directory_Change("/");
			INI_Write_Key_String("basic.ini","Netz","Sub",&gl_USART.Buffer_rx[13]);		// Schreibe den Wert

			printf("Neue Subnetzmaske: %u.%u.%u.%u\r\n",gl_Webserver.Subnetmask[0],gl_Webserver.Subnetmask[1],gl_Webserver.Subnetmask[2],gl_Webserver.Subnetmask[3]);
		} 
		else
		{
			printf("Subnetzmaske: %u.%u.%u.%u\r\n",gl_Webserver.Subnetmask[0],gl_Webserver.Subnetmask[1],gl_Webserver.Subnetmask[2],gl_Webserver.Subnetmask[3]);
		}
		return;
	}
	
	else if(STRCMP_ALT("Gateway",gl_USART.Buffer_rx)==0)	// Gateway 192.168.178.001
	{
		if (gl_USART.Buffer_rx[8]!='\n')
		{
			sscanf(&gl_USART.Buffer_rx[7],"%hhu.%hhu.%hhu.%hhu",&gl_Webserver.Gateway_IP[0],&gl_Webserver.Gateway_IP[1],&gl_Webserver.Gateway_IP[2],&gl_Webserver.Gateway_IP[3]);
			char *end = strstr(&gl_USART.Buffer_rx[8],"\r");
			*end=0;
			FAT32_Directory_Change("/");
			INI_Write_Key_String("basic.ini","Netz","Gate",&gl_USART.Buffer_rx[8]);		// Schreibe den Wert
			printf("Neues Gateway: %u.%u.%u.%u\r\n",gl_Webserver.Gateway_IP[0],gl_Webserver.Gateway_IP[1],gl_Webserver.Gateway_IP[2],gl_Webserver.Gateway_IP[3]);
		}
		else
		{
			printf("Gateway: %u.%u.%u.%u\r\n",gl_Webserver.Gateway_IP[0],gl_Webserver.Gateway_IP[1],gl_Webserver.Gateway_IP[2],gl_Webserver.Gateway_IP[3]);
		}
		return;
	}
	else if (STRCMP_ALT("NTP",gl_USART.Buffer_rx)==0)									// NTP 255.255.255.255
	{
		if (gl_USART.Buffer_rx[4]=='R')													// NTP R
		{
			if (NTP_Request_Time_from_Server(&gl_Webserver.NTP_IP[0]))
			{
				USART_Write_String("NTP nicht aktualisiert\r\n");
			}
			else
			{
				USART_Write_String("NTP aktualisiert\r\n");
				Main_Init_Tasktimer();													// Wenn die neue Systemzeit vorliegt, starte den Tasktimer neu
			}
		}
		else if (gl_USART.Buffer_rx[4]!='\n')
		{
			sscanf(&gl_USART.Buffer_rx[3],"%hhu.%hhu.%hhu.%hhu",&gl_Webserver.NTP_IP[0],&gl_Webserver.NTP_IP[1],&gl_Webserver.NTP_IP[2],&gl_Webserver.NTP_IP[3]);
			char *end = strstr(&gl_USART.Buffer_rx[4],"\r");
			*end=0;
			FAT32_Directory_Change("/");
			INI_Write_Key_String("basic.ini","Netz","NTP",&gl_USART.Buffer_rx[4]);		// Schreibe den Wert
			printf("Neuer NTP Standardserver IP: %u.%u.%u.%u\r\n",gl_Webserver.NTP_IP[0],gl_Webserver.NTP_IP[1],gl_Webserver.NTP_IP[2],gl_Webserver.NTP_IP[3]);
		}
		else
		{
			printf("NTP Standardserver IP: %u.%u.%u.%u\r\n",gl_Webserver.NTP_IP[0],gl_Webserver.NTP_IP[1],gl_Webserver.NTP_IP[2],gl_Webserver.NTP_IP[3]);
		}
		return;
	}
	else if (STRCMP_ALT("Systemzeit",gl_USART.Buffer_rx)==0)							// Systemzeit 15:22:59 22.12.1992 Di
	{
		struct Timestamp Zeit;
		uint8_t Wochentag1=0, Wochentag2=0;
		Zeit.Weekday=0;
		
		if (gl_USART.Buffer_rx[11]!='\n')
		{
			sscanf(&gl_USART.Buffer_rx[10],"%hhu:%hhu:%hhu %hhu.%hhu.%hu %c%c",&Zeit.Hour,&Zeit.Minute,&Zeit.Seconds,&Zeit.Day,&Zeit.Month,&Zeit.Year,&Wochentag1,&Wochentag2);

			Wochentag1 += Wochentag2;
			switch(Wochentag1)
			{
				case 'M'+'o':
					Zeit.Weekday=0;
				break;
				
				case 'D'+'i':
					Zeit.Weekday=1;
				break;
				
				case 'M'+'i':
					Zeit.Weekday=2;
				break;
				
				case 'D'+'o':
					Zeit.Weekday=3;
				break;
				
				case 'F'+'r':
					Zeit.Weekday=4;
				break;
				
				case 'S'+'a':
					Zeit.Weekday=5;
				break;
				
				case 'S'+'o':
					Zeit.Weekday=6;
				break;
				
				default:
					Zeit.Weekday=0;
				break;
			}
			DS1307_Set_Timestamp(&Zeit);
			Main_Init_Tasktimer();																				// Aufgaben erneuern
		}
		DS1307_Read_Timestamp(&gl_Time);
		USART_Write_String(gl_Timestamp_String);
		USART_Write_String("Sommerzeitbit muss eventuell gestellt werden\r\n");
		return;
	}
	else if (STRCMP_ALT("ARP",gl_USART.Buffer_rx)==0)			// ARP //Gibt die Aktuelle ARP Tabelle aus
	{
		USART_Write_String("ARP Tabelle:\r\n");
		for (uint8_t g=0;g<ARP_Max_Entries;g++)
		{
			printf("IP: %u.%u.%u.%u MAC: %02X:%02X:%02X:%02X:%02X:%02X Used: %u\r\n",gl_ARP_Table[g].IP[0],gl_ARP_Table[g].IP[1],gl_ARP_Table[g].IP[2],gl_ARP_Table[g].IP[3],gl_ARP_Table[g].MAC[0],gl_ARP_Table[g].MAC[1],gl_ARP_Table[g].MAC[2],gl_ARP_Table[g].MAC[3],gl_ARP_Table[g].MAC[4],gl_ARP_Table[g].MAC[5],gl_ARP_Table[g].Used);
		}
		return;
	}
	else if (STRCMP_ALT("TCP",gl_USART.Buffer_rx)==0)
	{
		char Status[14];
		
		if (gl_USART.Buffer_rx[4] == 'A')
		{
			for (uint8_t g=0;g<TCP_Max_Entries;g++)
			{
				strcpy(&Status[0],&gl_TCP_Status_Name_Table[gl_TCP_Table[g].Status][0]);
				printf("In Use: %u, Status: %s, TMSS: %u, Windowscale: %u, CMSS: %u, CIP: %03u.%03u.%03u.%03u, DPort: %u, SPort: %u, Seqnum: %lu, Acknum: %lu, Packetstatus: %u, Retranscount: %u, MAC: %02x:%02x:%02x:%02x:%02x:%02x, Rlength: %u, SRAMAdr: %u, FP: %p\r\n",gl_TCP_Table[g].In_use, Status,gl_TCP_Table[g].Transaction_MSS,gl_TCP_Table[g].Client_Windowscale,gl_TCP_Table[g].Client_MSS,gl_TCP_Table[g].Client_IP[0],gl_TCP_Table[g].Client_IP[1],gl_TCP_Table[g].Client_IP[2],gl_TCP_Table[g].Client_IP[3],gl_TCP_Table[g].Destport,gl_TCP_Table[g].Sourceport,gl_TCP_Table[g].Sequencenumber,gl_TCP_Table[g].Acknumber,gl_TCP_Table[g].Packetstatus,gl_TCP_Table[g].Retransmission.Retransmitcount,gl_TCP_Table[g].Retransmission.MAC[0],gl_TCP_Table[g].Retransmission.MAC[1],gl_TCP_Table[g].Retransmission.MAC[2],gl_TCP_Table[g].Retransmission.MAC[3],gl_TCP_Table[g].Retransmission.MAC[4],gl_TCP_Table[g].Retransmission.MAC[5],gl_TCP_Table[g].Retransmission.Length,gl_TCP_Table[g].Retransmission.SRAM_Address,*gl_TCP_FP_Table[g]);
			}
		}
		else
		{
			for (uint8_t g=0;g<TCP_Max_Entries;g++)
			{
				printf("In Benutzung: %u, IP: %u.%u.%u.%u, ",gl_TCP_Table[g].In_use, gl_TCP_Table[g].Client_IP[0], gl_TCP_Table[g].Client_IP[1], gl_TCP_Table[g].Client_IP[2], gl_TCP_Table[g].Client_IP[3]);
				printf("Cl.port: %u, Ser.port: %u, ",gl_TCP_Table[g].Destport, gl_TCP_Table[g].Sourceport);
				strcpy(&Status[0],&gl_TCP_Status_Name_Table[gl_TCP_Table[g].Status][0]);
				printf("Verbingungsstatus: %s, ", Status);
				printf("Paketstatus: %u\r\n",gl_TCP_Table[g].Packetstatus);
			}
		}
		return;
	}
	else if (STRCMP_ALT("Baud",gl_USART.Buffer_rx)==0)
	{	
		if (gl_USART.Buffer_rx[5]!='\n')
		{
			gl_USART_Baud = atol(&gl_USART.Buffer_rx[5]);						// Brauche den String als uint32
			char *end = strstr(&gl_USART.Buffer_rx[5],"\r");					// Mache aus dem \r am Ende des String eine 0
			*end=0;
			FAT32_Directory_Change("/");
			INI_Write_Key_String("basic.ini","Debug","Baud",&gl_USART.Buffer_rx[5]);	// Schreibe die Baud in den Key in der Basic.ini

			printf("Neue Baud: %lu\r\n",gl_USART_Baud);
			USART_Init(gl_USART_Baud);
		}
		else
		{
			printf("Aktuelle Baud: %lu\r\n",gl_USART_Baud);
		}
		return;
	}
	else if(STRCMP_ALT("Startseite",gl_USART.Buffer_rx)==0)
	{
		if (gl_USART.Buffer_rx[11]!='\n')
		{
			char *end = strstr(&gl_USART.Buffer_rx[11],"\r");
			*end=0;
			strncpy(&gl_Default_Page[0],&gl_USART.Buffer_rx[11],13);			// Kopiere die neue Defaultseite in den RAM
			FAT32_Directory_Change("/");
			INI_Write_Key_String("http.ini","HTTP","Default_Page",&gl_Default_Page[0]);		// Schreibe den Key in die HTTP.ini

			printf("Neue Startseite: %s\r\nBitte System neustarten!\r\n",&gl_Default_Page[0]);
		}
		else
		{
			printf("Aktuelle Startseite: %s\r\n",&gl_Default_Page[0]);
		}
		return;
	}
	else if(STRCMP_ALT("404",gl_USART.Buffer_rx)==0)
	{
		if (gl_USART.Buffer_rx[4]!='\n')
		{
			char *end = strstr(&gl_USART.Buffer_rx[4],"\r");
			*end=0;
			strncpy(&gl_Default_404[0],&gl_USART.Buffer_rx[4],13);							// Kopiere die neue 404-Seite in den RAM

			FAT32_Directory_Change("/");
			INI_Write_Key_String("http.ini","HTTP","Default_404",&gl_Default_404[0]);		// Schreibe den Key in die HTTP.ini
			printf("Neue 404-Seite: %s\r\nBitte System neustarten!\r\n",&gl_Default_404[0]);
		}
		else
		{
			printf("Aktuelle 404-Seite: %s\r\n",&gl_Default_404[0]);
		}
		return;
	}
	else if(STRCMP_ALT("Verzeichnis",gl_USART.Buffer_rx)==0)
	{
		if (gl_USART.Buffer_rx[12]!='\n')
		{
			char *end = strstr(&gl_USART.Buffer_rx[4],"\r");
			*end=0;
			strncpy(&gl_Default_Dir[0],&gl_USART.Buffer_rx[4],128);							// Kopiere das neue Verzeichnis in den RAM

			FAT32_Directory_Change("/");
			INI_Write_Key_String("http.ini","HTTP","Default_Dir",&gl_Default_Dir[0]);		// Schreibe den Key in die HTTP.ini
			printf("Neues Verzeichnis: %s\r\nBitte System neustarten!\r\n",&gl_Default_Dir[0]);
		}
		else
		{
			printf("Aktuelles Verzeichnis: %s\r\n",&gl_Default_Dir[0]);
		}
		return;
	}
	else if(STRCMP_ALT("Name",gl_USART.Buffer_rx)==0)
	{
		if (gl_USART.Buffer_rx[5]!='\n')
		{
			char *end = strstr(&gl_USART.Buffer_rx[4],"\r");
			*end=0;
			strncpy(&gl_DHCP_Cache.Name[0],&gl_USART.Buffer_rx[5],30);						// Kopiere den neuen Namen in den RAM

			FAT32_Directory_Change("/");
			INI_Write_Key_String("basic.ini","DHCP","Name",&gl_DHCP_Cache.Name[0]);			// Schreibe den Key in die HTTP.ini
			printf("Neuer DHCP Name: %s\r\nBitte System neustarten!\r\n",&gl_DHCP_Cache.Name[0]);
		}
		else
		{
			printf("Aktueller DHCP Name: %s\r\n",&gl_DHCP_Cache.Name[0]);
		}
		return;
	}
	else if (STRCMP_ALT("Zeitzone",gl_USART.Buffer_rx)==0)								// Zeitzone 1.0
	{
		if (gl_USART.Buffer_rx[9]!='\n')
		{
			sscanf(&gl_USART.Buffer_rx[8],"%f",&gl_NTP_Timezone);

			char *end = strstr(&gl_USART.Buffer_rx[9],"\r");
			*end=0;
			FAT32_Directory_Change("/");
			INI_Write_Key_String("basic.ini","NTP","Timezone",&gl_USART.Buffer_rx[9]);		// Schreibe den Wert
			printf("Neue Zeitzone Webserver: %1.2f\r\n",gl_NTP_Timezone);
		}
		else
		{
			printf("Zeitzone Webserver: %1.2f\r\n",gl_NTP_Timezone);
		}
		USART_Write_String("Systemzeit und Sommerzeitbit muss eventuell angepasst werden\r\n");
		return;
	}
	else if (STRCMP_ALT("Sommerzeit",gl_USART.Buffer_rx)==0)								// Sommerzeit 1
	{
		if (gl_USART.Buffer_rx[11]!='\n')
		{
			sscanf(&gl_USART.Buffer_rx[11],"%hhu",&gl_NTP_Summertime);
			char *end = strstr(&gl_USART.Buffer_rx[11],"\r");
			*end=0;
			FAT32_Directory_Change("/");
			INI_Write_Key_String("basic.ini","NTP","Summertime",&gl_USART.Buffer_rx[11]);	// Schreibe den Wert
			printf("Sommerzeit: %u\r\nZeitstempel neu einstellen\r\n",gl_NTP_Summertime);
			return;
		}
		printf("Sommerzeit: %u\r\n",gl_NTP_Summertime);
		return;
	}
	else if(STRCMP_ALT("LCD",gl_USART.Buffer_rx)==0)										// LCD 0 0 Test
	{
		uint8_t x, y;
		char Text[22];
		if(gl_USART.U0rx>USART_Buffer_rx_MAX)												// Sch�tze den Textstring vorm �berlauf
		{
			return;
		}
		sscanf(&gl_USART.Buffer_rx[4],"%hhu %hhu %s",&x, &y, &Text[0]);
		Genie_Clear_Display();
		Genie_Write_String_5_7(x,y,Text);
		USART_Write_String("Text an Display gesendet\r\n");
		return;
	}
	else if (STRCMP_ALT("Parser",&gl_USART.Buffer_rx[0])==0)								// Parser
	{
		printf("Aktuelles Parserzeichen ist: %c\r\nVewendung des Parserzeichen als Textzeichen: %c%c => %c \r\nDateiendung: *.psf\r\nBeispiel: %cIP%c\r\n\r\nParserfunktionen fuer GET:\r\n",Parser_Ident_char,Parser_Ident_char,Parser_Ident_char,Parser_Ident_char,Parser_Ident_char,Parser_Ident_char);		// Parserzeichen und Beispiel senden
		USART_Write_String("- Date\r\n");
		USART_Write_String("- Debugbaud\r\n");
		USART_Write_String("- Time\r\n");
		USART_Write_String("- IP\r\n");
		USART_Write_String("- NTP\r\n");
		USART_Write_String("- Subnetmask\r\n");
		USART_Write_String("- MAC\r\n");
		USART_Write_String("- DHCP\r\n");
		USART_Write_String("- Gateway\r\n");
		USART_Write_String("- Uptime\r\n");
		USART_Write_String("- WAV_Volume\r\n");
		USART_Write_String("- WAV_File[0-n]\r\n");
		USART_Write_String("- PWM[0-5]\r\n");
		USART_Write_String("- Wartungszustand\r\n");
		USART_Write_String("- Systemp\r\n");
		USART_Write_String("- Senstemp\r\n");
		USART_Write_String("- Supplyvoltage\r\n");
		USART_Write_String("- Setting_Chan[0-5]\r\n");
		USART_Write_String("- Tempcontrol_in_use\r\n");
		USART_Write_String("- Tempcontrol_trig\r\n");
		USART_Write_String("- Tempcontrol_fan\r\n");
		USART_Write_String("- DHCP\r\n\r\n");

		USART_Write_String("Parserfunktionen fuer POST:\r\n");
		USART_Write_String("- Wartung=[True, False]\r\n");
		USART_Write_String("- WAV=Play [Filename, P, S, V Volume]\r\n");
		USART_Write_String("- PWM_[0-5]=[Percent]\r\n");
		USART_Write_String("- WeatherInit=1\r\n");
		USART_Write_String("- chanset=0,1,2,3,4,5\r\n");
		USART_Write_String("- Tempcontrol_in_use=[True, False]\r\n");
		USART_Write_String("- tempconsetting=treshold,fan_channel\r\n");
		USART_Write_String("- chanset=0,1,2,3,4,5\r\n");
		USART_Write_String("- *.txt\r\n\r\n");
		return;
	}
	else if (STRCMP_ALT("Tasktimer",&gl_USART.Buffer_rx[0])==0)							// Tasktimer
	{
		printf("Counter: %lu\r\n",gl_Tasktimer_Timestamp);
		for (uint8_t g=0; g<Tasktimer_MAX_Tasks; g++)
		{
			printf("Tasknummer: %u, Task ausfuehren: %u, Wird benutzt: %u, Naechster Funktionsaufruf: %lu, Haeufigkeit in s: %lu, Text: %s\r\n",g,gl_Tasktimer_Tasks[g].Do_task,gl_Tasktimer_Tasks[g].Is_used,gl_Tasktimer_Tasks[g].Timestamp_planned,gl_Tasktimer_Tasks[g].Frequency, gl_Tasktimer_Tasks[g].Text);
		}
		return;
	}
	else if (STRCMP_ALT("ARP",&gl_USART.Buffer_rx[0])==0)									// ARP
	{
		for (uint8_t g=0;g<ARP_Max_Entries;g++)
		{
			printf("IP: %u.%u.%u.%u MAC: %02X:%02X:%02X:%02X:%02X:%02X Used: %u\r\n",gl_ARP_Table[g].IP[0],gl_ARP_Table[g].IP[1],gl_ARP_Table[g].IP[2],gl_ARP_Table[g].IP[3],gl_ARP_Table[g].MAC[0],gl_ARP_Table[g].MAC[1],gl_ARP_Table[g].MAC[2],gl_ARP_Table[g].MAC[3],gl_ARP_Table[g].MAC[4],gl_ARP_Table[g].MAC[5],gl_ARP_Table[g].Used);
		}
		return;
	}
	else if (STRCMP_ALT("ENC",&gl_USART.Buffer_rx[0])==0)									// ENC
	{
		ENC_Init_Dump();
		return;
	}
	else if (STRCMP_ALT("Play",&gl_USART.Buffer_rx[0])==0)								// Play text.wav, Play S
	{
		char Text[13];
		float Volume;
		if(gl_USART.U0rx>USART_Buffer_rx_MAX)																// Sch�tze den Textstring vorm �berlauf
		{
			return;
		}
		sscanf(&gl_USART.Buffer_rx[5],"%s",&Text[0]);
		if(strstr(&gl_USART.Buffer_rx[5],"WAV") || strstr(&gl_USART.Buffer_rx[5],".wav"))
		{
			FAT32_Directory_Change("/webserv/music/");
			if(WAV_Play_File(WAV_Filenumber,&Text[0]))
			{
				USART_Write_String("Datei nicht gefunden\r\n");
			}
			else
			{
				USART_Write_String("Datei wird abgespielt\r\n");
			}
			return;
		}
		else if(gl_USART.Buffer_rx[5] == 'S')
		{
			WAV_Stop();
			USART_Write_String("Wiedergabe abgebrochen\r\n");
			return;
		}
		else if(gl_USART.Buffer_rx[5] == 'P')
		{
			WAV_Pause();
			USART_Write_String("Wiedergabe pausiert\r\n");
			return;
		}
		else if(gl_USART.Buffer_rx[5] == 'C')
		{
			WAV_Continue();
			USART_Write_String("Wiedergabe fortsetzen\r\n");
			return;
		}
		else if(gl_USART.Buffer_rx[5] == 'V')
		{
			sscanf(&gl_USART.Buffer_rx[6],"%f",&Volume);
			WAV_Volume(Volume);
			USART_Write_String("Lautstaerke geaendert\r\n");
			return;
		}
		else if(gl_USART.Buffer_rx[5] == '\n')
		{
			if(FAT32_Directory_Change("/webserv/music/")==0)
			{
				FAT32_Directory_List();
			}
			return;
		}
		return;
	}
	else if (STRCMP_ALT("Test",&gl_USART.Buffer_rx[0])==0)														// Test
	{
		USART_Write_String("Eine wilde Testfunktion ist erschienen!\r\n");
		return;
	}
	else if (STRCMP_ALT("Power",&gl_USART.Buffer_rx[0])==0)														// Test
	{
		printf("Eingangsspannung: %2.3fV\r\n\r\nI_PWM0: %2.4fA\r\nI_PWM1: %2.4fA\r\nI_PWM2: %2.4fA\r\nI_PWM3: %2.4fA\r\nI_PWM4: %2.4fA\r\nI_PWM5: %2.4fA\r\n",Power_Read_Supplyvoltage(),gl_Power.PWM_Channel[0].Current,gl_Power.PWM_Channel[1].Current,gl_Power.PWM_Channel[2].Current,gl_Power.PWM_Channel[3].Current,gl_Power.PWM_Channel[4].Current,gl_Power.PWM_Channel[5].Current);
		return;
	}
	/*else if (STRCMP_ALT("Bootloader",&gl_USART.Buffer_rx[0])==0)												// ST USART Bootloader
	{
		USART_Write_String("Restart in Bootloadermode\r\n");

		__disable_irq();																						// Alle Interrupts aus

		RCC->CR |= (1<<0);																						// Schalte den HSI16 an
		while(!((RCC->CR & 0x00000002)==0b10));																	// Warte bis die Clock stabil l�uft
		RCC->CFGR &= 0xfffffffc;																				// Setze die HSI16 als Systemclock
		while(!((RCC->CFGR & 0x0000000c)==0b0000));																// Warte bis die HSI116 l�uft
		RCC->CR &= ~(1<<24);																					// PLL auschalten
		while((RCC->CR & 0x02000000) == 0x02000000); 															// Warte bis die PLL steht
		FLASH->ACR |= FLASH_ACR_LATENCY_1WS;																	// Latenz auf 1

		SYSCFG->MEMRMP = 0x01;																					// Setze den Bootloader an Adresse 0x00000000
		asm volatile ("movs r3, #0\nldr r3, [r3, #0]\nMSR msp, r3\n" : : : "r3", "sp");							// Setze die Adresse des Stackpointers auf 0x00000000
		((void (*)(void)) *((uint32_t*) 0x00000004))();															// Springe zum Bootloader
		return;
	}*/
	else if(STRCMP_ALT("Hilfe",gl_USART.Buffer_rx)==0)
	{
		USART_Write_String("- Neustart                                      // Hardreset des AVR durch WDT-Ueberlauf\r\n");
		USART_Write_String("- Info                                          // Gibt aktuelle Informationen zum Webserver wieder\r\n");
		USART_Write_String("- Baud [Neue Baud]                              // Aktuelle Baudrate der Debugschnittstelle\r\n");
		USART_Write_String("- Power                                         // Zeigt die Leistungsaufnahme der PWM Kanaele\r\n");
		USART_Write_String("- Play [File.wav]                               // Spielt eine WAV-Datei ab. Bei Eingabe ohne Argumente, gibt es die Songliste wieder\r\n");
		USART_Write_String("- LCD [x] [y] [Text]                            // Beispieltext an Display. Max 20 Zeichen\r\n");
		USART_Write_String("- ENC                                           // Registerdump des ENC28J60\r\n");
		USART_Write_String("- Tasktimer                                     // Gibt Informationen ueber die aktiven Tasks aus\r\n");
		USART_Write_String("- Systemzeit [17]:[0]:[0] [22].[12].[1992] [Di] // Gibt die Systemzeit ohne Eingabe der Argumente wieder\r\n");
		USART_Write_String("- Zeitzone [1.0]                                // Gibt die Zeitzone in der sich der Webserver befindet wieder\r\n");
		USART_Write_String("- Sommerzeit [0]                                // Gibt an ob auf dem System die Sommer- bzw. Winterzeit aktiv ist\r\n");
		
		USART_Write_String("- ARP                                           // Gibt die aktuelle ARP Tabelle aus\r\n");
		USART_Write_String("- TCP                                           // Gibt die aktuelle TCP Tabelle aus\r\n");
		USART_Write_String("- TCP A                                         // Gibt die detaillierte aktuelle TCP Tabelle aus\r\n");
		USART_Write_String("- IP [192].[168].[178].[14]                     // Gibt die aktuelle IP ohne Eingabe der Argumente wieder\r\n");
		USART_Write_String("- NTP [192].[168].[178].[26]                    // Gibt die aktuelle IP des Standard NTP Servers ohne Eingabe der Argumente wieder\r\n");
		USART_Write_String("- NTP R                                         // NTP Request senden\r\n");
		USART_Write_String("- MAC [12]:[0]:[0]:[0]:[0]:[1]                  // Gibt die aktuelle MAC ohne Eingabe der Argumente wieder\r\n");
		USART_Write_String("- Subnetzmaske [255].[255].[255].[0]            // Gibt die Subnetzmaske ohne Eingabe der Argumente wieder\r\n");
		USART_Write_String("- Gateway [192].[168].[178].[1]                 // Gibt die Gatewayaddresse ohne Eingabe der Argumente wieder\r\n");
		USART_Write_String("- Name [avr-webserver]                          // Name des Webersvers, der per DHCP uebermittelt wird. Max 30 Zeichen\r\n\r\n");
		
		USART_Write_String("- Startseite [index.htm]                        // Aendert die Startseite (8.3 Format), welche bei Eingabe der Adresse standardmaessig angezeigt wird\r\n");
		USART_Write_String("- Verzeichnis [/website/website/]               // Aendert das Standardverzeichnis (max 128 Zeichen, 8.3 Format), welches bei Eingabe der Adresse standardmaessig angezeigt wird\r\n");
		USART_Write_String("- 404 [404.htm]                                 // Aendert die 404-Seite (8.3 Format), welche bei falschen Eingaben der Adresse standardmaessig angezeigt wird\r\n");
		USART_Write_String("- Parser                                        // Gibt Informationen ueber den HTML GET und POST Parser aus\r\n\r\n");
		return;
	}
	else
	{
		USART_Write_String("Unbekannter Befehl\r\n");
		USART_Write_String("Gebe 'Hilfe' fuer die Befehlsuebersicht ein\r\n");
		return;
	}
}

void Debug_IRQ_Handler (void)
{
	if (USART_Regs->SR & USART_SR_RXNE)												// RX Interrupt
	{
		USART_Regs->SR &= ~USART_SR_RXNE;											// Interrupt zur�cksetzen

		gl_USART.Buffer_rx[gl_USART.U0rx] = Debug_USART->DR;

		if (gl_USART.Buffer_rx[gl_USART.U0rx] == '\n')								// Pruefe ob ein String empfangen wurde, wenn ja gebe bescheid
		{
			gl_USART.Command_arrived=1;
			return;
		}
		gl_USART.U0rx++;

		if (gl_USART.U0rx == USART_Buffer_rx_MAX)									// Sch�tze vor �berlauf
		{
			gl_USART.U0rx = 0;
		}
	}
}
