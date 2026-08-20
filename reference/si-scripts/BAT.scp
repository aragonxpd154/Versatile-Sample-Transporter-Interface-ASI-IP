

calculation cBATTransportLen
{
	fnBITLEN(transport_stream_loop) / 8
}

validation vBATLength
{
	(section_length == fnBITSUM(fNext) / 8) &&
	(section_length < 1022) &&
	(section_length > 12)
}

section_id BAT
{
	"Bouquet %5d Version %3d Section %3d [Pid %u (0x%X)]",f5,f7,f9,pid,pid
}

table BAT
{
	__tableHeaderLong1("0x4A", "vBATLength", "bouquet_id", "eBouquetId")

	reserved_future_use				4    	bslbf		eHidden		0xF		vSet;
	bouquet_descriptors_length			12    	uimsbf		eDec			cDescriptors;
	loop descriptors looplen(bouquet_descriptors_length)

	reserved_future_use				4    	bslbf		eHidden		0xF		vSet;
	
	transport_stream_loop_length		12    	uimsbf		eDec			cBATTransportLen;
	loop transport_stream_loop looplen(transport_stream_loop_length)
	{
		transport_stream_id			16    	uimsbf		eDecHex;
		original_network_id			16    	uimsbf		eDecHex;
		reserved_future_use			4    	bslbf		eHidden		0xF		vSet;

		transport_descriptors_length		12    	uimsbf		eDec			cDescriptors;
		loop descriptors looplen(transport_descriptors_length)
	}	
		
	CRC_32					32    	rpchof		eDecHex;
}
