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
from guide import Guide
from time import *
import marshal 
import Pmw
import ConfigParser


class EventList:
	def __init__(self, device):
		self.config_data = ConfigParser.ConfigParser()
		self.config_data.read("/etc/dvcr")

		self.record_event = {}

		dir = self.config_string("global", "recorddir") + "/"
		self.file_name = dir + device

	def load(self):
		try:
			file = open(self.file_name, "r")
		except:
			self.record_event = {}
			return

		self.record_event = marshal.load(file)

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
#
#	channel,start,length,show title
#
class	Events:
	def __init__(self):
		self.config_data = ConfigParser.ConfigParser()
		self.config_data.read("/etc/dvcr")
		self.device = "directtv"

		self.record_list = EventList(self.device)
		self.record_list.load()

	def record_frame(self):
		self.root = Tk()
		self.root.title("Recording Events")

	def build_time(self, time_value, root, row, column):
		time_frame = Frame(root)
		time_frame.grid(row=row, column=column)

		time_hour = strftime("%l",gmtime(time_value))
		time_min = strftime("%M",gmtime(time_value))
		time_ampm = strftime("%p",gmtime(time_value))
		
		hour = Pmw.EntryField(time_frame, 
			value=time_hour,
			)
		entry = hour.component("entry")
		entry.configure(width=2)

		min = Pmw.EntryField(time_frame, 
			value=time_min
			)
		entry = min.component("entry")
		entry.configure(width=2)

		ampm = Pmw.EntryField(time_frame, 
			value=time_ampm
			)
		entry = ampm.component("entry")
		entry.configure(width=2)

	
		hour.grid(row=0, column=1, padx=3)
		min.grid(row=0, column=2, padx=3)
		ampm.grid(row=0, colum=3, padx=3, ipadx=3)

		return time_frame

	def record_event(self, root, item, start, stop, channel, title):
		group = Pmw.Group(root, tag_text=item)

		today = strftime("%m/%d/%y",gmtime(start))

		month = Pmw.Counter(group.interior(),
			entryfield_value = today,
			entryfield_validate = { 
				'validator' : 'date', 
				'format' : 'mdy'},
			datatype = { 
				'counter' : 'date',
				'format' : 'mdy' }
			)
		month.component("entry").configure(width=8)

		month.grid(row=0, column=0)
				

		self.build_time(start, group.interior(), row=0, column=1)

		label = Label(group.interior(), text=" - ", width=3)
		label.grid(row=0, column=2, sticky="NSEW")

		self.build_time(stop, group.interior(), row=0, column=3)

		channel_w = Pmw.EntryField(group.interior(),
			value=channel,
			)
		entry = channel_w.component("entry")
		entry.configure(width=8)

		title_w = Pmw.EntryField(group.interior(),
			value=title,
			)
		entry = title_w.component("entry")
		entry.configure(width=30)

		channel_w.grid(row=1, column=0, sticky="NSW")
		title_w.grid(row=1, column=1, columnspan=3, sticky="NSW")
		
		group.pack(expand=1, fill=BOTH)
		return group

	def edit_events(self):
		self.record_frame()

		frame = Frame(self.root)
		frame.pack(fill='both', expand=1)

		scroll_area = Pmw.ScrolledFrame(frame,
			hscrollmode = 'dynamic')
		scroll_area.pack(fill='both', expand=1)

		frame_events = scroll_area.interior();

		for tag in self.record_list.tags():
			start, end, channel, title = \
				self.record_list.event(tag)
			print start, end, channel, title
			self.record_event(frame_events, tag, start, 
				end, channel, title)


		self.root.mainloop()
