#! /usr/bin/env python
#
# (c) 2007 by Nathan Smith (ndansmith@gmail.com)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

import sys, os
from threading import Thread
from mol_cfg_helper import *

#try:
import pygtk
pygtk.require('2.0')
import gtk
import gtk.glade
import molimg
#except:
#	print "Error importing modules.  Did you install Mac-on-Linux \
#	correctly?"

### GUI class for configuring a new guest OS
class CONFIG_OS:
	def __init__(self,gui):
		### Open up the glade file
		self.gladefile="add-os.glade"
		self.cgui = gtk.glade.XML(self.gladefile)
		self.gui = gui	
		### Initialize MOL_OS object
		self.mol_cfg = MOL_OS()

		### Keep track of the page
		self.page = 1
		
		### Grab various widgets
		self.add_os_window = self.cgui.get_widget("add_os_window")
		self.cancel_window = self.cgui.get_widget("cancel_window")
		self.notebook = self.cgui.get_widget("add_os_notebook")
		self.notebook_pages = self.notebook.get_n_pages()
		self.forward_button = self.cgui.get_widget("add_os_forward_b")
		self.back_button = self.cgui.get_widget("add_os_back_b")
		self.save_button = self.cgui.get_widget("add_os_finish_b")
		self.scsi_box = self.cgui.get_widget("scsi_box")
		self.add_volume_window = self.cgui.get_widget("add_volume_window")
		self.volume_chooser = self.cgui.get_widget("volume_chooser")
		self.os_volumes = self.cgui.get_widget("os_volumes")
		self.create_image_dialog = self.cgui.get_widget("create_image_dialog")
		self.new_image_chooser = self.cgui.get_widget("new_image_chooser")
		self.scsi_view = self.cgui.get_widget("scsi_devs")
		self.scsi_window = self.cgui.get_widget("scsi_window")

		### Associate values from the GUI
		self.os_name = self.cgui.get_widget("os_name")
		self.os_ram = self.cgui.get_widget("os_ram")
		self.os_usb = self.cgui.get_widget("os_enable_usb")
		self.os_type = self.cgui.get_widget("os_type")
		self.os_altivec = self.cgui.get_widget("os_enable_altivec")
		self.share_os = self.cgui.get_widget("share_os")
		self.add_volume_path = self.cgui.get_widget("add_volume_path")
		self.boot_checked = self.cgui.get_widget("boot_check")
		self.force_check = self.cgui.get_widget("force_check")
		self.cd_check = self.cgui.get_widget("cd_check")
		self.whole_check = self.cgui.get_widget("whole_check")
		self.new_image_path = self.cgui.get_widget("new_image_path")
		self.new_image_size = self.cgui.get_widget("new_image_size")
		self.new_scsi_dev = self.cgui.get_widget("new_scsi_dev")
		self.read_write = self.cgui.get_widget("read_write")
		self.os_type = self.cgui.get_widget("os_type")
		self.os_type_macos = self.cgui.get_widget("os_type_macos")
		self.os_enable_usb = self.cgui.get_widget("os_enable_usb")
		self.os_enable_altivec = self.cgui.get_widget("os_enable_altivec")
		self.share_os = self.cgui.get_widget("share_os")
		self.scsi_mode = self.cgui.get_widget("scsi_mode")

		### Listing volumes
		self.volumes = []
		self.volumes_list = gtk.ListStore(str, str, int)
		self.column1 = gtk.TreeViewColumn("Device or Image")
		self.column2 = gtk.TreeViewColumn("Options")
		self.rend1 = gtk.CellRendererText()
		self.column1.pack_start(self.rend1, True)
		self.column1.add_attribute(self.rend1, 'text', 0)
		self.rend2 = gtk.CellRendererText()
		self.column2.pack_start(self.rend2, True)
		self.column2.add_attribute(self.rend2, 'text', 1)
		self.os_volumes.append_column(self.column1)
		self.os_volumes.append_column(self.column2)
		self.os_volumes.set_model(self.volumes_list)

		### SCSI devices
		self.scsi_devs = []
		self.scsi_list = gtk.ListStore(str, int)
		self.column_scsi = gtk.TreeViewColumn("Device")
		self.rend_scsi = gtk.CellRendererText()
		self.column_scsi.pack_start(self.rend_scsi, True)
		self.column_scsi.add_attribute(self.rend_scsi, 'text', 0)
		self.scsi_view.append_column(self.column_scsi)
		self.scsi_view.set_model(self.scsi_list)

		### Connect signals
		dic = { "on_add_os_f_b_clicked" : self.forward,
			"on_add_os_cancel_b_clicked" : self.cancel,
			"on_add_os_finish_b_clicked" : self.write_config,
			"on_cancel_no_clicked" : self.hide_cancel,
			"on_cancel_yes_clicked" : self.quit,
			"on_scsi_mode_toggled" : self.scsi_box_off,
			"on_no_scsi_toggled" : self.scsi_box_off,
			"on_manual_scsi_toggled" : self.scsi_box_on,
			"on_cancel_commit_volume_clicked" : self.hide_add_volume,
			"on_os_add_volume_clicked" : self.show_add_volume,
			"on_add_volume_browse_clicked" : self.show_volume_chooser,
			"on_volume_chooser_cancel_clicked" : self.hide_volume_chooser,
			"on_volume_chooser_open_clicked" : self.choose_volume,
			"on_commit_volume_b_clicked" : self.commit_volume,
			"on_os_del_volume_clicked" : self.remove_volume,
			"on_os_new_volume_clicked" : self.show_create_image,
			"on_new_image_cancel_clicked" : self.hide_create_image,
			"on_new_image_ok_clicked" : self.create_image,
			"on_new_image_browse_clicked" : self.show_new_image_chooser,
			"on_new_volume_save_cancel_clicked" : self.hide_new_image_chooser,
			"on_new_volume_save_ok_clicked" : self.select_new_image,
			"on_os_type_toggled" : self.osx,
			"on_os_type_macos_toggled" : self.macos,
			"on_os_type_linux_toggled" : self.linux,
			"on_add_scsi_b_clicked" : self.show_scsi_window,
			"on_remove_scsi_b_clicked" : self.remove_scsi_device,
			"on_new_scsi_cancel_clicked" : self.hide_scsi_window,
			"on_new_scsi_add_clicked" : self.add_scsi_device,
			"on_add_os_b_b_clicked" : self.backward }
		self.cgui.signal_autoconnect(dic)
		self.add_os_window.connect("delete-event", self.cancel)
		self.cancel_window.connect("delete-event", self.hide_cancel)
		self.add_volume_window.connect("delete-event", self.hide_add_volume)
		self.volume_chooser.connect("delete-event", self.hide_volume_chooser)
		self.create_image_dialog.connect("delete-event", self.hide_create_image)
		self.new_image_chooser.connect("delete-event", self.hide_new_image_chooser)
		self.scsi_window.connect("delete-event", self.hide_scsi_window)

	### Forward one page
	def forward(self,w=None):
		self.page += 1
		self.notebook.next_page()
		if self.page == self.notebook_pages:
			self.forward_button.set_sensitive(False)
			self.save_button.set_sensitive(True)
		if self.page > 1:
			self.back_button.set_sensitive(True)
	
	### Backwards one page
	def backward(self,w=None):
		self.page -= 1
		self.notebook.prev_page()
		if self.page == 1:
			self.back_button.set_sensitive(False)
		if self.page < self.notebook_pages:
			self.forward_button.set_sensitive(True)
			self.save_button.set_sensitive(False)
	
	### Warn on closing w/o saving
	def cancel(self,w=None,e=None):
		self.cancel_window.show()
		return True

	def hide_cancel(self,w=None,e=None):
		self.cancel_window.hide()
		return True

	def quit(self,w=None):
		### Cleanup and go home
		self.add_os_window.destroy()
		self.cancel_window.destroy()

	### Add volume dialog
	def show_add_volume(self,w=None):
		self.add_volume_window.show()

	def hide_add_volume(self,w=None,e=None):
		self.add_volume_path.set_text("") 
		self.boot_checked.set_active(False)
		self.force_check.set_active(False)
		self.whole_check.set_active(False)
		self.cd_check.set_active(False)
		self.add_volume_window.hide()
		return True

	def show_volume_chooser(self,w=None,e=None):
		self.volume_chooser.show()
	
	def hide_volume_chooser(self,w=None,e=None):
		self.volume_chooser.hide()
		return True

	def choose_volume(self,w=None,e=None):
		self.add_volume_path.set_text(self.volume_chooser.get_filename())
		self.hide_volume_chooser()

	def hide_create_image(self,w=None,e=None):
		self.create_image_dialog.hide()
		self.new_image_path.set_text("")
		self.new_image_size.set_value(512)
		return True

	def show_create_image(self,w=None,e=None):
		self.create_image_dialog.show()

	def create_image(self,w=None,e=None):
		path = self.new_image_path.get_text()
		molimg.create_raw(path, int(self.new_image_size.get_value()))
		self.hide_create_image()
		self.show_add_volume()
		self.add_volume_path.set_text(path)

	def hide_new_image_chooser(self,w=None,e=None):
		self.new_image_chooser.hide()
		return True

	def show_new_image_chooser(self,w=None,e=None):
		self.new_image_chooser.show()

	def select_new_image(self,w=None,e=None):
		self.new_image_path.set_text(self.new_image_chooser.get_filename())
		self.hide_new_image_chooser()

	def remove_volume(self,w=None,e=None):
		model, iter = self.os_volumes.get_selection().get_selected()
		index = model.get_value(iter, 2)
		self.volumes.pop(index)
		self.refresh_volumes()

	def commit_volume(self,w=None,e=None):
		### For API a list starting with path and followed by options
		volume_info = [self.add_volume_path.get_text()]
		if self.boot_checked.get_active():
			volume_info.append("boot")
		if self.cd_check.get_active():
			volume_info.append("cdboot")
		if self.force_check.get_active():
			volume_info.append("force")
		if self.whole_check.get_active():
			volume_info.append("whole")
		if self.read_write.get_active():
			volume_info.append("rw")
		else:
			volume_info.append("ro")
		self.volumes.append(volume_info)
		self.hide_add_volume()
		self.refresh_volumes()
		
	def refresh_volumes(self,w=None,e=None):
		### Clean up the list
		self.volumes_list.clear()
		### Repopulate the list with options
		for i in range(len(self.volumes)):
			if self.volumes[i][0].startswith("/dev"):
				path = self.volumes[i][0]	
			else:
				path = self.volumes[i][0].split("/")[-1]
			opts = ""
			### For the gtk list opts are a single string
			for option in self.volumes[i][1:]:
				opts += (option + " ")
			self.volumes_list.append([path, opts, i])
		### Refresh the tree model
		self.os_volumes.set_model(self.volumes_list)

	### OS Specific options and minimums for RAM and HD size
	### Change to OS X
	def osx(self,w=None):
		self.os_ram.set_range(96,1000000)
		self.os_ram.set_value(256)

	### Change to Linux
	def linux(self,w=None):
		self.os_ram.set_range(64,1000000)
		self.os_ram.set_value(128)
		pass

	### Change to Mac OS
	def macos(self,w=None):
		self.os_ram.set_range(32,1000000)
		self.os_ram.set_value(128)

	### SCSI functions
	def show_scsi_window(self,w=None,e=None):
		self.scsi_window.show()

	def hide_scsi_window(self,w=None,e=None):
		self.new_scsi_dev.set_text("")
		self.scsi_window.hide()
		return True

	def add_scsi_device(self,w=None,e=None):
		### TODO: Regex eval of device path
		self.scsi_devs.append(self.new_scsi_dev.get_text())	
		self.hide_scsi_window()
		self.refresh_scsi()

	def remove_scsi_device(self,w=None,e=None):
		model, iter = self.scsi_view.get_selection().get_selected()
		index = model.get_value(iter, 1)
		self.scsi_devs.pop(index)
		self.refresh_scsi()

	def refresh_scsi(self,w=None,e=None):
		self.scsi_list.clear()
		for i in range(len(self.scsi_devs)):
			self.scsi_list.append([self.scsi_devs[i], i])
		self.scsi_view.set_model(self.scsi_list)

	def scsi_box_on(self,w=None):
		self.scsi_box.set_sensitive(True)
	
	def scsi_box_off(self,w=None):
		self.scsi_box.set_sensitive(False)

	### Misc callbacks
	
	### Write the configuration
	def write_config(self,w=None,e=None):
		### Name
		self.mol_cfg.name = self.os_name.get_text()
		### Type
		if self.os_type.get_active():
			self.mol_cfg.type = "osx"
			self.mol_cfg.fancy = "Mac OS X"
		elif self.os_type_macos.get_active():
			self.mol_cfg.type = "macos"
			self.mol_cfg.fancy = "Mac OS"
		else:
			self.mol_cfg.type = "linux"
			self.mol_cfg.fancy = "Linux"
		### RAM
		self.mol_cfg.ram = self.os_ram.get_value()
		### USB
		if self.os_enable_usb.get_active():
			self.mol_cfg.usb = 1
		else:
			self.mol_cfg.usb = 0
		### AltiVec
		if self.os_enable_altivec.get_active():
			self.mol_cfg.altivec = 1
		else:
			self.mol_cfg.altivec = 0
		### Local v. Shared
		if self.share_os.get_active():
			self.mol_cfg.shared = 1
		else:
			self.mol_cfg.shared = 0
		### Volumes
		self.mol_cfg.volumes = self.volumes
		### SCSI
		self.mol_cfg.scsi_devs = self.scsi_devs
		if self.scsi_mode.get_active():
			self.mol_cfg.auto_scsi = 1
		else:
			self.mol_cfg.auto_scsi = 0
		### Close the window:
		self.add_os_window.hide()
		self.mol_cfg.write()
		self.gui.get_os_list()
		self.quit()

### Main GUI class
class MOL_GUI:
	def __init__(self):
		### Initalize some variables
		self.molargs = ""
		### Load the glade file
		self.gladefile="mol-gui.glade"
		self.gui = gtk.glade.XML(self.gladefile)
		### Main window object
		self.window = self.gui.get_widget("main_window")
		### OS Selection menu setup
		self.os_box = self.gui.get_widget("os_box")
		self.os_box.set_headers_visible(True)
		self.column0 = gtk.TreeViewColumn("")
		self.column1 = gtk.TreeViewColumn("Name")
		self.column2 = gtk.TreeViewColumn("Type")
		self.rend0 = gtk.CellRendererPixbuf()
		self.rend1 = gtk.CellRendererText()
		self.rend2 = gtk.CellRendererText()
		self.rend3 = gtk.CellRendererText()
		self.column0.pack_start(self.rend0, True)
		self.column0.add_attribute(self.rend0, 'pixbuf', 0)
		self.column1.pack_start(self.rend1, True)
		self.column1.add_attribute(self.rend1, 'text', 1)
		self.column2.pack_start(self.rend2, True)
		self.column2.add_attribute(self.rend2, 'text', 2)
		self.os_box.append_column(self.column0)
		self.os_box.append_column(self.column1)
		self.os_box.append_column(self.column2)
		### Generate OS list just-in-time
		self.get_os_list()
		### Thread tracking 
		self.thread_track = TRACK_MOL()
		### About window
		self.about_window = self.gui.get_widget("info_window")
		if (self.about_window):
			self.about_window.connect("delete-event", self.about_hide)
		### Connect events to callbacks
		dic = { "on_add_os_b_clicked" : self.add_os,
			"on_del_os_b_clicked" : self.del_warn,
			"on_boot_b_clicked" : self.boot_chooser,
			"on_cd_boot_clicked" : self.cd_boot_chooser,
			"on_about_b_clicked" : self.about_mol,
			"on_mol_info_ok_clicked" : self.about_hide,
			"on_cancel_del_clicked" : self.del_warn_hide,
			"on_confirm_del_clicked" : self.del_os,
			"on_quit_b_clicked" : gtk.main_quit }
		self.gui.signal_autoconnect(dic)
		### Quit on attempted close
		if (self.window):
			self.window.connect("destroy", gtk.main_quit)
	
	### Add options to molargs to be passed to BOOT_MOL class (and then to mol binary)
	def add_opt(self,opt):
		# Each option is added to the molargs string with a separating space
		# e.g. --cdboot -X -5
		self.molargs += " %s" % opt

	def get_os_list(self,w=None):
		### Get list of bootable OSes
		bootable_oses = mol_list_os()
		self.os_list = gtk.ListStore(gtk.gdk.Pixbuf, str, str, str)
		for config in bootable_oses:
			name = config[0]
			type = config[1]
			path = config[2]
			### TODO Abs path for icons
			icon = gtk.gdk.pixbuf_new_from_file("images/" + type + ".png")
			self.os_list.append([icon, name, type, path])

		#osx_icon = gtk.gdk.pixbuf_new_from_file("images/osx.png")
		#macos_icon = gtk.gdk.pixbuf_new_from_file("images/macos.png")
		#linux_icon = gtk.gdk.pixbuf_new_from_file("images/linux.png")
		#self.os_list.append([osx_icon, 'Mac OS X', 'osx'])
		#self.os_list.append([macos_icon, 'Mac OS', 'macos'])
		#self.os_list.append([linux_icon, 'Linux', 'linux'])
		self.os_box.set_model(self.os_list)
		
	#######################################################################
	### Callback functions
	#######################################################################
	
	def test(self,w=None):
		print "test"

	### Config OS
	def add_os(self,w=None):
		blah = CONFIG_OS(self)

	### Delete OS
	def del_os(self,w=None):
		(model, iter)  = self.os_box.get_selection().get_selected()
		if not iter:
			self.del_warn_hide()
			return
		config_path = model.get_value(iter, 3)
		print "Deleting profile %s" % config_path 
		os.remove(config_path)
		self.del_warn_hide()
		### Refresh the list
		self.get_os_list()
	
	### Delete profile prompt
	def del_warn(self,w=None):
		self.warning_window = self.gui.get_widget("warning_window")
		self.warning_window.show()	

	def del_warn_hide(self,w=None,blah=None):
		self.warning_window.hide()
		return True

	### Boot button
	def boot_chooser(self,w=None):
		(model, iter)  = self.os_box.get_selection().get_selected()
		if not iter:
			return
		### FIXME mol2
		type = model.get_value(iter, 2)
		if type == "osx":
			self.add_opt('-X')
			self.boot()
		elif type == "macos":
			self.boot()
		elif type == "linux":
			self.add_opt('--linux')
			self.boot()

	### CD Boot button
	def cd_boot_chooser(self,w=None):
		self.add_opt('--cdboot')
		self.boot_chooser()

	### Boot OS X
	def boot(self,w=None):
		### Create boot mol thread object
		mol_boot = BOOT_MOL(self.molargs)
		### State the thread
		mol_boot.start()
		### Don't forget to wipe the molargs
		self.molargs = ""
		### Keep track of active threads
		### TODO find way to kill threads
		# self.thread_track.append(os_x_boot)

	### About MOL
	def about_mol(self,w=None):
		self.about_window = self.gui.get_widget("info_window")
		self.about_window.show()	

	def about_hide(self,w=None,blah=None):
		self.about_window.hide()
		return True

	### Main Loop
	def main(self):
		gtk.main()

### Start 'er up
def mol_cfg_gtk_init():
	mol = MOL_GUI()
	mol.main()

if __name__ == '__main__':
	mol_cfg_gtk_init()

###
# TODO
# Pass profile path to mol binary instead of just type
# Need a register of active threads
# Need to find a way to alter BOOT_MOL handler names for multiple instances of
#	one Guest-OS type
# Configure mol options (video etc)
# Edit existing configurations
# Power down button
# Remember last booted OS
# QCOW Support
# Error handling
# Menu signals connect
# Help text !!
