#!/usr/bin/python2
#
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
from time import *
import httplib
import sgmllib
import ConfigParser
#from os import *
#from sys import *
import os
import sys
import re
import signal
import string
from guide import Guide

def http_timeout(signum, frame):
	print "http timeout!";
	raise IOError, "Host not responding."

VERBOSE=1

class TvGuideParser(sgmllib.SGMLParser):

 
	def __init__(self, verbose=VERBOSE, checker=None):
		self.myverbose = verbose # now unused
		self.checker = checker
		self.base = None
		self.links = {}
		self.runtime = None
		self.channel = None
		self.title = None
		self.titleid = None
		self.svcid = None
		self.start_time=None
		self.starting_time = None
		self.event_time=None
		self.event_hour = None
		self.event_date = None
		self.starting_point = 973728000.0
		self.starting_point = 973728000.0 + (4 * 60 * 60)
		self.prime_start_time = 0;
		self.guide = Guide();
		sgmllib.SGMLParser.__init__(self)
#		self.zone_offset = time.timezone


	def translate(self, translate_data):
		self.translate_data = translate_data

	def filter(self, filter_data):
		self.filter_data = filter_data

	def set_event_time(self, event_time):
		self.event_time = event_time
		self.starting_time = None

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


	def start_td(self, attributes):
		for name, value in attributes:
			if name == 'colspan':
				self.runtime = atoi(value) * 5

	def start_tr(self, attributes):
		self.start_time = self.event_time
		self.channel = None

	def start_a(self, attributes):
		self.svcid = None
		self.titleid = None
#		self.print_attrib("start_a", attributes)

		for name, value in attributes:
			if (name == 'href'):
				start = find(value, "cl(")
				if start < 0:
					return
				start = start + 3
				end = find(value, ",")
				if end < 0:
					return
				self.svcid = value[start:end]
				start = end + 1
				end = find(value, ')');
				if end < 0:
					self.svcid = None
					reutrn
				self.titleid = value[start:end]

	def start_b(self, attributes):
		if self.starting_time == None:
			self.prime_start_time = 1;

	def end_b(self):
		self.prime_start_time = 0;

	def end_a(self):
		if self.svcid != None and self.titleid != None and self.title != None:
			if self.translate_data.has_key(self.channel):
				self.channel = self.translate_data[self.channel]

			if self.filter_data.has_key(self.channel):
				if VERBOSE:
					print "found titleid=",self.titleid, "title=",self.title, "runtime=",self.runtime, "time=",self.start_time, "channel=",self.channel
				curr_time = self.tvguide_seconds(self.start_time)

				self.guide.append([curr_time, self.channel,
					self.runtime, self.title,
					self.svcid +','+self.titleid])
	

			self.start_time = self.start_time + float(self.runtime)/(24.0 * 60.0)

		self.svcid = None
		self.titleid = None
		self.title = None
		self.runtime = None
		

	def handle_data(self, data):
		if self.starting_time == None:
			if self.prime_start_time == 1:
#				print "start time: ", data, "starting_time=", self.starting_time
				self.prime_start_time = 0

		if self.channel == None:
			self.channel = data
			self.start_time = self.event_time
			return

		if self.svcid != None and self.titleid != None:
			if self.title == None:
				self.title = data
			else:
				self.title = self.title + data

	def print_attrib(self, msg, attributes):
		print
		print
		print msg
		for name, value in attributes:
			print name, "->",value



class TvGuide(Guide):

	def __init__(self):
		Guide.__init__(self)
		self.parser = TvGuideParser()
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



	def  guide_url(self,  host, day, hour, service_id, translate, filter):
		http_loop = 0

		while http_loop < 20:
			signal.alarm(240)
			signal.signal(signal.SIGALRM, http_timeout)

#			try:
			if VERBOSE:
				print "host='%s' seconds=%f service_id=%s" % (host, day + hour, service_id)
			http = httplib.HTTP(host)
			http.set_debuglevel(VERBOSE)
			url = "/listings/index.asp"
			http.putrequest('POST',  url)
			ref = 'http://' + host + url
			http.putheader("Referer", ref)
			http.putheader("Connection", 'Keep-Alive')
			http.putheader("User-Agent", 'Mozilla/4.76 [en] (X11; U; Linux 2.4.2-XFS i686)')
			http.putheader("Host", host)
			http.putheader("Accept",'image/gif, image/x-xbitmap, image/jpeg, image/pjpeg, image/png, */*')
			http.putheader("Accept-Encoding", "gzip")
			http.putheader("Accept-Language", "en")
			http.putheader("Accept-Charset", 'iso-8859-1,*,utf-8')
			cookie = "sid=17; Country=USA; PPVProvider=%2E; TypeOfService=National; TimeRule=300%2C%2D60%3B0%2C10%2C0%2C5%2C2%2C0%2C0%2C0%3B0%2C4%2C0%2C1%2C2%2C0%2C0%2C0%3B; SITESERVER=ID=8b0c35c8658ce98bf309d037ab4064a6; nat=0; ServiceID="+ service_id + "; cb=TV00; AdHistory=3760%2C3338%2C3338%2C3338; FilterGenre=0; FilterChannel=0; tmpptfc=tmpyfki; ptfc=yfki; sid=17"
			http.putheader("Cookie", cookie)

			data = "serv_id=%s&zip=&gridtype=0&S=&N=&event_date=%d&event_hour=%f&frm_chanfltr=0&frm_cat_Fltr=0&x=20&y=6" % \
				(service_id, day, hour)
			size = "%d" % len(data)
			http.putheader("Content-type", "application/x-www-form-urlencoded");
			http.putheader("Content-length", size)
			http.endheaders()

			http.send(data);

			self.parser.set_event_time(day + hour)
			errcode, errmsg, headers = http.getreply()
			if errcode != 200:
				print
				print "http://%s%s" % (host, url)
				print "Error Code %d: %s" % (errcode, errmsg)
				return
			data = http.getfile()
			while(1):
				guide_data = data.readline()
#				print guide_data

				self.parser.feed(guide_data)
				if find(guide_data, "</html>") > -1:
					signal.alarm(0);
					return

#			except Exception, error:
#				http_loop = http_loop + 1
#				print error
#				print				
#				print "retry get %d" % http_loop
				signal.alarm(0);
#				sys.stdout.flush()
#				sleep(10)
#				continue

			break


	def tvguide_url(self, start, end):
		service_id = self.config_list("tvguide", "serviceid")
		for id in service_id:
			translate_data = self.config_list(id, "translate")
			translate = {}
			if translate_data != None:
				for data in translate_data:
					(src, dst) = re.split('->', data)
					translate[src] = dst
			filter_data = self.config_list(id, "filter")
			filter = {}
			if (filter_data == None):
				filter_data = range(1, 1000)
			for data in filter_data:
				filter[data] = 1

			self.parser.filter(filter)
			self.parser.translate(translate)

			host = self.config_string(id, "host")
			if host == None:
				print "No host found for ", id
				return

			s = start
			while s < end:
				self.guide_url(host, int(s),  \
					s - int(s), id, \
					translate, filter)
				s = s + 2.0/24.0

	def guide_whole_day(self, days):
		current = time() - timezone
		current = int(current/(24*60*60)) * (24*60*60) 
		
		start = int(self.seconds_tvguide(current))
		end = int(self.seconds_tvguide(current + days * 24 * 60 * 60))
		self.tvguide_url(start, end)


