

validation vCDTLength
{
	(section_length == fnBITSUM(fNext) / 8) &&
	(section_length < 4094) &&
	(section_length > 11)
}

section_id CDT
{
	"Table ID 0x%2X Download Data ID 0x%04X Version %3d Section %3d [Pid %u (0x%X)]",f0,f5,f7,f9,pid,pid
}

table CDT
{
	__tableHeaderLong1("0xC8", "vCDTLength", "download_data_id", "eDecHex")

	original_network_id			16	uimsbf		eDecHex;
	data_type				8	uimsbf		eDecHex;
	reserved_future_use			4	uimsbf		eHidden	0xF		vSet;
	descriptor_loop_length		12	uimsbf		eDecHex;
	
	loop descriptors looplen(descriptor_loop_length)

	loop data_module_bytes looplen (section_length - descriptor_loop_length - 14)
	{
		logo_type			8	uimsbf		eDecHex;
		reserved_for_future_use	7	bslbf		eHidden;
		logo_id				9	uimsbf		eDecHex;
		reserved_for_future_use	4	bslbf		eHidden;
		logo_version			12	uimsbf		eDecHex;
		data_size			16	uimsbf		eDecHex;

		rawbytes length (data_size)
	}

	CRC_32                          		32    	rpchof		eHex;
}
