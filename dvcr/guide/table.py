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
import Tkinter
import Pmw
import string

class Table(Pmw.MegaWidget):

	def __init__(self, parent = None, **kw):

		self.widgets = {}

		optiondefs = (
			("fixedrow", 	0,		Pmw.INITOPT),
			("fixedcol", 	0,		Pmw.INITOPT),
			("horizflex", 	'fixed',	self._horizflex),
			("horizfraction", 0.05,		Pmw.INITOPT),
			("hscrollmode",	'dynamic',	self._hscrollmode),
			("scrollmargin", 2,		Pmw.INITOPT),
			("vertflex",	"fixed",	self._vertflex),
			("vertfraction", 0.05,		Pmw.INITOPT),
			("vscrollmode",  'dynamic',	self._vscrollmode),
			("displaycol",	999,		Pmw.INITOPT),
			("displayrow",  999,		Pmw.INITOPT),
		)

		self.defineoptions(kw, optiondefs)

		Pmw.MegaWidget.__init__(self, parent)

		self.origInterior = self.interior()

		self.table = self.createcomponent("table", (), None,
			Tkinter.Frame, self.origInterior,
				background = 'blue'
			)

		self.vertScrollbar = self.createcomponent("vertscrollbar", 
			(), 'Scrollbar',
			Tkinter.Scrollbar, (self.origInterior,),
			orient='vertical',
			command=self.yview)

		self.horizScrollbar = self.createcomponent("horzscrollbar", 
			(), 'Scrollbar',
			Tkinter.Scrollbar, (self.origInterior,), 
			orient='horizontal',
			command=self.xview)


		self.table.grid(row=1, column=1)

		self.horizScrollbarOn = 0
		self.vertScrollbarOn = 0
		self.scrollTimer = None
		self.scrollRecurse = 0
		self.horizScrollbarNeeded = 0
		self.vertScrollbarNeeded = 0
		self.startX = 0.0
		self.maxX = 0
		self.startY = 0.0
		self.maxY = 0
		self.lastX = -1.0
		self.lastY = -1.0

		self._flexoptions = ("fixed", "expand", "shrink", "elastic");

		self.initialiseoptions(Table)

	def destroy(self):
		if self.scrollTimer is not None:
			self.after_cancel(self.scrollTimer)
			self.scrollTimer = None
		Pmw.MegaWidget.destroy(self)

	def _hscrollmode(self):
		mode = self['hscrollmode']

		if mode == 'static':
			if not self.horizScrollbarOn:
				self.toggleHorizScrollbar()
		elif mode == 'dynamic':
			if self.horizScrollbarNeeded != self.horizScrollbarOn:
				self.toggleHorizScrollbar()
		elif mode == 'none':
			if self.horizScrollbarOn:
				self.toggleHorizScrollbar()
		else:
			message = 'bad hscrollmode option "%s": should be static, dynamic, or none' % mode
			raise ValueError, message

	def _vscrollmode(self):
		mode = self['vscrollmode']

		if mode == 'static':
			if not self.vertScrollbarOn:
				self.togglevertScrollbar()
		elif mode == 'dynamic':
			if self.vertScrollbarNeeded != self.vertScrollbarOn:
				self.togglevertScrollbar()
		elif mode == 'none':
			if self.vertScrollbarOn:
				self.togglevertScrollbar()
		else:
			message = 'bad vscrollmode option "%s": should be static, dynamic, or none' % mode
			raise ValueError, message

	def _horizflex(self):
		flex = self['horizflex']

		if flex not in self._flexoptions:
			message = 'bad horizflex option "%s": should be one of %s' \
				% mode, str(self._fexoptions)
			raise ValueError, message

		self.reposition()

	def _vertflex(self):
		flex = self['vertflex']

		if flex not in self._flexoptions:
			message = 'bad vertflex option "%s": should be one of %s' \
				% mode, str(self._fexoptions)
			raise ValueError, message

		self.reposition()

	def reposition(self):
		if self.scrollTimer == None:
			self.scrollTimer = self.after_idle(self.scrollBothNow)

	def scrollBothNow(self):
		self.scrollTimer = None

		self.scrollRecurse = self.scrollRecurse + 1
		self.update_idletasks()
		self.scrollRecurse = self.scrollRecurse - 1
		if self.scrollRecurse != 0:
			return

		xview = self.getxview()
		yview = self.getyview()

		self.display()

		self.horizScrollbar.set(xview[0], xview[1])
		self.vertScrollbar.set(yview[0], yview[1])

		print "both ", xview, yview
		self.horizScrollbarNeeded = (xview != (0.0, 1.0))
		self.vertScrollbarNeeded = (yview != (0.0, 1.0))

		if (self['hscrollmode'] == self['vscrollmode'] == 'dynamic' and
			self.horizScrollbarNeeded != self.horizScrollbarOn and
			self.vertScrollbarNeeded != self.vertScrollbarOn and
			self.vertScrollbarOn != self.horizScrollbarOn):
			if self.horizScrollbarOn:
				self.toggleHorizScrollbar()
			else:
				self.toggleVertScrollbar()
			return

		if self['hscrollmode'] == 'dynamic':
			if self.horizScrollbarNeeded != self.horizScrollbarOn:
				self.toggleHorizScrollbar()

		if self['vscrollmode'] == 'dynamic':
			if self.vertScrollbarNeeded != self.vertScrollbarOn:
				self.toggleVertScrollbar()

	def toggleHorizScrollbar(self):
		self.horizScrollbarOn = not self.horizScrollbarOn

		interior = self.origInterior
		if self.horizScrollbarOn:
			self.horizScrollbar.grid(row=2, column=1,sticky='news')
			interior.grid_rowconfigure(1, minsize = self['scrollmargin'])
		else:
			self.horizScrollbar.grid_forget()
			interior.grid_rowconfigure(1, minsize = 0)

	def toggleVertScrollbar(self):
		self.vertScrollbarOn = not self.vertScrollbarOn

		interior = self.origInterior
		if self.vertScrollbarOn:
			self.vertScrollbar.grid(row=1, column=3,sticky='news')
			interior.grid_columnconfigure(3, minsize = self['scrollmargin'])
		else:
			self.vertScrollbar.grid_forget()
			interior.grid_columnconfigure(3, minsize = 0)

	def xview(self, mode, value, units = None):
		print "xview  value=" , value 

		fixed = self['fixedcol']

		if mode == 'moveto':
			self.startX = string.atof(value)
			if self.startX < 0.0:
				self.startX = 0.0
		else:
			print "not moveto"

		self.reposition()


	def yview(self, mode, value, units = None):
		print "yview  value=" , value 

		fixed = self['fixedrow']

		if mode == 'moveto':
			self.startY = string.atof(value)
			if self.startY < 0.0:
				self.startY = 0.0
		else:
			print "not moveto"

		self.reposition()

	def getxview(self):
		cols, rows = self.table.size()
		fixed = self['fixedcol']
		max = float(self.maxX - fixed + 1)
		start = self.startX
		stop = start + float(cols - fixed) / max
		if stop > 1.0:
			stop = 1.0
			start = 1.0 - (float(cols - fixed) / float(self.maxX))
			self.startX = start
		return (start, stop)

	def getyview(self):
		cols, rows = self.table.size()
		fixed = self['fixedrow']
		max = float(self.maxY - fixed + 1)
		start = self.startY
		stop = start + float(rows - fixed) / max
		if stop > 1.0:
			stop = 1.0
			start = 1.0 - (float(rows - fixed) / float(self.maxY))
			self.startY = start
		return (start, stop)
			
	def add(self, item, column=0, row=0, columnspan=1):
		key = "%d,%d" % (column, row)
		self.widgets[key] = (item, column, row, columnspan)
		if column > self.maxX:
			self.maxX = column
		if row > self.maxY:
			self.maxY = row

	def display(self):
		column = int(float(self.maxX-self['fixedcol'] + 1) * self.startX)
		row = int(float(self.maxY-self['fixedrow'] + 1) * self.startY)

		if column == self.lastX and row == self.lastY:
			return

		self.lastX = column
		self.lastY = row
		
		list = self.widgets.keys()

		for widget in self.table.grid_slaves():
			widget.grid_forget()


		for pos in list:
			widget, widget_column, widget_row, widget_span = \
				self.widgets[pos]

			if widget_row < self["fixedrow"]:
				if widget_column >= self["fixedcol"]:
					widget_column = widget_column - column
					if widget_column < self["fixedcol"]:
						continue
				if widget_column < 0:
					continue
				if widget_column >= self['displaycol']:
					continue
				widget.grid(column=widget_column, 
					row=widget_row,
					columnspan=widget_span,
					sticky="NSEW")
				continue

			if widget_column < self["fixedcol"]:
				widget_row = widget_row - row
				if widget_row < self["fixedrow"]:
					continue
				if widget_row >= self['displayrow']:
					continue
				widget.grid(column=widget_column, 
					row=widget_row,
					columnspan=widget_span,
					sticky="NSEW")
				continue

			col = widget_column - column
			widget_row = widget_row - row

			if col >= self['displaycol']:
				continue

			if col + widget_span > self['displaycol']:
				widget_span = self['displaycol'] - col

			if col + widget_span <= self["fixedcol"]:
				continue

			if col < self['fixedcol']:
				widget_span = widget_span - (self["fixedcol"] - col)
				col = self["fixedcol"]

			if widget_row >= self['displayrow']:
				continue

			if widget_row < self["fixedrow"]:
				continue

			widget.grid(column=col, 
				row=widget_row,
				columnspan=widget_span,
				sticky="NSEW")




		
