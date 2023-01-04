

association PMT
{
    assocStreamType 	2		0x85
}


descriptor program_identifier_descriptor
{
    descriptor_tag			8	uimsbf 	eHex	0x85;
    descriptor_length		8	uimsbf;

    provider_index		16	uimsbf;
    program_event_id		24	uimsbf;
    episode_field_indicator 	1	bslbf;
    episode_date_indicator 	1	bslbf;
    ISAN_field_indicator	 	1	bslbf;
    reserved			5	bslbf		eHidden 0x0	vClear;

    if (episode_field_indicator == 1)
    {
        if (episode_date_indicator == 0)
        {
            episode_number	12	uimsbf;
            version_number	12	uimsbf;
        }
        else if (episode_date_indicator == 1)
        {
            original_date_year	8	uimsbf;
            original_date_month	8	uimsbf;
            original_date_day	8	uimsbf;
        }

        program_id_string_length	8	uimsbf		eNA	cTextLen;
        program_id			8	iso_latin	eNA	eNA	program_id_string_length;
    }

    if (ISAN_field_indicator == 1)
    {
        ISAN_authority		8	uimsbf		eHex;
        ISAN_id			64	bslbf		eHex;
    }

}

/////////////////////////////////////////////////////////////////////////////////
// Program Identifier Table Section

valid_descriptors PIT
{
}

access_key PIT
{
    f0
}

section_id PIT
{
    "program_identifier PID %4d (%#x)",pid,pid
}

table PIT
{
    table_id				8		uimsbf		eHex		0xD0;
    section_syntax_indicator		1		bslbf		eHex	0x0		vClear;
    private_indicator			1		bslbf		eHidden	0x1		vSet;
    reserved				2		bslbf		eHidden	0x3		vSet;
    section_length				12		uimsbf		eDec		cByteLen;

    loop descriptors looplen(section_length - 4)

    CRC_32				32		rpchof		eNull;
}


// End of File /////////////////
