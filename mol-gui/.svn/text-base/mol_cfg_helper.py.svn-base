#!/usr/bin/python

###############################################################################
### Provide a backend for different frontends

import os
from commands import getoutput
from threading import Thread

### Class to obtain information about the MOL environment
class MOLRC:
	def __init__(self):
		# Initialize needed variables
		self.libdir = self.molrcget('-p')
		self.bindir = self.libdir + "/bin/"
		self.moddir = self.libdir + "/modules"
		self.version = self.molrcget('-V')
		self.kernel = getoutput(self.bindir + 'mol_uname -p')
		self.os = getoutput('uname')
		self.mods = getoutput(self.bindir + 'mol_uname -l ' + self.moddir + ' /usr/local/lib/mol/' + self.version + '/modules /lib/modules /usr/lib/mol/' + self.version + '/modules')
		self.modules = self.mods.splitlines()
		self.fbdev_prefs = self.molrcget('-F fbdev_prefs')
		self.mol_bin = self.bindir + 'mol'
		self.mol_dbg = self.bindir + 'moldeb'
		self.lockfile = self.molrcget('-L')

	### molrcget function
	def molrcget(self, arg):
		return getoutput('molrcget ' + arg)


### Tracking mol sessions
### To be called by the UI to keep track of mol's activities
class TRACK_MOL:
	def __init__(self):
		### Keep track of which MOL sessions are available 
		self.sessions = [0, 1, 2, 3, 4, 5, 6, 7]
		self.threads = []
	def checkout(self):
		### Take a number
		try:
			return self.sessions.pop()
		except:
			return -1 

	def checkin(self, mol_session):
		### Return a number to the pool
		self.sessions.append(mol_session)

### Booting
class BOOT_MOL(Thread):
	### Boot MOL in a thread so GUI loop continues
	### Import MOL tracker class object, id for this mol session, guest os
	### type
	def __init__(self, molargs=None):
		Thread.__init__(self)
		# These are the options which will be passed directly to the binary
		# Each option is separated by a space like on the CLI
		# e.g. --cdboot -X -5
		self.molargs = molargs
		# Obtain information about mol environment
		self.molrc = MOLRC()
		### Star MOL as a daemon so that quitting GUI does not kill it
		### Once we can kill mol sessions externally, change this
		self.setDaemon(1)	

	def run(self):
		self.mol_exec_string = (self.molrc.mol_bin + self.molargs)
		print "Starting MOL: %s" % self.mol_exec_string
		os.system(self.mol_exec_string)
		
### Volumes
class MOL_Volume:
	def __init__(self, path):
		### Device Path
		self.path = path
		### Read Only?
		self.ro = 0
		### Force usage of this device
		self.force = 0
		### Boot from this device
		self.boot = 0
		### Export the whole disk
		self.whole = 0
		### CD/DVD ROM Device?
		self.cd = 0
		### Print Header
		self.verbose = 1
		### Help Text
		self.help = {
			"path":"The path can be a complete disk (/dev/hda), a single partition (/dev/sda2) or a disk image (image.dmg).",
			"ro":"This disk is marked read only and cannot be written to.",
			"force":"Force the use of this device even if it's not detected as being a usable device.  (Be careful!)",
			"boot":"Boot from this device",
			"whole":"Export the whole disk.  (Be careful!)",
			"cd":"This device is an optical device",
			"header":"Volumes are specified through the blkdev keyword:\n\n\tblkdev:\t<path>\t<flags>\n"
		}


###############################################################################
### Video Configuration
class MOL_Video:
	def __init__(self):
		### Width
		self.width = 0
		### Height
		self.height = 0
		### Refresh Rate
		self.refresh = 0
		### Depth
		self.depth = 0

### Drivers
class MOL_Video_X(MOL_Video):
	pass
	
class MOL_Video_FB(MOL_Video):
	pass

class MOL_Video_XDGA(MOL_Video):
	pass

class MOL_Video_VNC(MOL_Video):
	pass

###############################################################################
### Network Configuration
class MOL_Net:
	def __init__(self):
		pass
	
### Drivers
class MOL_Net_TUN(MOL_Net):
	pass
class MOL_Net_Sheep(MOL_Net):
	pass


###############################################################################
### Sound
class MOL_Sound:
	def __init__(self):
		pass

### Drivers
class MOL_Sound_ALSA(MOL_Sound):
	pass

class MOL_Sound_OSS(MOL_Sound):
	pass

### OSX Driver
class MOL_Sound_CoreAudio(MOL_Sound):
	pass

### No sound support
class MOL_Sound_Null(MOL_Sound):
	pass


###############################################################################
### OS Configuration
class MOL_OS:
	def __init__(self):	
		### OS Name - Used for config file naming
		self.name = "Unknown"
		### OS Type (osx, macos, linux) - Used to customize config file for guest OS
		self.type = "osx"
		### Formal name of Guest OS
		self.fancy = "Mac OS X"
		### MB RAM assigned to this configuration
		self.ram = 256
		### Altivec Enabled?
		self.altivec = 0
		### Disk Volumes to be mounted 
		self.volumes = []
		### Autoprobe SCSI? (USB/Firewire are SCSI)
		self.auto_scsi = 0
		### If !self.auto_scsi, export devices
		self.scsi_devs = []
		### Generic SCSI for CDs (Mac OS only)
		self.gen_scsi_cd = "yes"
		### ROM support for Mac OS
		self.rom = "" 
		### Enable USB support
		self.usb = 1
		### PCI Proxy? (Only works with PIO) - ADVANCED
		self.pciproxy = 0
		### PCI Proxy Devices
		self.pciproxy_devs = []
		### Shared?
		self.shared = 1

		### Help Text
		self.help = {
			"include":"Other config files to include with this config are listed here",
			"ram_size":"Total amount of RAM to use with the virtualized instance, for best performance, use less than your physical ram",
			"disable_altivec":"Disable AltiVec, useful for machines without AltiVec support",
			"autoprobe_scsi":"MOL will automatically scan for SCSI devices if this is enabled",
			"enable_usb":"Add support for USB devices unclaimed by the kernel, requires usbfs support",
		}


	### TODO: decide whether OS type has a different write function or not
	def write(self):
		buffer = ["#  Mac-on-Linux master configuration file for Guest OS booting\n"]
		buffer.append("# Configuration name: " + self.name + "\n")
		buffer.append("include\t\t${etc}/molrc.video\ninclude\t\t${etc}/molrc.input\ninclude\t\t${etc}/molrc.net\ninclude\t\t${etc}/molrc.sound\n")
		### Start adding options to the config
		### RAM
		buffer.append("ram_size:\t\t%s\n" % self.ram)
		### AltiVec
		buffer.append("disable_altivec:\t\t%s\n" % self.altivec)
		### USB support
		buffer.append("enable_usb:\t\t%s\n" % self.usb)
		### Autoprobe SCSI
		buffer.append("autoprobe_scsi:\t\t%s\n" % self.auto_scsi)
		### SCSI devices
		for device in self.scsi_devs:
			buffer.append("scsi_dev:\t\t%s\n" % device)
		### Generic SCSI CD (Mac OS <= 9)
		if self.type == 'macos':
			buffer.append("generic_scsi_for_cds:\t\t%s\n" % self.gen_scsi_cd)
		### Block devices
		for device in self.volumes:
			dev_path = device[0]
			dev_opts = ""
			for option in device[1:]:
				dev_opts = dev_opts + " -" + option
			buffer.append("blkdev:\t\t" + dev_path + dev_opts + "\n")
		### New_world ROM (Mac OS only)
		if self.type == 'macos' and self.rom != "":
			buffer.append("newworld_rom:\t\t%s\n" % self.rom)
		### Open and write the file
		### TODO: need error handling here
		if self.shared:
			config_file = open('/etc/mol/profiles/' + self.name + '.' + self.type,'w')
			for line in buffer:
				config_file.write(line)
			config_file.close()
		else:
			config_file = open(gethome() + '/.mol/' + self.name + '.' + self.type,'w')
			for line in buffer:
				config_file.write(line)
			config_file.close()
			
	### Alter a boot option on a volume
	def edit_bootflag(self,device,flag):
		index = self.volumes.index(device)
		if flag in self.volumes[index]:
			### Unset the flag
			self.volumes[index].remove(flag)
			### Make sure either ro or rw is set
			if (flag == 'ro'):
				self.volumes[index].append('rw')
			elif (flag == 'rw'):
				self.volumes[index].append('ro')
		else:
			### Add the flag to the device
			self.volumes[index].append(flag)
			### Don't allow ro and rw to exist
			if (flag =='ro' and 'rw' in self.volumes[index]):
				self.volumes[index].remove('rw')
			elif (flag == 'rw' and 'ro' in self.volumes[index]):
				self.volumes[index].remove('ro')


### MOL Default configuration
	### Object for configuration

### Open the molrc.conf file (import settings)
	### Open the system default

### Save the molrc.conf file
	### Save to system default

### Add a Boot Option
	### Add a nickname
	### (GUI) Add an Icon

### Remove a Boot Option
	### Remove that option

### Configure a Boot Option
	### OS Select (OS9/OSX/Linux/etc.)
	### RAM
	### Disks
	### etc.


### Get a list of OSes to Boot
def mol_list_os():
	bootable_oses = []
	shared_configs = os.listdir('/etc/mol/profiles')
	### Shared configurations
	for config in shared_configs:
		### Split into name, type
		path = '/etc/mol/profiles/' + config
		new_os = config.split('.')
		### Absolute path of profile
		new_os.append(path)
		### Add to list of bootable OSes
		bootable_oses.append(new_os)
	### Personal configurations
	home = gethome()
	user_configs = os.listdir(home + '/.mol')
	for conf in user_configs:
		### Name and type
		home_path = home + '/.mol/' + conf
		new_os = conf.split('.')
		### Absolute path of profile
		new_os.append(home_path)
		bootable_oses.append(new_os)
	### Return list of bootable OSes to the UI
	return bootable_oses

# Returns a string of the user's home direcotry
def gethome():
	return str(getoutput('echo $HOME'))

### Initialize ~/.mol directory
home = gethome()
if not os.path.exists(home + '/.mol'):
	os.mkdir(home + '/.mol')
