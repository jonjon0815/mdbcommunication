#include "BillValidator.h"
#include "CoinChanger.h"
#include "MDBSerial.h"
#include "CardReader.h"

MDBSerial mdb(1);
CoinChanger changer(mdb);
BillValidator validator(mdb);
CardReader card(mdb);

UART uart;

void(* resetFunc) (void) = 0;//declare reset function at address 0


void setup()
{
  Serial.begin(9600);
  uart = UART(0);
  uart.begin();
  Logger::SetUART(&uart);
  
  mdb.begin();
  card.Reset();
  Serial.println("Init");

}
void loop()
{
  long credit = 0;
  card.Update(credit);

  delay(50);

  if (Serial.available() > 0) { // Überprüfen, ob Daten im Seriellen Puffer vorhanden sind
    receivedChar = Serial.read(); // Einzelnen Buchstaben aus dem Puffer lesen

    switch (receivedChar) {
      case 'A':
      case 'a':
        // Aktion für Buchstabe 'A' oder 'a'
        
        //int payment = (Serial.read()-48)*1000+(Serial.read()-48)*100+(Serial.read()-48)*10+(Serial.read()-48);
        card.sendPayment(200,11);
        break;
      case 'D':
      case 'd':
        // Aktion für Buchstabe 'A' oder 'a'
        
        //int payment = (Serial.read()-48)*1000+(Serial.read()-48)*100+(Serial.read()-48)*10+(Serial.read()-48);
        card.sendPayment(1,11);
        break;
      case 'E':
      case 'e':
        // Aktion für Buchstabe 'A' oder 'a'
        
        //int payment = (Serial.read()-48)*1000+(Serial.read()-48)*100+(Serial.read()-48)*10+(Serial.read()-48);
        card.sendPayment(800,11);
        break;
      case 'F':
      case 'f':
        // Aktion für Buchstabe 'A' oder 'a'
        
        //int payment = (Serial.read()-48)*1000+(Serial.read()-48)*100+(Serial.read()-48)*10+(Serial.read()-48);
        card.sendPayment(255,11);
        break;
      case 'B':
      case 'b':
        // Aktion für Buchstabe 'B' oder 'b'
        card.sendPayment(1000,11);
        break;
      case 'C':
      case 'c':
        card.cancelPayment();
        break;
      // Füge hier weitere Buchstaben und Aktionen hinzu

      default:
        // Aktion für unbekannte Buchstabe
        break;
    }
  }

}
