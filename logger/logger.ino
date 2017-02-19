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
//logger.ino 
//Author: Katie Awty-Carroll (klh5@aber.ac.uk)
// 

/*
 * logger -               Script to collect data from OneWire temperature sensors (DS18B20) and photodiode (VTB8441BH and/or VTB8440BH), and send to coordinator. 
 * 
 *                        Currently set up to allow up to 5 OneWire sensors and one photodiode.
*/

//Libraries
#include <EEPROM.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <LowPower.h>
#include <RFM69.h>
#include <SPI.h>
#include <avr/wdt.h>

//Radio setup
#define NETWORKID     100                 //All nodes that communicate need to be on the same network
#define GATEWAYID     1                   //The ID of the gateway node
#define FREQUENCY   RF69_433MHZ
#define ENCRYPTKEY "1111AAAA2222BBBB"     //Needs to be 16 characters

//Sensor setup
#define PAR_PIN A1                        //Pin for photodiode
#define ONE_WIRE_BUS 4                    //Pin for OneWire sensors, which are all on the same bus
#define MAX_ONEWIRE_SENSORS 5             //The maximum number of OneWire sensors
#define ID_INDEX 0                        //Node ID is held at index 0
#define OW_INDEX 1                        //Number of Onewire sensors is held at index 1
#define VOLTAGE_PIN A0                    //The pin to read v_in from
#define R1 1000.0                         //Value of resistor 1 in voltage divider
#define R2 330.0                          //Value of resistor 2 in voltage divider

//Global variables
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature ow_sensors(&oneWire);
DeviceAddress onewire_list[MAX_ONEWIRE_SENSORS+1];                                                 
RFM69 radio;
int node_id;              //Unique to each node
int num_ow_sensors = 0;   //Starts at 0, since we haven't found any yet
float temp_totals[MAX_ONEWIRE_SENSORS+1];
int readings_taken;
float par_total;
bool node_init = false;

//Struct to hold the data that will be sent to the gateway node, makes sending/receiving easier
typedef struct {

  byte num_temp_sensors;
  float all_temp[MAX_ONEWIRE_SENSORS+1];
  float light;
  float voltage;
  int num_readings;
} sensor_data; 

sensor_data packet;
  
void setup(void) {
  
  //Fetch node id from EEPROM
  node_id = EEPROM.read(ID_INDEX);

  //Set up OneWire sensors
  ow_sensors.begin();
  fetch_ow_sensors();                     //Reads the addresses of OneWire sensors from EEPROM
  ow_sensors.setWaitForConversion(false); //We want to take PAR readings whilst waiting for temperature
  ow_sensors.setResolution(12);           //All OneWire sensors should have the same resolution (DS18B20 is capable of up to 12 bit)

  //Set up radio
  radio.initialize(FREQUENCY, node_id, NETWORKID);
  radio.encrypt(ENCRYPTKEY);                      //Use encryption
}

void loop(void) { 

  char radio_in[5];
  
  radio.receiveDone(); //Wake radio up - need it in RX mode
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_ON);   //Will wake back up if a packet is received
  
  wdt_enable(WDTO_2S);
   
  if(radio.receiveDone()) {

    if(radio.DATALEN == 4) {                             //Filter out bad/unwanted packets
      sscanf((const char*)radio.DATA, "%s", radio_in);   //Copy data from radio packet into radio_in
      radio.sleep();                                     //Don't need this for a while

      if(strcmp(radio_in, "INIT") == 0) {
        reset_data();
        node_init = true;
        wdt_disable();
        long_sleep();
      }
      else if(strcmp(radio_in, "READ") == 0 && node_init == true) {
        get_readings();
        readings_taken++;
        if(readings_taken > 10) {                       //More than 10 readings means a SEND command has been missed, and the number of readings needs to be reset
          reset_data();                                 
          node_init = false;
        }
        wdt_disable();
        long_sleep();
      }
      else if(strcmp(radio_in, "SEND") == 0) {          //Send command means that a 5 minute interval has been reached
        if(node_init == false) {                        //If the node has not already been initialized, then initialize it
          reset_data();
          node_init = true;
          wdt_disable();
          long_sleep();
        }
        else {
          get_readings();                               //If the node has been initialized, it can just carry on
          readings_taken++;
          packet.num_readings = readings_taken;
          average_readings();
          get_voltage();
          wdt_disable();
          send_packet();                        
          reset_data();
          sleep_remain();
        }
      }
    }
  }
}

/*
 * Sleep for 27 seconds (3*8)+2+1. Originally 28 seconds but some nodes were missing packets
 */
void long_sleep() {

  byte i; 

  for(i=0; i<3; i++) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_ON);
  }
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_ON);
  LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_ON);
}

/*
 * Sleep for the remaining time after sending a packet. Since it could take several seconds for a logger to send, this time needs to be taken into account.
 */
void sleep_remain() {

  int i;
  int num_sleeps = 266 - node_id; 

  //Sleep for up to ~16 more seconds, depending on how much it has already slept
  for(i=0; i<num_sleeps; i++) {
    LowPower.powerDown(SLEEP_60MS, ADC_OFF, BOD_ON); //16
  }

  //Sleep for another 11 seconds, in larger increments, to get to 27 seconds
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_ON);
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_ON); //19
  LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_ON);
}

void get_readings() {

  float light_reading;
  byte i;
  
  ow_sensors.requestTemperatures();
  light_reading = get_par(); 
  par_total = par_total + light_reading;
  
  for(i=0; i<num_ow_sensors; i++) {
    float temp_celsius = ow_sensors.getTempC(onewire_list[i]);
    temp_totals[i] = temp_totals[i]+temp_celsius;
  }
}

void average_readings() {

  float par_reading;
  byte i;
  
  for(i=0; i<num_ow_sensors; i++) {
    temp_totals[i] = temp_totals[i]/readings_taken;
    packet.all_temp[i] = temp_totals[i];
    temp_totals[i] = 0;
  }

  par_reading = par_total / readings_taken;
  packet.light = par_reading;
}

/*
 * Send a packet containing the data struct to the GATEWAY node. Try NUM_RETRIES amount of times, if an ACK is not received back, then sending failed. Each logger will sleep for a different length of time, determined by its node ID. So logger 2 will 
 * sleep for 60MS, node 3 for 120MS, node 4 for 180MS and so on (node 1 is the gateway). This gives every logger a 60ms window in which it will be clear to send.
 * 
 */
void send_packet() {

  byte i;
  
  for(i=1; i<node_id; i++) {
    LowPower.powerDown(SLEEP_60MS, ADC_OFF, BOD_ON);
  }
  
  radio.sendWithRetry(GATEWAYID, (const void*)(&packet), sizeof(packet));
  radio.sleep();      //Need to sleep the radio again after sending
}

/*
 * Take readings from the photodiode for 1 second to obtain an approximate PPFD reading.
 */
float get_par() {

  long total = 0;
  int num_readings = 0;
  unsigned long start_time;
  float par;

  start_time = millis();
 
  while(millis() - start_time < 1000) {
    int reading = analogRead(PAR_PIN);
    total += reading;
    num_readings++;
  }
  
  par = total/num_readings;
  return par;
}

/*
 * Read VOLTAGE_PIN 10 times to get an average input voltage, using a voltage divider to get the voltage to an acceptable level for input, i.e. <= 3.3V. Then do some maths to work out the actual voltage based on the values of the resistors in the voltage divider.
 * See documentation for an explanation of how battery voltage is calculated.
 * The current resistor setup is safe for inputs up to about 12V.
 */
void get_voltage() {

  int total = 0;
  byte i;

  for(i=0; i<10; i++) {
    total += analogRead(VOLTAGE_PIN);
  }
  
  float v_in = (total/10.0) * 0.0032 * (1/(R2/(R1+R2)));
  
  packet.voltage = v_in;
}

/*
 * Set all sensor data values in the packet to -99, so that it's obvious if data is missing at the other end. Reset all data on the node to 0.
 */
void reset_data() {

  byte i;
  
  //Reset data in packet
  packet.light = -99;
  packet.num_readings = 0;
  packet.voltage = 0;
  
  for(i=0; i<MAX_ONEWIRE_SENSORS; i++) {
    packet.all_temp[i] = -99;
  }

  //Reset readings
  readings_taken = 0;
  par_total = 0;
  
  for(i=0; i<num_ow_sensors; i++) {
    temp_totals[i] = 0;
  }
}

/*
 * Fetch the OneWire sensor addresses from EEPROM, test that they are valid, and if they are add them to the array of attached OneWire sensors. This way, the OneWire sensors will be read from in the same order in which they appear in EEPROM. This allows us
 * to look up sensors in the database according to this order. 
 */
void fetch_ow_sensors() {
  
  byte i, j;
  int index = 2;
  byte curr_address[8];
  DeviceAddress curr_device;
  
  num_ow_sensors = EEPROM.read(OW_INDEX);
  packet.num_temp_sensors = num_ow_sensors;

  if(num_ow_sensors > 0) {
    for(i=0;i<num_ow_sensors;i++) {
      for(j=0;j<8;j++) {
          curr_address[j] = EEPROM.read(index);
          index++;
        }
      
      memcpy(curr_device, curr_address, 8);
      if(ow_sensors.validAddress(curr_device)) {
        memcpy(onewire_list[i], curr_device, 8);
      }
    }
  }
}

