#pragma once



#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <string.h>
#include <RF24Network.h>
#include <RF24Network_config.h>
#include <Sync.h>
#include <coapPawel.h>

#include <Process.h>

#include <nRF24L01.h>
#include <RF24.h>
#include <RF24_config.h>


//RADIO PART

RF24 radio(7, 8);               // RF24 obiekt do radia
RF24Network network(radio);      // network uzywa radio
const uint16_t this_node = 00;    //  adres radiowy uno
const uint16_t other_node = 01;   // adres radiowy mini


//struktura zawierajaca nasze dane o wiadomisci
struct frame_t
{

  bool type; //false-GET | true-PUT
  bool resource; // false-LIGHT | true-ENCODER

  int8_t state; // 0 - OFF/OBSERVE, 255 - ON/NOOBSERVE
  uint8_t Etag;


};




//numer accepta ktory wysyla copper
uint8_t accept_number = 0;



uint8_t oldaccept_number = 0; //okresla content format wiadomosci ktore sa wystlane w wyniku obserwowania encodera

//obecnosc opcji etag, dziala w ten sposob, ze jak cos sie poda w copperze, to jest rozpotrywany tylko jedna wersja etag
bool etag_o = false;

//czy aktualnie jest wlaczone obserwawnie enkodera
bool observe = false;

//czy w ostatniej wiadomosci od coppera pojawila sie opcja observa
//by nie dalo sie w lighcie wlaczyc observe
bool lastobserve = false;

//typ wiadomosci
bool noncon = false; // NON false | CON true

//sluzy do obslugi put light
//jezeli jest jedynka to znaczy by wlaczyc swiatlo, 0 by wylaczyc
byte payl;




//sluzy do stargeo tokena przy uzywaniu opcji observa
uint8_t oldTKL = 0;



//stala alakcja pamieci na maksymalna mozliwa wielkosc tokena
byte oldTOK[15];

//licznik odebranych wiadomosci radiowych
uint16_t receivedRadioMessages = 0;

//ile danych otrzymalismy
uint16_t receivedDataSize = 0;



//typ wiadomosci radiowej, do arduino mini, put w
bool typ; //false-GET | true-PUT

//licznik parametru message ID
uint16_t MID_counter = 0;


bool payload_sended = false; // flaga diagnostic payload

//payload jaki wysylamy z powrotem do coppera
byte pay[60];

//ETHERNET PART
byte mac[] = { 0x00, 0xaa, 0xbb, 0xcc, 0xde, 0xf4 }; //MAC address

const uint8_t MAX_BUFFER = 80; // max UDP payload rozmiar



//zapisujemy do niego czesc otrzymana od coppera
byte packetBuffer[MAX_BUFFER];

//inicjalizacja portu UDP
EthernetUDP Udp;

//UDP port
short localPort = 1221;   //UDP port

//pakiet w ktorym przechowujemy rzeczy pobrane z naglowka wiadomosci
Packet packet;



void setup() {
  for (int i = 0; i < 15; i++) {
    oldTOK[i] = 0;
  }
  Serial.begin(115200);
  Serial.println(F("Uruchamianie Serwera COAP"));
  //Serial.println(F("Kanal radiowy"));
  //Serial.println(110, DEC);
  Serial.println(F("Adres radiowy"));
  Serial.println(this_node, OCT);
  SPI.begin();

  if (Ethernet.begin(mac) == 0)
  {
    Serial.println(F("Nie skonfigurowano lacznasci Ethernet przez DHCP"));

    for (;;)//rob nic
      ;
  }
  Serial.println(F("IP: "));
  //drukowanie IP jakie zostako nam przydzielone
  Serial.println(Ethernet.localIP());
  Serial.println(F("Port: 1221"));
  Udp.begin(localPort);
  radio.begin();
  network.begin(110, this_node); // kanal i adres
}

void MessageLogs(frame_t &message);

void ResetStartValues();

void EncoderMessage(frame_t &message);

void LightMessage(frame_t &message);

void PrintContentFormatOption();

void loop() {
  network.update(); // sprawdz lacznosc radiowa


  while (network.available())//obsluga wiadomosci radiowej
  {


    RF24NetworkHeader header;       // naglowek wiadomosci radiowej
    frame_t message;              // payload radiowy
    network.read(header, &message, sizeof(message));

    //logi wiadomosci
    //MessageLogs(message);

    //  Serial.println("\nStan odpowiedzi:   ");
    //Serial.println(message.state);
    uint8_t iftoken = 0;
    if (etag_o)
    {
      iftoken = 2;
    }
    if (message.resource == true && message.type == true) { //obsluz wiadomosc zwiazana z observe, mini zobaczylo zmiane encodera i wyslalo ta wiadomosc


      byte head[4 + oldTKL + 2 + 2 + 2];
      //Serial.println("oldTKL");
      //Serial.println(oldTKL);
      //MID_counter++;

      for (int j = 0; j < 12; j++)
      {
        pay[j] = 32;
      }



      //jest przesuniecie 1 o 6 bitow, by ustawic Ver, a potem sklejamy poszczegolne elementy w pierwszy bajt
      head[0] = 1 << 6 | 1 << 4 | packet.TKL;

      //ustawiamy: 69= success response
      head[1] = 69;




      //ustawienie MID
      head[2] = MID_counter >> 8;
      head[3] = MID_counter;

      //ustawienia bajtow odpowiedzialnych za token
      for (int i = 0; i < packet.TKL; i++)
      {


        head[4 + i] = packet.token[i];

      }

      //jezeli jest opcja etag
      //sklejanie bitow skladajcych sie na opcje etag
      //


      //ustawieanie opcion delta i opction lenght
      //opcja etag ma numer 4
      //nasz etag bedzie mial dlugosc 1
      head[4 + packet.TKL] = 4 << 4 | 1;
      //wklejamy tresc etag, etag moze miec jeden bajt
      head[5 + packet.TKL] = message.Etag;
      head[6 + packet.TKL] = 1 | (6 - 4 << 4); //observe

      head[7 + packet.TKL] = 1; //40;
      //dwie nastepne linijki dotycza conent format
      //ustala numer delty i opcji
      head[8 + packet.TKL] = 1 | (12 - 6 << 4);
      //40=plain tekst
      head[9 + packet.TKL] = oldaccept_number; //40;
      accept_number = oldaccept_number;



      EncoderMessage(message);



      //wysyl do copera tego co dostalismy od arduino mini
      sendToClient(head, sizeof(head), pay, sizeof(pay));
      // MID_counter++;
    }
    else {//obsluz wiadomosc zwiazana z zadaniem GET coppera
      byte head[4 + packet.TKL + 2 + iftoken];



      for (int j = 0; j < 12; j++)
      {
        pay[j] = 32;
      }
      //ustawienie rodzaju odpowiedzi
      byte T = 0;
      //jak CON to wysylamy zadanie ACK
      if (noncon)
      {
        T = 2 << 4;
      }
      //jak NON tez chcemy NON
      else
      {
        T = 1 << 4;
      }
      //jest przesuniecie 1 o 6 bitow, by ustawic Ver, a potem sklejamy poszczegolne elementy w pierwszy bajt
      head[0] = 1 << 6 | T | packet.TKL;

      //ustawiamy: 69= success response
      head[1] = 69;




      if (noncon)
      {
        head[2] = packet.MessageID >> 8;
        head[3] = packet.MessageID;
      }
      //jak NON tez chcemy NON
      else
      {
        //    ++MID_counter;
        head[2] = MID_counter >> 8;
        head[3] = MID_counter;
      }
      //ustawienia bajtow odpowiedzialnych za token
      for (int i = 0; i < packet.TKL; i++)
      {

        //4 to header lenght
        head[4 + i] = packet.token[i];
        //head[4 + i] = TOK[i];
      }

      //jezeli jest opcja etag
      //sklejanie bitow skladajcych sie na opcje
      //
      if (etag_o)
      {

        //ustawieanie opcion delta i opction lenght
        //opcja etag ma numer 4
        //nasz etag bedzie mial dlugosc 1
        head[4 + packet.TKL] = 4 << 4 | 1;
        //wklejamy tresc etag, etag moze miec jeden bajt
        head[5 + packet.TKL] = message.Etag;

        //dwie nastepne linijki dotycza conent format
        //ustala numer delty i opcji
        head[6 + packet.TKL] = 1 | (12 - 4 << 4);
        //40=plain tekst
        head[7 + packet.TKL] = accept_number; //40;
      }
      else
      {
        //sklejanie opcji bez etaga
        //ustawianie content format, numer delty i opcji
        head[4 + packet.TKL] = 1 | (12 - 0 << 4);
        //40=plain tekst
        head[5 + packet.TKL] = accept_number;//0;
      }

      //ustawianie payloadu
      //endkoder
      if (message.resource == true)
      {

        EncoderMessage(message);
      }
      //lampka
      else
      {
        LightMessage(message);
      }

      //wysyl do copera tego co dostalismy od arduino mini
      sendToClient(head, sizeof(head), pay, sizeof(pay));
    }
    //liczenie metryk radia
    receivedRadioMessages = receivedRadioMessages + 1;
    receivedDataSize += sizeof(message);

  }



  //CZESC Z COPPEREM


  //  //ETHERNET PART
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    //ustawianie domyslnych wartosci pol
    ResetStartValues();

    Udp.read(packetBuffer, MAX_BUFFER); //odczytaj pakiet od klienta
    Serial.println(packetSize);
    //
    //T==0 CON
    //T==1 NON
    //T==2 ACK
    //T==3 RST
    //
    //Code==0.01 GET
    //Code==0.02 POST
    //Code==0.03
    //Code==0.04 DELETE
    //    /*0                   1                   2                   3
    //        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //       |Ver| T |  TKL  |      Code     |          Message ID           |
    //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //       |   Token (if any, TKL bytes) ...
    //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //       |   Options (if any) ...
    //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //       |1 1 1 1 1 1 1 1|    Payload (if any) ...
    //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //    */
    //
    //flag to indicate if coap field is handled
    bool flag = false;
    //flag to indicate diagnostic payload
    bool payload_sended = false;

    //header is first 4B of coap message
    int header_length = 4;

    packet.GetPacket(packetBuffer);

    // PrintHeader(packet);

    //typ NON
    if (packet.T == 1)
    {
      Serial.println(F("Typ: NON"));
      noncon = false; // NON false | CON true
      flag = true;
    }
    //jezeli jet typu CON
    else if (packet.T == 0)
    {
      Serial.println(F("Typ: CON"));
      noncon = true; // NON false | CON true
      flag = true;
    }


    else
    {
      Serial.println(F("Inny typ"));
      flag = true;
      payload_sended = true;
      sendDiagnosticPayload();
      return;
    }

    switch (packet.CodeClass)
    {
      //1-GET 2-POST 3- 4-DELETE
      case 0:
        {
          //request
          //GET
          if (packet.CodeDetail == 1)
          {
            typ = false; //false-GET | true-PUT
            Serial.println(F("GET"));
          }
          //PUT
          else if (packet.CodeDetail == 3)
          {
            typ = true;
            Serial.println(F("PUT"));
          }
          //PING
          else if (packet.CodeDetail == 0)
          {

            Serial.println(F("PING"));
          }
          //delete nie obslugujemy
          else
          {
            Serial.println(F("Inne zapytanie"));
            sendDiagnosticPayload(); //other request, sending diagnostic payload
            payload_sended = true;
          }
          break;
        }
      //odpowiedzi
      case 2:
        {
          //success response
          Serial.println(F("success response"));
          flag = false;
          break;
        }
      case 4:
        {
          //client error response
          Serial.println(F("client error response"));
          flag = true;
          sendDiagnosticPayload();
          payload_sended = true;
          break;
        }
      case 5:
        {
          //server error response
          //Serial.println(F("server error response"));
          flag = true;
          sendDiagnosticPayload();
          payload_sended = true;
          break;
        }
    }




    //iterator po ktorym bedziemy wczytywali opcje
    uint8_t iter = header_length + packet.TKL;

    //until byte is not flag 11111111
    byte options[MAX_BUFFER];
    //Uri-path for indicating the resource
    char * uri_path_option;

    //resource id 0 - light, 1 - encoder, 2 - radio, 3 - well-known/core
    uint8_t resource_id;

    //content format option
    byte content_format_option;

    //number that indicates option
    uint8_t option_no = 0;

    //length of large resource
    uint8_t wellknownLength = 0;
    //length of header without options
    uint8_t h_len_wo_opt = iter;

    // .wellknown/core payload
    byte *body;
    //wellknown/core flag
    bool wellknownflag = false;
    //if etag option received
    bool etag_flag = false;



    //loop until flag 11111111 that ends header
    while ((packetBuffer[iter] != 255) && (packetBuffer[iter] != 0) && iter < packetSize)
    {
      //Serial.print("Iteracja = ");
      //Serial.println(iter);
      uint8_t opt_delta = packetBuffer[iter] >> 4;
      uint8_t opt_length = packetBuffer[iter] & 15;

      byte *opt_value;
      //jezeli Option_Delta==13 potrzebujemy dodatkowego bajtu
      if (opt_delta == 13)
      {
        //Serial.println("opt_delta=13");

        ++iter;
        option_no += packetBuffer[iter] - 13;
      }
      //jezeli Option_Delta==14 potrzebujemy dwóch dodatkowych bajtow
      else if (opt_delta == 14)
      {
        //Serial.println("opt_delta=14");

        ++iter;
        uint8_t number = packetBuffer[iter] | packetBuffer[++iter] << 8;
        option_no += number - 269;
      }
      else
      {
        option_no += opt_delta;
      }

      if (opt_length == 13)
      {
        //extended length by 1B
        //Serial.println("opt_length=13");
        ++iter;
        opt_value = (byte*)malloc(packetBuffer[iter] - 13);
      }
      else if (opt_length == 14)
      {
        //extended length by 2B
        //Serial.println("opt_length=14");
        int number = packetBuffer[iter] | packetBuffer[++iter] << 8;
        opt_value = (byte*)malloc(number - 269);
      }
      else
      {
        opt_value = (byte*)malloc(opt_length);
      }

      //Serial.println(F("delta: "));
      //Serial.println(opt_delta, BIN);
      //Serial.println(F("length: "));
      //Serial.println(opt_length, BIN);
      Serial.println(F("option_no: "));
      Serial.println(option_no, DEC);

      for (int i = 0; i < opt_length; ++i)
      {
        ++iter;
        opt_value[i] = packetBuffer[iter];
      }

      switch (option_no)
      {
        case 4:
          {
            Serial.println(F("ETag opcja: "));

            etag_flag = true; //set etag flag to add to map
            etag_o = true;
            break;
          }

        case 6:
          {
            Serial.println(F("Observe opcja: "));

            lastobserve = true;
            if (opt_value[0] == 0)
            {
              observe = true;
            }
            if (opt_value[0] == 1)
            {
              observe = false;
            }
            break;
          }
        case 11:
          {
            //Serial.println(F("Uri-Path opcja"));

            uri_path_option = (char*)malloc(opt_length);
            for (int i = 0; i < opt_length; ++i)
            {
              uri_path_option[i] = opt_value[i];
              Serial.println(uri_path_option[i]);
            }
            //identyfikacja zasobu
            if (strncmp(uri_path_option, "light", 5) == 0)
            {
              Serial.println(F("LAMPKA"));
              resource_id = 0;
            }
            else if (strncmp(uri_path_option, "encoder", 7) == 0)
            {
              Serial.println(F("ENKODER"));
              resource_id = 1;

            }
            else if (strncmp(uri_path_option, "radio", 5) == 0)
            {
              Serial.println(F("RADIO"));
              resource_id = 2;
            }
            else if (strncmp(uri_path_option, ".well-known", 11) == 0)
            {
              wellknownflag = true; //set well-known/core flag
            }
            else if (strncmp(uri_path_option, "core", 4) == 0 && wellknownflag == true)
            {
              Serial.println(F(".well-known/core"));
              //body = "<E>;rt=\"E\";if=\"sensor\",<L>;rt=\"L\";if=\"sensor\",<R>;rt=\"R\";if=\"sensor\"";
              //body = "<encoder>;rt=\"encoder\";if=\"sensor\",<light>;rt=\"light\";if=\"sensor\",<radio>;rt=\"radio\";if=\"sensor\"";

              body = "<encoder>;rt=\"encoder\",<light>;rt=\"light\",<radio>;rt=\"radio\"";

              wellknownLength = strlen(body);
              resource_id = 3;
            }
            break;
          }
        case 12:
          {
            Serial.println(F("Content-Format opcja:"));

            if (opt_length == 0)
            {
              content_format_option = 0;
            }
            else
            {
              content_format_option = opt_value[0];
            }
            //  Serial.print(F("Content-Format: "));
            Serial.println(content_format_option, DEC);

            //wypisanie opcji content format
            //PrintContentFormatOption(content_format_option);
            break;
          }
        case 17:
          {
            Serial.println(F("Accept opcja:"));
            byte accept_option;

            if (opt_length == 0)
            {
              accept_option = 0;
              accept_number = 0;
            }
            else
            {
              accept_option = opt_value[0];
              accept_number = opt_value[0];

            }
            oldaccept_number = accept_number;
            oldTKL = packet.TKL;

            for (int t = 0; t < oldTKL; t++) {
              oldTOK[t] = packet.token[t];


            }
            //  Serial.println("Accept Number");
            Serial.println(accept_number);

            break;
          }
      }
      //przesuniecie iteratora na nastepna opcje
      ++iter;
    }


    //gdy jest jakis payload to trzeba go tez obsluzyc
    if (packetBuffer[iter] != 0)
    {
      //zwiekszenie iteratora o 1, w pierwszej instancji jest to po to, by pominac bajt samych jedynek
      ++iter;
      //ustalenie rozmiaru payloadu
      byte payload[packetSize - iter];
      for (int j = 0; j < packetSize - iter; ++j)
      {
        payload[j] = packetBuffer[iter + j];

      }
      payl = payload[0];

    }


    //obsluga PING
    if (noncon) {
      if (packet.CodeClass == 0 && packetBuffer[1] == 0) {
        flag = false;
        byte pingack[4];
        pingack[0] = 1 << 6 | 2 << 4 | packet.TKL;
        pingack[1] = 0;//2<<5;
        pingack[2] = packet.MessageID >> 8;
        pingack[3] = packet.MessageID;
        sendemptyACKtoClient(pingack, sizeof(pingack));
        Serial.println(F("PING"));
      }




    }


    //flag na false nic nie robi, gdy flag== true, znaczy ze mamy wyslac do arduino mini lub klienta


    if (flag)
    {
      if (payload_sended == false)
      {
        if (uri_path_option != NULL)
        {
          byte headerToSend[h_len_wo_opt + 2];

          uint8_t it = 0;
          //ustawienie rodzaju odpowiedzi
          byte T = 0;
          //jak CON to wysylamy rzadanie ACK
          if (noncon)
          {
            T = 2 << 4;
          }
          //jak NON tez chcemy NON
          else
          {
            T = 1 << 4;
          }
          //jest przesuniecie 1 o 6 bitow, by ustawic Ver, a potem sklejamy poszczegolne elementy w pierwszy bajt
          headerToSend[it] = 1 << 6 | T | packet.TKL;


          //headerToSend[it] = packetBuffer[0];
          //Code 2.05 OK 01000101
          headerToSend[++it] = 69;
          //MID ab

          if (noncon)
          {
            headerToSend[++it] = packet.MessageID >> 8;
            headerToSend[++it] = packet.MessageID;
          }
          //jak NON tez chcemy NON
          else
          {
            ++MID_counter;
            headerToSend[++it] = MID_counter >> 8;
            headerToSend[++it] = MID_counter;
          }

          //jezeli istnieje token to trzeba go skopiowac do wiadomosci
          if (packet.TKL != 0)
          {
            //memcpy ( &headerToSend+4, &packetBuffer+4, TKL );
            for (uint8_t i = 0; i < packet.TKL; ++i)
            {
              headerToSend[i + 4] = packetBuffer[i + 4];
            }
            //Serial.println(headerToSend[it + packet.TKL], HEX);
          }
          it += packet.TKL;
          ///options
          int delta_offset = 0;

          if (etag_flag == true)
          {
            //ETag delta 4 length 1
            headerToSend[++it] = 1 | (4 << 4);
            delta_offset = 4;
            //Etag value last added to map
            //headerToSend[++it] = map_iterator;
          }

          //Content Format 12 length 1
          headerToSend[++it] = 1 | (12 - delta_offset << 4);
          delta_offset = 12;


          if (wellknownflag == true)
          {
            //content-format = 40
            headerToSend[++it] = 40;
            wellknownflag == false;
          }
          else
          {
            //dodawanie opcji wyswietlania takiej jaka chce accept
            headerToSend[++it] = accept_number;
            //content-format = 0

          }


          ++it;
          //resource id 0 - light, 1 - encoder, 2 - radio, 3 - well-known/core
          //lampka
          if (resource_id == 0)
          {
            struct frame_t payload1;
            //false-GET | true-PUT
            if (typ)
            {
              RF24NetworkHeader header2(other_node);
              payload1.type = true;
              payload1.resource = false;
              payload1.state = 69;

              //48 w Ascii znaczy 0
              if (payl == 48)
              {
                payload1.state = 0;
              }
              //49 w ASCII znaczy 1-wlaczenie lampki
              else if (payl == 49)
              {
                payload1.state = 1;
              }


              payload1.Etag = 255;
              //wyslanie do arduono mini
              network.write(header2, &payload1, sizeof(payload1));

              if (noncon == true) {
                byte headack[4 + packet.TKL];
                headack[0] = 1 << 6 | 2 << 4 | packet.TKL;
                headack[1] = 68;


                headack[2] = packet.MessageID >> 8;
                headack[3] = packet.MessageID;
                for (uint8_t i = 0; i < packet.TKL; i++)
                {

                  //4 to header lenght
                  headack[4 + i] = packet.token[i];

                }



                sendemptyACKtoClient(headack, sizeof(headack));
              }


            }
            else
            {
              RF24NetworkHeader header2(other_node);
              //znaczy ze GET
              payload1.type = false;
              //znaczy ze lampka
              payload1.resource = false;

              network.write(header2, &payload1, sizeof(payload1));
            }



          }
          //jezeli enkoder
          if (resource_id == 1)
          {
            if (lastobserve) {  //wyslij do mini wiadomosc z zadaniem pobrania stanu i ustawieniena stanu obserwowania
              struct frame_t payload;
              RF24NetworkHeader header2(other_node);
              payload.resource = true;

              //  Serial.println(F("OLD COPPY"));
              //Serial.println(oldTKL);
              if (observe) {

                payload.state = 0;
                //zapisz parametry z jakimi maja isc wiadomosci obserwujace


              }
              else {

                payload.state = 1;
              }



              payload.type = true;





              network.write(header2, &payload, sizeof(payload));
            }
            else { //wyslij do mini wiadomosc z zadaniem pobrania stanu
              struct frame_t payload;
              RF24NetworkHeader headerGET(other_node);
              payload.resource = true;
              payload.type = false;
              network.write(headerGET, &payload, sizeof(payload));

            }



          }
          //jezeli radio
          if (resource_id == 2)
          {
            //sklejanie paylodu z metrykami radia

            //wyzerowanie contentu poprzez wstawienie spacji jako napisu
            for (uint8_t i = 0; i < sizeof(pay); i++)
            {
              pay[i] = 32;
            }
            if (accept_number == 0)
            {
              pay[0] = 77; //M
              pay[1] = 83; //S
              pay[2] = 71; //G
              pay[3] = 83; //S
              pay[4] = 58; //dwukropek
              pay[5] = 32; //spacja
              //pay[6] = 32; //spacja
              String myString = String(receivedRadioMessages);
              //ustawienie wartosci enkodera
              uint8_t i = 6;

              for (uint8_t j = 0; j < myString.length(); j++)
              {
                pay[i] = myString.charAt(j);
                i++;
              }
              pay[i] = 32; //spacja
              pay[++i] = 83;//S
              pay[++i] = 73; // I
              pay[++i] = 90;//Z
              pay[++i] = 69;//E
              pay[++i] = 65;//A
              pay[++i] = 76;//L
              pay[++i] = 76;//L
              pay[++i] = 58;//:
              pay[++i] = 32; //spacja

              i++;
              myString = String(receivedDataSize);
              for (uint8_t j = 0; j < myString.length(); j++)
              {
                pay[i] = myString.charAt(j);
                i++;
              }
              pay[i] = 32; //spacja
              pay[++i] = 83;//S
              pay[++i] = 73; // I
              pay[++i] = 90;//Z
              pay[++i] = 69;//E
              pay[++i] = 79;//O
              pay[++i] = 78;//N
              pay[++i] = 69;//E
              pay[++i] = 58;//:
              pay[++i] = 32; //spacja
              ++i;
              myString = String(sizeof(struct frame_t));
              for (uint8_t j = 0; j < myString.length(); j++)
              {
                pay[i] = myString.charAt(j);
                i++;
              }

            }
            else  if (accept_number == 1)
            {
              pay[0] = 60; // <
              pay[1] = 77; //M
              pay[2] = 83; //S
              pay[3] = 71; //G
              pay[4] = 83; //S
              pay[5] = 62; //>

              uint8_t i = 6;
              String myString = String(receivedRadioMessages);
              for (uint8_t j = 0; j < myString.length(); j++)
              {
                pay[i] = myString.charAt(j);
                i++;
              }

              pay[i] = 60; //<
              pay[++i] = 47; // /
              pay[++i] = 77; //M
              pay[++i] = 83; //S
              pay[++i] = 71; //G
              pay[++i] = 83; //S
              pay[++i] = 62; //>
              pay[++i] = 60; //<
              pay[++i] = 83;//S
              pay[++i] = 73; // I
              pay[++i] = 90;//Z
              pay[++i] = 69;//E
              pay[++i] = 65;//A
              pay[++i] = 76;//L
              pay[++i] = 76;//L
              pay[++i] = 62; //>
              ++i;
              myString = String(receivedDataSize);
              for (uint8_t j = 0; j < myString.length(); j++)
              {
                pay[i] = myString.charAt(j);
                i++;
              }
              pay[i] = 60; //<
              pay[++i] = 47; // /
              pay[++i] = 83;//S
              pay[++i] = 73; // I
              pay[++i] = 90;//Z
              pay[++i] = 69;//E
              pay[++i] = 65;//A
              pay[++i] = 76;//L
              pay[++i] = 76;//L
              pay[++i] = 62; // >
              pay[++i] = 60; //<
              pay[++i] = 83;//S
              pay[++i] = 73; // I
              pay[++i] = 90;//Z
              pay[++i] = 69;//E
              pay[++i] = 79;//O
              pay[++i] = 78;//N
              pay[++i] = 69;//E
              pay[++i] = 62; //>
              ++i;
              myString = String(sizeof(struct frame_t));
              for (int j = 0; j < myString.length(); j++)
              {
                pay[i] = myString.charAt(j);
                i++;
              }
              pay[i] = 60; //<
              pay[++i] = 47; // /
              pay[++i] = 83;//S
              pay[++i] = 73; // I
              pay[++i] = 90;//Z
              pay[++i] = 69;//E
              pay[++i] = 79;//O
              pay[++i] = 78;//N
              pay[++i] = 69;//E
              pay[++i] = 62; // >



            }





            sendToClient(headerToSend, sizeof(headerToSend), pay, sizeof(pay));

          }
          if (resource_id == 3)
          {
            //send specified block of .wellknown/core

            byte payloadToSend[wellknownLength];
            for (uint8_t i = 0; i < wellknownLength; ++i)
            {
              payloadToSend[i] = body[i];

            }


            sendToClient(headerToSend, sizeof(headerToSend), payloadToSend, sizeof(payloadToSend)); // send wellknown/core to coap client
          }
        }
      }
    }
  }
}


void PrintContentFormatOption(byte content_format_option)
{
  if (content_format_option == 0)
  {
    Serial.println(F("text/plain"));
  }
  else if (content_format_option == 40)
  {
    Serial.println(F("application/link-format"));
  }
  else if (content_format_option == 41)
  {
    Serial.println(F("application/xml"));
  }
  else if (content_format_option == 42)
  {
    Serial.println(F("application/octet-stream"));
  }
  else if (content_format_option == 47)
  {
    Serial.println(F("application/exi"));
  }
  else if (content_format_option == 50)
  {
    Serial.println(F("application/json"));
  }
}

//drukuj header
void PrintHeader(Packet &packet)
{
  Serial.println();
  Serial.print("Ver: ");
  Serial.println(packet.Ver);

  Serial.print("T: ");
  Serial.println(packet.T);

  Serial.print("TKL: ");
  Serial.println(packet.Ver);

  Serial.print("TKL: ");
  Serial.println(packet.Ver);

  Serial.print("Code Class:   ");
  Serial.print(packet.CodeClass);
  Serial.print("   Detail:  ");
  Serial.println(packet.CodeDetail);

  Serial.print("MessageID: ");
  Serial.println(packet.MessageID);

  Serial.println();
}
//ustaw payload wiadomosci lampki
void LightMessage(frame_t &message)
{
  for (uint8_t i = 0; i < sizeof(pay); i++)
  {
    pay[i] = 32; //spacja
  }

  if (message.state == 1)
  {
    pay[7] = 79;//O
    pay[8] = 78;//N

  }
  else if (message.state == 0)
  {
    pay[7] = 79;//O
    pay[8] = 70;//F
    pay[9] = 70;//F
  }
  pay[1] = 76;//L
  pay[2] = 73;//I
  pay[3] = 71;//G
  pay[4] = 72;//H
  pay[5] = 84;//T
  //ustawienie wiadomosci z odpowiednim stanem lampki


  if (accept_number == 1)
  {
    pay[0] = 60;//<
    pay[6] = 62; //>

    //kto by sie czepial jednej spacji w xml, a jaka oszczednosc pamieci
    pay[10] = 60;//<
    pay[11] = 47;// /
    pay[12] = 76; //L
    pay[13] = 73;//I
    pay[14] = 71;//G
    pay[15] = 72;//H
    pay[16] = 84;//T
    pay[17] = 62; //>

  }


}

//ustaw payload wiadomosci enkodera
void EncoderMessage(frame_t &message)
{
  for (uint8_t i = 0; i < sizeof(pay); i++)
  {
    pay[i] = 32; //spacja
  }

  pay[1] = 69;//E
  pay[2] = 78;//N
  pay[3] = 67;//C
  pay[4] = 79;//O
  pay[5] = 68;//D
  pay[6] = 69;//E
  pay[7] = 82;//R
  String myString = String(message.state);
  //ustawienie wartosci enkodera
  uint8_t i = 0;
  while (i < myString.length())
  {
    pay[9 + i] = myString.charAt(i);
    i++;
  }

  if (accept_number == 1)
  {
    uint8_t i = 9 + myString.length();
    pay[0] = 60; //<
    pay[8] = 62; // >
    pay[i] = 60; //<
    pay[++i] = 47; // /
    pay[++i ] = 69; //E
    pay[++i] = 78; //N
    pay[++i] = 67; //C
    pay[++i ] = 79; //O
    pay[++i ] = 68; //D
    pay[++i] = 69; //E
    pay[++i] = 82; //R
    pay[++i ] = 62; //>
    /*  pay[i + 1] = 69; //E
      pay[i + 2] = 78; //N
      pay[i + 3] = 67; //C
      pay[i + 4] = 79; //O
      pay[i + 5] = 68; //D
      pay[i + 6] = 69; //E
      pay[i + 7] = 82; //R
      pay[i + 8] = 62; //>*/
  }
}
//reset zmiennych
void ResetStartValues()
{



  etag_o = false;
  lastobserve = false;
}
//logi
void MessageLogs(frame_t &message)
{
  Serial.print("Type ");

  if (message.type == true)
  {
    Serial.print("PUT");
  }
  else
  {
    Serial.print("GET");
  }
  Serial.print(" Resource ");
  if (message.resource == true)
  {
    Serial.print("ENCODER");
  }
  else
  {
    Serial.print("LIGHT");
  }
}




//wyslij pakiet do klienta
void sendToClient(byte *header, int header_size, byte *payload, int payload_size)
{
  //rozmiar pakietu: header + flag + payload
  byte packet[header_size + 1 + payload_size];

  //wypelnij pakiet headerem
  memcpy(&packet, header, header_size);
  //ustawienie samych jedynek na bicie rozdzielajacym header i pakiet
  packet[header_size] = 255;

  //uzupelnienie packetu payloadem
  int it = 0;
  for (uint8_t i = header_size + 1; i < sizeof(packet) / sizeof(packet[0]); ++i)
  {
    packet[i] = payload[it];
    ++it;
  }

  Serial.println(F("wysylanie wiadomosci do klienta"));
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  int r = Udp.write(packet, sizeof(packet));

  Udp.endPacket();
}
void sendemptyACKtoClient(byte *header, int header_size) // wysylanie pustego ack na potrzeby CON PUT i PING
{
  //packet size header + flag + payload
  byte packet[header_size];

  //fill packet array with function arguments
  memcpy(&packet, header, header_size);
  Serial.println(F("sending message to client"));
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  int r = Udp.write(packet, sizeof(packet));
  //Serial.println(r);
  Udp.endPacket();
}
//function which sends diagnostic payload to coap client
void sendDiagnosticPayload()
{

  uint8_t buffer_size = 4; //rozmiar headera
  byte header[buffer_size];
  //Ver: 01 NON: 01 TKL: 0000
  header[0] = 80;
  //Code: 5.01 - not implemented 10100001
  //4.06
  header[1] = 4 << 5 | 6 ;
  ++MID_counter;
  header[2] = MID_counter >> 8;
  header[3] = MID_counter;


  Serial.println(F("wysylam diagnostyczny payload"));
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  uint8_t r = Udp.write(header, buffer_size);
  Serial.println(r);
  Udp.endPacket();
}
