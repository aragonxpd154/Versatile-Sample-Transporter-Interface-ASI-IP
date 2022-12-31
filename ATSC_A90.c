
#include "DB_Common.scp"

//
// Enums
//
enum eDataServiceProfile
{
	0x00		"reserved"
	0x01		"ATSC Data Broadcast Service Profile G1"
	0x02		"ATSC Data Broadcast Service Profile G2"
	0x03		"ATSC Data Broadcast Service Profile G3"
	0x04		"ATSC Data Broadcast Service Profile A1"
	default		"ATSC Reserved"
}

enum eDataServiceLevel
{
	0x00		"No synchronised stream in service"
	0x01		"Level 1: DBESn = 120120 bytes"
	0x04		"Level 4: DBESn = 480480 bytes"
	0x10		"Level 16: DBESn = 1921920 bytes"
	0x40		"Level 64: DBESn = 7687680 bytes"
	default		"ATSC reserved"
}

enum eAppIdDesc
{
	0x0000		"DASE application"
	0x0001..0x7FFF	"ATSC reserved"
	default			"User private"
}

enum eProtocolEncap
{
	0x00		"Not in a MPEG-2 Transport Stream"
	0x01		"Asynchronous non-flow controlled DSM-CC sections"
	0x02		"Non-streaming synchronised DSM-CC sections"
	0x03		"Asynchonous MPE datagrams using LLC/SNAP header"
	0x04		"Asynchronous IP datagrams"
	0x05		"Synchronised streaming data in PES"
	0x06		"Synchronous streaming data in PES"
	0x07		"Synchronised streaming MPE in PES"
	0x08		"Synchronous streaming MPE in PES"
	0x09		"Synchronised streaming IP in PES"
	0x0A		"Synchronous streaming IP in PES"
	0x0B		"Proprietary data piping"
	0x0C		"SCTE DVS 051 asynchronous protocol"
	0x0D		"Asynchonous carousel scenario in DSM-CC sections"
	0x0E		"Reserved"
	0x0F..0x7F	"ATSC reserved"
	default		"User defined"
}

enum eActionType
{
	0x00		"Run-time data"
	0x01		"Bootstrap data"
	0x02..0x3F	"ATSC reserved"
	default		"User defined"
}

enum eDeviceMapping
{
	0x00		"Unspecified"
	0x01		"deviceId[7..0]"
	0x02		"deviceId[15..0]"
	0x03		"deviceId[23..0]"
	0x04		"deviceId[31..0]"
	0x05		"deviceId[39..0]"
	0x06		"deviceId[47..0]"
	0x07		"Reserved"
}

enum eResourceDesc
{
	0x09			"IPResourceDescriptor"
	0x14			"deferredMpegProgramElement"
	0x15			"IPV6ResourceDescriptor"
	0x16			"URLResourceDescriptor"
	0x8000..0xFFFE	"User defined"
	0xFFFF		"Type owner"
	default			eHex
}

//
// Descriptors
//
descriptor data_service_descriptor
{
	descriptor_tag					8	uimsbf 	eDecHex 	0xA4;
	descriptor_length				8	uimsbf;
	data_service_profile				8	uimsbf		eDataServiceProfile;
	data_service_level				8	uimsbf		eDataServiceLevel;
	private_data_length				8	uimsbf;

	if (private_data_length > 0)
	{
		loop private_data looplen(private_data_length)
		{
			data_byte				8	uimsbf		eHex;
		}
	}
}	

descriptor PID_count_descriptor
{
	descriptor_tag					8	uimsbf eDecHex 	0xA5;
	descriptor_length				8	uimsbf;
	reserved					3    	bslbf	eHidden	0x7		vSet;
	total_number_of_PIDs			13	uimsbf;
	reserved					3    	bslbf	eHidden	0x7		vSet;
	min_number_of_PIDs				13	uimsbf;
}

descriptor download_descriptor
{
	descriptor_tag					8	uimsbf eDecHex 	0xA6;
	descriptor_length				8	uimsbf;
	download_id					32	uimsbf	eDecHex;
	carousel_period				32	uimsbf	eDecHex;
	control_msg_time_out_value			32	uimsbf	eDec;
}

descriptor MPE_descriptor
{
	descriptor_tag					8	uimsbf eDecHex 	0xA7;
	descriptor_length				8	uimsbf;
	deviceId_address_range			3	uimsbf	eDeviceMapping;
	deviceId_IP_mapping_flag			1	bslbf;
	alignment_indicator				1	bslbf;
	reserved					3    	bslbf	eHidden	0x7		vSet;
	max_sections_per_datagram			8	uimsbf;
}

//
// Macros
//

macro __tableHeaderA4(_TABLEID_, _VSECTLEN,_EXTNAME_,_EEXTNAME_)
{
    group table_header
    {
	table_id 					8    	uimsbf	AtscTableId	_TABLEID_;
	section_syntax_indicator 			1    	bslbf	eDec		0x1		vSet;
	private_indicator				1    	bslbf	eHidden	0x1		vSet;
	reserved					2    	bslbf	eHidden	0x3		vSet;
	section_length					12    	uimsbf	eDec		cByteLen	_VSECTLEN;
	_EXTNAME_					16    	uimsbf _EEXTNAME_;
	reserved					2    	bslbf	eHidden	0x3		vSet;
	version_number				5    	uimsbf	eDec;
	current_next_indicator			1    	bslbf	eDec;
	section_number				8    	uimsbf	eDec;
	last_section_number				8   	uimsbf	eDec;
    }
}

macro TAP()
{
	group Tap
	{
		tap_id			16	uimsbf;
		use			16	uimsbf;
		associationTag	16	uimsbf	eHex;
		group selector
		{
			SelectorLength		8	uimsbf;
			if (SelectorLength > 0)
			{
				selectorType		16	uimsbf	eHex;
				loop selector looplen (SelectorLength - 2)
				{
					selector_byte	8	uimsbf	eHex;
				}
			}
		}
	}
}

macro	IPResourceDescriptor()
{
	sourceIpAddress		32	uimsbf	eIPAddress;
	sourceIpPort			16	uimsbf	eDecHex;
	destinationIpAddress		32	uimsbf	eIPAddress;
	destinationIpPort		16	uimsbf	eDecHex;
	ipProtocol			16	uimsbf	eIPProtocol;
}

macro deferredMpegProgramElement()
{
	originatorId			16	uimsbf	eDec		0x0	vSet;
	mpegTransportStreamId	16	uimsbf	eDecHex;
	mpegProgramNum		16	uimsbf	eDecHex;
	streamType			8	uimsbf	eStreamType;
	associationTag		16	uimsbf	eHex;
}

macro IPV6ResourceDescriptor()
{
	group sourceIpV6Address
	{
		section1		32	uimsbf	eHex;
		section2		32	uimsbf	eHex;
		section3		32	uimsbf	eHex;
		section4		32	uimsbf	eHex;
	}
	sourceIpPort			16	uimsbf	eDecHex;
	group destinationIpV6Address
	{
		section1		32	uimsbf	eHex;
		section2		32	uimsbf	eHex;
		section3		32	uimsbf	eHex;
		section4		32	uimsbf	eHex;
	}
	destinationIpPort		16	uimsbf	eDecHex;
	ipProtocol			16	uimsbf	eIPProtocol;
}

macro URLResourceDescriptor()
{
	URL_length			16	uimsbf		eDec	cTextLen;
	URL				8	iso_latin	eNA	eNA		URL_length;
}


macro DSMCCResourceDescriptor()
{		
	group dsmccResourceDescriptor
	{
		group commonDescriptorHeader
		{
			resourceRequestId		16	uimsbf		eDecHex;
			resourceDescriptorType	16	uimsbf		eResourceDesc;
			resourceNum			16	uimsbf		eHex;
			associationTag		16	uimsbf		eDecHex;
		
			group resourceFlags
			{
				allocatingEntity	2	bslbf		eHex;
				negotiationType	4	bslbf		eHex;
				presentationView	2	bslbf		eHex;
			}
			resourceStatus		8	uimsbf		eHex;
			resourceLength		16	uimsbf;
			resourceDataFieldCount	16	uimsbf		eDec;
		}

		if (resourceDescriptorType == 0xFFFF)
		{
			typeOwnerId		24	uimsbf		eDecHex;
			typeOwnerValue	24	uimsbf		eDecHex;
		}
		else if (resourceDescriptorType == 0x09)
		{
			IPResourceDescriptor()
		}
		else if (resourceDescriptorType == 0x14)
		{
			deferredMpegProgramElement()
		}
		else if (resourceDescriptorType == 0x15)
		{
			IPV6ResourceDescriptor()
		}
		else if (resourceDescriptorType == 0x16)
		{
			URLResourceDescriptor()
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// Tables
/////////////////////////////////////////////////////////////////////////////////

association PMT
{
	assocStreamType	0xCF		0x95
	assocStreamType	0xD1		0x95
}

/////////////////////////////////////////////////////////////////////////////////
//
// Data Event Table (DET)

valid_descriptors DET
{
	0xA4,0xA5, 0xAA
}

validation vDETLength
{
	(section_length == fnBITSUM(fNext) / 8) && (section_length < 4094) && (section_length > 10)
}

section_id DET
{
	"Version %3d Section %3d Source Id %5d [Pid %u (0x%X)]",f7,f9,f5,pid,pid
}

access_key DET 
{
	pid,f0,f5,f7,f9
}

table DET
{
	__tableHeaderA4("0xCE","vDETLength","source_id","eDecHex")

	protocol_version			8	uimsbf;
	num_data_in_section			8	uimsbf		eDec		cIterations;

	loop event_data looplen(num_data_in_section) eIterations
	{
		reserved			2		uimsbf		eHidden	0x3	vSet;
		data_id			14		uimsbf;
		start_time			32		uimsbf		eATSCTime;
		reserved			2		uimsbf		eHidden	0x3	vSet;
		ETM_Location			2		uimsbf		EtmLocation	0x0;
		length_in_seconds		20		uimsbf;
		
		title_length			8		uimsbf		eDec		cNextByteLen;
		MultiStringStruct("title_text", "title_length")

		reserved			4		uimsbf		eHidden	0xF;
		descriptors_length		12		uimsbf		eDec		cDescriptors;

		loop descriptors looplen(descriptors_length)
	}
	CRC_32				32		rpchof		eHex;
}

/////////////////////////////////////////////////////////////////////////////////
//
// Long Term Service Table (LTST)

valid_descriptors LTST
{
}

validation vLTSTLength
{
	(section_length == fnBITSUM(fNext) / 8) && (section_length < 4094) && (section_length > 11)
}

section_id LTST
{
	"Version %3d Section %3d Source Id %5d [Pid %u (0x%X)]",f7,f9,f5,pid,pid
}

access_key LTST 
{
	pid,f0,f5,f7,f9
}

table LTST
{
	__tableHeaderA4("0xD2","vLTSTLength","table_id_extension","eNA")

	protocol_version			8	uimsbf;
	num_source_id_in_section		8	uimsbf		eDec		cIterations;

	loop source_id_data looplen(num_source_id_in_section) eIterations
	{
		source_id			16		uimsbf;
		num_data_events		8		uimsbf;

		loop data_events looplen(num_data_events) eIterations
		{
			reserved		2		uimsbf		eHidden	0x3	vSet;
			data_id		14		uimsbf;
			start_time		32		uimsbf		eATSCTime;
			reserved		2		uimsbf		eHidden	0x3	vSet;
			ETM_Location		2		uimsbf		EtmLocation	0x0;
			length_in_seconds	20		uimsbf;
		
			title_length		8		uimsbf		eDec		cNextByteLen;
			MultiStringStruct("title_text", "title_length")

			reserved		4		uimsbf		eHidden	0xF;
			descriptors_length	12		uimsbf		eDec		cDescriptors;

			loop descriptors looplen(descriptors_length)
		}
	}
	CRC_32				32		rpchof		eHex;
}

/////////////////////////////////////////////////////////////////////////////////
//
// Data Service Table (DST)

valid_descriptors DST
{
	0xA6,0xA7
}

validation vDSTLength
{
	(section_length == fnBITSUM(fNext) / 8) && (section_length < 4094) && (section_length > 11)
}

section_id DST
{
	"Version %3d Section %3d Source Id %5d [Pid %u (0x%X)]",f7,f9,f5,pid,pid
}

access_key DST 
{
	pid,f0,f5,f7,f9
}

table DST
{
	__tableHeaderA4("0xCF","vDSTLength","table_id_extension","eNA")

	group data_service_table_bytes
	{
		sdf_protocol_version			8	uimsbf;
		application_count_in_section		8	uimsbf;

		if (application_count_in_section > 0)
		{
			loop application_counts looplen(application_count_in_section) eIterations
			{
				COMPATIBILITYDESCRIPTOR()
				app_id_byte_length		16	uimsbf	eDec;
				if (app_id_byte_length > 1)
				{
					app_id_description	16	uimsbf	eAppIdDesc;
					loop app_id looplen(app_id_byte_length - 2)
					{
						app_id_byte	8	uimsbf	eHex;
					}
				}

				tap_count			8	uimsbf;
				loop taps looplen(tap_count) eIterations
				{
					protocol_encapsulation	8	uimsbf	eProtocolEncap;
					action_type			7	uimsbf	eActionType;
					resource_allocation		1	bslbf;
					TAP()

					tap_info_length		16	uimsbf;
					loop descriptors looplen(tap_info_length)
				}

				app_info_length		16	uimsbf		eDec	cDescriptors;
				loop descriptors looplen(app_info_length)
				PRIVATEDATA("app_data","16")
			}
		}		// if application_count_in_section 

		service_info_length		16	uimsbf		eDec	cDescriptors;
		loop descriptors looplen(service_info_length)
		PRIVATEDATA("service_private_data","16")
	}			// data_service_table_bytes

	CRC_32				32		rpchof		eHex;
}

/////////////////////////////////////////////////////////////////////////////////
//
// Network Resources Table (NRT)

valid_descriptors NRT
{
}

validation vNRTLength
{
	(fThis == fnBITSUM(fNext) / 8) && (fThis < 4094) && (fThis > 9)
}

section_id NRT
{
	"Version %3d Section %3d Source Id %5d [Pid %u (0x%X)]",f7,f9,f5,pid,pid
}

access_key NRT 
{
	pid,f0,f5,f7,f9
}

table NRT
{
    	group table_header
    	{
		table_id 					8    	uimsbf	AtscTableId	0xD1;
		section_syntax_indicator 			1    	bslbf	eDec		0x1		vSet;
		private_indicator				1    	bslbf	eHidden	0x1		vSet;
		reserved					2    	bslbf	eHidden	0x3		vSet;
		private_section_length			12  	uimsbf	eDec		cByteLen	vNRTLength;
		table_id_extension				16    	uimsbf;
		reserved					2    	bslbf	eHidden	0x3		vSet;
		version_number				5    	uimsbf	eDec;
		current_next_indicator			1    	bslbf	eDec;
		section_number				8    	uimsbf	eDec;
		last_section_number				8   	uimsbf	eDec;
    	}

	group network_resources_table_bytes
	{
		resource_descriptor_count_in_section	8	uimsbf;

		if (resource_descriptor_count_in_section > 0)
		{
			loop resource_descriptors looplen(resource_descriptor_count_in_section) eIterations
			{
				COMPATIBILITYDESCRIPTOR()

				DSMCCResourceDescriptor()
			}
		}
	}
	CRC_32				32		rpchof		eHex;
}

