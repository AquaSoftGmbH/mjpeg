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
from string import *
import ConfigParser
import time
import marshal 
import events
import os

class Recorder:
	def __init__(self):
		self.config_data = ConfigParser.ConfigParser()
		self.config_data.read("/etc/dvcr")
		self.events = events.EventList(None)
		self.record_dirs = self.config_list("global", "videodir")
		self.recorder_devices = self.config_list("global", "recorders")
		self.running = {}
		self.device = None
		self.events.load()

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

	def  find_device(self, start, end, channel):
		for device in self.recorder_devices:
			if self.running.has_key(device):
				continue

			return device
		return None

	def process_args(self, start, end, channel, title, args):
		list = []
		for item in args:
			if item[0:1] !='%':
				list.append(item)
				continue

			if item == "%file-name":
				file_name = "/video/" + \
					time.strftime("%y%m%d.%H%M.", \
					time.localtime(start)) + \
					channel + "." + title + "-%d.avi"
				list.append(file_name)
				continue

			if item == "%size":
				amount = "%d" % (end - time.time() - 2)
				list.append(amount)
				continue

			print "Unknown item ", item

		return list

	def check_done(self):
		for device in self.running.keys():
			pid, tag = self.running[device]

			new_pid, status = os.waitpid(pid, os.WNOHANG)
			if new_pid != pid:
				continue

			if os.WIFEXITED(status):
				if os.WEXITSTATUS(status):
					print "process tag ", tag, \
						" exited ", \
						os.WEXITSTATUS(status)
				self.events.repeat(tag)
				del self.running[device]
				

	def scan(self):
		self.check_done()

		current_time = time.time()

		for tag in self.events.tags():
			self.events.repeat(tag)
			start, end, channel, title, repeat = \
				self.events.event(tag)

			start = start + time.timezone
			end = end + time.timezone

			if end <= current_time:
				print "Title: %s start: %s end: %s deleted" % \
					(title, time.strftime("%H:%M %m/%d" ,time.localtime(start)),
					time.strftime("%H:%M", time.localtime(end)))
				self.events.delete(tag)
				continue		

			if start <= current_time:

				recording_device = self.find_device(start, 
					end, channel)

				if recording_device == None:
					continue

				recording_program = self.config_string(\
					recording_device, "program")
				
				recording_args = [recording_program]
				args = self.config_list(recording_device, 
					"params")

				for item in args:
					recording_args.append(item)

				args = self.process_args(start, end, 
					channel, title,recording_args)

				print "running ", args
				pid = os.spawnv(os.P_NOWAIT, 
					recording_program, args) 
				
				self.running[recording_device] = pid, tag
