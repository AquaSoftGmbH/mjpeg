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
import guidedbm
import os

dbm = guidedbm.guide_dbm()

class Recorder:
	def __init__(self):
		self.config_data = ConfigParser.ConfigParser()
		self.config_data.read("/etc/dvcr")
		self.events = events.EventList(None)
		self.record_dirs = self.config_list("global", "videodir")
		self.recorder_devices = self.config_list("global", "recorders")
		self.running = {}
		self.device = None


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

	def  find_device(self, event_data):
		for device in self.recorder_devices:
			if self.running.has_key(device):
				continue

			return device
		return None

	def process_args(self, event_data, args):
		list = []
		print "args processing:", event_data
		for item in args:
			if item[0:1] !='%':
				list.append(item)
				continue

			if item == "%file-name":
				dirs = self.config_list("global", "videodir");
				last_free_space = -1
				last_free_dir = dirs[0]
				for dir in dirs:
					free_space = os.statvfs(dir)
					print "check free space %s ->%d" % (dir,free_space[4])
					if last_free_space <  free_space[4]:
						last_free_space = free_space[4]
						last_free_dir = dir
				dir = last_free_dir
				file_name = dir + "/" + \
					time.strftime("%y%m%d.%H%M.", \
					time.localtime(event_data["start_time"])) + \
					event_data["station_name"] + "." + \
					event_data["title"] + "-%d.avi"
				list.append(file_name)
				continue

			if item == "%size":
				amount = "%d" % (event_data["stop_time"] - \
					time.time() - 2)
				list.append(amount)
				continue

			if item == "%station":
				list.append(event_data["station_station"])
				continue

			print "Unknown item ", item

		return list

	def repeat(self, event_data):
		changes = 0
		current_time = time.time() - time.timezone 
#
# there is a error in that daylight saving time screw thing up
#
#		ch_id, guide_id, channel, start_time, end_time, \
#			length, repeat, desc = dbm.get_event_data(id)

		if event_data["repeat"] == "Delete":
			dbm.delete_event_data(event_data)
			return

		if event_data["repeat"] == 'Once':
			dbm.delete_event_data(event_data)
			return

		if event_data["repeat"] == "Daily":
			start_time = start_time + 24 * 60 * 60
			end_time = end_time + 24 * 60 * 60

		if event_data["repeat"] == "Weekly":
			start_time = start_time + 24 * 60 * 60 * 7
			end_time = end_time + 24 * 60 * 60 * 7

		if event_data["repeat"] == "Monday-Friday":
			start_time = start_time + 24 * 60 * 60
			end_time = end_time + 24 * 60 * 60

		event_data["start_time"] = start_time
		event_data["stop_time"] = stop_time
		dbm.set_event_data(event_data)

	def check_done(self):
		for device in self.running.keys():
			pid, event_data = self.running[device]

			new_pid, status = os.waitpid(pid, os.WNOHANG)
			if new_pid != pid:
				continue
			if os.WIFEXITED(status) or os.WIFSIGNALED(status):
				if os.WIFSIGNALED(status):
					print "process tag ", event_data["id"], \
						"signal ", os.WTERMSIG(status)
				if os.WIFEXITED(status):
					print "process tag ", event_data["id"], \
						" exited ", \
						os.WEXITSTATUS(status)
				self.repeat(event_data)
				del self.running[device]
				
	def scan(self):
		self.check_done()

		current_time = time.time()

		for id in dbm.event_ids():

			running_id = -1;
			for running in self.running.keys(): 
				running_pid, running_id = self.running[running]
				if running_id == id:
					break
			if running_id == id:
				continue

			event_data = {}
			dbm.get_event_data(id, event_data)
			if time.daylight:
				event_data["start_time"] = event_data["start_time"] - 60 * 60
				event_data["stop_time"] = event_data["stop_time"] - 60 * 60

#			print "start: ",event_data["start_time"]," stop: ", event_data["stop_time"]," current: ",current_time

			if event_data["stop_time"] <= current_time:
				print "delete it"
				dbm.delete_event_data(event_data)
				continue		

			if event_data["start_time"] - 30 <= current_time:
				recording_device = self.find_device(event_data)

				if recording_device == None:
					continue

				if self.running.has_key(recording_device):
					continue

				channel_program = self.config_string(\
					recording_device, "channel")

				if channel_program != None:
					channel_args = [channel_program]
					args = self.config_list(recording_device, 
						"channel_params")


					for item in args:
						channel_args.append(item)

					args = self.process_args(event_data, \
						channel_args)

					print "running ", args
					pid = os.fork()
					if pid == 0:
						os.execvp(channel_program, args)
					else:
						os.wait(pid)

				recording_program = self.config_string(\
					recording_device, "recorder")

				recording_args = [recording_program]
				args = self.config_list(recording_device, 
					"record_params")

				for item in args:
					recording_args.append(item)

				args = self.process_args(event_data, \
					recording_args)

				print "running ", args
				pid = os.fork()
				if pid == 0:
					os.execvp(recording_program, args) 
				else:
					self.running[recording_device] = pid, event_data



