#!/usr/bin/python
#
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
from httplib import *
from string import *
from time import *
import ConfigParser
from guide import Guide

class TvGuide(Guide):

	def __init__(self):
		Guide.__init__(self)
		self.starting_point = 973728000.0
	#
	# convert tvguide time to unix time
	#
	def tvguide_seconds(self, start_time):
		# 86400 = 1800 * 48
		time_sec = start_time - 36839.2083333333
		temp = self.starting_point + time_sec * 86400 + 290
		temp = temp - (temp % 300)
		return temp
	#
	# convret unix time to tvguide time
	#
	def seconds_tvguide(self, start_time):
		start_time = (start_time - self.starting_point) / 86400
		start_time = start_time + 36839.2083333333333
		return start_time
	#
	# need to parse cl('svcid','titleid')
	#
	def parse_cl(self, line):
		empty = None, None, None
		start = find(line, "cl('")
		if (start < 0):
			return empty

		start = start + 4
		end = find(line[start:], "'") 
		if (end < 0):
			return empty
		end = end + start

		svcid = line[start:end]

		start = find(line[end:], ",'")
		if (start < 0):
			return empty
		start = start + end + 2

		end = find(line[start:], "'")
		if (end < 0):
			return empty
		end = end + start

		titleid = line[start:end]

		return end, svcid, titleid
	#
	#
	#	parseing view=chan&i=svcid&S=??&ST=start_time&N=station>name</a>
	#
	def parse_station(self, line):
		empty = None, None, None

		start = find(line,"view=chan")
		if start < 0:
			return empty

		start_time = find(line[start:], "ST=")
		if start_time < 0:
			return empty
		start_time = start_time + start + 3
		end_time = find(line[start_time:], "&")
		if end_time < 0:
			return empty
		end_time = start_time + end_time
		time_start = line[start_time:end_time]

		chan_start = end_time + 3
		chan_end = find(line[end_time:], ">")
		if chan_end < 0:
			return empty

		chan_end = chan_end + end_time
		channel = line[chan_start:chan_end]

		name_start = chan_end + 1
		name_end = find(line[name_start:], "</a>")
		if name_end < 0:
			return empty

		name_end = name_end + name_start
		name = line[name_start: name_end]
		start = find(name, '&nbsp;')

		if start > 0:
			name = name[start+6:]

		st = self.tvguide_seconds(atof(time_start))

		return st, channel, name
	#
	#
	#
	def parse_show(self, line):
		empty = None, None, None, None

		start = find(line, "colspan=") + 8;
		if start <= 8:
			return empty
		if line[start:start+1] == '"':
			start = start + 1
		end = start + 1
		if find("0123456789", line[end:end+1]) >= 0:
			end = end + 1

		span = atoi(line[start:end])

		start_href = find(line[end:], "<a href=")
		if start_href < 0:
			return empty

		end, svcid, titleid = self.parse_cl(line[start_href:])
		if end == None:
			return empty
		end = end + start_href

		start_show = find(line[end:], ">")
		if start_show < 0:
			return empty

		start_show = start_show + end + 1
		end_show = find(line[start_show:], "</a>")
		if end_show < 0:
			return empty

		end_show = end_show + start_show
		data = line[start_show:end_show]
		runtime = span * 5

		return runtime, svcid, titleid, data


	def parse_tvguide_data(self, guide_data):
		for line in guide_data:
			channel = None
			name = None
			lines = split(line, "</TD>")
			for data in lines:
				new_time, new_channel, new_name = \
					self.parse_station(data)
				run_time, svcid, titleid, show_name = \
					self.parse_show(data)
				if new_time != None:
					curr_time = new_time
					channel = new_channel
					name = new_name
					continue

				if run_time == None:
					continue

				self.append([curr_time, name, channel, 
					run_time, show_name,
					svcid +','+titleid])
				curr_time = curr_time + run_time * 60

	def  guide_url(self,  url):
		host = self.config_string("tvguide", "host")
		http_loop = 20
		while http_loop:
			try:
				http = HTTP(host)
				http.putrequest("GET", url)
				http.putheader("Accept", "text/html")
				http.putheader("Accept", "text/plain");
				http.endheaders()

				errcode, errmsg, headers = http.getreply()
			except:
				print "retry get"
				sleep(60)
				http_loop = http_loop - 1
				continue
			break

		data = http.getfile()	
		guide_data = split(data.read(), "\n")
		self.parse_tvguide_data(guide_data)

	def tvguide_url(self, start, end):
		service_id = self.config_list("tvguide", "serviceid")
		for id in service_id:
			while start < end:
				time_st = "%f" % start
				url = "/listings/index.asp?I=" + id + "&ST=" + time_st
				self.guide_url(url)
				start = start + 1.0/12.0

	def guide_file(file_name, guide):
		file = open("test", "r")
		guide_data = file.readlines()
		self.parse_tvguide_data(guide_data, guide)


	def guide_whole_day(self, offset):
		current = time() - timezone
		current = current - (current % (24 * 60 * 60))
		
		start = self.seconds_tvguide(current - (4 * 60 * 60))
		end = self.seconds_tvguide(current + 28 * 60 * 60)
		self.tvguide_url(start, end)


