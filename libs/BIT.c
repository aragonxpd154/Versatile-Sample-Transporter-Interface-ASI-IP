

validation vBITLength
{
	(section_length == fnBITSUM(fNext) / 8) &&
	(section_length < 4094)
}

section_id BIT
{
	"Table ID 0x%2X Network ID 0x%4X Version %3d Section %3d [Pid %u (0x%X)]",f0,f5,f7,f9,pid,pid
}

table BIT
{
	__tableHeaderLong1("0xC4", "vBITLength", "network_id", "eDecHex")

	reserved_future_use			4	bslbf    	eHidden	0x7;
	first_descriptors_length        	12   	uimsbf		eDecHex	cDescriptors;
	
	loop descriptors looplen(first_descriptors_length)

	loop broadcaster_loop looplen(section_length - first_descriptors_length - 11)	
	{
  	    broadcaster_id			8    	uimsbf 	eBroadcastorID;
  	    reserved_future_use       	  	4    	bslbf		eHidden		0xF;
  	    broadcaster_descriptors_length  	12  	uimsbf		eDecHex	cDescriptors;
	    loop descriptors looplen(broadcaster_descriptors_length)
	}

	CRC_32                          32    	rpchof	eHex;
}
