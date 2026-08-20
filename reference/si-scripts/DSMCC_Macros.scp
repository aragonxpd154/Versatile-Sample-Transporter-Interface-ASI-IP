

enum eFormatId
{
	0x00			"standard boot"
	0x01			"enhanced boot"
	default			eHex
}

validation vObjectKeyLength
{
	fThis <= 4
}

calculation cSelectorByteLen
{
	fnBITLEN(selector) / 8
}

validation vSelectorByteLength
{
	selector_byte_length == 0x8
}

// tag = 0x01
macro type_descriptor
{
	descriptor_length	8		uimsbf;
	type			8		iso_latin	eNA eNA descriptor_length;
}

//tag = 0x02
macro name_descriptor
{
	descriptor_length	8		uimsbf;
	name			8		iso_latin	eNA eNA descriptor_length;
}

validation info_len 
{
	descriptor_length - 3
}

//tag = 0x03
macro info_descriptor
{
	descriptor_length	8		uimsbf;
	ISO_639_language_code	24	bslbf		eISOLatin;
	info			8		iso_latin	eNA eNA info_len;
}

// tag = 0x04
macro module_link_descriptor
{
	descriptor_length	8		uimsbf;
	position		8		uimsbf;
	module_id		16		uimsbf;
}

// tag = 0x05
macro CRC32_descriptor
{
	descriptor_length	8		uimsbf;
	CRC_32		32		uimsbf;
}


// tag = 0x06
macro location_descriptor
{
	descriptor_length	8		uimsbf;
	location_tag		8		uimsbf;
}

// tag = 0x07
macro est_download_time_descriptor
{
	descriptor_length	8		uimsbf;
	est_download_time	32		uimsbf;
}

// tag = 0x08
macro group_link_descriptor
{
	descriptor_length	8		uimsbf;
	Position		8		uimsbf;
	group_id		32		uimsbf;
}

// tag = 0x09
macro compressed_module_descriptor
{
	descriptor_length	8		uimsbf;
	compression_method		8	uimsbf;
	original_size		32		uimsbf;
}

// tag = 0x10
macro compressed_module_descriptor{
	descriptor_length 8 uimsbf;
	compression_method 8 uimsbf;
original_size 64;
}

// tag = 0x11
macro group_link_descriptor{
	descriptor_length 8 uimsbf;
	est_download_time 32 uimsbf;
	group_id 64;
}

// tag = 0x13
macro carousel_identifier
{
	descriptor_length		8	uimsbf;	// N1
	carousel_id			32	uimsbf;
    if (descriptor_length > 4)
    {
    	FormatID			8	uimsbf		eFormatId;
	    if(FormatID == 0x00)
	    {
		     loop private_data looplen(descriptor_length - 5) // N1 - 5
		    {
			    private_data_byte	8	uimsbf;
		    }
		    
	    }
	    if(FormatID == 0x01)
	    {
		    ModuleVersion 	8 	uimsbf;
		    ModuleId 		16 	uimsbf;
		    BlockSize 		16 	uimsbf;
		    ModuleSize 		32 	uimsbf;
			// ModuleSize 	64 uimsbf;
		    CompressionMethod 	8 	uimsbf;
		    OriginalSize 		32 	uimsbf;
		    TimeOut 		8 	uimsbf;
		    ObjectKeyLength 	8 	uimsbf		eDecHex eNA	vObjectKeyLength;  // N2 <=4
		    loop ObjectKey	looplen(ObjectKeyLength)
		    {
			    ObjectKeyData	8	uimsbf	eHex;
		    }
		     loop private_data looplen(descriptor_length - ObjectKeyLength - 21)
		    {
			    private_data_byte	8	uimsbf;
		    }
	    }
    }
}

// tag = 0x14
macro association_tag_macro
{

	descriptor_length			8	uimsbf;
	association_tag     			16	uimsbf		eDecHex;
	use					16	uimsbf		eHex;

	if (use == 0)
	{ 
		selector_byte_length		8	uimsbf		eDec		0x8	vSelectorByteLength;	// transaction_id and timeout
		transaction_id        		32	uimsbf		eHex;
		timeout			32	uimsbf		eHex;
	}
	else
	{ 
		selector_byte_length		8	uimsbf		eDec		cSelectorByteLen; 
		loop selector looplen(selector_byte_length)
		{
			selector_byte		8	uimsbf;
		}
	}

	loop private_data looplen(descriptor_length - 5 - selector_byte_length)
	{
		private_data_byte		8	uimsbf;
	}

}


// tag = 0x15
macro deferred_association_tags
{
	descriptor_length			8	uimsbf;
	association_tags_loop_length	8	uimsbf  	eDec		cNextByteLen;

	loop association_tags_loop looplen(association_tags_loop_length)
	{
		association_tag		16	uimsbf		eDecHex;
	}

	transport_stream_id			16	uimsbf		eHex;
	program_number			16	uimsbf;

	loop private_data looplen(descriptor_length - 5 - association_tags_loop_length)
	{
		private_data_byte		8	uimsbf;
	}
}

// Stream Event specific descriptors

// tag = 0x17

macro npt_reference
{
	descriptor_length		8	uimsbf;
	postDiscontinuityIndicator	1	bslbf;
	contentId			7	uimsbf;
	reserved			7	uimsbf		eDecHex	0x7F		vSet;
	STC_Reference		33	uimsbf;
	reserved			31	bslbf		eDecHex	0x7FFFFFFF	vSet;
	NPT_Reference_value	33	uimsbf;
	scaleNumerator		16	uimsbf;
	scaleDenominator		16	uimsbf;
}


// tag = 0x18
macro npt_endpoint
{
	descriptor_length		8	uimsbf;
	reserved			15	bslbf		eDecHex	0x7FFF	vSet;
	startNPT			33	uimsbf;
	reserved			31	bslbf		eDecHex	0x7FFFFFFF	vSet;
	stopNPT			33	uimsbf;
}

// tag= 0x19
macro stream_mode
{
	descriptor_length		8	uimsbf;
	streamMode			8	uimsbf		eStreamMode;
	reserved			8	bslbf		eDecHex	0xFF	vSet;
}

// tag = 0x1A
macro stream_event
{
    	descriptor_length		8	uimsbf;
    	eventId			16	uimsbf  	eDec;
    	reserved			31	bslbf		eDecHex	0x7FFFFFFF	vSet;
	eventNPT			33	uimsbf;

    	loop private_data looplen(descriptor_length - 10)
    	{
        		private_data_byte	8	uimsbf;
    	}
}

