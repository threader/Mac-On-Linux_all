#!/usr/bin/python
try:
	import sys, os, commands

except ImportError, e:
	print "Module loading failed with: " + str(e)
	print "Please check that xac was installed correctly"

### Main Dialog Class
class Dialog:
	def __init__(self, text):	
		self.type = "none"
		self.text = text
		self.height = 0
		self.width = 0
		self.defaultno = 0

	def height(self, height):
		if height != None:
			self.height = hieght
		return self.height

	def width(self, width):
		if width != None:
			self.width = width
		return self.width

	### Draws the object with dialog
	def draw(self, cmd=""):
		c = '"' + str(self.text) + '"'
		c = c + " " + str(self.height)
		c = c + " " + str(self.width) + " "
		if cmd:
			cmd = c + cmd
		else:
			cmd = c

		if self.defaultno:
			cmd = "--defaultno " + cmd

		c = 'dialog --' + self.type + ' ' + cmd + ' 2>&1 > /dev/tty'	

		### Call dialog	
		result = os.popen(c)
		value = result.read()
		if result.close():
			return 0
		else:
			return value

	### Add something to the Dialog object (depends on the type)
	def add(self, item):
		pass

### Dialog box with some text and an ok button
class Dialog_msgbox(Dialog):
	def __init__(self, text):
		Dialog.__init__(self,text)
		self.type = "msgbox"

### Yes/No dialog
class Dialog_yesno(Dialog):
	def __init__(self, text):
		Dialog.__init__(self, text)
		self.type = "yesno"

### Type in a line of text
class Dialog_inputbox(Dialog):
	def __init__(self, text):
		Dialog.__init__(self,text)
		self.type = "inputbox"
		self.start_value = None

	def add(self, value):
		if value:
			self.start_value = value
		else:
			self.start_value = None

	def draw(self):
		return Dialog.draw(self, self.start_value)			

### Menu
class Dialog_menu(Dialog):
	def __init__(self, text):
		Dialog.__init__(self,text)
		self.type = "menu"
		self.options = []
		self.menu_height = 0

	def draw(self):
		if len(self.options) > 0:
			c = str(self.menu_height) + " " + " ".join(self.options)
			return Dialog.draw(self, c)
		
		### No result if the menu can't be drawn
		return 0

	def add(self, tag, item):
		if tag and item:
			self.options.append('"' +  tag + '" "' + item + '"')
