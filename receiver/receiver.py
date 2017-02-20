#!/usr/bin/python

 # Copyright (C) 2017		Aberystwyth University	
 
 # This program is free software: you can redistribute it and/or modify
 # it under the terms of the GNU General Public License as published by
 # the Free Software Foundation, either version 3 of the License, or
 # (at your option) any later version.

 # This program is distributed in the hope that it will be useful,
 # but WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 # GNU General Public License for more details.

 # You should have received a copy of the GNU General Public License
 # along with this program.  If not, see <http://www.gnu.org/licenses/>.

 # receiver.py 
 # Author: Katie Awty-Carroll (klh5@aber.ac.uk)
 # 

import serial
import time
import MySQLdb
import sys
from configparser import ConfigParser

"""
receiver.py - 		Python 2.7.6 script to read and process data received from coordinator, and send it to the database.

"""

#Packet:
#SENDER_ID NETWORK_ID VOLTAGE LIGHT NUM_TEMP TEMP_1 TEMP_2 TEMP_3 TEMP_4 TEMP_5 NUM_READINGS RSSI

len_packet = 12 		#Expected length of packet, once it has been split into seperate readings
time_req = "TIME" 		#Packet which indicates a time update is required

parser = ConfigParser()

#The directory where the config file is stored
parser.read("config")

#Fetch database details from config file
db_host = parser.get("db_connect", "host")
db_user = parser.get("db_connect", "user")
db_pass = parser.get("db_connect", "password")
db_schema = parser.get("db_connect", "schema")
	
try:
	serial_conn = serial.Serial("/dev/ttyUSB0", 115200)
except OSError as e:
	print "ERROR: Could not open serial port: %s" % (e)
	sys.exit(1)	

while(True):
	
	#Set up empty db object, so that we can check if the connection is made later on
	db = None
	
	#Read data from the serial port
	packet = serial_conn.readline()

	#Split the packet up by whitespace
	data = packet.split()
			
	#If the packet is a request for a timestamp, send one back
	if time_req in packet:

		#Time packets start with a "T" so that they can be easily recognised by the coordinator
		epoch_time = "T" + str(time.time())

		#Print time to the serial port
		serial_conn.write(epoch_time)
			
	#If the packet is a data packet, process it
	elif len(data) == len_packet:
		
		#Fetch the time			
		date_time = time.strftime("%Y-%m-%d %H:%M:%S")

		#Print the time and the packet, for logging purposes
		print date_time +  " " + packet,

		#Process the packet
		try:
			#Connect to the database
			db = MySQLdb.connect(db_host, db_user, db_pass, db_schema)

			#Create the cursor to fetch the data
			cursor = db.cursor()

			#Query to find sensors
			find_sensors = "SELECT * FROM sensors JOIN locations ON locations.location_sensor_id = sensors.sensor_id WHERE node_id = '%s' AND network = '%s';" % (data[0], data[1])  
	
			#Execute the query
			cursor.execute(find_sensors)

			#Fetch the list of sensors	
			sensor_list = cursor.fetchall()

			for sensor in sensor_list:

				#Check if the sensor is a temperature sensor
				if sensor[3] == "temperature":	

					#Get the order of the sensor relative to the node. Sensors are read from 1-5, and a sensor's place in that order is recorded in the database					
					sensor_order = sensor[14]	

					#Find the temperature reading in the packet.
					temp_reading = data[4+int(sensor_order)]

					#Prepare the SQL statment
					send_tmp_reading = "INSERT INTO readings (reading_sensor_id, reading, date_time, voltage) VALUES ('%s', '%s', '%s', '%s');" % (sensor[0], temp_reading, date_time, data[2])	

					#Execute the statement
					cursor.execute(send_tmp_reading)
		
				#Check if the sensor is a light sensor
				if sensor[3] == "light":	

					#Find the light reading in the packet - there is only one, and it is always in the same place				
					light_reading = data[3]

					#Prepare the SQL statement
					send_light_reading = "INSERT INTO readings (reading_sensor_id, reading, date_time, voltage) VALUES ('%s', '%s', '%s', '%s');" % (sensor[0], light_reading, date_time, data[2])

					#Execute the statement
					cursor.execute(send_light_reading)

			#Commit all of the changes to the database
			db.commit()

		except MySQLdb.Error as e:
			print "***ERROR***: Database error: %s %s" % (e[0], e[1])
			if db:
				db.rollback()
		finally:
			if db:
				db.close()	
			
	#If the packet is not recognisable, just print it. Unrecognised packets should still be logged in case there is any garbled data
	else:
		print "Received unknown packet: %s" % (packet),
