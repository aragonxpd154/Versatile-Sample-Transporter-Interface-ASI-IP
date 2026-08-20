

//************************************************************************************************
// Enumerations
//
//************************************************************************************************

enum eScramblingControl2
{
	0x0		"Not scrambled"
	default		eDec
}

enum eCompDescType
{
	0x00		"Pad descriptor"
	0x01		"System hardware descriptor"
	0x02		"System software descriptor"
	0x03..0x3F	"ISO/IEC 13818-6 reserved"
	default		"User defined"
}


enum eCompSpecifierType
{
	0x00		"ISO/IEC 13818-6 reserved"
	0x01		"IEEE OUI"
	0x02..0x7F	"ISO/IEC 13818-6 reserved"
	default		"User defined"
}

//************************************************************************************************
// Validation
//
//************************************************************************************************

validation vIHL
{
	fThis >= 5
}

validation vIPLength
{
	//fThis == _DATAGRAMLEN_
	fThis == section_length - 13 - (LLC_SNAP_flag * 8)
}

//************************************************************************************************
// Macros
//
//************************************************************************************************

macro COMPATIBILITYDESCRIPTOR
{
	group compatibilityDescriptor
	{
	    compatibilityDescriptorLength	16	uimsbf;	
	    if (compatibilityDescriptorLength > 1)
	    {
		compatibilityDescriptorCount		16	uimsbf;
	    	loop CompatibilityDescriptor looplen(compatibilityDescriptorCount) eIterations
	    	{	
			descriptorType		8	uimsbf		eCompDescType;
			descriptorLength		8	uimsbf;
			SpecifierType			8	uimsbf		eCompSpecifierType;
			SpecifierData			24	uimsbf;
			model				16	uimsbf;
			version			16	uimsbf;
			SubDescriptorCount		8	uimsbf;
			loop SubDescriptorCount looplen(SubDescriptorCount) eIterations
			{
				subDescriptorType		8	uimsbf;
				subDescriptorLength		8	uimsbf;			
				loop subDescriptorLength looplen(subDescriptorLength)
				{
					additionalInformation	8	uimsbf;
				}
			}
		}
	    }
	}
}


calculation cServiceGatewayInfo
{
    fnBITLEN(ServiceGatewayInfo)/8
}


enum eBIOP
{
    0x49534F40      "BIOP::ConnBinder"
    0x49534F50      "BIOP::ObjectLocation"
    default         eDecHex
}


enum eTapUse
{
    0           "UNKNOWN_USE"
    1           "MPEG_TS_UP_USE"
    2           "MPEG_TS_DOWN_USE"
    3           "MPEG_ES_UP_USE"
    4           "MPEG_ES_DOWN_USE"
    5           "DOWNLOAD_CTL_USE"
    6           "DOWNLOAD_CTL_UP_USE"
    7           "DOWNLOAD_CTL_DOWN_USE"
    8           "DOWNLOAD_DATA_USE"
    9           "DOWNLOAD_DATA_UP_USE"
    10          "DOWNLOAD_DATA_DOWN_USE"
    11          "STR_NPT_USE"
    12          "STR_STATUS_AND_EVENT_USE"
    13          "STR_EVENT_USE"
    14          "STR_STATUS_USE"
    15          "RPC_USE"
    16          "IP_USE"
    17          "SDB_CTRL_USE"
    18          "T120_TAP1"
    19          "T120_TAP2"
    20          "T120_TAP3"
    21          "T120_TAP4"
    22          "BIOP_DELIVERY_PARA_USE"
    23          "BIOP_OBJECT_USE"
    24          "BIOP_ES_USE"
    25          "BIOP_PROGRAM_USE"
    26          "BIOP_DNL_CTRL_USE"
    default     eDec
}


macro BIOP_CONNBINDER
{
    tapsCount               8       uimsbf  eDecHex;

    loop BIOP_Tap looplen(tapsCount) eIterations
    {
        Id                  16      uimsbf;
        Use                 16      uimsbf  eTapUse;
        AssociationTag      16      uimsbf  eDecHex;
        SelectorLength      8       uimsbf;

        if (SelectorLength>0)
        {
            selectorType    16      uimsbf  eDecHex;
            transactionId   32      uimsbf  eDecHex;
            timeout         32      uimsbf  eDecHex;
        }
    }
}


macro BIOP_OBJECTLOCATION
{
    carouselId              32      uimsbf;
    moduleId                16      uimsbf  eDecHex;
    version_major           8       uimsbf;
    version_minor           8       uimsbf;
    object_keyLength        8       uimsbf;

    loop object_key looplen(object_keyLength)
    {
        object_byte         8       uimsbf  eHex;
    }
}


macro TAGGED_PROFILES
{
    taggedProfilesCount             32      uimsbf  eNA eNA eNA 1;
    loop BIOP_ProfileBody looplen(taggedProfilesCount) eIterations
    {
        profileIdTag                32      uimsbf  eDecHex;
        profileDataLength           32      uimsbf;
        profileByteOrder            8       uimsbf;
        componentsCount             8       uimsbf;

        loop components looplen(componentsCount) eIterations
        {
            componentTag            32      uimsbf  eBIOP;
            componentDataLength     8       uimsbf;

            if (componentTag == 0x49534F40)
            {
                BIOP_CONNBINDER()
            }

            if (componentTag == 0x49534F50)
            {
                BIOP_OBJECTLOCATION()
            }
        }
    }
}


macro IOR
{
    typeIdLength                32      uimsbf;
    typeId                      32      uimsbf      eISOLatin;

    // Alignment gap
    if (typeIdLength%4 != 0)
    {
        loop alignment_gap looplen(typeIdLength%4)
        {
            alignment_byte      8       uimsbf;
        }
    }

    TAGGED_PROFILES()
}


macro PRIVATEDATA(_NAME_, _BITLENGTH_)
{
    _NAME_Length                    _BITLENGTH_ uimsbf;

	rawbytes length(_NAME_Length)
}


//************************************************************************************************
// MPE/IP macros
//
//************************************************************************************************
macro LLC_SNAP_HEADER()
{
	group LLC_SNAP_Header
	{
		DSAPAddress					8	uimsbf		eHex;
		SSAPAddress					8	uimsbf		eHex;
		Control						8	uimsbf		eHex;
		OUI						24	uimsbf		eHex;
		ProtocolType					16	uimsbf		eHex;
	}
}

macro IP_DATAGRAM(_DATAGRAMLEN_)
{
	// Display IP header
	group IP_Datagram
	{	
		Version			4	uimsbf		eDecHex	eNA		eNA		4;
		if (Version == 4)
		{
			IP_HeaderV4("_DATAGRAMLEN_")
		}
		else if (Version == 6)
		{
			IP_HeaderV6("_DATAGRAMLEN_")
		}
		else
		{
			// unknown version, abort this datagram interpretation
			rawbytes length(0xFFFF)
		}
	}
}

macro IP_HeaderV4(_DATAGRAMLEN_)
{
	IHL					4	uimsbf		eDec		eNA		vIHL		5;
	TypeOfService				8	uimsbf		eHex;
	TotalLength				16	uimsbf		eDec		eNA		eNA		20;
	Identification				16	uimsbf;
	Flags					3	uimsbf		eHex;
	FragmentOffset			13	uimsbf;
	TimeToLive				8	uimsbf;
	Protocol				8	uimsbf		eIPProtocol;
	HeaderChecksum			16	uimsbf		eHex;
	SourceAddress			32	uimsbf		eIPAddress;
	DestinationAddress			32	uimsbf		eIPAddress;
		
	loop Options looplen((IHL * 4) - 20)
	{
		OptionByte				8	uimsbf		eHex;
	}

	// if TCP or UDP, display port numbers
	if ((Protocol == 6) || (Protocol == 17))
	{
		SourcePort					16	uimsbf;
		DestinationPort				16	uimsbf;
		rawbytes length(TotalLength - (IHL * 4) - 4)	
	}
	else
	{
		rawbytes length(TotalLength - (IHL * 4))	
	}

	if (section_number == last_section_number)
	{
		loop stuffing looplen(_DATAGRAMLEN_ - TotalLength)
		{
			stuffing_byte				8	uimsbf		eDecHex;
		}
	}
}

macro IP_HeaderV6(_DATAGRAMLEN_)
{
	Priority					4	uimsbf;
	FlowLabel				24	uimsbf;
	PayloadLength			16	uimsbf;
	NextHeader				8	uimsbf;
	HopLimit				8	uimsbf;
	SourceAddress1			32	uimsbf;
	SourceAddress2			32	uimsbf;
	SourceAddress3			32	uimsbf;
	SourceAddress4			32	uimsbf;
	DestinationAddress1			32	uimsbf;
	DestinationAddress2			32	uimsbf;
	DestinationAddress3			32	uimsbf;
	DestinationAddress4			32	uimsbf;

	rawbytes length(PayloadLength)	

	if (section_number == last_section_number)
	{
		loop stuffing looplen(_DATAGRAMLEN_ - PayloadLength - 40)
		{
			stuffing_byte				8	uimsbf		eDecHex;
		}
	}
}

