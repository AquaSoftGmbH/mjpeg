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
import fcntl, struct, V4L

class V4l:

	def __init__(self, device_name="/dev/video"):
		self.device = open(device_name, "r+")
		self.fd = self.device.fileno()

	def ioctl(self, code, format, input=""):
		size = struct.calcsize(format)
		
		if len(input) != size:
			buffer = ' ' * size
		else:
			buffer = input
		buffer = fcntl.ioctl(self.fd, code, buffer)
		return struct.unpack(format, buffer)


	def query_capability(self):
		buffer = self.ioctl(V4L.VIDIOCGCAP, "32siiiiiii")
		return buffer

	def get_frame_buffer(self):
		buffer = self.ioctl(V4L.VIDIOCGFBUF, "piiii")
		return buffer

	def set_frame_buffer(self, frame, height, width, depth, bytesperline):
		buffer = struct.pack("piiii", frame, height, width, depth, bytesperline)
		self.ioctl(V4L.VIDIOCSFBUF, "piiii", buffer)

	def get_capture_window(self):
		buffer = self.ioct(V4L.VIDIOCGWIN, "iiiiii")
		return buffer

	def set_capture_window(self, x, y, width, height, chromakey, flags):
		buffer = struct.pack("iiiiiipi", x, y, width, height, chromakey, flags, 0, 0)
		self.ioctl(V4L.VIDIOCSWIN, "iiiiiipi", buffer)

	def capture(self, enable):
		buffer = struct.pack("i", enable)
		self.ioctl(V4L.VIDIOCCAPTURE, buffer)

	def get_video_subcapture(self):
		buffer = self.ioctl(V4L.VIDOCGCAPTURE, "iiiihh")
		return buffer

	def set_video_subcapture(self, x, y, width, height, decimation, flags):
		buffer = struct.pack("iiiihh", x, y, width, height, decimation, flags)
		self.ioctl(V4L.VIDIOCSCAPTURE, "iiiihh", buffer)

	def get_video_source(self, channel):
		buffer = struct.pack("i32siihh", channel, "", 0, 0, 0, 0)
		buffer = self.ioctl(V4L.VIDIOCGCHAN, "i32siihh", buffer)
		return buffer

	def set_video_source(self, channel, norm):
		buffer = struct.pack("i32siihh", channel, "", 0, 0, 0, norm)
		self.ioctl(V4L.VIDIOCSCHAN, "i32siihh", channel, "", 0, 0, 0, norm)

	def get_image_properties(self):
		buffer = self.ioctl(V4L.VIDIOCGPICT, "hhhhhhh")
		return buffer

	def set_image_properties(self, brightness, hue, colour, contrast, whiteness, depth, palette):
		buffer = struct.pack("hhhhhhh", brigthness, hue, colour, contrast, whiteness, depth, palette)
		self.ioctl(V4L.VIDIOCSPICT, "hhhhhhh", buffer)

	def get_tuning(self, tuner):
		buffer = struct.pack("i32siiihh", tuner, "", 0, 0, 0, 0, 0)
		buffer = self.ioctl(V4L.VIDIOCGTUNER, "i32sihh", buffer)
		return buffer

	def set_tuning(self, tuner, norm):
		buffer = struct.pack("i32sihh",tuner, "", 0, 0, 0, norm, 0)
		buffer = self.ioctl(V4L.VIDIOCSTUNER, "i32sihh", buffer)

	def get_frequency(self):
		buffer = self.ioctl(V4L.VIDIOCGFREQ, "i")
		return buffer

	def set_frequency(self, frequency):
		buffer = struct.pack(V4L.VIDIOCSFREQ, "i", frequency)
		self.ioctl("i", buffer)

	def get_audio(self, channel):
		buffer = struct.pack("ihhhi16shhh", channel, 0, 0, 0, 0, "", 0, 0, 0)
		buffer = self.ioctl(V4L.VIDIOCGAUDIO, "ihhhi16shhh", buffer)

	def set_audio(self, channel, volume, bass, treble, flags, mode, balance, step):
		buffer = struct.pack(V4L.VIDIOCSAUDIO, "ihhhi16shhh", channel, volume, bass, treble, flags, "", mode, blance, step)

	def decode_cap_types(self, types):
		string = ""
		if types & V4L.VID_TYPE_CAPTURE:
			string = string + "capture "
		if types & V4L.VID_TYPE_TUNER:
			string = string + "tuner "
		if types & V4L.VID_TYPE_TELETEXT:
			string = string + "teletext "
		if types & V4L.VID_TYPE_OVERLAY:
			string = string + "overlay "
		if types & V4L.VID_TYPE_CHROMAKEY:
			string = string + "chromakey "
		if types & V4L.VID_TYPE_CLIPPING:
			string = string + "clipping "
		if types & V4L.VID_TYPE_FRAMERAM:
			string = string + "frameram "
		if types & V4L.VID_TYPE_SCALES:
			string = string + "scales "
		if types & V4L.VID_TYPE_MONOCHROME:
			string = string + "monochrome "
		if types & V4L.VID_TYPE_SUBCAPTURE:
			string = string + "subcapture "
		return string


	def decode_chan(self, name, tuner, flags, type, norm):
		string = name
		if flags & V4L.VIDEO_VC_TUNER:
			string = string + " tuner=%d" % tuner
		if flags & V4L.VIDEO_VC_AUDIO:
			string = string + " audio"
		if type == V4L.VIDEO_TYPE_TV:
			string = string + " type=tv"
		elif type == V4L.VIDEO_TYPE_CAMERA:
			string = string + " type=camera"
		if norm == V4L.VIDEO_MODE_PAL:
	                string = string + " norm=PAL"
	   	elif norm == V4L.VIDEO_MODE_NTSC:
        	        string = string + " norm=NTSC"
	  	elif norm == V4L.VIDEO_MODE_SECAM:
        	        string = string + " norm=SECAM"
		elif norm == V4L.VIDEO_MODE_AUTO:
        	        string = string + " norm=AUTO"
		return string

