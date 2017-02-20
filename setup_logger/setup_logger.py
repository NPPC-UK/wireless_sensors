#!/usr/bin/env python

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

 # setup_logger.py 
 # Author: Katie Awty-Carroll (klh5@aber.ac.uk)
 # 

import serial
import MySQLdb
import sys
from datetime import datetime

"""
setup_node.py - 		Python 2.7.6 script for setting up nodes. 

						This script is designed to fetch OneWire sensor ID's from a node, and add them to the database. The node must be running setup_node.ino.

						This script has very little error checking at the moment so be careful.

"""

print "Setting up..."

#The directory where the config file is stored
parser.read("config")

#Fetch database details from config file
db_host = parser.get("db_connect", "host")
db_user = parser.get("db_connect", "user")
db_pass = parser.get("db_connect", "password")
db_schema = parser.get("db_connect", "schema")

#Clear input buffer, because menu will print
def clear_menu(serial_conn):
	
	for i in range(0, 6):
		serial_conn.readline()

#Set up the SQL statement to add a sensor location
def get_location_data(node_id, sensor_id, sensor_order):
	
	loc_statement = None

	print "Sensor X location: "
	x_loc = raw_input()

	if x_loc.isdigit():
		print "X location is OK"

		print "Sensor Y location: "
		y_loc = raw_input()

		if y_loc.isdigit():
			print "Y location is OK"

			print "Sensor Z location: "
			z_loc = raw_input()

			if z_loc.isdigit():
				print "Z location is OK"

				print "Sensor compartment: "
				comp = raw_input()

				if comp.isdigit():
					print "Compartment number is OK"

					print "Sensor network ID: "
					network_id = raw_input()
				
					if network_id.isdigit():
						print "Network ID is OK"
						loc_statement = "INSERT INTO locations (location_sensor_id, x_location, y_location, z_location, compartment, node_id, node_order, network) VALUES ('%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s');" % (sensor_id, x_loc, y_loc, z_loc, comp, node_id, sensor_order, network_id)

					else:
						print "Network ID is not numeric"

				else:
					print "Compartment number is not numeric"

			else:
				print "Z location is not numeric"

		else:
			print "Y location is not numeric"

	else:
		print "X location is not numeric"

	return loc_statement

#Set up the SQL statement to add a sensor calibration record
def get_calibration_data(sensor_id):

	cal_statement = None

	print "Calibration date (YYYY-MM-DD): "
	cal_date = raw_input()

	try:
		val_date = datetime.strptime(cal_date, '%Y-%m-%d')

		print "Equation (e.g. x*2/(12-1.5)): "
		cal_equation = raw_input()

		try:
			x = 1
			equation_res = eval(cal_equation)

			print "Instrument used to calibrate: "
			cal_instrument = raw_input()

			print "Who performed the calibration: "
			cal_person = raw_input()

			cal_statement = "INSERT INTO calibration_data VALUES (equation_sensor_id, calibration_date, equation, calibration_instrument, who_calibrated) VALUES ('%s', '%s', '%s', '%s', '%s')" % (sensor_id, cal_date, cal_equation, cal_instrument, cal_person)

		except SyntaxError:
			print "Equation cannot be evaluated - check your syntax"

	except ValueError:
		print "Date needs to be in YYYY-MM-DD format"

	return cal_statement

def main():

	calibration_flag = "NO"

	#Connect to serial port so that we can communicate with the Moteino
	try:
		serial_conn = serial.Serial("/dev/ttyUSB0", 115200)
		print "Connected to serial port"
		clear_menu(serial_conn)
	except OSError as e:
		print "ERROR: Could not open serial port: %s" % (e)
		sys.exit(1)	

	try:
		#Connect to database
		db = MySQLdb.connect(db_host, db_user, db_pass, db_schema)

		#Set up cursor to fetch data
		cursor = db.cursor()

		print "Connected to database"

		print "Fetching Node ID..."

		serial_conn.write('6')

		node_id = serial_conn.readline()
		print "Node ID is " + node_id

		#Check that the node ID is within range
		if int(node_id) > 1 and int(node_id) < 255:

			clear_menu(serial_conn)

			print "Fetching OneWire sensors from node..."
	
			#Send instruction to get OneWire sensor data
			serial_conn.write('7')

			#Fetch reply
			num_sensors = serial_conn.read(1)
			print "Number of sensors in EEPROM: " + num_sensors

			#Check that the number of sensors is within range
			if int(num_sensors) > 0 and int(num_sensors) <= 5:
				for i in range(1, int(num_sensors)+1):
					sensor_addr = serial_conn.readline()
				
					print "Received address of device " + str(i)
					sensor_addr = sensor_addr.strip()
					print sensor_addr

					print "Date of purchase for this sensor (YYYY-MM-DD): "
					dop = raw_input()

					print "Has this sensor been calibrated? (y/n) "
					if_calibrated = raw_input()

					if if_calibrated == "y":
						calibration_flag = "YES"
	
					print "Preparing sensor SQL statement..."

					add_sensor = "INSERT INTO sensors (manufacturer, calibrated, sensor_type, measurement_unit, date_purchased, serial_number) VALUES ('Dallas OneWire', '%s', 'temperature', 'degrees celsius', '%s', '%s');" % (calibration_flag, dop, sensor_addr)
				
					cursor.execute(add_sensor)
					
					#Commit the change so that we can then fetch the sensor ID
					db.commit()
				
					#The ID of the sensor we just added will be the highest value in the auto incrementing column
					cursor.execute("SELECT sensor_id FROM sensors ORDER BY sensor_id DESC LIMIT 1;")

					sensor_id = cursor.fetchone()
				
					#Add location data
					print "Add location data? (y/n) "
					if_location = raw_input()

					if if_location == "y":

						#Add location data
						location_record = get_location_data(node_id, sensor_id[0], i)
						if location_record != None:
							print "Adding location data"
							cursor.execute(location_record)
						else:
							print "Invalid location data"

					if calibration_flag == "YES":

						#Calibration flag has been set to YES, so add calibration data
						"Calibration data needs to be added for this sensor"
						calibration_record = get_calibration_data(sensor_id)
						if calibration_record != None:
							print "Adding calibration data"
							cursor.execute(calibration_record)
						else:
							#User entered values are probably incorrect. Check if the user wants to change the calibration flag to NO
							print "Invalid calibration data. Set calibrated field to NO? (y/n) "
							if_reset = raw_input()
							if if_reset == "y":
								update_cal = "UPDATE sensors SET calibrated = 'NO' WHERE sensor_id = '%s'" % (sensor_id)
								cursor.execute(update_cal)
							else:
								print "Warning: Calibrated flag is set to YES, but no calibration data has been added"
								
				
					#Commit calibration and location data
					db.commit()

					print "Changes to database have been committed"
					print "Done"

			else:
				print "Invalid number of sensors"
		
		else:
			print "Node ID is invalid or has not been set"
	
	#Catch any errors associated with accessing the database
	except MySQLdb.Error as e:
		print "***ERROR***: Database error: {} {}" % (e[0], e[1])
		db.rollback()
	finally:
		db.close()

main()

