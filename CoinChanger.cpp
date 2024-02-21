#include "CoinChanger.h"
#include <Arduino.h>

CoinChanger::CoinChanger(MDBSerial &mdb) : MDBDevice(mdb)
{
	ADDRESS = 0x08;
	STATUS = 0x02;
	DISPENSE = 0x05;
	
	m_acceptedCoins = 0xFFFF; //all coins enabled by default
	m_dispenseableCoins = 0xFFFF;
	
	m_resetCount = 0;
	
	m_credit = 0;
	m_coin_scaling_factor = 0;
	m_decimal_places = 0;
	m_coin_type_routing = 0;
	
	for (int i = 0; i < 16; i++)
	{
		m_coin_type_credit[i] = 0;
		m_tube_status[i] = 0;
	}
	m_tube_full_status = 0;
	
	m_software_version = 0;
	m_optional_features = 3;
	
	m_alternative_payout_supported = false;
	m_extended_diagnostic_supported = false;
	m_manual_fill_and_payout_supported = false;
	m_file_transport_layer_supported = false;
	
	m_update_count = 0;
	
	m_value_to_dispense = 0;
	m_dispensed_value = 0;
}

void CoinChanger::Activate(bool activate){
	if(activate){
		m_acceptedCoins = 0xFFFF;
	}else{
		m_acceptedCoins = 0x0;
	}
	type();
}

void CoinChanger::Update(unsigned long &change)
{
	poll();
	tube_status();
	change = m_change;
	if ((m_update_count % 50) == 0)
	{
		if (m_feature_level >= 3)
		{
			expansion_send_diagnostic_status();
		}
		m_update_count = 0;
	}
	type();
	m_update_count++;
}

bool CoinChanger::Reset()
{
	int count = 0;
	//wait for CC to response
	while (poll() < 0)
	{
		if (count > MAX_RESET_POLL)
		{
			//debug << F("CC: NOT CONNECTED") << endl;
			return false;
		}
		delay(100);
		count++;
	}
	
	count = 0;

	//wait for CC to power up
	while (poll() > 0 && count < MAX_RESET_POLL)
	{
		m_mdb->SendCommand(ADDRESS, RESET);
		if ((m_mdb->GetResponse() == ACK))
		{	
			int count2 = 0;
			while (poll() != JUST_RESET) 
			{
				if (count2 > MAX_RESET_POLL)
				{
					//debug << F("CC: NO JUST RESET RECEIVED") << endl;
					return false;
				}
				delay(100);
				count2++;
			}
			//debug << F("CC: RESET COMPLETED") << endl;
			return true;
		}
		count++;
		delay(100);
	}
	//debug << F("CC: RESET FAILED") << endl;
	return false;
}

bool CoinChanger::init()
{
	if (!setup())
		return false;
	tube_status();
	Print();
	//debug << F("CC: INIT COMPLETED") << endl;
	return true;
}


bool CoinChanger::Dispense(unsigned long value)
{
	tube_status(); //to get actual available change
	//to make sure we have a value even to 5c
	if ((value % 5) > 0)
		value += 5 - (value % 5);

	if (m_change < value)
	{
		Dispense(m_change);
		return false;
	}
	m_value_to_dispense = value;
	int val = value / m_coin_scaling_factor;

	if (m_alternative_payout_supported && expansion_payout(val))
	{
		return true;
	}
	else
	{
		//warning << F("CC: OLD DISPENSE FUNCTION USED") << endl;
		bool dispensed_anything = false;
		for (int i = 15; i >= 0; i--) // since we have 6 tubes
		{
			int count = m_value_to_dispense / (m_coin_type_credit[i] * m_coin_scaling_factor);
			count = min(count, m_tube_status[i]); //check if sufficient coins in tube
			if (count <= 0)
				continue;
			if (dispense(i, count))
			{
				unsigned long val = count * m_coin_type_credit[i] * m_coin_scaling_factor;
				m_value_to_dispense -= val;
				dispensed_anything = true;
			}
			if (m_value_to_dispense <= 0)
				break;
		}
		
		if (m_value_to_dispense == 0)
			return true;
		
		if (dispensed_anything)
		{
			m_value_to_dispense += 5;
			return Dispense(m_value_to_dispense);
		}
	}
	return false;
}

bool CoinChanger::dispense(int coin, int count)
{
	coin = coin % 6; //since we have only 6 tubes
	tube_status();
	if (count > m_tube_status[coin])
		return false;
	int out = (count << 4) | coin;
	m_mdb->SendCommand(ADDRESS, DISPENSE, &out, 1);
	if (m_mdb->GetResponse() != ACK)
	{
		//warning << F("CC: DISPENSE FAILED") << endl;
		return false;
	}
	poll(); //wait for dispense to finish
	return true;
}

void CoinChanger::Print()
{
	//debug << F("## CoinChanger ##") << endl;
	//debug << F("credit: ") << m_credit << endl;
	//debug << F("country: ") << m_country << endl;
	//debug << F("feature level: ") << (int)m_feature_level << endl;
	//debug << F("coin scaling factor: ") << (int)m_coin_scaling_factor << endl;
	//debug << F("decimal places: ") << (int)m_decimal_places << endl;
	//debug << F("coin type routing: ") << m_coin_type_routing << endl;
	
	//debug << F("coin type credits: ");
	for (int i = 0; i < 16; i++)
	{
		//debug << (int)m_coin_type_credit[i] << " ";
    }
    //debug << endl;
	
	//debug << F("tube full status: ") << (int)m_tube_full_status << endl;
	
	//debug << F("tube status: ");
	for (int i = 0; i < 16; i++)
	{
		//debug << (int)m_tube_status[i] << " ";
    }
	//debug << endl;
	
	//debug << F("software version: ") << m_software_version << endl;
	//debug << F("optional features: ") << m_optional_features << endl;
	//debug << F("alternative payout supported: ") << (bool)m_alternative_payout_supported << endl;
	//debug << F("extended diagnostic supported: ") << (bool)m_extended_diagnostic_supported << endl;
	//debug << F("mauall fill and payout supported: ") << (bool)m_manual_fill_and_payout_supported << endl;
	//debug << F("file transport layer supported: ") << (bool)m_file_transport_layer_supported << endl;
	//debug << F("######") << endl;
}

int CoinChanger::poll()
{
	int response_size = 16;
	bool reset = false;
	bool payout_busy = false;
	bool busy = false;
  //Serial.println("3.001");
	for (int i = 0; i < 64; i++)
		m_buffer[i] = 0;
  //Serial.println("3.002");
	m_mdb->SendCommand(ADDRESS, POLL);
  //Serial.println("3.003");
	int answer = m_mdb->GetResponse(m_buffer, &m_count, response_size);
  //Serial.println("3.004");
	if (answer == ACK)
	{
		//debug << F("CC: poll got ack") << endl;
		return 1;
	}
  //Serial.println("3.005");
	if (answer > 0 && m_count > 0)
	{
		m_mdb->Ack();
	}
	else
		return -1;
	//Serial.println("3.006");
  //for(int i = 0; i < 64; ++i){
    //Serial.println(m_buffer[i], HEX);
 //}
  //Serial.println(String(m_count));
  //Serial.println(String(response_size));
	//max of 16 bytes as response
	for (int i = 0; i < m_count && i < response_size; i++)
	{
    //Serial.println("3.0066");
		//coins dispensed manually
		if (m_buffer[i] & 0b10000000)
		{
			
			int count = (m_buffer[i] & 0b01110000) >> 4;
			int type = m_buffer[i] & 0b00001111;
			int coins_in_tube = m_buffer[i + 1];

			i++; //cause we used 2 bytes
		}
		//coins deposited
		else if (m_buffer[i] & 0b01000000)
		{
			int routing = (m_buffer[i] & 0b00110000) >> 4;
			int type = m_buffer[i] & 0b00001111;
			int those_coins_in_tube = m_buffer[i + 1];
			if (routing < 2)
			{
				m_credit += (m_coin_type_credit[type] * m_coin_scaling_factor);
				//debug << F("CC: coin accepted ") << m_coin_type_credit[type] * m_coin_scaling_factor << endl;
			}
			else
			{
				//debug << F("CC: coin rejected") << endl;
			}
			i++; //cause we used 2 bytes
		}
		//slug
		else if (m_buffer[i] & 0b00100000)
		{
			int slug_count = m_buffer[i] & 0b00011111;
			//debug << F("CC: slug") << endl;
		}
		//status
		else
		{
			switch (m_buffer[i])
			{
			case 1:
				//debug << F("CC: escrow request") << endl;
				m_escrow = true;
				break;
			case 2:
        digitalWrite(37, HIGH);
				//warning << F("CC: changer payout busy") << endl;
				payout_busy = true;
				break;
			case 3:
				//debug << F("CC: no credit") << endl;
				break;
			case 4:
				//error << F("CC: defective tube sensor") << endl;
				m_error = 1;
				break;
			case 5:
				//debug << F("CC: double arrival") << endl;
				break;
			case 6:
				//debug << F("CC: acceptor unplugged") << endl;
				m_error = 2;
				break;
			case 7:
				//debug << F("CC: tube jam") << endl;
				m_error = 3;
				break;
			case 8:
				//warning << F("CC: ROM checksum error") << endl;
				m_error = 4;
				break;
			case 9:
				//debug << F("CC: coin routing error") << endl;
				m_error = 5;
				break;
			case 10:
				//debug << F("CC: changer busy") << endl;
				busy = true;
				break;
			case 11:
				//debug << F("CC: changer was reset") << endl;
				reset = true;
				break;
			case 12:
				//warning << F("CC: coin jam") << endl;
				m_error = 6;
				break;
			case 13:
				//debug << F("CC: possible credited coin removal") << endl;
				m_error = 7;
				break;
			default:
				//debug << F("CC: default: ");
				for ( ; i < m_count; i++){ // print the bytes that could not be parsed
					//debug << m_buffer[i];
				}
				//debug << endl;
			}
		}
	}
 
	if (reset && init()){
    //Serial.println("3.0067");	
		return JUST_RESET;
	}
  //Serial.println("3.00667");  
	if (busy || payout_busy)
	{
    
    //Serial.println("3.0061");
		delay(250);
		//return poll();
		//delay(50);
	}
	return 1;
}

bool CoinChanger::setup(int it)
{
	int response_size = 23;
	m_mdb->SendCommand(ADDRESS, SETUP);
	int answer = m_mdb->GetResponse(m_buffer, &m_count, response_size);
	if (answer >= 0 && m_count == response_size)
	{
		m_mdb->Ack();
		m_feature_level = m_buffer[0];
		m_country = m_buffer[1] << 8 | m_buffer[2];
		m_coin_scaling_factor = m_buffer[3];
		m_decimal_places = m_buffer[4];
		m_coin_type_routing = m_buffer[5] << 8 | m_buffer[6];
		for (int i = 0; i < 16; i++)
		{
			m_coin_type_credit[i] = m_buffer[7 + i];
		}

		if (m_feature_level >= 3)
		{
			expansion_identification();
			expansion_feature_enable();
		}
		return true;
	}
	if (it < MAX_RESET)
	{
		delay(50);
		return setup(++it);
	}
	//error << F("CC: SETUP ERROR") << endl;
	return false;
}

void CoinChanger::checkTubes(){
  tube_status();
  for (int i = 0; i < 16; i++)
  {
    switch(m_coin_type_credit[i] * m_coin_scaling_factor){
      case 100:
        if(m_tube_status[i]<10){
          byte sent[] = {'E', 0x05, '1', '0', 'c', 't', ' ', 'n', 'a', 'c', 'h', 'f', 'u', 'e', 'l', 'l', 'e', 'n'};
          Serial2.write(sent, sizeof(sent));
        }
      case 200:
        if(m_tube_status[i]<10){
          byte sent[] = {'E', 0x05, '2', '0', 'c', 't', ' ', 'n', 'a', 'c', 'h', 'f', 'u', 'e', 'l', 'l', 'e', 'n'};
          Serial2.write(sent, sizeof(sent));
        }
      case 500:
        if(m_tube_status[i]<10){
          byte sent[] = {'E', 0x05, '5', '0', 'c', 't', ' ', 'n', 'a', 'c', 'h', 'f', 'u', 'e', 'l', 'l', 'e', 'n'};
          Serial2.write(sent, sizeof(sent));
        }
      case 1000:
        if(m_tube_status[i]<10){
          byte sent[] = {'E', 0x05, '1', 'E', ' ', 'n', 'a', 'c', 'h', 'f', 'u', 'e', 'l', 'l', 'e', 'n'};
          Serial2.write(sent, sizeof(sent));
        }
      case 2000:
        if(m_tube_status[i]<6){
          byte sent[] = {'E', 0x05, '2', 'E', ' ', 'n', 'a', 'c', 'h', 'f', 'u', 'e', 'l', 'l', 'e', 'n'};
          Serial2.write(sent, sizeof(sent));
        }
    }
  }
}

void CoinChanger::tube_status(int it)
{
	int response_size = 18;
	m_mdb->SendCommand(ADDRESS, STATUS);
	int answer = m_mdb->GetResponse(m_buffer, &m_count, response_size);
	if (answer != -1 && m_count == response_size)
	{
		m_mdb->Ack();
		//if bit is set, the tube is full
		m_tube_full_status = m_buffer[0] << 8 | m_buffer[1];
		for (int i = 0; i < 16; i++)
		{
			//number of coins in the tube
			m_tube_status[i] = m_buffer[2 + i];
		}
		m_change = 0;
		for (int i = 0; i < 16; i++)
		{
			m_change += m_coin_type_credit[i] * m_tube_status[i] * m_coin_scaling_factor;
		}
		return;
	}
	if (it < MAX_RESET)
	{
		delay(50);
		return tube_status(++it);
	}
	//warning << F("CC: STATUS ERROR") << endl;
}

void CoinChanger::type(int it)
{
	int out[] = { uint8_t((m_acceptedCoins & 0xff00) >> 8), 
					uint8_t(m_acceptedCoins & 0xff), 
					uint8_t((m_dispenseableCoins & 0xff00) >> 8), 
					uint8_t(m_dispenseableCoins & 0xff) };
					
	m_mdb->SendCommand(ADDRESS, TYPE, out, 4);
	if (m_mdb->GetResponse() != ACK)
	{
		if (it < MAX_RESET)
		{
			delay(50);
			return type(++it);
		}
		//error << F("CC: TYPE ERROR") << endl;
    m_error = 1;
	}
}

void CoinChanger::expansion_identification(int it)
{
	int response_size = 33;
	m_mdb->SendCommand(ADDRESS, EXPANSION, IDENTIFICATION);
	int answer = m_mdb->GetResponse(m_buffer, &m_count, response_size);
	if (answer > 0 && m_count == response_size)
	{
		m_mdb->Ack();
		// * 1L to overcome 16bit integer error
		m_manufacturer_code = (m_buffer[0] * 1L) << 16 | m_buffer[1] << 8 | m_buffer[2];
		for (int i = 0; i < 12; i++)
		{
			m_serial_number[i] = m_buffer[3 + i];
			m_model_number[i] = m_buffer[15 + i];
		}

		m_software_version = m_buffer[27] << 8 | m_buffer[28];
		m_optional_features = (m_buffer[29] * 1L) << 24 | (m_buffer[30] * 1L) << 16 | m_buffer[31] << 8 | m_buffer[32];

		if (m_optional_features & 0b1)
		{
			m_alternative_payout_supported = true;
		}
		if (m_optional_features & 0b10)
		{
			m_extended_diagnostic_supported = true;
		}
		if (m_optional_features & 0b100)
		{
			m_manual_fill_and_payout_supported = true;
		}
		if (m_optional_features & 0b1000)
		{ 
			m_file_transport_layer_supported = true;
		}
		return;
	}
	if (it < MAX_RESET)
	{
		delay(50);
		return expansion_identification(++it);
	}
	//error << F("CC: EXP ID ERROR") << endl;
}

void CoinChanger::expansion_feature_enable(int it)
{
	int out[] = { 0x00, 0x00, 0x00, 0x03 };
	m_mdb->SendCommand(ADDRESS, EXPANSION, FEATURE_ENABLE, out, 4);
	if (m_mdb->GetResponse() != ACK)
	{
		if (it < MAX_RESET)
		{
			delay(50);
			return expansion_feature_enable(++it);
		}
		//error << F("CC: EXP FEATURE ENABLE ERROR") << endl;
	}
	m_mdb->Ack();
}

bool CoinChanger::expansion_payout(int value)
{
	m_mdb->SendCommand(ADDRESS, EXPANSION, PAYOUT, &value, 1);
	if (m_mdb->GetResponse() != ACK)
	{
		//warning << F("CC: dispense failed") << endl;
		return false;
	}
	expansion_payout_value_poll();
	
	//debug << F("CC: dispense: ") << m_value_to_dispense << " -> " << m_dispensed_value << endl;
	
	if (m_value_to_dispense > m_dispensed_value)
	{
		m_value_to_dispense -= m_dispensed_value;
		return false;
	}

	return true;
}

//number of each coin dispensed since last alternative payout (expansion_payout)
//-> response to alternative payout (expansion_payout)
//changer clears output data after an ack from controller
void CoinChanger::expansion_payout_status(int it)
{
	int response_size = 16;
	m_mdb->SendCommand(ADDRESS, EXPANSION, PAYOUT_STATUS);
	int answer = m_mdb->GetResponse(m_buffer, &m_count, response_size);
	if (answer == ACK)
	{
		//debug << F("CC: payout busy") << endl;
		delay(500);		
		expansion_payout_status(++it);
	}
	else if (answer > 0 && m_count > 0)
	{
		m_mdb->Ack(); //to clear data 
		//debug << F("CC: payd out: ");
 		for (int i = 0; i < m_count; i++)
		{
			//debug << (int)m_buffer[i] << " ";
			m_dispensed_value += m_coin_type_credit[i] * m_coin_scaling_factor * (int)m_buffer[i];
		}
		//debug << endl;
	}
}

//scaled value that indicates the amount of change payd out since last poll or
//after the initial alternative_payout
void CoinChanger::expansion_payout_value_poll()
{
	int response_size = 1;
	m_mdb->SendCommand(ADDRESS, EXPANSION, PAYOUT_VALUE_POLL);
	int answer = m_mdb->GetResponse(m_buffer, &m_count, response_size);
	if (answer == ACK) //payout finished 
	{
		expansion_payout_status();
	}
	else if (answer > 0 && m_count == response_size)
	{
		m_mdb->Ack();
		//debug << (int)m_buffer[0] << "-";
		delay(50);
		expansion_payout_value_poll();
	}
}

//should be send by the vmc every 1-10 seconds
void CoinChanger::expansion_send_diagnostic_status()
{
	int response_size = 2;
	bool powering_up = false;
	
	m_mdb->SendCommand(ADDRESS, EXPANSION, SEND_DIAGNOSTIC_STATUS);
	int answer = m_mdb->GetResponse(m_buffer, &m_count, response_size);
	
	if (answer > 0 && m_count >= response_size)
	{		
		m_mdb->Ack();
		
		for (int i = 0; i < m_count / response_size; i++)
		{
			switch (m_buffer[0])
			{
			case 1:
				//debug <<  F("CC: powering up") << endl;
				powering_up = true;
				break;
			case 2:
				//debug <<  F("CC: powering down") << endl;
				break;
			case 3:
				//debug <<  F("CC: OK") << endl;
				break;
			case 4:
				//debug <<  F("CC: keypad shifted") << endl;
				break;
				
			case 5:
				switch (m_buffer[1])
				{
				case 10:
					//console <<  F("CC: manual fill / payout active") << endl;
					break;
				case 20:
					//debug <<  F("CC: new inventory information available") << endl;
					break;
				}
				break;
				
			case 6:
				//debug << F("CC: inhibited by VMC") << endl;
				break;
				
			case 10:
				switch(m_buffer[1])
				{
				case 0:
					//error << F("CC: non specific error") << endl;
					break;
				case 1:
					//error << F("CC: check sum error #1") << endl;
					break;
				case 2:
					//error << F("CC: check sum error #2") << endl;
					break;
				case 3:
					//error << F("CC: low line voltage detected") << endl;
					break;
				}
				break;
				
			case 11:
				switch(m_buffer[1])
				{
					case 0:
						//error << F("CC: non specific discriminator error") << endl;
						break;
					case 10:
						//error << F("CC: flight deck open") << endl;
						break;
					case 11:
						//error << F("CC: escrow return stuck open") << endl;
						break;
					case 30:
						//error << F("CC: coin jam in sensor") << endl;
						break;
					case 41:
						//error << F("CC: discrimination below specified standard") << endl;
						break;
					case 50:
						//error << F("CC: validation sensor A out of range") << endl;
						break;
					case 51:
						//error << F("CC: validation sensor B out of range") << endl;
						break;
					case 52:
						//error << F("CC: validation sensor C out of range") << endl;
						break;
					case 53:
						//error << F("CC: operation temperature exceeded") << endl;
						break;
					case 54:
						//error << F("CC: sizing optics failure") << endl;
						break;
				}
				break;
			
			case 12:
				switch(m_buffer[1])
				{
					case 0:
						//error << F("CC: non specific accept gate error") << endl;
						break;
					case 30:
						//error << F("CC: coins entered gate, but did not exit") << endl;
						break;
					case 31:
						//error << F("CC: accept gate alarm active") << endl;
						break;
					case 40:
						//error << F("CC: accept gate open, but no coin detected") << endl;
						break;
					case 50:
						//error << F("CC: post gate sensor covered before gate openend") << endl;
						break;
				}
				break;
			
			case 13:
				switch(m_buffer[1])
				{
					case 0:
						//error << F("CC: non specific separator error") << endl;
						break;
					case 10:
						//error << F("CC: sort sensor error") << endl;
						break;
				}
				break;

			case 14:
				switch(m_buffer[1])
				{
					case 0:
						//error << F("CC: non specific dispenser error") << endl;
						break;
				}
				break;
				
			case 15:
				switch(m_buffer[1])
				{
					case 0:
						//error << F("CC: non specific cassette error") << endl;
						break;
					case 2:
						//error << F("CC: cassette removed") << endl;
						break;
					case 3:
						//error << F("CC: cash box sensor error") << endl;
						break;
					case 4:
						//error << F("CC: sunlight on tube sensors") << endl;
						break;
				}
				break;
			}
		}
		
		if (powering_up)
		{
			delay(1000);
			expansion_send_diagnostic_status();
		}
	}
	else{
		//warning << F("CC: diagnostic status failed") << endl;
	}
}
