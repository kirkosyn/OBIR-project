#pragma once

#include <Arduino.h>

class Header
{
public:
	uint8_t Ver;
	uint8_t T;
	uint8_t TKL;
	uint8_t Code;
	uint8_t CodeDetail;
	uint8_t CodeClass;
	uint16_t MessageID;

	Header(byte byte0, byte byte1, byte byte2, byte byte3)
	{
		Ver = byte0 >> 6;
		T = (byte0 >> 4) & 3;
		TKL = byte0 & 15;
		CodeClass = byte1 >> 5;
		CodeDetail = byte1 & 31;
		MessageID=((uint16_t)(byte2 << 8)) | byte3;
	}
};

class Option
{
public:

	byte value;
	uint8_t optionNumber;

};

class Packet
{
public:
	//czesc header
	uint8_t Ver;
	uint8_t T;
	uint8_t TKL;
	uint8_t Code;
	uint8_t CodeDetail;
	uint8_t CodeClass;
	uint16_t MessageID;

	//nie wiem czemu to dziala, bez zadeklarowania rozmiaru na poczatku
	byte token[];


	byte options[];

	//uint8_t optionsNumbers[];

	void GetPacket(byte packet[])
	{
		Ver = packet[0] >> 6;
		T = (packet[0] >> 4) & 3;
		TKL = packet[0] & 15;
		CodeClass = packet[1] >> 5;
		CodeDetail = packet[1] & 31;
		MessageID = ((uint16_t)(packet[2] << 8)) | packet[3];


	
		for (int i = 0; i < TKL; ++i)
		{
			//4 bo taka jest dlugosc headera
			token[i] = packet[i + 4];		
		}
		//indeks odkad zaczynamy szukac ocji
		/*int i = TKL + 4;
		int table = 0;
		/*while (packet[i] != 255)
		{
			options[table] = packet[i];
			i++;
			table++;
		}

		while (packet[i] != 255)
		{
			uint8_t opt_delta = packet[i] >> 4;
			uint8_t opt_length = packet[i] & 15;
		}*/
	}

	void GetOptions()
	{

	}
};

