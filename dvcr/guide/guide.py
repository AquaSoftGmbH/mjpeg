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
from events import *
import sys
import marshal 
import ConfigParser
import Pmw
from table import *

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

def set_weight(grid, start_row=0, start_column=0):
	columns, rows = grid.grid_size()

	column = start_column
	while column < columns:
		grid.grid_columnconfigure(column, weight=1)
		column = column + 1

	row = start_row
	while row < rows:
		grid.grid_rowconfigure(row, weight=1)
		row = row + 1

class Guide:
	def __init__(self):
		self.list_dict = {}
		self.config_data = ConfigParser.ConfigParser()
		self.config_data.read("/etc/dvcr")
		self.root = None
		self.columns = 3
		self.column_list = (1, 2, 3)
		self.seconds_per_day = 24 * 60 * 60
		self.zone_offset = timezone
		self.station_width = 0
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
		list_keys = self.list_dict.keys()
		list_keys.sort()

		for key in list_keys:
			data = self.list_dict[key]
			print "%s %s %6.6s %-10s %3d mins. %s" % \
				(key, strftime("%m/%d/%y %H:%M", gmtime(data[0])), 
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
		print "key=",key
		listing = self.list_dict[key]
		print "listing=", listing
		stop = listing[0] + listing[3] * 60;
		self.events.add(key,  listing[2], listing[0], stop,
			"aa", listing[4])
		self.events.display_events(key)

	def info_in(self, key, aa):
		listing = self.list_dict[key]
		start = strftime("%l:%M %p",gmtime(listing[0]))
		end = strftime("%l:%M %p",gmtime(listing[0] + listing[3] * 60))

		output = "%s   %s-%s %d mins" % (listing[4], start, end, listing[3])
		self.info_time.configure(text=output)

	def info_out(self, key, aa):
		self.info_time.configure(text=" ")


	def event_add(self, data):
		print event_add

	def time_button(self, time_index, col):
		label = Button(self.listing_frame, 
			text=self.time_label[time_index], width=15)
		self.listing_widget.add(label, 
			row=0, 
			column=col)
		return label

	def station_button(self,station_id, row):
		end = find(station_id, ":")
		if end > -1:
			station_id = station_id[:end]

		label = Button(self.listing_frame, 
			text=station_id, 
			command=GuideCmd(print_data, station_id))
		self.listing_widget.add(label, 
			row=row, 
			column=0)
		print label.bbox()
		return label

	def listing_button(self,list_time, station_id, row, col, command):
		listing = self.search(station_id, list_time)
		if listing == None:
			return 	1
		listing_next = self.next(station_id, list_time)
		if (listing_next != None):
			list_time = list_time - self.zone_offset
			next_time = listing_next[0] - (listing_next[0] % 1800)
			span = int((next_time - list_time)/1800)
		else:
			span = self.columns

		if span < 1:
			span = 1

		end = find(station_id, ":")
		if end > -1:
			station_id = station_id[end + 1:]
		key_time = listing[0] - (listing[0] % 1800)
		key = strftime("%d%H%M", gmtime(key_time)) + station_id

		label = Button(self.listing_frame,
			command=GuideCmd(self.info, key),
			text=listing[4])
		self.listing_widget.add(label, 
			column=col,
			row=row,
			columnspan=span)
		label.bind("<Enter>", GuideCmd(self.info_in, key)) 
		label.bind("<Leave>", GuideCmd(self.info_out, key)) 

		return span

	def  done(self):
		print "executing done"
		sys.exit(0)

	def edit_events(self):
		self.events.display_events(None)

	def display_guide(self, x, y, width, height, seconds):
		station_list = self.config_list("display", "stations")
		seconds = (seconds/(24 * 60 * 60)) * (24 * 60 * 60)
		year, month, day, hour, min, sec, weekday, julday, saving = localtime(seconds)
		seconds = mktime(year, month, day, 0, 0, 0, weekday, julday, saving)
		date_string = strftime("%m/%d/%Y", localtime(seconds))

		self.root = Tk()
		self.root.title("Program Listings for " + date_string)

		self.events = EventList(self.root)
		self.events.load()

		self.load(seconds)

		menu_bar = Pmw.MenuBar(self.root,
			hull_relief = 'raised',
			hull_borderwidth=3)
		menu_bar.addmenu('File', "Quit program")
		menu_bar.addmenuitem("File", 'command', 
			"Exit Program", 
			label="Exit",
			command=GuideCmd(self.done))
		menu_bar.addmenu('Edit', "Edit event list")
		menu_bar.addmenuitem("Edit", 'command',
			"Edit Recording Events",
			label="Events",
			command=GuideCmd(self.edit_events))

		self.menu_bar = menu_bar

		self.info_frame = Frame(self.root, background="green")
		self.listing_widget = Table(self.root,
			fixedcol=1,
			fixedrow=1,
			displayrow=5,
			displaycol=4)
		self.listing_frame = self.listing_widget.component("table")

		year, month, day, hour, min, sec, weekday, julday, saving = localtime(seconds)
		time_index = hour * 2 + min / 30

		self.buttons = []

		station_size = 0
		for station_id in station_list:
			end = find(station_id, ":")
			if end < 0:
				end = len(station_id)
			if end > station_size:
				station_size = end
					


		self.info_time = Label(self.info_frame, text="")
		self.info_time.grid(row=0, column=0, sticky="NSEW")
		set_weight(self.info_frame)

		label = Button(self.listing_frame, 
			text=" ", width=station_size + 2)
		self.listing_widget.add(label, 
			row=0, 
			column=0)
		label.grid(row=0, column=0, sticky='NSEW' )

		col = 1
		while col <= 48:
			self.time_button(col-1, col)
			col = col + 1

		current_row = 1
		for station in station_list:
			self.station_button(station, current_row)
			col = 1
			while col <= 48:
				span = self.listing_button(seconds + (col-1)
					* 1800, station, current_row,col, "")
				if span < 1:
					span = 1
				col = col + span
			current_row = current_row + 1


		set_weight(self.listing_frame, start_column=1)

		self.menu_bar.pack(expand=YES, fill=BOTH, side='top')

		self.info_frame.pack(expand=YES, fill=BOTH, side='top')
		self.listing_widget.pack(expand=YES, fill=BOTH, side='top')

		self.info_frame.grid_propagate(flag=1)
		self.listing_widget.grid_propagate(flag=1)

#		self.base_frame.bind("<Configure>", 
#			lambda event=None, self=self: self.callback1())
#		self.base_frame.bind("<Map>", 
#			lambda event=None, self=self: self.callback1())

		self.root.mainloop()


		

