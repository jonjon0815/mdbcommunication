#include "CardReader.h"
#include <Arduino.h>


CardReader::CardReader(MDBSerial &mdb) : MDBDevice(mdb)
{
	ADDRESS = 0x10;
	SECURITY = 0x02;
	ESCROW = 0x05;
	STACKER = 0x06;
	
	m_resetCount = 0;
	
	m_credit = 0;
	m_full = false;
	m_bill_in_escrow = false;
	m_bill_scaling_factor = 0;
	m_decimal_places = 0;
	m_stacker_capacity = 0;
	m_security_levels = 0;
	m_can_escrow = 0;
	for (int i = 0; i < 16; i++)
		m_bill_type_credit[i] = 0;
}

bool CardReader::Update(long cc_change)
{
	poll();
	return true;
}

bool CardReader::Reset()
{
	int count = 0;
	//wait for BV to response
	while (poll() < 0)
	{
		if (count > MAX_RESET_POLL)
		{
			debug << F("BV: NOT CONNECTED") << endl;
			return false;
		}
		delay(100);
		count++;
	}
	
	count = 0;
	
	//wait for BV to power up
	while (poll() > 0 && count < MAX_RESET_POLL)
	{
		m_mdb->SendCommand(ADDRESS, RESET_CARD);
		if (m_mdb->GetResponse() == ACK)
		{
			int count2 = 0;
			while(poll() != JUST_RESET_CARD)
			{
				if (count2 > MAX_RESET_POLL)
				{
					debug << F("BV: NO JUST RESET RECEIVED") << endl;
					return false;
				}
				delay(100);
				count2++;
			}
			debug << F("BV: RESET COMPLETED") << endl;
			
			return true;
		}
		count++;
		delay(100);
	}
	debug << F("BV: RESET FAILED") << endl;
  m_error = 1;
	return false;
}

bool CardReader::init()
{
	if (!setup())
		return false;
	Print();
	return true;
}

void CardReader::Print()
{
}

int CardReader::poll()
{
	int response_size = 16;
	bool reset = false;
	for (int i = 0; i < 64; i++)
		m_buffer[i] = 0;
	m_mdb->SendCommand(ADDRESS, POLL_CARD);
	int answer = m_mdb->GetResponse(m_buffer, &m_count, response_size);
	if (answer == ACK)
	{
		return 1;
	}

	if (answer > 0 && m_count > 0)
		m_mdb->Ack();
	else
		return -1;
	
	char mode = m_buffer[0];
	
	for(int i = 0; i < m_count; ++i){
		Serial.println("POLL");
		Serial.println(m_buffer[i], HEX);
	}
	if (mode == 0x00) {
		error << F("CR: Revived Just Reset") << endl;
		reset = true;
	} else if (mode == 0x01) {
		error << F("CR: Recieved Card Terminal Info") << endl;
	} else if (mode == 0x02) {
		error << F("CR: Display Request denied") << endl;
	} else if (mode == 0x03) {
		error << F("CR: Beginn Session") << endl;
	} else if (mode == 0x04) {
		error << F("CR: Session Cancel Request") << endl;
		int out[] = { 0x04 };
		m_mdb->SendCommand(ADDRESS, 0x13, out, 1);
	} else if (mode == 0x05) {
		error << F("CR: Vend Approved") << endl;
		int out2[] = { 0x02, 0x00, 0x01 };
		m_mdb->SendCommand(ADDRESS, 0x13, out2, 3);
		
		int answer = m_mdb->GetResponse(m_buffer, &m_count, 0);
		if(answer == ACK){
			delay(50);
			int out1[] = { 0x04 };
			m_mdb->SendCommand(ADDRESS, 0x13, out1, 1);
		}
		
	} else if (mode == 0x06) {
		int out1[] = { 0x04 };
		m_mdb->SendCommand(ADDRESS, 0x13, out1, 1);
		error << F("CR: Vend Denied") << endl;
	} else if (mode == 0x07) {
		error << F("CR: End Sessionn") << endl;
	} else if (mode == 0x08) {
		error << F("CR: Cancelled") << endl;
	} else if (mode == 0x09) {
		error << F("CR: Perifial ID") << endl;
	} else if (mode == 0x0A) {
		error << F("CR: Error") << endl;
	} else if (mode == 0x0B) {
		error << F("CR: CMD out of Sequence") << endl;
	} else if (mode == 0x0D || mode == 0x0E || mode == 0x0F) {
		error << F("CR: Revalue") << endl;
	} else if (mode == 0xFF) {
		error << F("CR: Diagnose") << endl;
	} else {
		error << F("CR: Unknown Command") << endl;
		for (int i = 0; i < 64; i++){
			m_buffer[i] = 0;
		}
	}

	if (reset && init())
		return JUST_RESET_CARD;
	return 1;
}

bool CardReader::setup(int it)
{
	int response_size = 8;
	for (int i = 0; i < 64; i++)
		m_buffer[i] = 0;
	int out[] = { 0x00, 0x03, 0x00, 0x00, 0x00 };
	m_mdb->SendCommand(ADDRESS, SETUP_CARD, out, 5);
	int answer = m_mdb->GetResponse(m_buffer, &m_count, response_size);
	//Serial.println(String(m_count));
	if (answer > 0 && m_count == response_size)
	{	
		m_mdb->Ack();
		m_reader_feature = m_buffer[1];
		m_country_code = m_buffer[2] << 8 | m_buffer[3];
		m_scale_factor = m_buffer[4];
		m_decimal_places = m_buffer[5];
		m_max_time = m_buffer[6];
		m_options = m_buffer[7];
		Serial.println("Feature Level");
		Serial.println(String(m_reader_feature+42));
		
		Serial.println("Scale");
		Serial.println(String(m_scale_factor+42));
		Serial.println("Decimal");
		Serial.println(String(m_decimal_places+42));
		
		delay(100);
		int out1[] = { 0x01, 0xFF, 0xFF, 0x00, 0x00};
		m_mdb->SendCommand(ADDRESS, 0x11, out1, 5);
		response_size = 0;
		for (int i = 0; i < 64; i++)
			m_buffer[i] = 0;
		int answer1 = m_mdb->GetResponse(m_buffer, &m_count, response_size);
		if(answer1 > 0){
			m_mdb->Ack();
		}
		Serial.println("Ans1");
		Serial.println(String(answer1));
		delay(100);
		
		
		int out2[] = { 0x00, 0x4E, 0x45, 0x43, 0x01, 0x01, 0x10, 0x20, 0x10, 0x2E,0x20, 0xC6, 0x00, 0x3C, 0x00, 0x01, 0x01, 0xC1, 0x3F, 0x00, 0x2F, 0x15, 0xF7, 0x1A, 0x10, 0x2C, 0x00, 0x00, 0x00, 0x01 };
		m_mdb->SendCommand(ADDRESS, 0x17, out2, 30);
		response_size = 34;
		for (int i = 0; i < 64; i++)
			m_buffer[i] = 0;
		int answer2 = m_mdb->GetResponse(m_buffer, &m_count, response_size);
		if(answer2 > 0){
			m_mdb->Ack();
		}
		Serial.println("Ans2");
		Serial.println(String(answer2));
		delay(100);
		
		int out4[] = { 0x04, 0x00, 0x00, 0x00, 0b00100010 };
		m_mdb->SendCommand(ADDRESS, 0x17, out4, 5);
		response_size = 0;
		for (int i = 0; i < 64; i++)
			m_buffer[i] = 0;
		int answer4 = m_mdb->GetResponse(m_buffer, &m_count, response_size);
		if(answer4 > 0){
			m_mdb->Ack();
		}
		Serial.println("Ans4");
		Serial.println(String(answer4));
		delay(100);

   int out5[] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x13, 0x4E };
    m_mdb->SendCommand(ADDRESS, 0x11, out5, 11);
    response_size = 0;
    for (int i = 0; i < 64; i++)
      m_buffer[i] = 0;
    int answer5 = m_mdb->GetResponse(m_buffer, &m_count, response_size);
    if(answer5 > 0){
      m_mdb->Ack();
    }
    Serial.println("Ans5");
    Serial.println(String(answer5));
    delay(100);
		
		int out3[] = { 0x01 };
		m_mdb->SendCommand(ADDRESS, 0x14, out3, 1);
		response_size = 0;
		for (int i = 0; i < 64; i++)
			m_buffer[i] = 0;
		int answer3 = m_mdb->GetResponse(m_buffer, &m_count, response_size);
		if(answer3 > 0){
			m_mdb->Ack();
		}
		Serial.println("Ans3");
		Serial.println(String(answer3));
		delay(100);
		
		return true;

	}
	if (it < MAX_RESET)
	{
		delay(50);
		return setup(++it);
	}
	//error << F("BV: SETUP ERROR") << endl;
	return false;
}

void CardReader::sendPayment(int value, char product){
	byte lowerByte, upperByte;

	  // Aufteilen des Integers in untere und obere Bytes
	lowerByte = value & 0xFF;       // Untere 8 Bit
	upperByte = (value >> 8) & 0xFF; 
	
	Serial.println(String(lowerByte));
	Serial.println(String(upperByte));
	
	int out1[] = { 0x00, 0x00, 0x00, upperByte, lowerByte, 0xFF, 0xFF };
	m_mdb->SendCommand(ADDRESS, 0x13, out1, 7);
	int response_size = 0;
	for (int i = 0; i < 64; i++)
		m_buffer[i] = 0;
	int answer1 = m_mdb->GetResponse(m_buffer, &m_count, response_size);
	if(answer1 > 0){
		m_mdb->Ack();
	}
}

void CardReader::cancelPayment(){
	int out[] = { 0x01 };
	m_mdb->SendCommand(ADDRESS, 0x13, out, 1);
	
	int response_size = 0;
	int answer1 = m_mdb->GetResponse(m_buffer, &m_count, response_size);
	if(answer1 > 0){
		m_mdb->Ack();
	}
}