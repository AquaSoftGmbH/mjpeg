#    Copyright (C) 2000 Mike Bernson <mike@mlb.org>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
from string import *
from time import *
from Tkinter import *
import marshal 
import ConfigParser

class GuideCmd:
	def __init__(self, func, *args, **kw):
		self.func = func
		self.args = args
		self.kw = kw

	def __call__(self, *args, **kw):
		args = self.args + args
		kw.update(self.kw)
		apply(self.func, args, kw)

def print_data(data):
	print data

class Guide:

	def __init__(self):
		self.listing = []
		self.list_dict = {}
		self.config_data = ConfigParser.ConfigParser()
		self.config_data.read("/etc/dvcr")
		self.root = None
		self.seconds_per_day = 24 * 60 * 60
		self.zone_offset = timezone
		self.time_label = ( 
			"12:00 AM", "12:30 AM", " 1:00 AM", " 1:30 AM", 
			" 2:00 AM", " 2:30 AM", " 3:00 AM", " 3:30 AM", 
			" 4:00 AM", " 4:30 AM", " 5:00 AM", " 5:30 AM",
			" 6:00 AM", " 6:30 AM", " 7:00 AM", " 7:30 AM", 
			" 8:00 AM", " 8:30 AM", " 9:00 AM", " 9:30 AM", 
			"10:00 AM", "10:30 AM", "11:00 AM", "11:30 AM",
			"12:00 PM", "12:30 PM", " 1:00 PM", " 1:30 PM", 
			" 2:00 PM", " 2:30 PM", " 3:00 PM", " 3:30 PM", 
			" 4:00 PM", " 4:30 PM", " 5:00 PM", " 5:30 PM",
			" 6:00 PM", " 6:30 PM", " 7:00 PM", " 7:30 PM", 
			" 8:00 PM", " 8:30 PM", " 9:00 PM", " 9:30 PM", 
			"10:00 PM", "10:30 PM", "11:00 PM", "11:30 PM",
			"12:00 AM", "12:30 AM", " 1:00 AM", " 1:30 AM", 
			" 2:00 AM", " 2:30 AM", " 3:00 AM", " 3:30 AM", 
			" 4:00 AM", " 4:30 AM", " 5:00 AM", " 5:30 AM",
			" 6:00 AM", " 6:30 AM", " 7:00 AM", " 7:30 AM", 
			" 8:00 AM", " 8:30 AM", " 9:00 AM", " 9:30 AM", 
			)



	def config_string(self, section, name):
		try:
			return self.config_data.get(section, name)
		except:
			return None

	def config_list(self, section, name):
		try:
			data = self.config_data.get(section, name)
		except:
			return None

		return map(strip, split(data, ","))
		

	def output(self):
		self.listing.sort()

		for data in self.listing:
			print "%s %6.6s %-10s %3d mins. %s" % \
				(strftime("%m/%d/%y %H:%M", gmtime(data[0])), 
				data[2], data[1], data[3], data[4])

	def search(self, station_id, list_time):
		end = find(station_id, ":")
		if end > -1:
			station_id = station_id[end + 1:]

		list_time = list_time - self.zone_offset
		key_time = list_time - (list_time % 1800)
		zero_time = list_time - 3600 * 5

		while key_time > zero_time:
			key = strftime("%d%H%M", gmtime(key_time)) + station_id
			if self.list_dict.has_key(key):
				return self.list_dict[key]
			key_time = key_time - 1800
		return None

	def next(self, station_id, list_time):
		end = find(station_id, ":")
		if end > -1:
			station_id = station_id[end + 1:]

		list_time = list_time - self.zone_offset
		key_time = list_time - (list_time % 1800)
		key_time = key_time + 1800
		zero_time = list_time + 3600 * 5

		while key_time < zero_time:
			key = strftime("%d%H%M", gmtime(key_time)) + station_id
			if self.list_dict.has_key(key):
				return self.list_dict[key]
			key_time = key_time + 1800
		return None
		
	def append(self, new_entry):
		listing = self.search(new_entry[2], 
			new_entry[0] + self.zone_offset)
		if listing != None:
			if listing[0] + listing[3] * 60 == new_entry[0]:
				if listing[2] == new_entry[2]:
					if listing[5] == new_entry[5]:
						new_entry[0] = listing[0]
						new_entry[3] = new_entry[3] + \
							listing[3]

		key_time = new_entry[0] - (new_entry[0] % 1800)
		key = strftime("%d%H%M", gmtime(key_time)) + new_entry[2]
		if self.list_dict.has_key(key):
			del self.list_dict[key]
		self.list_dict[key] = new_entry
		
	def save(self, seconds):
		dir = self.config_string("global", "guidedir") + "/"
		file_name = dir+strftime("%y.%m.%d", localtime(seconds))
		file = open(file_name, "w")
		marshal.dump(self.list_dict, file)

	def load(self, seconds):
		dir = self.config_string("global", "guidedir") + "/"
		file_name = dir + strftime("%y.%m.%d", localtime(seconds))
		file = open(file_name, "r")
		self.list_dict = marshal.load(file)

	def info(self, key):
		print "key=%s" % key
		listing = self.list_dict[key]
		print listing

	def time_button(self, time_index, col):
		label = Button(self.root, 
			text=self.time_label[time_index], 
			width=20)
		label.grid(row=0, 
			column=col,
			sticky="nsew")
		return label

	def station_button(self,station_id, row):
		end = find(station_id, ":")
		if end > -1:
			station_id = station_id[:end]

		label = Button(self.root, 
			text=station_id, 
			command=GuideCmd(print_data, station_id))
		label.grid(row=row,
			column=0,
			sticky="nsew")
		return label

	def listing_button(self,list_time, station_id, row, col, command):
		listing = self.search(station_id, list_time)
		print "search=" , listing
		if listing == None:
			return 	-1
		listing_next = self.next(station_id, list_time)
		print "next=", listing_next
		if (listing_next != None):
			list_time = list_time - self.zone_offset
			list_time = list_time % self.seconds_per_day
			current_index = int(list_time / 1800)
			time_today = listing_next[0] % self.seconds_per_day
			end_index = int(time_today / 1800)
			span = end_index - current_index
		else:
			span = 5

		if span < 1:
			span = 1

		end = find(station_id, ":")
		if end > -1:
			station_id = station_id[end + 1:]
		key_time = listing[0] - (listing[0] % 1800)
		key = strftime("%d%H%M", gmtime(key_time)) + station_id


		label = Button(self.root,
			command=GuideCmd(self.info, key),
			text=listing[4])
		label.grid(row=row,
			column=col,
			columnspan=span,
			sticky="nsew")
		return span

	def display_guide(self, x, y, width, height, seconds):
		station_list = self.config_list("display", "stations")
		date_string = strftime("%m/%d/%Y %I:%M %p", localtime(seconds))
		self.root = Tk()
		self.root.title("Program Listings for " + date_string)
		self.load(seconds)

		year, month, day, hour, min, sec, weekday, julday, saving = localtime(seconds)
		time_index = hour * 2 + min / 30

		self.time_button(time_index, 1)
		self.time_button(time_index+1, 2)
		self.time_button(time_index+2, 3)
		self.time_button(time_index+3, 4)

		current_row = 1
		for station in station_list:
			self.station_button(station, current_row)
			col = 1
			while col <= 4:
				span = self.listing_button(seconds + (col-1)
					* 1800, station, current_row,col, "")
				col = col + span

			current_row = current_row + 1

		self.root.mainloop()
		
		
