#!/usr/bin/python

###############################################################################
### Provide a default dialog based frontend
###############################################################################

try:
	import sys, os, commands, re
	### Import the backend
	from mol_cfg_helper import *

except ImportError, e:
	print "Module loading failed with: " + str(e)
	print "Please check that MOL is installed correctly!"
	sys.exit()

###############################################################################
### Dialog Wrapper
###############################################################################

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

	def draw(self):
		return Dialog.draw(self)

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

###############################################################################
### Configuration menu 
###############################################################################

def mol_dialog_main():
	### Main dialog instance
	mm = Dialog_menu('MOL - Main Menu')
	### Get available Boot Targets

	### List available Boot Targets

	### Add a Boot Target
	mm.add('Add','Add an OS')	
	### Configuration menu
	mm.add('Configure','Configure MOL')
	### Exit
	mm.add('Quit', 'Quit without saving')
	return mm

### Add an OS
def mol_dialog_add():
	add = Dialog_menu('MOL - Add an OS')
	### Add an OS X OS
	add.add('OS_X','Mac OS X')
	### Add a Mac Classic OS
	add.add('OS_9','Mac OS 9 or earlier')
	### Add a Linux OS
	### When Linux is booting again
	# add.add('Linux','Linux')
	### Exit
	add.add('Back','Cancel')
	return add
	
def mol_dialog_config():
	### Main dialog instance
	cfg = Dialog_menu('MOL - Configuration Menu')
	### Configure Video
	cfg.add('Video', 'Configure MOL Video')
	### Configure Sound
	cfg.add('Sound', 'Configure MOL Sound')
	### Configure Input
	cfg.add('Input', 'Configure MOL Input')
	### Save
	cfg.add('Save', 'Save your configuration')
	### Exit
	cfg.add('Back', 'Quit without saving')
	return cfg


###############################################################################
### ROM helper functions
###############################################################################

### Help for new world ROMs
def rom_help():
	Dialog_msgbox("By default, MOL loads the 'Mac OS ROM' file directly \
	from the System\n\
	Folder of the startup disk. If this is not desirable (unlikely),\n\
	then the ROM can be loaded from the linux side by using this\n\
	feature. Note that the ROM file must be copied to the\n\
	linux side as a *binary* without any kind of encoding (avoid\n\
	MacBinary in particular).").draw()
	return

### Rom menu
def rom_menu():
	response = Dialog_menu('Mac OS Classic ROM menu')
	### Users should probably skip this
	response.add('Done','Do not specify a ROM (recommended)')
	### Help!
	response.add('Help','Help with New World ROMs')
	### Specify a new-world ROM
	response.add('Add','Add a New World ROM')
	return response

###############################################################################
### SCSI helper functions
###############################################################################

### Help SCSI devices
def scsi_help():
	Dialog_msgbox('MOL - SCSI device help\n\nMac-on-Linux can use SCSI  \
	devices attached to your computer in the guest OS.  You can either \
	allow MOL to add all attached devices automatically by enabling auto \
	probing of SCSI devices, or you can choose to enter devices manually by\
	specifying the host, channel, and ID of each device \
	(e.g. 0:2:4).').draw()
	return

### Dialog for deleting a SCSI device
def del_scsi_menu(devices):
	menu = Dialog_menu('Select the device you would like to delete')
	### Populate the menu
	for i in range(len(devices)):
		menu.add(str(i+1),devices[i])
	menu.add('Done','Done')
	### Return volume index + 1 (for looks and to avoid 0 = cancel conflict)
	return menu

### Dialog for adding a SCSI device
def scsi_add(cfg):
	done = 0
	while (not done):
		ask = Dialog_inputbox('Plese specify a SCSI device\n\
		Pattern: host:channel:id\nExample: 0:0:1')
		new_scsi = ask.draw()
		scsi_regex = re.compile("^.:.:.$")
		if (new_scsi == 0):
			return
		elif (not new_scsi):
			return
		elif (not scsi_regex.match(new_scsi)):
			Dialog_msgbox('Not a valid SCSI device path\n\n\
			Hint: Should match the pattern a:b:c\nExample: 0:0:1').draw()
			return
		else:
			 cfg.scsi_devs.append(new_scsi)
			 return
### Main SCSI device menu
def scsi_menu(devices):
	device_list = ""
	if (len(devices) > 0):
		device_list = "\n\nList of currently configured devices:"
		### Print a list of configured volumes
		for item in devices:
			device_list = device_list + '\n\t' + item
	response = Dialog_menu('MOL - SCSI device menu' + device_list)
	### Continue
	response.add('Done','Finished')
	### Auto probing of SCSI devices
	response.add('Auto','Enable auto probing of SCSI devices')
	### Add a new device
	response.add('Add','Add a new SCSI device')
	### Option to delete previously entered SCSI devices
	if (len(devices) > 0):
		### Delete a device
		response.add('Delete','Delete a device')
	### Help prompt
	response.add('Help','Help')
	return response

###############################################################################
### Block device helper functions
###############################################################################

def blkdev_help():
	Dialog_msgbox('MOL - Volumes help\n\nMac-on-Linux exports volumes for\
	the guest OS to use.  A volume can be an entire disk (e.g. /dev/hda or\
	/dev/cdrom), a single partition (e.g. /dev/hda3), or an image file \
	(e.g. /home/user/mol.img).  Also, there are a number of options which \
	can be specified for each volume, including whether MOL should boot \
	from it, whether the volume is writeable, and if the volume is a \
	CD-ROM (which is usefull for the --cdboot option).  Unformatted \
	volumes will require the -force option (which is found under advanced \
	options).  You must specify at least one volume.').draw()
	return

def ro_dialog():
	read = Dialog_menu('Is the volume writable?')
	### Read-write
	read.add('rw','Read-Write')
	### Read only
	read.add('ro','Read-Only')
	return read

def del_blkdev_menu(volumes):
	menu = Dialog_menu('Select the volume you would like to delete')
	### Populate the menu
	for i in range(len(volumes)):
		menu.add(str(i+1),volumes[i][0])
	menu.add('Done','Done')
	### Return volume index + 1 (for looks and to avoid '0' conflict)
	return menu

def edit_bootflags_menu(device):
	opts = ""
	for option in device[1:]:
		opts = opts + " -" + option
	### Draw the menu
	menu = Dialog_menu('MOL - Edit volume options\n\nSelected volume:\n\
	\t' + device[0] + opts +'\n\nToggle which option?')
	### List boot options
	menu.add('boot','Boot from this volume')
	menu.add('cd','This is a CD-ROM device')
	menu.add('ro','This volume is read-only')
	menu.add('rw','This volume is writeable')
	menu.add('force','Force MOL to use this volume (required if the volume is unformatted)')
	menu.add('whole','Export the whole device to MOL')
	menu.add('boot1','Boot from this volume above any others')
	menu.add('Done','Done')
	return menu

def edit_bootflags(cfg,device):
	done = 0
	while (not done):
		boot_menu = edit_bootflags_menu(device)
		sel = boot_menu.draw()
		### Cancel escpaes the function
		if (sel == 0):
			return
		### End the loop
		elif (sel == 'Done'):
			done = 1
		### Toggle the selected bootflag
		else:
			cfg.edit_bootflag(device,sel)

def edit_blkdev_menu(volumes):
	menu = Dialog_menu('Select which volume you want to edit')
	### Populate menu with a list of devices and options
	for d in range(len(volumes)):
		opts = ""
		for option in volumes[d][1:]:
			opts = opts + " -" + option
		menu.add(str((d+1)),str(volumes[d][0])+ opts)
	menu.add('Done','Done')
	return menu

def edit_blkdev(cfg,volumes):
	done = 0
	while (not done):
		edit_menu = edit_blkdev_menu(volumes)
		sel = edit_menu.draw()
		### Bail if user cancles
		if (sel == 0):
			return
		### All done
		if (sel == 'Done'):
			done = 1
		### Send selected device to have its bootflags tweaked (ouch!)
		else:
			index = (int(sel)-1)
			edit_bootflags(cfg,volumes[index])

def blkdev_menu(volumes):
	device_list = ""
	if (len(volumes) > 0):
		device_list = "\n\nList of currently configured volumes and options:"
		### Print a list of configured volumes
		for item in volumes:
			opts = ""
			for option in item[1:]:
				opts = opts + " -" + option
			device_list = device_list + '\n\t' + item[0] + opts
	response = Dialog_menu('MOL - Add volume menu' + device_list)
	### Add a new device
	response.add('Add','Add a new volume')
	### Option to edit or delete previously entered block devices
	if (len(volumes) > 0):
		### Edit a device's boot flags
		response.add('Edit',"Edit a volume's options")
		### Delete a device
		response.add('Delete','Delete a volume')
	### Help prompt
	response.add('Help','Help')
	### Continue
	response.add('Done','Finished')
	return response

def add_blkdev(cfg):
	track = 0
	blk_dev=[]
	### Addition dialog
	while (track == 0):
		### TODO Image creation
		try:
			blk_dev_p = Dialog_inputbox("Please specify a volume's path")
			blk_dev.append(str(blk_dev_p.draw()))
			if (not blk_dev):
				Dialog_msgbox('You must specify a volume').draw()
			elif (blk_dev == 0):
				return
			else:
				if (not os.path.exists(blk_dev[0]) \
				and Dialog_yesno('Path does not exist. Proceed anyway?').draw() == 0):
					return
				track += 1
		except:
			Dialog_msgbox('Not a valid path').draw()
	### Boot device?
	if (Dialog_yesno('Boot from this volume?').draw() != 0):
		blk_dev.append('boot')
	### CD-Rom?
	if (Dialog_yesno('Is this a CD device?').draw() != 0):
		blk_dev.append('cd')
		blk_dev.append('ro')
	else:
		### Writeable media?
		read_prompt = ro_dialog()
		blk_dev.append(str(read_prompt.draw()))
	### Configure adavanced options?
	if (Dialog_yesno('Would you like to configure advanced options for this volume?').draw() != 0):
		### FIXME Help dialog for each of these
		### Force
		if (Dialog_yesno('Force MOL to use this volume?\n\
		(required for unformatted volumes)').draw() != 0):
			blk_dev.append('force')
		### Whole
		if (Dialog_yesno('Export the entire device?').draw() != 0):
			blk_dev.append('whole')
		### Boot1?
		if (Dialog_yesno('Force MOL to boot from this volume?\n\
		(in spite of other boot options)').draw() != 0):
			blk_dev.append('boot1')
	cfg.volumes.append(blk_dev)	

###############################################################################
### Create Guest OS configuation
###############################################################################

def mol_cfg_os(type):
	### Create a OS class handler 
	mol_cfg = MOL_OS()
	### OS type passed as argument && and set Fancy name
	mol_cfg.type = type
	step = 0
	if (mol_cfg.type == 'osx'):
		mol_cfg.fancy = "Mac OS X"
	elif (mol_cfg.type == 'macos'):
		mol_cfg.fancy = "Mac OS Classic"
	elif (mol_cfg.type == 'linux'):
		mol_cfg.fancy == "Linux"
	
	### Configuration needs a name
	while step == 0:
		name_prompt = Dialog_inputbox('Name this configuation')
		mol_cfg.name = str(name_prompt.draw())
		### FIXME More complex regex
		name_regex = re.compile("/")
		### Bail on cancel
		if (mol_cfg.name == '0'):
			return
		### Check for invalid paths (/var/lib/mol/profiles)
		elif (name_regex.search(mol_cfg.name)):
			Dialog_msgbox('Not a valid configuration name:\n%s' % mol_cfg.name).draw()
		elif (len(mol_cfg.name) > 0):
			step +=1
		else:
			Dialog_msgbox('You must specify a configuaration name').draw()
	
	### Guest OS RAM (MB)
	### MAYBE Add help
	while step == 1:
		try:
			ram_prompt = Dialog_inputbox('RAM (MB)')
			raw_ram = int(ram_prompt.draw())
			if raw_ram == 0:
				return
			elif (len(str(raw_ram)) > 0):
				### The MAC OS X bootloader requires 128MB
				if (mol_cfg.type == 'osx' and raw_ram < 128):
					Dialog_msgbox('OS X requires at least 128 MB of RAM.').draw()
				else:
					mol_cfg.ram = str(raw_ram)
					step += 1
			else:
				Dialog_msgbox('You must specify the amount of RAM').draw()
		except ValueError:
			Dialog_msgbox('Invalid RAM value').draw()
	
	### Give option to disable AltiVec
	### FIXME Add help
	if (Dialog_yesno('Disable AltiVec?').draw() == 0):
		mol_cfg.altivec = "no"
	else:
		mol_cfg.altivec = "yes"
	
	### Enable USB suppot
	### FIXME Add help
	if (Dialog_yesno('Enable USB?').draw() == 0):
		mol_cfg.usb = "no"
	else:
		mol_cfg.usb = "yes"
	
	### Add block devices to the config
	while step == 2:
		volumes_menu = blkdev_menu(mol_cfg.volumes)
		sel = volumes_menu.draw()
		if (sel == 0):
			return
		elif (sel == "Done"):
			if (len(mol_cfg.volumes) > 0):
				step += 1
			else:
				Dialog_msgbox('You must specify at least one device.').draw()
		elif (sel == "Add"):
			### Add new device to the list
			add_blkdev(mol_cfg)
		### Edit the block devices
		elif (sel == "Edit"):
			edit_blkdev(mol_cfg,mol_cfg.volumes)
		### Delete the block device
		elif (sel == "Delete"):
			### -1 to get the proper array index
			index = (int(del_blkdev_menu(mol_cfg.volumes).draw())-1)
			### Use the class function for removing a block device
			mol_cfg.volumes.pop(index)
		### Help display
		elif (sel == "Help"):
			blkdev_help()
	
	### Generic SCSI CD (Mac OS only)
	###  FIXME Add Help
	if (mol_cfg.type == 'macos'):
		if (Dialog_yesno('Use Generic SCSI for CDs?').draw() != 0):
			mol_cfg.gen_scsi_cd == 'yes'
		else:
			mol_cfg.gen_scsi_cd == 'no'
	
	### SCSI manual
	while (step == 3):
		s_menu = scsi_menu(mol_cfg.scsi_devs)
		sel = s_menu.draw()
		if (sel == 0):
			return
		elif (sel == 'Auto'):
			mol_cfg.auto_scsi = "yes"
			step += 1
		elif (sel == 'Done'):
			mol_cfg.auto_scsi = "no"
			step += 1
		elif (sel == 'Add'):
			### Add new device to the list
			scsi_add(mol_cfg)
		elif (sel == 'Delete'):
			### -1 to get index of SCSI dev to be removed
			index = (int(del_scsi_menu(mol_cfg.scsi_devs).draw())-1)
			### Excise the SCSI device
			mol_cfg.scsi_devs.pop(index)
		elif (sel == 'Help'):
			scsi_help()
	
	### Specify a ROM (Mac OS only)
	if (mol_cfg.type == 'macos'):
		while (step == 4):
			r_menu = rom_menu()
			sel = r_menu.draw()
			if (sel == 0):
				return
			elif (sel == 'Done'):
				step += 1
			elif (sel == 'Help'):
				rom_help()
			elif (sel == 'Add'):
				rom_prompt = Dialog_inputbox('Please specify the path to your ROM:')
				nw_rom = rom_prompt.draw()
				if (os.path.exists(nw_rom)):
					mol_cfg.rom = nw_rom
					step += 1
				else:
					Dialog_msgbox("Error: The specified path does not exist!").draw()

	### Write it to the config file
	### Prepare the confirmation string
	config_string = []
	config_string.append("MOL - Guest OS configuration\n\nName:\t%s\n" % mol_cfg.name)
	config_string.append("Type:\t%s\n" % mol_cfg.fancy)
	config_string.append("RAM:\t%s MB\n" % mol_cfg.ram)
	config_string.append("Disable AltiVec:\t%s\n" % mol_cfg.altivec)
	config_string.append("Enable USB:\t%s\n" % mol_cfg.usb)
	config_string.append("Volumes:\n")
	for volume in mol_cfg.volumes:
		opts = ""
		for option in volume[1:]:
			opts = opts + ' -' + option
		config_string.append("\t" + volume[0] + opts + "\n")
	config_string.append("Enable SCSI autoprobing:\t%s\n" % mol_cfg.auto_scsi)
	if (mol_cfg.auto_scsi == 'no'):
		config_string.append("SCSI Devices:\n")
		if (len(mol_cfg.scsi_devs) > 0):
			for device in mol_cfg.scsi_devs:
				config_string.append("\t" + device + "\n")
		else:
			config_string.append("\tnone\n")
	### Generic SCSI for CDs (Mac OS <= 9 only)
	if (mol_cfg.type == 'macos'):
		config_string.append("Generic SCSI for CDs:\t%s\n" % mol_cfg.gen_scsi_cd)
	### New World ROM (Mac OS only)
	if (mol_cfg.type == 'macos' and mol_cfg.rom != ""):
		config_string.append("ROM:\t%s\n" % mol_cfg.rom)
	config_string.append("\n\nWrite this configuration?")
	### Join the array to create one huge string
	final_config_string = ''.join(config_string)
	if (Dialog_yesno(final_config_string).draw() != 0):
		try:
			mol_cfg.write()
			Dialog_msgbox('Config file written').draw()
		except:
			Dialog_msgbox('Error: config file not written').draw()
	else:
		return

###############################################################################
### Main loop
###############################################################################

def mol_cfg_dialog_init():
	mm = mol_dialog_main()

	while(1):
		result = mm.draw()

		if result == "Quit" or result == 0:
			sys.exit()
		### Create a new MOL machine configuation
		elif result == "Add":
			add = mol_dialog_add()	
			done = 0
			while(not done):
				result = add.draw()
				if result == "Back" or result == 0:
					done = 1
				elif result == "OS_X":
					mol_cfg_os('osx')
					done = 1
				elif result == "OS_9":
					mol_cfg_os('macos')
					done = 1
		
		### Adjust global mol settings
		elif result == "Configure":
			cfg = mol_dialog_config()	
			done = 0
			while(not done):
				result = cfg.draw()
				if result == "Back" or result == 0:
					done = 1

if __name__ == '__main__':
	mol_cfg_dialog_init()

### TODO:
# Use HELP texts from the mol_cfg_helper module
# Add lots of HELP text to the Guest OS configuration process
