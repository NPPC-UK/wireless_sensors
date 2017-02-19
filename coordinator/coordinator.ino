//Copyright (C) 2017    Aberystwyth University  
// 
//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//coordinator.ino 
//Author: Katie Awty-Carroll (klh5@aber.ac.uk)
// 

/*
 * coordinator -           Script to co-ordinate readings and receive packets from nodes running logger, and output them through the serial port. 
 * 
 *                         Keeps time by requesting timestamps through the serial port.
 * 
 *                         Sends ACK to node once it has received a packet.
 *                         
 */

//Libraries
#include <RFM69.h>
#include <SPI.h>
#include <Time.h>
#include <TimeLib.h>

//Radio setup
#define NODEID      1
#define NETWORKID   100
#define FREQUENCY   RF69_433MHZ
#define ENCRYPTKEY  "1111AAAA2222BBBB"  //Needs to be 16 characters

//Extra constants
#define MAX_ONEWIRE_SENSORS 5           //The maximum number of OneWire sensors
#define WAIT_TIME 18000                  //The amount of time allocated to listening for packets once a "SEND" has been issued

//Global variables
RFM69 radio;
bool sent_read;
bool sent_send;
char take_reading[5] = "READ";
char send_data[5] = "SEND";
char init_nodes[5] = "INIT";

//Struct to hold the data that will be sent to the gateway node, makes sending/receiving easier (and packets smaller, as no need to send seperators)
typedef struct {

  byte num_temp_sensors;
  float all_temp[MAX_ONEWIRE_SENSORS+1];
  float light;
  float voltage;
  int num_readings;
} sensor_data; 

void setup() {

  Serial.begin(115200);
  sent_read = true;
  sent_send = true;
  fetch_time(true);

  //Set up radio
  radio.initialize(FREQUENCY, NODEID, NETWORKID);
  radio.encrypt(ENCRYPTKEY);
  radio.promiscuous(false);   //Only look at packets sent to this gateway (99)

  while(true) {   //Wait for the next 5 minute interval to start logging
    fetch_time(false);
    if(minute() % 5 == 0 && second() == 0) {
      break;
    }
  }

  Serial.println("Resetting nodes");
  radio.send(255, init_nodes, strlen(init_nodes));
}

void loop() {

  fetch_time(false);
  time_t t = now();
  
  if(second(t) == 0) {
    if(minute(t) % 5 == 0 || minute(t) == 0) {
      if(sent_send == false) {
        Serial.println("Sending send data command");
        radio.send(255, send_data, strlen(send_data));
        sent_send = true;
        waitForReadings();
      }
    }
    else {
      sent_send = false;
      if(sent_read == false) {
        Serial.println("Sending read sensor command");
        sent_read = true;
        radio.send(255, take_reading, strlen(take_reading));
      }
    }
  }
  else if(second(t) == 30) {
    if(sent_read == false) {
      Serial.println("Sending read sensor command");
      sent_read = true;
      radio.send(255, take_reading, strlen(take_reading));
    }
  }
  else {
    sent_read = false;
  }
}

//Data being printed to the serial port currently has the format: SENDER_ID VOLTAGE LIGHT NUM_TEMP TEMP_1...TEMP_5 NUM_READINGS RSSI
void waitForReadings() {
  
  byte i;
  unsigned long start_time = millis();
  sensor_data packet;

  while(millis() - start_time < WAIT_TIME) {

    if(radio.receiveDone()) {

      if (radio.DATALEN != sizeof(sensor_data)) {
        Serial.println("INVALID PACKET");
      }
      else {
        packet = *(sensor_data*)radio.DATA;
        byte sender_id = radio.SENDERID;
        int sender_rssi = radio.readRSSI();

        if(radio.ACKRequested()) {
          radio.sendACK();
        } 
      
        Serial.print(sender_id); Serial.print(" ");
        Serial.print(NETWORKID); Serial.print(" ");
        Serial.print(packet.voltage); Serial.print(" ");
        Serial.print(packet.light); Serial.print(" "); 
        Serial.print(packet.num_temp_sensors); Serial.print(" ");

      
        for(i=0; i<MAX_ONEWIRE_SENSORS; i++) {
          Serial.print(packet.all_temp[i]); Serial.print(" ");
        }

        Serial.print(packet.num_readings); Serial.print(" ");
        Serial.println(sender_rssi);
      }
    }
  }
}

/*
 * Request the current time through the serial port. If if_wait is true, delay until a response is detected through the serial port. This is so that the coordinator will always receive at least one accurate timestamp before logging begins.
 */
void fetch_time(boolean if_wait) {

  unsigned long comp_time;
  
  Serial.println("TIME");

  if(if_wait) {
    while(!Serial.available()) {    //Wait for reply from Python script
      flash_led();
    }   
  }
  if(Serial.find('T')) {
    comp_time = Serial.parseInt();
    setTime(comp_time);
  }
}

/*
 * Briefly flash the onboard LED, which is attached to pin 9
*/
void flash_led()
{
  pinMode(9, OUTPUT);
  digitalWrite(9,HIGH);
  delay(50);
  digitalWrite(9,LOW);
}

