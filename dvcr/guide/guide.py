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
from string import *
from time import *
import marshal 

class GuideCmd:
	def __init__(self, func, *args, **kw):
		self.func = func
		self.args = args
		self.kw = kw

	def __call__(self, *args, **kw):
		args = self.args + args
		kw.update(self.kw)
		apply(self.func, args, kw)



class Guide:

	def __init__(self):
		self.listing = []

	def output(self):
		self.listing.sort()

		for data in self.listing:
			print "%s %-10s %3d mins. %s" % \
				(strftime("%m/%d/%y %H:%M", gmtime(data[0])), 
				data[1], data[3], data[4])

	def append(self, new_entry):
		self.listing.append(new_entry)

	def save(self):
		file_name = strftime("%y.%m.%d", gmtime(self.listing[0][0]))
		file = open(file_name, "w")
		marshal.dump(self.listing, file)

	def load(self, seconds):
		file_name = strftime("%y.%m.%d", gmtime(seconds))
		file = open(file_name, "r")
		self.listing = marshal.load(file)

		
		
