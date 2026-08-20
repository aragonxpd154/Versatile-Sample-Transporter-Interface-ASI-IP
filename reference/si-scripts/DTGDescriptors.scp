

//------------------------------------------------------------------------------------------------
//
// Enumerations
//
//------------------------------------------------------------------------------------------------

enum eUKapplicationType
{
	0x102		"UKEngineProfile1"
}

enum eDTGLinkage
{
	0x00		"reserved for future use"
	0x01		"information service"
	0x02		"Electronic Programme Guide (EPG) service"
	0x03		"CA replacement service"
	0x04		"transport stream containing complete Network/Bouquet SI"
	0x05		"Service replacement service"
	0x06		"Data broadcast service"
	0x07..0x7F	"reserved for future use"
	0x80		"parent service"
	0x81..0xFE	"user defined"
	0xFF		"reserved for future use"
}

enum eLogicalChannel
{
    0              "undefined"
    1..99	   "reserved for user allocation"
    100..199	   "BBC"
    200..299	   "ONdigital"
    300..399	   "ITV"
    400..499	   "Channel 4"
    500..599	   "Channel 5"
    600..699	   "SDN"
    700..899	   "ONdigital"
    900..999	   "Teletext Ltd"
    1000..1023     "rfu"
    default        "Unknown"
}


//------------------------------------------------------------------------------------------------
//
// Descriptors
//
//------------------------------------------------------------------------------------------------

valid_descriptors PMT
{
	0x80
}

descriptor application_descriptor
{
    descriptor_tag                  8	uimsbf	eDec	0x80;		
    descriptor_length               8	uimsbf;		
	
    loop application_types looplen(descriptor_length)
    {
        application_type_id		    32	uimsbf	eHex;
		application_type_id_revision 8	uimsbf;
		application_specific_data_len 8	uimsbf;

		loop specific_data looplen(application_specific_data_len)
		{
			specific_data_byte		8	uimsbf	eHex;
		}
    }		
}

	
valid_descriptors NIT_actual
{
	0x83
}

descriptor logical_channel_descriptor
{
    descriptor_tag                  8	uimsbf	eDec	0x83;		
    descriptor_length               8	uimsbf;
 
    loop services looplen(descriptor_length)
    {
        service_id		    16	uimsbf;
    	reserved		    6	bslbf	eHidden	0x3F;
        logical_channel_number      10	uimsbf  eLogicalChannel;
    }
}

validation vNameID
{
    fThis > 0
}

validation vServiceNameLen
{
	fThis < 33
}

valid_descriptors SDT_actual
{
	0x84
}

valid_descriptors SDT_other
{
	0x84
}

descriptor preferred_name_list_descriptor
{
    descriptor_tag                  8	uimsbf	eDec	0x84;		
    descriptor_length               8	uimsbf;

    loop preferred_names looplen(descriptor_length)
    {
        ISO_639_language_code	    24	bslbf   eISOLatin;
		name_count		    8   uimsbf  eDec	cTextLen;

		loop alternative_names looplen(name_count) eIterations
		{
			name_id		    8   uimsbf		eDec	eNA					vNameID;
			name_length		8   uimsbf		eDec	cTextLen			vServiceNameLen;
			name		    8	iso_latin	eNA     eNA					name_length;
		}
	}
}

valid_descriptors EIT_actual_pf
{
	0x85
}

valid_descriptors EIT_other_pf
{
	0x85
}

valid_descriptors EIT_actual_schedule
{
	0x85
}

valid_descriptors EIT_other_schedule
{
	0x85
}

descriptor preferred_name_identifier_descriptor
{
    descriptor_tag                  8	uimsbf	eDec	0x85;		
    descriptor_length               8	uimsbf;
    name_id		            8	uimsbf;
}



// End of File /////////////////
