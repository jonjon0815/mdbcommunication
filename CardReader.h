#pragma once

#include "MDBDevice.h"

class CardReader : public MDBDevice
{
public:
	CardReader(MDBSerial &mdb);

	bool Update(long cc_change);

	bool Reset();
	void Print();
	void sendPayment(int value, char product);
	void cancelPayment();
	void sendAge();
	inline unsigned long GetCredit() { return m_credit; }
	inline void ClearCredit() { m_credit = 0; }
  inline bool ErrorDetected() { return m_error>0; }
  inline void ClearError() { m_error = 0; }

private:
	bool init();
	int poll();
	bool setup(int it = 0);
	void security(int it = 0);
	void type(int bills[], int it = 0);
	void stacker(int it = 0);
	void escrow(bool accept, int it = 0);

	int ADDRESS;
	int SECURITY;
	int ESCROW;
	int STACKER;
  int m_error = 0;

	unsigned long m_change;
	unsigned long m_credit;

	bool m_full;
	int m_bills_in_stacker;
	
	bool m_bill_in_escrow;
	
	char m_bill_scaling_factor;
	unsigned int m_stacker_capacity;
	unsigned int m_security_levels;
	char m_can_escrow;
	char m_bill_type_credit[16];
	
	char m_reader_feature;
	unsigned int m_country_code;
	char m_scale_factor;
	char m_decimal_places;
	char m_max_time;
	char m_options;
	
};
