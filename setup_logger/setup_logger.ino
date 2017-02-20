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
//setup_logger.ino 
//Author: Katie Awty-Carroll (klh5@aber.ac.uk)
// 

/*
 * setup_logger -               Script used to set up wireless sensor nodes. Can be used to set/view the node ID in EEPROM, and set/view OneWire addresses in EEPROM. 
 * 
 *                              Can also be used to reset the EEPROM (write all bytes to 0).
 * 
 *                        
*/

#include <EEPROM.h>
#include <DallasTemperature.h>
#include <OneWire.h>

#define ONE_WIRE_BUS 4                    //Pin for OneWire sensors, which are all on the same bus
#define ID_INDEX 0                        //Node ID is held at index 0
#define OW_INDEX 1                        //Number of Onewire sensors is held at index 1

OneWire onewire(ONE_WIRE_BUS); 			  
DallasTemperature ow_sensors(&onewire);

void setup() {
  
  Serial.begin(115200);
  ow_sensors.begin();
  
}

/*
 * Shows the menu options, and accepts user input as to what they want to do next.
 */
void loop() {

  Serial.println("Select an option:\n'1' to set node ID\n'2' to set OneWire sensors (this will erase the current OneWire data)\n'3' To view node ID\n'4' To view OneWire sensors\n'5' to reset EEPROM (set all bytes to 0)");
  while(!Serial.available()) {}

  char user_in = Serial.read();
  
  switch(user_in) {

    case('1'):
      set_node_id();
      break;

    case('2'):
      set_ow_sensors();
      break;

    case('3'):
      view_node_id();
      break;

    case('4'):
      get_ow_sensors();
      break;

    case('5'):
      wipe_memory();
      break;

    case('6'):
      get_node_id();
      break;

    case('7'):
      get_ow_simple();
      break;

    default:
      Serial.println("Invalid input.");
  }
}

/*
 * Sets the node id in EEPROM, according to what the user specifies. Checks that the new id is within the correct range. Prints the new node id back to the user.
 */
void set_node_id() {

  int node_id;

  Serial.println("Enter new node ID:");
  while(!Serial.available()) {}

  node_id = Serial.parseInt();

  if(node_id > 1 && node_id < 255) {            //1 is the gateway, and 255 is the broadcast address, so they are out of bounds
    Serial.println("Setting node id...");
    EEPROM.write(ID_INDEX, node_id);
    Serial.println("Node ID set.");

    int id = EEPROM.read(0);
    Serial.print("Node ID is: "); Serial.println(id);
  }
  else {
    Serial.println("Node ID must be greater than 1 and less than 255.");
  }
}

void view_node_id() {

  int id = EEPROM.read(0);

  if(id == 0 || id == 255) {
    Serial.println("Node ID has not been set.");
  }
  else {
    Serial.print("Node ID is: "); Serial.println(id);
  }
}

/*
 * Prints the node id currently in EEPROM, with no padding
 */
void get_node_id() {

  Serial.println(EEPROM.read(0));
}

/*
 * Sets the addresses of the OneWire sensors in EEPROM. It is important that they are added in the right order, so that the physical locations of sensors can be derived from the
 * order in which their readings appear in packets. Example: the sensor connected to "onewire3" on the board is read third by the logger and appears third in the data packet 
 * which is sent back to the coordinator.
 * 
 * As this function relies on comparing each sensor to all sensors currently in EEPROM, any sensors currently in EEPROM are erased first.
 * 
 * Sensors are added one by one from onewire1 to onewire5. Less than 5 sensors can be added; the user is asked to specify how many are being added at the start.
 */
void set_ow_sensors() {

  int index = 2;
  int comp_index = 2;
  byte curr_address[9];
  byte comp_address[9];
  int user_num;
  boolean is_new = true;
  int comp_val;

  wipe_ow();                                                //Wipe the current OneWire data
  
  Serial.println("OneWire sensors need to be added one by one to connectors 1-5. This allows mapping between their physical location and the order they are read from.\nLess than 5 sensors can be added.");
  Serial.println("Number of sensors being connected:");

   while(!Serial.available()) {}

  user_num = Serial.parseInt();
  Serial.print("Adding "); Serial.print(user_num); Serial.println(" sensors...");

  EEPROM.write(OW_INDEX, user_num);

  for(int i=1; i<=user_num;i++) {                           //For each sensor the user is adding
    
    onewire.reset_search();                                 //Reset the search - otherwise the new sensor will not be detected
    Serial.print("Attach sensor "); Serial.println(i);
    Serial.println("Enter 'a' when attached.");             //Attach the new sensor
    
    while(!Serial.find('a')) {}                             //Wait for user response

    while(onewire.search(curr_address)) {                   //Loop through all available addresses     
      
      comp_index = 2;                                       //Reset index to start again from beginning of EEPROM        
      is_new = true;
          
      for(int k=1; k<i; k++) {                             //For each sensor already in EEPROM
        for(int l=0;l<8;l++) {                             //Copy its address to comp_address
          comp_address[l] = EEPROM.read(comp_index);
          comp_index++;
        }
        comp_val = memcmp(comp_address, curr_address, 8);   //Compare comp_address and curr_address in memory
        if(comp_val == 0) {                                 //If comp_val is 0, they are the same
          is_new = false;                                   //The sensor can't be new if it has matched one already stored
        }
      }
      if(is_new == true) {                                  //If curr_sensor has been compared with all sensors in EEPROM, and is_new is still true, it must be new
        break; 
      }
    }
    
	//Print the new address
    Serial.print("New address is "); print_ow_address(curr_address);

	//Check that the new address is valid
    if(ow_sensors.validAddress(curr_address)) {
      Serial.println("Address is valid.");
          
    for(int j=0;j<8;j++) {
      EEPROM.write(index, curr_address[j]);
      index++;
    }
    }
  }
  Serial.println("Done.");
  get_ow_sensors();
}

/*
 * Retrieves the addresses of all the onewire sensors in EEPROM, in order.
 */
void get_ow_sensors() {

  int num_sensors;
  int index = 2;
  byte curr_address[9];
  DeviceAddress curr_device;
  
  num_sensors = EEPROM.read(OW_INDEX);

  if(num_sensors > 0) {
    Serial.print("Looking for "); Serial.print(num_sensors); Serial.println(" sensors in EEPROM.");
    
    for(int i=1; i<=num_sensors; i++) {
      for(int j=0;j<8;j++) {
          curr_address[j] = EEPROM.read(index);
          index++;
        }
        
      Serial.print("Sensor "); Serial.print(i); Serial.print(" address: "); 
      print_ow_address(curr_address);
      memcpy(curr_device, curr_address, 8);
      if(ow_sensors.validAddress(curr_device)) {
        Serial.println("Address is valid.");
      }
      else {
        Serial.println("Address is not valid.");
      }
    }
  }
  else {
    Serial.println("No OneWire sensors in EEPROM.");
  }
}

/*
 * Prints a onewire address.
 */
void print_ow_address(DeviceAddress address) {
  
  for(int i=0; i<8; i++) {
    Serial.print(address[i], HEX);
  }
  Serial.println();
}

/*
 * Sets all bytes in EEPROM to equal 0.
 */
void wipe_memory() {

  Serial.print("Wiping EEPROM...");
  for(int i=0; i<EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  }
  Serial.println("Done.");
}

/*
 * Sets all bytes after the node ID byte to 0, wiping the current OneWire ROM codes from memory
 */
void wipe_ow() {

  Serial.print("Wiping OneWire data...");
  for(int i=OW_INDEX; i<44; i++) {
    EEPROM.write(i, 0);
  }
  Serial.println("Done.");
}

/*
 * Retrieves the addresses of all the onewire sensors in EEPROM, in order, in a format that the companion Python script can understand.
 */
void get_ow_simple() {

  int num_sensors;
  int index = 2;
  byte curr_address[9];
  
  num_sensors = EEPROM.read(OW_INDEX);

  Serial.print(num_sensors);
    
  for(int i=1; i<=num_sensors; i++) {
    for(int j=0;j<8;j++) {
      curr_address[j] = EEPROM.read(index);
        index++;
    }

    print_ow_address(curr_address);
  }
}



