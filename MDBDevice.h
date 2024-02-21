#pragma once

#include "MDBSerial.h"
#include "Logger.h"
#include <Arduino.h>

#define RESET 					0x00
#define RESET_CARD           0x00
#define SETUP			 		0x01
#define SETUP_CARD          0x11
#define POLL 					0x03
#define POLL_DCM           0x6A
#define POLL_CARD           0x12
#define TYPE 					0x04
#define EXPANSION 				0x07
#define IDENTIFICATION 			0x00
#define FEATURE_ENABLE 			0x01

#define JUST_RESET 		 		0x0B
#define JUST_RESET_CARD         0x00

#define NO_RESPONSE 			2000

#define MAX_RESET 				5
#define MAX_RESET_POLL 			15

#define SETUP_TIME 				200
#define RESPONSE_TIME 			5

#define WARNING					1
#define ERROR					2
#define SEVERE					3

class MDBDevice
{
public:
	explicit
	MDBDevice(MDBSerial &mdb) : m_mdb(&mdb) {}

	virtual bool Reset() = 0;

	virtual void Print() = 0;
	
protected:
	virtual int poll() = 0;
	
	MDBSerial *m_mdb;

	int m_resetCount;
	
	int m_count;
	char m_buffer[64];

	char m_feature_level;
	unsigned int m_country;

	unsigned long m_manufacturer_code;
	char m_serial_number[12];
	char m_model_number[12];
};
