

#include "DSMCCv2_Common.scp"

enum eTableIDs
{
	0x3B	"0x%2X) (DSMCC UN"
	0x3C	"0x%2X) (DSMCC Download"
	0x3D	"0x%2X) (DSMCC Stream Descriptors"
	0x3F	"0x%2X) (DSMCC Addressable Section"
}

valid_descriptors PMT
{
	0x13..0x1A			// from 13818-6
}

valid_descriptors TS_description
{
	0x13..0x15			// from 13818-6
}

association DSMCC_StreamDescriptors
{
	assocStreamType 	2		12
}

valid_descriptors DSMCC_StreamDescriptors
{
	19..26					// from 13818-6
}

//
// DSM-CC Sections
//

access_key DSMCC_UN
{
	pid,f0,f5,f7
}

section_id DSMCC_UN
{
	"Table ID 0x%02X Pid %6u (0x%04X) Message ID %6u Extension %5d Version %3d",f0,pid,pid,f13,f5,f7
}

table DSMCC_UN
{
	__tableHeaderLong1(	"0x3b", " ", "table_id_extension", "eDecHex")

	DSMCC_MESSAGEHEADER("0x1002")

	if (messageId == 0x1006)	// DSI
	{
		DSMCC_DSI_BODY()
	}

	if (messageId == 0x1002)	// DII
	{
		DSMCC_DII_BODY()
	}

	CRC()
}


access_key DSMCC_DDB
{
	pid,f0,f5,f7,f9
}

section_id DSMCC_DDB
{
	"Table ID 0x%02X Pid %6u (0x%04X) Message ID %6u (0x%04X) Extension %5d (0x%04X) Version %3d Section %3d",f0,pid,pid,f13,f13,f5,f5,f7,f9
}

table DSMCC_DDB
{
	__tableHeaderLong1(	"0x3c", " ", "table_id_extension", "eDecHex")

	DSMCC_MESSAGEHEADER("0x1003")

	DSMCC_DDB_BODY()

	CRC()
}

access_key DSMCC_StreamDescriptors
{
	pid,f0,f5,f7,f9
}

table DSMCC_StreamDescriptors
{
	__tableHeaderLong1(	"0x3d", " ", "table_id_extension", "eDecHex")

	loop stream_descriptors looplen(section_length - 9)
	{
		STREAM_DESCRIPTORS()
	}

	CRC()
}

access_key DSMCC_AddressableSection
{
	pid,f0,f5,f7,f9
}

table DSMCC_AddressableSection
{
	table_id 				8    	uimsbf	eHex		0x3F;
	'0'			 		1    	bslbf	eHidden;
	error_detection_type			1    	bslbf;
	reserved				2    	bslbf	eHidden	0x3;
	addressable_section_length		12    	uimsbf	eDec		cByteLen;
	deviceId_7_0				8    	uimsbf	eHex;
	deviceId_15_8				8    	uimsbf	eHex;
	reserved				2    	bslbf	eHidden		0x3;
	payload_scrambling_control		2    	bslbf	eScramblingControl2;
	address_scrambling_control		2    	bslbf	eScramblingControl2;
	LLC_SNAP_flag			1    	bslbf;
	'1'					1    	bslbf	eHidden;
	section_number			8    	uimsbf;
	last_section_number			8    	uimsbf;
	deviceId_23_16			8    	uimsbf	eHex;
	deviceId_31_24			8    	uimsbf	eHex;
	deviceId_39_32			8    	uimsbf	eHex;
	deviceId_47_40			8    	uimsbf	eHex;

	if (payload_scrambling_control != 0)
	{
		// data is scrambled
		rawbytes length(addressable_section_length - 13)
	}
	else if (LLC_SNAP_flag == 1)
	{
		LLC_SNAP_HEADER()
		IP_DATAGRAM("addressable_section_length - 21")	
	}
	else
	{
		IP_DATAGRAM("addressable_section_length - 13")	
	}

	CRC()
}
