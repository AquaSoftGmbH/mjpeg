import termios, TERMIOS

class Sony:
	codes = {
		"on" : "\xfa\02",
		"off" : "\xfa\02",
		"exit" : "\xfa\x45\x00\x00\xc5",
		"up": "\xfa\x45\x00\x00\x9c",
		"down" : "\xfa\x45\x00\x00\x9d",
		"left" :"\xfa\x45\x00\x00\x9b",
		"right" : "\xfa\x45\x00\x00\x9a",
		"next" : "\xfa\x45\x00\x00\xd2",
		"last" : "\xfa\x45\x00\x00\xd3",
		"jump" : "\xfa\x45\x00\x00\xd8",
		"select": "\xfa\x45\x00\x00\xc3",
		"guide" : "\xfa\x45\x00\x00\xe5",
		"menu" : "\xfa\x45\x00\x00\xf7",
		"channel" : "\xfa\x46"
	}

	def __init__(self, device_name="/dev/ttyS1"):
		IFLAG = 0
		OFLAG = 1
		CFLAG = 2
		LFLAG = 3
		ISPEED = 4
		OSPEED = 5
		self.device = open(device_name, 'r+')
		self.fd = self.device.fileno()
		tc = termios.tcgetattr(self.fd)
		termios.tcflush(self.fd, TERMIOS.TCIOFLUSH)
		tc[IFLAG] = 0;
		tc[LFLAG] = 0;
		tc[CFLAG] = TERMIOS.CLOCAL | TERMIOS.CS8 | TERMIOS.CREAD
		tc[ISPEED] = TERMIOS.B9600
		tc[OSPEED] = TERMIOS.B9600
		termios.tcsetattr(self.fd, TERMIOS.TCSANOW, tc)

	def channel(self, chan):
		output = self.codes["channel"] + chr(chan/256) + chr(chan&0xff)
		self.device.write(output)
		self.device.flush()

	def press(self, button):
		if self.codes.has_key(button):
			self.device.write(self.codes[button])
			self.device.flush()

	def key_on(self):
		self.press("on")

	def key_off(self):
		self_press("off")

	def key_exit(self):
		self_press("exit")

	def key_up(self):
		self.press("up")

	def key_down(self):
		self.press("down")

	def key_left(self):
		self.press("left")

	def key_right(self):
		self.press("right")

	def key_next(self):
		self.press("next")

	def key_last(self):
		self.press("last")

	def key_jump(self):
		self.press("jump")

	def key_select(self):
		self.press("select")

	def key_guide(self):
		self.press("guide")

	def key_menu(self):
		self.press("menu")

