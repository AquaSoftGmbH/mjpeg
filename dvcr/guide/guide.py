#    COPYRIGHT (C) 2000 Mike Bernson <mike@mlb.org>
#
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

from os import *

from string import *
from Tkinter import *
from events import *
from DateTime import Date, Time, Timestamp, ISO, \
	DateTimeType, DateTimeDeltaType
import MySQLdb
import time
import sys
import ConfigParser
import Pmw
import re
import guidedbm
from table import *

dbm = guidedbm.guide_dbm()

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

def TicksDateTime(DateTime):
	aDateTime = "%s" % DateTime
	date = ISO.ParseDateTime(aDateTime)
	return date.ticks()

class Guide:
	def __init__(self):
		self.list_dict = {}
		self.config_data = ConfigParser.ConfigParser()
		self.config_data.read("/etc/dvcr")
		self.root = None
		self.columns = 5
		self.column_list = (1, 2, 3)
		self.seconds_per_day = 24 * 60 * 60
		self.zone_offset = time.timezone
		self.station_width = 0
		self.times = (
			"12:00 AM", "01:00 AM", "02:00 AM", "03:00 AM", 
			"04:00 AM", "05:00 AM", "06:00 AM", "07:00 AM",
			"08:00 AM", "09:00 AM", "10:00 AM", "11:00 AM",
			"12:00 PM", "01:00 PM", "02:00 PM", "03:00 PM", 
			"04:00 PM", "05:00 PM", "06:00 PM", "07:00 PM",
			"08:00 PM", "09:00 PM", "10:00 PM", "11:00 PM")

		self.guide = MySQLdb.Connect(db='tvguide', host='mike.mlb.org');
		self.cursor = self.guide.cursor();

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
				(key, time.strftime("%m/%d/%y %H:%M", time.gmtime(data[0])), 
				data[2], data[1], data[3], data[4])

	def station_id(self, station_id):
		stmt = """Select id, name, description From stations
			Where station = '%s'""" % station_id
		self.cursor.execute(stmt);
		
		name = station_id
		desc = name
		i = int(name)
		if self.cursor.rowcount > 0:
			id, name, desc = self.cursor.fetchone();
			i = int(id)

		return [i, name, desc]

	def search(self, station_id, list_time):
		stmt = """Select id, station, start_time, end_time,
			length, title from listings 
			Where start_time<FROM_UNIXTIME(%d) and 
				end_time>FROM_UNIXTIME(%d) and 
				station=%s Order by start_time""" % \
				(list_time + (30 * 60), list_time, station_id)
		self.cursor.execute(stmt);
		if self.cursor.rowcount < 1:
			return None

		id, channel, start, end, length, title = self.cursor.fetchone()
		s_time = TicksDateTime(start)
		e_time = TicksDateTime(end)
		i = int(id)
		l = int(length)

		return [i, channel, s_time, e_time, l, title, ""]

#
#	add new record to the data base for listing. Check to see that 
#	record is not already loaded.
#		
	def append(self, new_entry):
		start_time = new_entry[0] + self.zone_offset;
		end_time = start_time + (new_entry[2] * 60)
		length = new_entry[2];
		station_id = new_entry[1]
		title = re.escape(new_entry[3]);
		#
		# check to see if entry already exists 
		#
		stmt = """Select id, start_time, end_time, length, station, title
				From listings Where 
				(start_time=FROM_UNIXTIME(%d) or
				  end_time=FROM_UNIXTIME(%d)) and
				station='%s'""" % \
				(start_time, end_time, station_id)
		self.cursor.execute(stmt);
		if self.cursor.rowcount > 0:
			return;
		#
		# check to see if we are added to end of existing entry
		#
		stmt = """Select id, start_time, end_time, length, station, title
				From listings Where 
				end_time=FROM_UNIXTIME(%d) and
				station='%s' and title='%s'""" % \
				(start_time, station_id, title)
		self.cursor.execute(stmt);
		if self.cursor.rowcount > 0:
			id, s_time, etime, len, ch, t = self.cursor.fetchone()
			length = length + len;
			stmt = """Update listings Set length=%d,
					end_time=FROM_UNIXTIME(%d)
					Where id=%d""" % \
					(length, end_time, id)
			self.cursor.execute(stmt)
			return
		#
		# New data record add to data base.
		#
		stmt = """Insert  into listings (start_time, end_time, length, 
			station, title) 
			Values (FROM_UNIXTIME(%d),
				FROM_UNIXTIME(%d),
				%d, '%s', '%s')""" % (start_time, end_time,
				length, station_id, title)

		self.cursor.execute(stmt)
		
	def info(self, key):
		data = {}
		data["id"] = int(key)
		if dbm.get_listing_data(data):
			return

		station_id, station_name, station_des = \
			self.station_id(data["station"])

		self.events.add(data["id"], station_id, data["start_time"],
			data["end_time"], "Once", data["title"])
		self.events.display_events(key)

	def new_time(self, key):
		new_time = self.day_index.get() + " " + self.time_index.get()
		current = time.strptime(new_time, "%a %m/%d/%Y %I:%M %p")
		starting = time.mktime(current)
		time_data = time.localtime(starting)

		if current[8] != new_time[8]:
			if current[8]:
				starting = starting + 3600
			else:
				starting = starting - 3600

		self.display_guide(0, 0, 640, 480, starting)


	def info_in(self, key, aa):
		data = {}
		data["id"] = int(key)
		if dbm.get_listing_data(data):
			return

		station_id, station_name, station_desc = \
			self.station_id(data["station"])

		start = time.strftime("%l:%M %p", time.localtime(data["start_time"]))
		end = time.strftime("%l:%M %p",time.localtime(data["end_time"]))

		output = "%s %s  %s - %s   %d mins" % \
			(station_name, data["title"], start, end, data["length"])
		self.info_time.configure(text=output)


	def info_out(self, key, aa):
		self.info_time.configure(text=" ")


	def event_add(self, data):
		print event_add

	def time_button(self, sec, col):
		sec = int(sec/(60 * 30)) * 60 * 30
		time_text = time.strftime("%I:%M %p", time.localtime(sec))
		label = Button(self.listing_frame, text=time_text, width=15)
		label.grid(row=0, column=col, stick='NSEW')
		return label
	#
	# lookup station id in database and get name 
	# of station. Then make a button at row for 
	# that station.
	#
	def station_button(self,station_id, row):
		station_id, name, desc = self.station_id(station_id)

		label = Button(self.listing_frame, 
			text=name, 
			command=GuideCmd(print_data, name))
		label.grid(row=row, column=0, stick='NSEW')
		return label

	def  done(self):
		print "executing done"
		sys.exit(0)

	def edit_events(self):
		self.events.display_events(None)

	def create_display(self, x, y, width, height, seconds):
		station_list = self.config_list("display", "stations")
		starting = int((seconds/(24 * 60 * 60))) * (24 * 60 * 60) 

		date_string = time.strftime("%m/%d/%Y", 
			time.localtime(seconds))

		self.root = Tk()
		self.root.title("Program Listings for " + date_string)

		self.events = EventList(self.root)

		self.menu_bar = Pmw.MenuBar(self.root,
			hull_relief = 'raised',
			hull_borderwidth=3)
		self.menu_bar.addmenu('File', "Quit program")
		self.menu_bar.addmenuitem("File", 'command', 
			"Exit Program", 
			label="Exit",
			command=GuideCmd(self.done))
		self.menu_bar.addmenu('Edit', "Edit event list")
		self.menu_bar.addmenuitem("Edit", 'command',
			"Edit Recording Events",
			label="Events",
			command=GuideCmd(self.edit_events))

		self.info_frame = Frame(self.root)
		self.info_time = Label(self.info_frame, text="")
		self.info_time.grid(row=0, column=0, sticky="NSW")
		set_weight(self.info_frame)
		set_weight(self.info_time)

		self.time_frame = Frame(self.root)
		self.day_index = StringVar()
		day1 = time.strftime("%a %m/%d/%Y", time.localtime(seconds - 86499 * 6))
		day2 = time.strftime("%a %m/%d/%Y", time.localtime(seconds - 86499 * 5))
		day3 = time.strftime("%a %m/%d/%Y", time.localtime(seconds - 86499 * 4))
		day4 = time.strftime("%a %m/%d/%Y", time.localtime(seconds - 86499 * 3))
		day5 = time.strftime("%a %m/%d/%Y", time.localtime(seconds - 86499 * 2))
		day6 = time.strftime("%a %m/%d/%Y", time.localtime(seconds - 86499 * 1))
		day7 = time.strftime("%a %m/%d/%Y", time.localtime(seconds - 86499 * 0))
		day8 = time.strftime("%a %m/%d/%Y", time.localtime(seconds + 86499 * 1))
		day9 = time.strftime("%a %m/%d/%Y", time.localtime(seconds + 86499 * 2))
		day10 = time.strftime("%a %m/%d/%Y", time.localtime(seconds + 86499 * 3))
		day11 = time.strftime("%a %m/%d/%Y", time.localtime(seconds + 86499 * 4))
		day12 = time.strftime("%a %m/%d/%Y", time.localtime(seconds + 86499 * 5))
		day13 = time.strftime("%a %m/%d/%Y", time.localtime(seconds + 86499 * 6))
		day14 = time.strftime("%a %m/%d/%Y", time.localtime(seconds + 86499 * 7))

		self.day_index.set(day7);
		self.dayOptions = Pmw.OptionMenu(self.time_frame, labelpos=W,
			menubutton_textvariable=self.day_index,
			items = ( day1, day2, day3, day4, day5, day6, day7,
				  day8, day9, day10, day11, day12, day13, day4),
			menubutton_width=14);

		self.time_index = StringVar()
		self.time_index.set("12:00 AM")
		self.timeOptions = Pmw.OptionMenu(self.time_frame, labelpos=W,
			menubutton_textvariable=self.time_index,
			items = (
			"12:00 AM", "01:00 AM", "02:00 AM", "03:00 AM", 
			"04:00 AM", "05:00 AM", "06:00 AM", "07:00 AM",
			"08:00 AM", "09:00 AM", "10:00 AM", "11:00 AM",
			"12:00 PM", "01:00 PM", "02:00 PM", "03:00 PM", 
			"04:00 PM", "05:00 PM", "06:00 PM", "07:00 PM",
			"08:00 PM", "09:00 PM", "10:00 PM", "11:00 PM"),
			menubutton_width=10)


		self.timego = Button(self.time_frame, text="GO", width=4,
			command=GuideCmd(self.new_time, id));

		self.dayOptions.grid(row=0, column = 0)
		self.timeOptions.grid(row=0, column = 1)
		self.timego.grid(row=0, column=2, sticky='NSEW')

		self.listing_frame = Frame(self.root)

		self.buttons = []


		label = Button(self.listing_frame, 
			text=" ", width=8)
		label.grid(row=0, column=0, sticky='NSEW' )

		set_weight(self.listing_frame, start_column=1)
		self.menu_bar.pack(expand=YES, fill=BOTH, side='top')

		self.info_frame.pack(expand=YES, fill=BOTH, side='top')
		self.time_frame.pack(expand=YES, fill=BOTH, side='top')
		self.listing_frame.pack(expand=YES, fill=BOTH, side='top')

		self.info_frame.grid_propagate(flag=1)
		self.listing_frame.grid_propagate(flag=1)



	def display_guide(self, x, y, width, height, seconds):
		time_data = time.localtime(seconds)
		station_list = self.config_list("display", "stations")

		date_string = time.strftime("%m/%d/%Y", time_data)

		seconds = int(seconds/1800)*1800

		time_index = time_data[3]
		self.time_index.set(self.times[time_index])

		self.root.title("Program Listings for " + date_string)

		self.events = EventList(self.root)

		self.buttons = []

		label = Button(self.listing_frame, 
			text=" ", width=8)
		label.grid(row=0, column=0, sticky='NSEW' )

		col = 1
		sec = seconds

		while col <= self.columns:
			self.time_button(sec, col)
			sec = sec + (30 * 60)
			col = col + 1

		current_row = 1

		for station in station_list:
			sec = int(seconds/1800) * 1800
			data = dbm.search_listing(station, sec, sec + self.columns * 1800 + 1800)
			self.station_button(station, current_row)
			col = 1
			index = 0
			while col <= self.columns:
				id, channel, start_time, end_time, length, title = data[index]
				index += 1
				if end_time <= sec:
					continue

				if index >= len(data):
					span = 100
				else:
					span =(data[index][2] - sec)/ 1800

				if span + col > self.columns + 1:
					span = self.columns - col + 1

				if span < 1:
					span = 1

				span = int(span)
				if len(title) > span * 12:
					title = title[:span * 12]
		
				label = Button(self.listing_frame,
					command=GuideCmd(self.info, id),
					text=title)
				label.grid(column=col, row=current_row, columnspan=span, stick='NSEW')
				label.bind("<Enter>", GuideCmd(self.info_in, id)) 
				label.bind("<Leave>", GuideCmd(self.info_out, id)) 

				col = col + span
				sec = sec + span * (30 * 60)

			current_row = current_row + 1

		set_weight(self.listing_frame, start_column=1)



		

