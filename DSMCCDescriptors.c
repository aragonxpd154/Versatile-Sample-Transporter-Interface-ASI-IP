

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

descriptor carousel_identifier_descriptor
{
	descriptor_tag			8	uimsbf		eDecHex	0x13;
	descriptor_length		8	uimsbf;	// N1
	carousel_id			32	uimsbf;
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

calculation cSelectorByteLen
{
	fnBITLEN(selector) / 8
}

validation vSelectorByteLength
{
	selector_byte_length == 0x8
}

descriptor association_tag_descriptor
{
	descriptor_tag				8	uimsbf		eDecHex	0x14;
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

descriptor deferred_association_tags_descriptor
{
	descriptor_tag				8	uimsbf		eDecHex	0x15;
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

descriptor NPT_reference_descriptor
{
	descriptor_tag			8	uimsbf		eDecHex	0x17;
	descriptor_length		8	uimsbf;
	postDiscontinuityIndicator	1	bslbf;
	contentId			7	uimsbf;
	reserved			7	bslbf		eHidden	0x7F		vSet;
	STC_Reference		33	uimsbf;
	reserved			31	bslbf		eHidden	0x7FFFFFFF	vSet;
	NPT_Reference		33	uimsbf;
	scaleNumerator		16	uimsbf;
	scaleDenominator		16	uimsbf;
}

descriptor NPT_endpoint_descriptor
{
	descriptor_tag			8	uimsbf		eDecHex	0x18;
	descriptor_length		8	uimsbf;
	reserved			15	bslbf		eHidden	0x7FFF	vSet;
	startNPT			33	uimsbf;
	reserved			31	bslbf		eHidden	0x7FFFFFFF	vSet;
	stopNPT			33	uimsbf;
}

descriptor stream_mode_descriptor
{
	descriptor_tag			8	uimsbf		eDecHex	0x19;
	descriptor_length		8	uimsbf;
	streamMode			8	uimsbf		eStreamMode;
	reserved			8	bslbf		eHidden	0xFFFF	vSet;
}

descriptor stream_event_descriptor
{
    	descriptor_tag			8	uimsbf		eDecHex	0x1A;
    	descriptor_length		8	uimsbf;
    	eventId			16	uimsbf  	eDec;
    	reserved			31	bslbf		eHidden	0x7FFFFFFF	vSet;
	eventNPT			33	uimsbf;

    	loop private_data looplen(descriptor_length - 10)
    	{
        		private_data_byte	8	uimsbf;
    	}
}
