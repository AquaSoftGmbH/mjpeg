#    Copyright (C) 2000 Mike Bernson <mike@mlb.org>
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
from string import *
from Tkinter import *
from events import *
from DateTime import Date, Time, Timestamp, ISO, \
	DateTimeType, DateTimeDeltaType
import time
import sys
import ConfigParser
import Pmw
import re
import guidedbm
from table import *

dbm = guidedbm.guide_dbm()

class Event:
	def __init__(self, id, row):
		self.id = id
		self.channel = StringVar()
		self.title = StringVar()
		self.start_hour = IntVar()
		self.start_min = IntVar()
		self.start_ampm = StringVar()
		self.stop_hour = IntVar()
		self.stop_min = IntVar()
		self.stop_ampm = StringVar()
		self.day= IntVar()
		self.month = IntVar()
		self.year = IntVar()
		self.repeat = StringVar()
		self.group_data = None
		self.group_frame = None
		self.event_data = {}

		dbm.get_event_data(id, self.event_data)

		self.set_start(self.event_data["start_time"])
		self.set_stop(self.event_data["stop_time"])
		self.set_title(self.event_data["title"])
		self.set_channel(self.event_data["station_name"])
		self.set_repeat(self.event_data["repeat"])
#		self.group(self.scroll_area.interior(), row)

	def __del__(self):
		self.group_data.destroy()

	def show(self):
		self.group_data.focus_force()

	def get_start(self):
		hour = self.start_hour.get()
		if hour == 12:
			hour = 0
		if lower(self.start_ampm.get()) == 'pm':
			hour = hour + 12

		current = (self.year.get(), 
			self.month.get(), self.day.get(),
			hour, self.start_min.get(), 0, 0, 0, -1)

		return time.mktime(current)

	def set_start(self, time_value):
		if time_value == None:
			time_value = time() - time.timezone
			time_value = time_value - (time_value % (30 * 60))
			time_value = time_value + 30 * 60

		self.start_hour.set(time.strftime("%l",time.localtime(time_value)))
		self.start_min.set(time.strftime("%M",time.localtime(time_value)))
		self.start_ampm.set(time.strftime("%p",time.localtime(time_value)))

		self.day.set(time.strftime("%d",time.localtime(time_value)))
		self.month.set(time.strftime("%m",time.localtime(time_value)))
		self.year.set(time.strftime("%g",time.localtime(time_value)))

	def get_stop(self):
		hour = self.start_hour.get()
		if hour == 12:
			hour = 0
		if lower(self.start_ampm.get()) == 'pm':
			hour = hour + 12
		start_time = hour * 60 + self.start_min.get()

		hour = self.stop_hour.get()
		if hour == 12:
			hour = 0
		if lower(self.stop_ampm.get()) == 'pm':
			hour = hour + 12

		end_time = hour * 60 + self.stop_min.get()

		if end_time > start_time:
			current = (self.year.get(), 
				self.month.get(), self.day.get(),
				hour, self.stop_min.get(), 0, 0, 0, -1)
		else:
			current = (self.year.get(), 
				self.month.get(), self.day.get()+1,
				hour, self.stop_min.get(), 0, 0, 0, -1)

		return time.mktime(current)

	def set_stop(self, time_value):
		if time_value == None:
			self.stop_hour.set("")
			self.stop_min.set("")
			self.stop_ampm.set("")
		else:
			self.stop_hour.set(time.strftime("%l",time.localtime(time_value)))

			self.stop_min.set(time.strftime("%M",time.localtime(time_value)))
			self.stop_ampm.set(time.strftime("%p",time.localtime(time_value)))

	def get_title(self):
		return self.title.get()

	def set_title(self, title):
		if self.title.get() == title:
			return
		if title == None:
			self.title.set("")
		else:
			self.title.set(title)


	def get_channel(self):
		return self.channel.get()
	
	def set_channel(self, channel):
		if self.channel.get() == channel:
			return

		if channel == None:
			self.channel.set("")
		else:
			self.channel.set(channel)

	def get_repeat(self):
		return self.repeat.get()

	def set_repeat(self, repeat):
		if self.repeat.get() == repeat:
			return

		if repeat == "":
			repeat = "Once"
		if repeat == None:
			self.repeat.set("Once")
		else:
			self.repeat.set(repeat)

	def withdraw(self):
		self.group_data.grid_forget()

	def display(self, row):
		self.group_data.grid(row=row, column=0, sticky='NSEW', 
			ipadx=4, ipady=4)

	def once(self):
		self.set_repeat("Once")

	def week(self):
		self.set_repeat("Monday-Friday")

	def weekly(self):
		self.set_repeat("Weekly")

	def daily(self):
		self.set_repeat("Daily")

	def delete(self):
		self.set_repeat("Delete")

	def update(self):
		self.event_data["start_time"] = self.get_start()
		self.event_data["stop_time"] = self.get_stop()
		self.event_data["station_channel"] = self.get_channel()
		self.event_data["repeat"] = self.get_repeat()
		self.event_data["title"] = self.get_title()
		self.event_data["length"] = int((self.event_data["stop_time"]-\
			self.event_data["start_time"])/60)

		if dbm.get_channel_name(self.event_data):
			dbm.get_channel_station(self.event_data)

		self.event_data["channel_id"] = self.event_data["station_id"]
			
		event_data = {}	
		dbm.get_event_data(self.event_data["id"], event_data)

		if self.event_data["channel_id"]!=event_data["channel_id"] or\
		   self.event_data["listing_id"]!=event_data["listing_id"] or\
		   self.event_data["start_time"]!=event_data["start_time"] or\
		   self.event_data["stop_time"]!=event_data["stop_time"] or\
		   self.event_data["length"]!=event_data["length"] or\
		   self.event_data["repeat"]!=event_data["repeat"] or\
		   self.event_data["title"]!=event_data["title"]:
			dbm.set_event_data(self.event_data)

	def group(self, root, row):
		self.group_data = Pmw.Group(root, 
			tag_textvariable=self.repeat,
			tag_pyclass=Menubutton)
		self.group_frame = self.group_data.interior()

		command_button = self.group_data.component('tag')
		command_button.menu = Menu(command_button)
		command_button.menu.add_command(label='Once',
			underline=0, command=self.once)
		command_button.menu.add_command(label='Monday-Friday', 
			underline=0, command=self.week)
		command_button.menu.add_command(label='Weekly', 
			underline=0, command=self.weekly)
		command_button.menu.add_command(label='Daily', 
			underline=0, command=self.daily)
		command_button.menu.add_command(label='Delete', 
			underline=0, command=self.delete)
		command_button['menu'] = command_button.menu

		date_frame = Frame(self.group_frame)

		month = Entry(date_frame,
			textvariable = self.month,
			width = 2)
		
		day = Entry(date_frame, 
			textvariable = self.day,
			width = 2)

		year = Entry(date_frame, 
			textvariable = self.year,
			width = 2)

		label1 = Label(date_frame, text="/", width=1)
		label2 = Label(date_frame, text="/", width=1)

		month.grid(row=0, column=0)
		label1.grid(row=0, column=1)
		day.grid(row=0, column=2)
		label2.grid(row=0, column=3)
		year.grid(row=0, column=4)

		start_time_frame = Frame(self.group_frame)

		hour = Entry(start_time_frame,
			textvariable = self.start_hour,
			width = 2)
		
		min = Entry(start_time_frame, 
			textvariable = self.start_min,
			width = 2)

		ampm = Entry(start_time_frame,
			textvariable = self.start_ampm,
			width = 2)

		hour.grid(row=0, column=0, padx=3)
		min.grid(row=0, column=2, padx=3)
		ampm.grid(row=0, column=3, padx=3, ipadx=3)

		stop_time_frame = Frame(self.group_frame)

		hour = Entry(stop_time_frame,
			textvariable = self.stop_hour,
			width = 2)
		
		min = Entry(stop_time_frame, 
			textvariable = self.stop_min,
			width = 2)

		ampm = Entry(stop_time_frame,
			textvariable = self.stop_ampm,
			width = 2)

		hour.grid(row=0, column=0, padx=3)
		min.grid(row=0, column=2, padx=3)
		ampm.grid(row=0, column=3, padx=3, ipadx=3)

		dash = Label(self.group_frame,
			text = " - ",
			width = 3)
		date_frame.grid(row=0, column=0)
		start_time_frame.grid(row = 0, column=1)
		dash.grid(row=0, column = 2, sticky="NSEW")
		stop_time_frame.grid(row=0, column=3)

		channel = Entry(self.group_frame,
			textvariable=self.channel,
			width = 12)

		title = Entry(self.group_frame,
			textvariable=self.title,
			width=30)

		channel.grid(row=1, column=0, sticky="NSW")
		title.grid(row=1, column = 1, columnspan=3, sticky="NSW")

		self.group_data.grid(row=row, column=0, sticky='NSEW', 
			ipadx=4, ipady=4)

class EventList:
	def __init__(self, root):
		self.root = root
		self.config_data = ConfigParser.ConfigParser()
		self.config_data.read("/etc/dvcr")
		self.last_add = []

		self.record_event = {}
		self.event_groups = {}
		if root != None:
			self.dialog = Pmw.Dialog(root,
				title="Reording Events",
				buttons=("OK", "Add", "Cancel"),
				command=self.execute,
				defaultbutton='OK')
			self.dialog.withdraw()
	
			self.scroll_area = Pmw.ScrolledFrame(self.dialog.interior(),
				hscrollmode = 'dynamic')
			self.scroll_area.pack(fill=BOTH, expand=1, pady=4, padx=4)

	def repeat(self, tag):
		changes = 0
		current_time = time.time() - time.timezone 

		start, end, channel, title, repeat = self.event(tag)

		if repeat == "Delete":
			self.delete(tag)
			return

		if end > current_time:
			return

		if repeat == 'Once':
			self.delete(tag)
			changes = changes + 1

		if repeat == "Daily":
			start = start + 24 * 60 * 60
			end = end + 24 * 60 * 60

		if repeat == "Weekly":
			start = start + 24 * 60 * 60 * 7
			end = end + 24 * 60 * 60 * 7

		if repeat == "Monday-Friday":
			start = start + 24 * 60 * 60
			end = end + 24 * 60 * 60

		dict = self.record_event[tag]
		dict['start'] = start
		dict['stop'] = end

		if self.root != None:
			event = self.event_groups[tag]
			event.set_start(start)
			event.set_stop(end)

	def load_data(self):
		self.event_groups = {}

		ids = dbm.event_ids()

		row = 0
		for id in ids:
			if self.root != None:
				current = Event(id, row)
				self.event_groups[id] = current
				current.group(self.scroll_area.interior(), row)
				row = row + 1

	def add(self, id=None, channel_id=None, start=None, stop=None, \
		repeat='Once', title=None):
		self.last_add = []
		data = {}


		data["station_id"] = channel_id
		dbm.get_channel_id(data)

		data["channel_id"] = data["station_id"]
		data["listing_id"] = id
		data["start_time"] = start
		data["stop_time"] = stop
		data["length"] = length = int((stop - start)/60)
		data["repeat"] = repeat
		data["title"] = title

		ids = dbm.add_event_data(data)
		print "add: ", data

		for last_id in ids:
			self.last_add.append(int(last_id))

	def update(self):
		for id in self.event_groups.keys():
			self.event_groups[id].update()

	def last_delete(self):
		for tag in self.last_add:
			self.delete(tag)

	def delete(self, id):
		if self.record_event.has_key(id):
			del self.record_event[id]
		if self.event_groups.has_key(id):
			del self.event_groups[id]
		stmt = "Delete From events where id=%d" %id
		self.cursor.execute(stmt);

	def event_cmp(self, a1, a2):
		diff = self.record_event[a1]['start'] - \
			self.record_event[a2]['start']
		if diff < 0:
			return -1
		if diff > 0:
			return 1
		return 0
		
	def tags(self):
		tags = self.record_event.keys()
		tags.sort(self.event_cmp)
		return tags

	def recorder(tag):
		if self.record_event.has_key(tag):
			dict = self.record_event[tag]
			if dict['repeat'] == 'Once':
				dict['repeat'] = "Deleted"
				return

			self.repeat(tag)

	def event(self, tag):
		if self.record_event.has_key(tag):
			dict = self.record_event[tag]
			return dict['start'], \
				dict['stop'], \
				dict['channel'], \
				dict['title'] , \
				dict['repeat']
		return None

	def list(self):
		for tag in self.tags():
			print self.event(tag)

	def config_string(self, section, name):
		try:
			return self.config_data.get(section, name)
		except:
			return None

	def execute(self, button):
		if button == 'OK':
			self.dialog.deactivate()
			self.last_add = []
			self.update()
			return

		if button == 'Cancel':
			self.dialog.deactivate()
			self.last_delete()
			self.last_add = []		

		if button == "Add":
			self.add("x1")
			return


	def display_events(self, group_tag):
		self.load_data()

		if group_tag != None:
			if self.event_groups.has_key(group_tag):
				self.event_groups[group_tag].show()
		else:
			self.last_add = []

		for tag in self.tags():
			self.event_groups[tag].withdraw()
			self.repeat(tag)

		row = 0
		for tag in self.tags():
			self.event_groups[tag].display(row)
			row = row + 1

		result = self.dialog.activate()
		
