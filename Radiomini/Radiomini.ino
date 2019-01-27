

#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>

#include <RF24Network.h>
#include <RF24Network_config.h>
#include <Sync.h>

#include <nRF24L01.h>
#include <RF24.h>
#include <RF24_config.h>
RF24 radio(7, 8);               //  RF24 obiekt do radia
RF24Network network(radio);      // network uzywa radio
const uint16_t this_node = 01;    // adres radiowy mini
const uint16_t other_node = 00;   // adres radiowy uno

//etag mowi na ktore wywolanie zasobu to jest
//etag do light
uint8_t EtagL = 1;
//etag do enkodera
uint8_t EtagE = 1;

//zmienna stwierdzajaca czy enkoder jest teraz obserwowany czy nie
bool observing = false;

struct frame_t { // struktura wiadomosci radiowej

  bool type; //false-GET | true-PUT

  bool resource; // false-LIGHT | true-ENCODER

  //stan mowi do enkodera czy ma byc obserwowlany czy nie
  //do lampki czy ma byc wlaczona wylaczona
  int8_t state; // 0 - OFF/OBSERVE, 255 - ON/NOOBSERVE

  uint8_t Etag;


};

Encoder myEnc(2, 3);

char pos = 0;
char old_pos = -999;


void setup() {

  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);

  Serial.begin(115200);

  Serial.println(F("Radio channel:"));
  Serial.println(110, DEC);
  Serial.println(F("Radio address:"));
  Serial.println(this_node, OCT);
  SPI.begin();
  radio.begin();

  network.begin( 110, this_node);
}

void loop() {
  RF24NetworkHeader headerresponse(other_node);// naglowek wiadomosci radiowej
  frame_t messageresponse;


  network.update(); // sprawdz czy przyszla wiadomosc radiowa
  while (network.available() ) {



    //sekcja odpowiadajaca za odebranie wiadomosci
    RF24NetworkHeader header;
    frame_t message;              // payload
    network.read(header, &message, sizeof(message));

    //wypisanie jakiego typu dostalismy wiadomosc
    Serial.print("Typ ");
    if (message.type == true) {
      Serial.print("PUT");
    } else {
      Serial.print("GET");
    }
    Serial.print(" Resource ");
    if (message.resource == true) {
      Serial.print("ENCODER");
    } else {
      Serial.print("LIGHT");
    }
    Serial.println(message.state);


    if (message.resource) { //ENCODER

      if (message.type) { //PUT ustaw obserwowanie i wyslij stan

        if (message.state == 0)
        {
          //wlacz obserwowanie

          observing = true;
          messageresponse.type = true;

          messageresponse.resource = true;
          messageresponse.Etag = EtagE;
          EtagE++;
          pos = myEnc.read();
          old_pos = pos;
          messageresponse.state = pos;


          network.write(headerresponse, &messageresponse, sizeof(messageresponse));
        }
        //
        else if (message.state == 1)
        {
          //wylacz obserwowanie
          observing = false;



          // pobierz stan enkodera

          messageresponse.type = false;
          messageresponse.resource = true;
          pos = myEnc.read();
          old_pos = pos;
          messageresponse.state = pos;
          messageresponse.Etag = EtagE;
          EtagE++;
          network.write(headerresponse, &messageresponse, sizeof(messageresponse));
        }
      } else { //GET  wyslij stan

        messageresponse.type = false;
        messageresponse.resource = true;
        pos = myEnc.read();
        old_pos = pos;
        messageresponse.state = pos;
        messageresponse.Etag = EtagE;
        EtagE++;
        network.write(headerresponse, &messageresponse, sizeof(messageresponse));






      }


    }
    else { //LIGHT

      if (message.type)
      { //PUT wlacz/wylacz lampke
        Serial.println("PUT");
        if (message.state == 1)
        {
          digitalWrite(5, LOW);
          messageresponse.state = 1;
        }
        if (message.state == 0)
        {
          digitalWrite(5, HIGH);
          messageresponse.state = 0;
        }


        messageresponse.type = true;
        messageresponse.resource = false;



      }
      else
      { //GET pobierz stan lampki
        //Serial.println("GET1");
        messageresponse.type = false;
        messageresponse.resource = false;

        //messageresponse.state=analogRead(5);
        if (digitalRead(5) == LOW) { //PUT
          messageresponse.state = 1;
        }
        if (digitalRead(5) == HIGH) {
          messageresponse.state = 0;
        }
        messageresponse.Etag = EtagL;
        EtagL++;
        network.write(headerresponse, &messageresponse, sizeof(messageresponse));

      }



    }





  }


  //obsluga obserwowania enkodera
  if (observing) {
    pos = myEnc.read();
    if (pos != old_pos) {
      Serial.println("Zmiana pozycji");
      // messageresponse.type = false;
      messageresponse.type = true;
      //znaczy ze enkder
      messageresponse.resource = true;

      old_pos = pos;
      messageresponse.state = pos;
      messageresponse.Etag = EtagE;
      EtagE++;
      network.write(headerresponse, &messageresponse, sizeof(messageresponse));

    }





  }
}
