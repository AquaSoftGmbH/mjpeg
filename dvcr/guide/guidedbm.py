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
import MySQLdb
import time
import sys
import ConfigParser
import Pmw
import re
#from table import *

def TicksDateTime(DateTime):
	aDateTime = "%s" % DateTime
	date = ISO.ParseDateTime(aDateTime)
	return date.ticks()

def DateTime(seconds):
	return time.strftime("%m/%d/%y %H:%M", time.localtime(seconds))

class guide_dbm:

	def __init__(self):
		self.guide = MySQLdb.Connect(db='tvguide',host='mike.mlb.org');
		self.cursor = self.guide.cursor();

	def search_listing(self, channel, start_time, end_time):
		if channel == None:
			stmt = """select id, station, start_time, end_time, 
				length, title from listings
				where (start_time < FROM_UNIXTIME(%d) and 
						end_time > FROM_UNIXTIME(%d)) or
					(start_time > FROM_UNIXTIME(%d) and 
						start_time < FROM_UNIXTIME(%d))
					Order by start_time""" % \
					(start_time+1, start_time-1, start_time-1, end_time+1801)
		else:
			stmt = """select id, station, start_time, end_time, 
				length, title from listings
				where station=%s and 
					((start_time < FROM_UNIXTIME(%d) and 
						end_time > FROM_UNIXTIME(%d)) or
					(start_time > FROM_UNIXTIME(%d) and 
						start_time < FROM_UNIXTIME(%d)))
					Order by station, start_time""" % \
					(channel, start_time+1, start_time-1, start_time-1, end_time+1801)

		self.cursor.execute(stmt);
		data = [];
		left = self.cursor.rowcount
		while(left> 0):
			left = left - 1
			id, channel, start, end, length, title = self.cursor.fetchone()
			data.append((id, channel, TicksDateTime(start), \
				TicksDateTime(end), int(length), title))
		return data

	def get_listing_data(self, data):
		stmt = """Select id, station, start_time, end_time,
			length, title from listings 
			Where id = %d""" % data['id']
		self.cursor.execute(stmt);
		if self.cursor.rowcount < 1:
			return None

		id, channel, start, end, length, title = self.cursor.fetchone()

		data["station"] = channel
		data["start_time"] = s_time = TicksDateTime(start)
		data["end_time"] = TicksDateTime(end)
		data["length"] = int(length)
		data["title"] = title


	def get_channel_id(self, data):
		stmt = """Select station, name, description From stations
			Where id=%d""" % data["station_id"]
		self.cursor.execute(stmt);

		if self.cursor.rowcount > 0:
			station, name, desc = self.cursor.fetchone();
			data["station_name"] = name
			data["station_station"] = station
			data["station_desc"] = desc
			return 0

		data["station_name"] = id
		data["station_station"] = id
		data["station_desc"] = id
		return 1

	def get_channel_station(self, station):
		stmt = """Select id, name, description From stations
			Where station='%s'""" % station
		self.cursor.execute(stmt);
		
		id = station
		name = station
		desc =station
		if self.cursor.rowcount > 0:
			i, name, desc = self.cursor.fetchone();
			id = int(i)

		return [id, name, desc]


	def get_channel_name(self, data):
		stmt = """Select id, station, description From stations
			Where name='%s'""" % data["station_name"]
		self.cursor.execute(stmt);
		
		if self.cursor.rowcount > 0:
			id, station, desc = self.cursor.fetchone();
			data["station_id"] = int(id)
			data["station_station"] = station
			data["station_desc"]  = desc
			return 0
			
		name = upper(data["station_name"])

		stmt = """Select id, station, description From stations
			Where name='%s'""" % name
		self.cursor.execute(stmt);

		if self.cursor.rowcount > 0:
			i, station, desc = self.cursor.fetchone();
			data["station_id"] = int(id)
			data["station_station"] = station
			data["station_desc"]  = desc
			return 0
		
		data["station_id"] = name
		data["station_station"] = name
		data["station_desc"]  = name
		return 1
		

	def event_ids(self):
		stmt = """Select id From events
				Where start_time > FROM_UNIXTIME(%d)
				Order by start_time, channel_id""" % 0

		self.cursor.execute(stmt)
		ids = []

		for i in self.cursor.fetchall():
			ids.append(int(i[0]))

		return ids

	def delete_event_data(self, data):
		stmt = "Delete from events Where id=%d" % data["id"]
		self.cursor.execute(stmt)


	def set_event_data(self, data):
		data["length"] = int((data["stop_time"]-data["start_time"])/60)
		t = re.escape(data["title"])

		stmt = """Update events Set listing_id=%d,
				channel_id=%d,
				start_time=FROM_UNIXTIME(%d),
				end_time=FROM_UNIXTIME(%d),
				length=%d,
				repeat='%s',
				description='%s' where id=%d""" % \
					(data["listing_id"], data["channel_id"],\
					 data["start_time"],data["stop_time"],\
					 data["length"],  data["repeat"], \
					 t, data["id"])
		self.cursor.execute(stmt)

	def add_event_data(self, data):
		data["length"] = int((data["stop_time"]-data["start_time"])/60)
		t = re.escape(data["title"])

		
		stmt = """Insert into events(channel_id, listing_id, 
				start_time, end_time, length, repeat,
				description) 
			Values (%d, %d, FROM_UNIXTIME(%d), 
				FROM_UNIXTIME(%d), %d, '%s', '%s')""" % \
			(data["channel_id"], data["listing_id"], 
			 data["start_time"], data["stop_time"], 
			 data["length"], data["repeat"], t)
		self.cursor.execute(stmt);

		stmt = "Select id From events Where id=LAST_INSERT_ID()"
		self.cursor.execute(stmt);

		return_list = [];
		for last_id in self.cursor.fetchall():
			return_list.append(int(last_id[0]))

		return return_list

	def get_event_data(self, id, data):
		stmt = """Select id, listing_id, channel_id, start_time, \
			end_time, length, repeat, description From events
			Where id=%d""" % id
		self.cursor.execute(stmt);

		i, listing_id, ch_id, start, end, length, repeat, desc = \
			self.cursor.fetchone()

		data["station_id"] = int(ch_id)
		self.get_channel_id(data)

		data["id"] = int(i)
		data["channel_id"] = int(ch_id)
		data["listing_id"] = int(listing_id)
		data["start_time"] = TicksDateTime(start)
		data["stop_time"]= TicksDateTime(end)
		data["length"] = int(length)
		data["repeat"] = repeat
		data["title"] = desc

		return data

