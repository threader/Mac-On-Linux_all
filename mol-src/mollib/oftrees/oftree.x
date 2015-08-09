# -*- sh -*-***************
#  OF Tree for MacOS X
# *************************

#
# **************** NODE device-tree ***************
#
{
    (mol_phandle 0xFF83A248 )
    (property name               <str>  	"device-tree" )
    (property device_type        <str>  	"bootrom" )
    (property compatible         <str>  	"AAPL,MOL", "MacRISC", "Power Macintosh" )
    (property model              <str>  	"AAPL,MOL" )
    (property pci-OF-bus-map     <word>
        0x000000ff 0xffffffff 0xffffffff 0xffffffff 
        0xffffffff 0xffffffff 0xffffffff 0xffffffff 
        0xffffffff 0xffffffff 0xffffffff 0xffffffff 
        0xffffffff 0xffffffff 0xffffffff 0xffffffff 
        0xffffffff 0xffffffff 0xffffffff 0xffffffff 
        0xffffffff 0xffffffff 0xffffffff 0xffffffff 
        0xffffffff 0xffffffff 0xffffffff 0xffffffff 
        0xffffffff 0xffffffff 0xffffffff 0xffffffff 
        0xffffffff 0xffffffff 0xffffffff 0xffffffff 
        0xffffffff 0xffffffff 0xffffffff 0xffffffff 
        0xffffffff 0xffffffff 0xffffffff 0xffffffff 
        0xffffffff 0xffffffff 0xffffffff 0xffffffff 
        0xffffffff 0xffffffff 0xffffffff 0xffffffff 
        0xffffffff 0xffffffff 0xffffffff 0xffffffff 
        0xffffffff 0xffffffff 0xffffffff 0xffffffff 
        0xffffffff 0xffffffff 0xffffffff 0xffffffff  )
    (property clock-frequency    <word> 	0x03F7EC51 )
    (property #size-cells        <word> 	0x00000001 )
    (property #address-cells     <word> 	0x00000001 )
    (property system-id          <str>  	"0000000000000" )
    (property copyright          <str>  	
        "Copyright 1983-1999 Apple Computer, Inc. All Rights Reserved" )
    (property color-code	 <byte>        0xFF 0x06  )
    (property customer-sw-config <str>  	"           " )
    (property serial-number <byte> 
        0x48 0x5A 0x53 0x4D 0x37 0x37 0x31 0x36 0x53 0x2F 0x41 0x00 
        0x00 0x55 0x56 0x30 0x30 0x37 0x34 0x50 0x41 0x00 0x00 0x00 
        0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 
        0x00 0x00 0x00 0x00 0x00 0x00 0x00  )

    #
    # **************** NODE cpus ***************
    #
    {
        (property name               <str>  	"cpus" )
        (mol_phandle 0xFF83B2F0 )
        (property #size-cells        <word> 	0x00000000 )
        (property #address-cells     <word> 	0x00000001 )
    
	#
	# **************** NODE cpu ***************
	# Placeholder for the imported CPU node

	{
	    (property name           <str>  	"dummy_cpu" )
	    (property device_type    <str>  	"cpu" )
	}
    }

    #
    # **************** NODE chosen ***************
    #
    {
        (property name               <str>  	"chosen" )
        (mol_phandle 0xFF83C3A0 )
        (property interrupt-controller <word> 	0x00001234 )
        (property cpu                <word> 	0xFF9DD640 )
        (property bootargs           <str>  	"" )
        (property pmu                <word> 	0xFF9CF280 )
        (property nvram              <word> 	0xFF9CF400 )
        (property mmu                <word> 	0xFF9DD640 )
        (property memory             <word> 	0xFF9DD700 )
        (property stdout             <word> 	0xFF9CE5C0 )
        (property stdin              <word> 	0xFF9CEC00 )
    }

    #
    # **************** NODE memory ***************
    #
    {
        (mol_phandle 0xFF83C530 )
        (property name               <str>  	"memory" )
        (property device_type        <str>  	"memory" )

        (property available          <word>
            0x00003000 0x001fd000 0x00220000 0x000e0000 
            0x00400000 0x00c00000 0x01400000 0x0aa00000  )
        (property slot-names <byte> 
            0x00 0x00 0x00 0x03 0x44 0x49 0x4D 0x4D 0x30 0x2F 0x42 0x55 
            0x49 0x4C 0x54 0x2D 0x49 0x4E 0x00 0x44 0x49 0x4D 0x4D 0x31 
            0x2F 0x4A 0x31 0x32 0x00  )
        (property reg                <word>
            0x00000000 0x04000000 0x04000000 0x08000000  )
    }

    #
    # **************** NODE openprom ***************
    #
    {
        (mol_phandle 0xFF83C748 )
        (property name               <str>  	"openprom" )
        (property device_type        <str>  	"BootROM" )
        (property model              <str>  	"OpenFirmware 3" )
        (property boot-syntax        <word> 	0x00000001 )
        (property supports-bootinfo  <word>     )
        (property relative-addressing <word>    )
        
        #
        # **************** NODE client-services ***************
        #
        {
            (mol_phandle 0xFF83C878 )
            (property name               <str>  	"client-services" )
        }
    }
    #
    # **************** NODE options ***************
    #
    {
        (mol_phandle 0xFF83DDA0 )
        (property name               <str>  	"options" )
        (property boot-args          <word>       )
        (property aapl,pci           <word>
            0x2f6f6666 0x73637265 0x656e2d64 0x6973706c 0x6179ff01 
            0x67707266 0xff01ff81 0x01070101 0xff8104ff 0x012f4066 
            0x32303030 0x3030302f 0x4031372f 0x40663330 0x30ff0162 
            0x6b6c74ff 0x011f07ff 0x81010aff 0x8181ff81 0x03ff012f 
            0x40663030 0x30303030 0x302f4031 0x30ff0141 0x444d53ff 
            0x01ff8101 0x2a2aff81 0x81ff8101 0x8214ff81 0x01ff0100  )
        (property boot-script        <str>  	"" )
        (property default-router-ip  <str>  	"" )
        (property default-subnet-mask <str>  	"" )
        (property default-gateway-ip <str>  	"" )
        (property default-server-ip  <str>  	"" )
        (property default-client-ip  <str>  	"" )
        (property boot-command       <str>  	"mac-boot" )
        (property oem-logo           <str>  	"" )
        (property oem-banner         <str>  	"" )
        (property mouse-device       <str>  	"mouse" )
        (property output-device-1    <str>  	"scca" )
        (property input-device-1     <str>  	"scca" )
        (property output-device      <str>  	"screen" )
        (property input-device       <str>  	"keyboard" )
        (property diag-file          <str>  	",diags" )
        (property diag-device        <str>  	"enet" )
        (property console-screen     <str>  	"" )
        (property boot-screen        <str>  	"" )
        (property boot-file          <str>  	"" )
        (property boot-device        <str>  	"" )
        (property selftest-#megs     <str>  	"0" )
        (property screen-#rows       <str>  	"40" )
        (property screen-#columns    <str>  	"100" )
        (property pci-probe-mask     <str>  	"-1" )
        (property virt-size          <str>  	"-1" )
        (property virt-base          <str>  	"-1" )
        (property load-base          <str>  	"0x800000" )
        (property real-size          <str>  	"-1" )
        (property real-base          <str>  	"-1" )
        (property default-mac-address? <str>  	"false" )
        (property use-generic?       <str>  	"false" )
        (property use-nvramrc?       <str>  	"false" )
        (property oem-logo?          <str>  	"false" )
        (property oem-banner?        <str>  	"false" )
        (property fcode-debug?       <str>  	"false" )
        (property diag-switch?       <str>  	"false" )
        (property auto-boot?         <str>  	"true" )
        (property real-mode?         <str>  	"false" )
        (property little-endian?     <str>  	"false" )
    }

    #
    # **************** MOL nvram ***************
    #
    {
        (mol_phandle 0xFF8932C0 )
        (property name               <str>  	"mol-nvram" )
        (property device_type        <str>  	"mol-nvram" )
        (property #bytes             <word> 	0x00002000 )
        (property reg                <word>
            0xfff04000 0x00004000  )
    }

    #
    # **************** NODE osi-pic (mol) ***************
    #
    {
	(property name			<str>	"osi-pic" )
	(property device_type		<str>	"interrupt-controller" )
	(mol_phandle				0x00001234 )
	(property at-boot			)
	(property interrupt-controller	<word>	)
	(property #interrupt-cells	<word>	0x00000001 )
    }

    #
    # ####################### MOL keyboard ############################
    #
    {
	(property name			<str>	"mol-keyboard" )
	(property device_type		<str>	"mol-keyboard" )
	(property interrupt-parent	<word>	0x00001234 )
	(property interrupts		<word>	0x00000003 )
    }

    #
    # ####################### MOL audio ############################
    #
    {
	(property name			<str>	"mol-audio" )
	(property device_type		<str>	"sound" )
	(property interrupt-parent	<word>	0x00001234 )
	(property interrupts		<word>	0x00000001 )
    }


    #
    # ####################### MOL mouse ############################
    #
    {
	(property name			<str>	"mol-mouse" )
	(property device_type		<str>	"mol-mouse" )
	(property interrupt-parent	<word>	0x00001234 )
	(property interrupts		<word>	0x00000004 )
    }

    #
    # **************** NODE mol-enet ***************
    #
    {
	(property name			<str>	"mol-enet" )
	(property device_type		<str>	"mol-enet" )
	(property interrupt-parent	<word>	0x00001234 )
	(property interrupts		<word>	0x00000005 )
    }

    #
    # ####################### MOL blk device ##########################
    #
    {
	(property name			<str>  	"mol-blk" )
	(property interrupt-parent	<word>	0x00001234 )
	(property interrupts		<word>	0x00000002 )
	#unit_string == channel
	(unit_string 0 )
	#(property channel		<word>	0x00000000 )
	{
	    (unit_string * )
	    (property name	     <str>	"disk" )
	    (property device_type    <str>	"block" )
	}
    }

    #
    # ####################### MOL SCSI ############################
    #
    {
	(property name			<str>	"mol-scsi" )
	(property device_type		<str>	"scsi" )
	(property interrupt-parent	<word>	0x00001234 )
	(property interrupts		<word>	0x00000006 )
	{
	    (unit_string * )
	    (property name	     <str>	"disk" )
	    (property device_type    <str>	"block" )
	}
    }


    #
    # ####################### NODE mac-io ##########################
    #  NOTE: This dummy node is necessary (the kernel panics otherwise)
    {
       (mol_phandle 0xFF898040 )
       (property name               <str>      "mac-io" )
       (property device_type        <str>      "mac-io" )

       (property assigned-addresses <word>
            0x8200b810 0x00000000 0x80000000 0x00000000 0x00080000  )
       (property model              <str>      "AAPL,Keylargo-DUMMY" )
    }

    #
    # **************** NODE pci ***************
    #
    {
        (property name               <str>  	"pci" )
        (property device_type        <str>  	"pci" )

	# Map 0x08 (device  1) to IRQ 24 (0x18) (usb)
	# Map 0x80 (device 16) to IRQ 23 (0x17) (mol-video)

        (property interrupt-map      <word>
            0x00008000 0x00000000 0x00000000 0x00000000 0x00001234 0x00000017
            0x00000800 0x00000000 0x00000000 0x00000000 0x00001234 0x00000018  )
        (property interrupt-map-mask <word>
            0x0000f800 0x00000000 0x00000000 0x00000000  )
	(property interrupt-parent   <word>	0x00001234 )

        (property bus-range          <word>	0x00000000 0x00000000  )
        (property ranges             <word>
            0x01000000 0x00000000 0x00000000 0xfe000000 0x00000000 0x00800000 
            0x02000000 0x00000000 0x00000000 0xfd000000 0x00000000 0x01000000 
            0x02000000 0x00000000 0x80000000 0x80000000 0x00000000 0x7d000000  )
        (property bus-master-capable <word> 	0x00012000 )
        (property slot-names         <word>	0x00010000 0x4a313200  )
        (property clock-frequency    <word> 	0x01FCA055 )
        (property #interrupt-cells   <word> 	0x00000001 )
        (property #size-cells        <word> 	0x00000002 )
        (property #address-cells     <word> 	0x00000003 )
        (property reg                <word>	0x80000000 0x7f000000  )
        (property used-by-rtas       <word>	)
        (property built-in           <word>	)
        (property compatible         <str>  	"grackle" )
        (property model              <str>  	"MOT,MPC106" )

        #
        # **************** NODE usb ***************
        # /pci@f2000000/usb@1b
        #
        {
            (unit_string 1b )
            (mol_phandle 0xFF995AC8 )
            (property device_type        <str>  	"usb" )
            (property name               <str>  	"usb" )
            (property linux,phandle      <word> 	0xFF995AC8 )
            (property sleep-power-state  <str>  	"D3cold" )
            (property assigned-addresses <word>
                0x83000810 0x00000000 0x80082000 0x00000000 0x00001000  )
            (property compatible         <str>  	"pci1033,35", "pciclass,0c0310" )
            (property #size-cells        <word> 	0x00000000 )
            (property #address-cells     <word> 	0x00000001 )
            (property reg                <word>
                0x00000800 0x00000000 0x00000000 0x00000000 0x00000000 
                0x02000810 0x00000000 0x00000000 0x00000000 0x00001000  )
            (property devsel-speed       <word> 	0x00000001 )
            (property subsystem-id       <word> 	0x00000035 )
            (property subsystem-vendor-id <word> 	0x00001033 )
            (property max-latency        <word> 	0x0000002A )
            (property min-grant          <word> 	0x00000001 )
            (property interrupts         <word> 	0x00000001 )
            (property class-code         <word> 	0x000C0310 )
            (property revision-id        <word> 	0x00000043 )
            (property device-id          <word> 	0x00000035 )
            (property vendor-id          <word> 	0x00001033 )
            
            #
            # **************** NODE device ***************
            # /pci@f2000000/usb@1b/device@1
            #
            {
                (unit_string 1 )
                (mol_phandle 0xFF9DC8A0 )
                (property name               <str>  	"device" )
                (property linux,phandle      <word> 	0xFF9DC8A0 )
                (property #size-cells        <word> 	0x00000000 )
                (property #address-cells     <word> 	0x00000002 )
                (property assigned-address   <word> 	0x00000001 )
                (property reg                <word> 	0x00000001 )
                
                #
                # **************** NODE keyboard ***************
                # /pci@f2000000/usb@1b/device@1/keyboard@0
                #
                {
                    (unit_string 0 )
                    (mol_phandle 0xFF9DCA48 )
                    (property name               <str>  	"keyboard" )
                    (property device_type        <str>  	"keyboard" )
                    (property linux,phandle      <word> 	0xFF9DCA48 )
                    (property endpoints          <word> 	0x00080381 )
                    (property reg                <word>
                        0x00000000 0x00000001  )
                }
                #
                # **************** NODE mouse ***************
                # /pci@f2000000/usb@1b/device@1/mouse@1
                #
                {
                    (unit_string 1 )
                    (mol_phandle 0xFF9DCE08 )
                    (property device_type        <str>  	"mouse" )
                    (property name               <str>  	"mouse" )
                    (property linux,phandle      <word> 	0xFF9DCE08 )
                    (property endpoints          <word> 	0x00040382 )
                    (property #buttons           <word> 	0x00000001 )
                    (property reg                <word>
                        0x00000001 0x00000001  )
                }
            }
        }

    	#
	# ####################### MOL video ############################
	#
  	{
	    (mol_phandle 0x2000 )
	    (property name <str> 	"MacOnLinuxVideo" )
	    (property device_type <str> "display" )
	    (property model <str>	"yonk" )

	    (property AAPL,boot-display <word> 0x2000 )

            (property vendor-id	<word>	0x00006666 )
            (property device-id	<word>	0x00006666 )
            (property revision-id	<word>	0x00001011 )
	    (property interrupts 	<word> 	0x00000001 )
	    (property min-grant 	<word> 	0x0 )
	    (property max-latency 	<word> 	0x0 )
            (property class-code	<word>	0x00030000 )
	    (property devsel-speed <word> 	0x0 )

	    (property power-consumption <word> 
		0x00000000 0x00000000 0x007270e0 0x007270e0
		0x00000000 0x00000000 0x007b98a0 0x007b98a0 )

            (property reg <word>
		0x00008000 0x00000000 0x00000000 0x00000000 0x00000000 
		0x02008010 0x00000000 0x00000000 0x00000000 0x01000000 )

	    (property assigned-addresses <word>
		0x82008010 0x00000000 0x81000000 0x00000000 0x01000000 )
        }
    }
}
