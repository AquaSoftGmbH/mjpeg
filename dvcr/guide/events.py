#    Copyright (C) 2000-2001 Mike Bernson <mike@mlb.org>
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
from Tkinter import *
from time import *
import marshal 
import Pmw
import ConfigParser

class Event:
	def __init__(self):
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

	def set_start(self, time_value):
		self.start_hour.set(strftime("%l",gmtime(time_value)))
		self.start_min.set(strftime("%M",gmtime(time_value)))
		self.start_ampm.set(strftime("%p",gmtime(time_value)))

		self.day.set(strftime("%d",gmtime(time_value)))
		self.month.set(strftime("%m",gmtime(time_value)))
		self.year.set(strftime("%g",gmtime(time_value)))


	def set_stop(self, time_value):
		self.stop_hour.set(strftime("%l",gmtime(time_value)))
		self.stop_min.set(strftime("%M",gmtime(time_value)))
		self.stop_ampm.set(strftime("%p",gmtime(time_value)))

	def set_title(self, title):
		self.title.set(title)

	def set_channel(self, channel):
		self.channel.set(channel)

	def group(self, root):
		self.group_data = Pmw.Group(root, tag_text=" ")
		self.group_frame = self.group_data.interior()

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
			width = 8)

		title = Entry(self.group_frame,
			textvariable=self.title,
			width=30)

		channel.grid(row=1, column=0, sticky="NSW")
		title.grid(row=1, column = 1, columnspan=3, sticky="NSW")

		self.group_data.pack(expand=1, fill=BOTH, ipadx=4, ipady=4)

class EventList:
	def __init__(self, root):
		self.config_data = ConfigParser.ConfigParser()
		self.config_data.read("/etc/dvcr")

		self.record_event = {}
		self.event_groups = {}
		self.dialog = Pmw.Dialog(root,
			title="Reording Events",
			buttons=("OK", "Add", "Cancel"),
			defaultbutton='OK')

		self.scroll_area = Pmw.ScrolledFrame(self.dialog.interior(),
			hscrollmode = 'dynamic')
		self.scroll_area.pack(fill=BOTH, expand=1, pady=4, padx=4)

		dir = self.config_string("global", "recorddir") + "/"
		self.file_name = dir + "directtv"

	def load(self):
		try:
			file = open(self.file_name, "r")
		except:
			self.record_event = {}
			return

		self.record_event = marshal.load(file)

		for tag in self.tags():
			start, end, channel, title = \
				self.event(tag)
			current = Event()
			self.event_groups[tag] = current
			current.set_start(start)
			current.set_stop(end)
			current.set_title(title)
			current.set_channel(channel)
			current.group(self.scroll_area.interior())
		self.dialog.deactivate()


	def save(self):
		try:
			file = open(self.file_name, "w")
		except:
			print "could not create evnt file ", self.file_name
			return

		marshal.dump(self.record_event, file)

	def add(self, id, channel, start, stop, repeat, title):
		self.record_event[id] = { 
			"channel" : channel, 
			"start": start, 
			"stop" : stop,
			"repeat" : repeat,
			"title" : title
			}

	def tags(self):
		tags = self.record_event.keys()
		tags.sort(cmp)
		return tags

	def event(self, tag):
		if self.record_event.has_key(tag):
			dict = self.record_event[tag]
			return dict['start'], \
				dict['stop'], \
				dict['channel'], \
				dict['title']
				

		return None

	def list(self):
		for tag in self.tags():
			print self.event(tag)

	def config_string(self, section, name):
		try:
			return self.config_data.get(section, name)
		except:
			return None

	def display_events(self):
		self.dialog.activate()
