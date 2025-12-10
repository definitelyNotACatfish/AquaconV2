// Includes
#include "main.h"
#include "HTTP_uC.h"
#include "allerlei.h"
#include "Stack.h"
#include "HTTP.h"
#include "ADC.h"
#include "Power.h"
#include "DS18B20.h"
#include <string.h>
#include <stdio.h>

/*

************************************************************************************************
* Erweiterung des HTTP Servers um per GET direkt Informationen vom �C zu bekommen			   *
* 07.06.2019 � Frederinn															   *
************************************************************************************************

*/

// Variablen


// Funktionen
void HTTP_uC_GET_requests(uint8_t Position_in_Table,char *Action)
{
	char Responsestring[HTTP_uC_Maxlen];
	char Header_p_Response[115 + HTTP_uC_Maxlen];
	strcpy(&Header_p_Response[0],"HTTP/1.1 200 OK\r\nServer: STM32 Webserver 1.0\r\nContent-Language: de\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");

	if (STRCMP_ALT("ADCs",&Action[0])==0)																	// Lese die 4 Analogeing�nge und die Vin aus
	{
		float ADC_Volt[16];																					// Speichere die Spannungen zwischen
		uint32_t arr[16]={0};

		for(uint32_t g=0;g<2000;g++)																		// Mittelwert aus 2000 Messungen �ber 20ms verteilt
		{
			arr[0]+=ADC1_Read_Channel(0,ADC_Samplingrate_480CLK);
			arr[1]+=ADC1_Read_Channel(1,ADC_Samplingrate_480CLK);
			arr[2]+=ADC1_Read_Channel(2,ADC_Samplingrate_480CLK);
			arr[3]+=ADC1_Read_Channel(3,ADC_Samplingrate_480CLK);
			_delay_us(10);
		}

		ADC_Volt[0] = (((double)(3.3/4096.0) * arr[0])/24900.0) * (124900.0/2000.0) * 0.9844462; 			// 4 Analogeing�nge einlesen
		ADC_Volt[1] = (((double)(3.3/4096.0) * arr[1])/24900.0) * (124900.0/2000.0) * 0.9866400;
		ADC_Volt[2] = (((double)(3.3/4096.0) * arr[2])/24900.0) * (124900.0/2000.0) * 0.9862000;
		ADC_Volt[3] = (((double)(3.3/4096.0) * arr[3])/24900.0) * (124900.0/2000.0) * 0.9857781;
		ADC_Volt[14] = Power_Read_Supplyvoltage();															 // VCC hat einen anderen Vorteiler

		sprintf(&Responsestring[0],"ADC0,%3.4f V,ADC1,%3.4f V,ADC2,%3.4f V,ADC3,%3.4f V,ADC14,%3.4f V,",ADC_Volt[0],ADC_Volt[1],ADC_Volt[2],ADC_Volt[3],ADC_Volt[14]);		// Drucke String
	}
	else if (STRCMP_ALT("Power",&Action[0])==0)																	// Drucke die Stromaufnahme der PWM Kan�le in den String
	{
		sprintf(&Responsestring[0],"POWER0,%1.3f A,POWER1,%1.3f A,POWER2,%1.3f A,POWER3,%1.3f A,POWER4,%1.3f A,POWER5,%1.3f A,",gl_Power.PWM_Channel[0].Current,gl_Power.PWM_Channel[1].Current,gl_Power.PWM_Channel[2].Current,gl_Power.PWM_Channel[3].Current,gl_Power.PWM_Channel[4].Current,gl_Power.PWM_Channel[5].Current);		// Drucke String
	}
	else if (STRCMP_ALT("Telegraf",&Action[0])==0)															// JSON f�r Telegraf Adapter
	{
		float Flow = 0;
		uint32_t arr = 0;

		for(uint32_t g=0;g<2000;g++)																		// Mittelwert aus 2000 Messungen �ber 20ms verteilt
		{
			arr += ADC1_Read_Channel(3,ADC_Samplingrate_480CLK);
			_delay_us(10);
		}

		Flow = (((double)(3.3/4096.0) * arr)/24900.0) * (124900.0/2000.0) * 0.9857781;						// Count to volage
		Flow = (Flow/0.33)*60;																				// Calculate flow in l/h

		sprintf(&Responsestring[0],"{\"Temp\": %2.2f,\"Flow\": %3.0f}", gl_DS18B20.Temperature, Flow);  // Drucke String
	}
	else																									// Wenn nicht gefunden, schreibe Fehler
	{
		sprintf(&Responsestring[0],"%s couln't be resolved",&Action[0]);									// Drucke String
	}

	strcat(&Header_p_Response[0],&Responsestring[0]);														// F�ge Header und Antwort zusammen
	TCP_Write_Data(Position_in_Table,&Header_p_Response[0],strlen(&Header_p_Response[0]));					// Schreibe Daten
}
