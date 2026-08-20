
//

descriptor ATSC_stuffing_descriptor
{
	descriptor_tag		8	uimsbf eDec 0x80;
	descriptor_length	8	uimsbf;
}

// the scripted AC-3 descriptor will work slightly differently in the mux
// to the non-scripted version

calculation TextLen
{
	fnBITLEN(text) / 8
}

validation UnicodeLen
{
	textlen/2	
}

validation IsoLen
{
	textlen
}

enum eFullService
{
	0	"partial service"
	1	"full service"
}	

enum eTextCode
{
	0	"Unicode"
	1	"Iso Latin-1"
}

enum eAc3Priority
{
	0	"%d) (reserved"
	1	"%d) (primary audio"
	2	"%d) (other audio"
	3	"%d) (not specified"
}

// KPJ: Some elements do not appear to the user, because the descriptor_length indicates
// they are not present. However this puts us in a catch-22 situation when we wish to
// add them. This approach (by RGM) provides a solution.
// Each element that has been omitted, due to the descriptor_length, is represented as a loop
// of length 0. This ensures that an element exists in the UI. As soon as the user chooses to
// add an element to this 'loop', the descriptor_length is changed, and the real object replaces
// the placeholder.  __test should evaluate to 0 when the placeholder is in place and 1 when
// an element has been added to the loop (otherwise a warning occurs as a loop with 0 iterations
// momentarily has 1 iteration .
macro _placeholder(__name, __test)
{
	loop __name looplen(__test) eIterations
	{
		placeholder					8	bslbf;
	}	
}

// This is called an "audio_stream_descriptor" according to A52/A Aug. 2001.
// The name of the placeholder is changed, according to which element it is supposed to represent.
descriptor ATSC_audio_stream_descriptor
{
	descriptor_tag							8	uimsbf		eDecHex	0x81;
	descriptor_length						8	uimsbf		eNA;
	sample_rate_code						3	bslbf		esample_rate_code;
	bsid								5	bslbf		eDecHex;
	bit_rate_code							6 	bslbf		ebit_rate_code;
	surround_mode						2	bslbf		esurround_mode;
	bsmod								3 	bslbf		eHex;
	num_channels							4	bslbf		echannels;
	full_svc							1	bslbf		eFullService;
	if (descriptor_length <= 3)
	{
		_placeholder("langcod", "descriptor_length > 3")
	}
	else
	{
		langcod		8	bslbf		eDecHex;

		if (descriptor_length <= 4)
		{
			if(num_channels == 0)
			{
				_placeholder("langcod2", "descriptor_length > 4")
			}
			else
			{
				if(bsmod < 2)
				{
					_placeholder("mainid", "descriptor_length > 4")
				}
				else
				{
					_placeholder("asvcflags", "descriptor_length > 4")
				}
			}
		}
		else
		{
			if (num_channels == 0)
			{
				langcod2				8	bslbf 		eDecHex;
			}
			if(descriptor_length <= 5 && num_channels == 0)
			{
				if(bsmod < 2)
				{
					_placeholder("mainid", "descriptor_length > 5")
				}
				else
				{
					_placeholder("asvcflags", "descriptor_length > 5")
				}
			}
			else
			{
				if (bsmod < 2)
				{
					mainid				3	uimsbf		eDecHex;
					priority				2	bslbf		eAc3Priority;
					reserved			3 	bslbf		eHidden	0x7;
				}
				else
				{
					asvcflags			8	bslbf		eDecHex;
				}

				if(descriptor_length <= 5 || (descriptor_length <= 6 && num_channels == 0))
				{
					_placeholder("text", "descriptor_length > 6 || (descriptor_length > 5 && num_channels > 0)")
				}
				else
				{
					textlen				7	uimsbf		eDecHex		TextLen;
					text_code			1	bslbf		eTextCode;
					if(text_code == 0)
					{
						text			16	unicode	eNA		eNA	UnicodeLen;
					}
					else
					{
						text			8	iso_latin	eNA		eNA	IsoLen;
					}

					if (descriptor_length <= (6 + textlen) ||
				    	    (descriptor_length <= (7 + textlen) && num_channels == 0))
					{
						_placeholder("language", "(descriptor_length > (7 + textlen)) || (descriptor_length > (6 +textlen) && num_channels > 0)")
					}
					else
					{
						language_flag		1	bslbf		eDec;
						language_flag_2	1	bslbf		eDec;
						reserved		6	bslbf		eHidden	0x3F;

						if (language_flag == 1)
						{
							language	24	bslbf		eISOLatin	eNA	eNA	0x656e67;
						}

						if (language_flag_2 == 1)
						{
							language_2	24	bslbf		eISOLatin	eNA	eNA	0x656e67;
						}

						if (num_channels == 0)
						{	// langcod2 field present
							loop additional_info looplen(descriptor_length - 8 - textlen - (3*language_flag) - (3*language_flag_2))
							{
								additional_info_byte		8	uimsbf		eDecHex;
							}
						}
						else
						{
							loop additional_info looplen(descriptor_length - 7 - textlen- (3*language_flag) - (3*language_flag_2))
							{
								additional_info_byte		8	uimsbf		eDecHex;
							}
						}
					}
				}
			}
		}
	}
}

descriptor caption_service_descriptor
{
	descriptor_tag					8	uimsbf 	eDecHex 	0x86;
	descriptor_length				8	uimsbf;
	reserved					3	bslbf		eHidden	0x7;
	number_of_services				5	uimsbf		eDec		cIterations;
	loop caption_streams looplen(number_of_services) eIterations
	{	
		language				24  	bslbf		eISOLatin	eNA		eNA			0x656e67; // eng;
		cc_type				1	bslbf;
		reserved				1	bslbf		eHidden		0x1		vSet;
		if (cc_type == 0)
		{
			reserved			5	bslbf		eHidden	0x1F;
			line21_field			1	bslbf		eDec;
		}
		else
		{
			caption_service_number	6	uimsbf		eDec;
		}
		easy_reader				1	bslbf 			EasyReader;
		wide_aspect_ratio			1	bslbf 			WideAspectRatio;
		reserved				14	bslbf			eHidden	0x3FFF;		
	}	
}
	
descriptor content_advisory_descriptor
{
	descriptor_tag					8	uimsbf 	eDecHex 	0x87;
	descriptor_length				8	uimsbf;
	reserved					2	bslbf	eHidden		0x3;
	rating_region_count				6	uimsbf	eHidden		cIterations;
	loop rating_regions looplen(rating_region_count) eIterations
	{
		rating_region				8	uimsbf;
		rated_dimensions			8	uimsbf	eDec		cIterations;
		loop rated_dimension looplen(rated_dimensions) eIterations
		{
			rating_dimension		8	uimsbf;
			reserved			4	bslbf	eHidden	0xF;
			rating_value			4	uimsbf;
		}
		
		rating_description_length		8 	uimsbf		eDec		cNextByteLen;
		MultiStringStruct("rating_description_text", "rating_description_length")
	}
}

descriptor extended_channel_name_descriptor
{
	descriptor_tag					8	uimsbf 	eDecHex	0xA0;
	descriptor_length				8	uimsbf;
	MultiStringStruct("long_channel_name_text", "descriptor_length")
}

descriptor service_location_descriptor
{
	descriptor_tag					8	uimsbf 	eDecHex	0xA1;
	descriptor_length				8	uimsbf;
	reserved					3	bslbf	eHidden	0x7;
	PCR_PID					13	uimsbf	eHex;
	number_elements				8	uimsbf	eDec		cIterations;

	loop elements looplen(number_elements) eIterations
	{
		stream_type				8	uimsbf	AtscStreamType;
		reserved				3	bslbf	eHidden	0x7;
		elementary_PID			13	uimsbf	eHex;
		ISO_639_language_code		24  	bslbf	eISOLatin	eNA	eNA	0x656e67; // eng;
	}
}

descriptor ATSC_time_shifted_service_descriptor
{	
	descriptor_tag		8	uimsbf eDecHex		0xA2;
	descriptor_length	8	uimsbf;
	reserved		3	bslbf	eHidden		0x7;
	number_of_services	5	uimsbf	eDec	cIterations;
	loop services looplen(number_of_services) eIterations
	{
		reserved	6	bslbf	eHidden		0x3F;
		time_shift	10	uimsbf	eDec;
		reserved	4	bslbf	eHidden		0xF;
		major_channel_number	10	uimsbf	eDec;
		minor_channel_number	10 	uimsbf	eDec;
	}
}

descriptor component_name_descriptor
{
	descriptor_tag		8	uimsbf eDecHex		0xA3;
	descriptor_length	8	uimsbf;
	MultiStringStruct("component_name_string", "descriptor_length")
}

descriptor dcc_departing_request_descriptor
{		
	descriptor_tag					8	uimsbf		eDecHex	0xA8;
	descriptor_length				8	uimsbf;
	dcc_departing_request_type			8	uimsbf		DccDepartingRequestType;
	dcc_departing_request_text_length		8	uimsbf		eDec	cNextByteLen;
	MultiStringStruct("dcc_departing_request_text", "dcc_departing_request_text_length")
}		

descriptor dcc_arriving_request_descriptor 
{		
	descriptor_tag					8	uimsbf		eDecHex	0xA9;
	descriptor_length				8	uimsbf;
	dcc_arriving_request_type			8	uimsbf;
	dcc_arriving_request_text_length		8	uimsbf		eDec	cNextByteLen;
	MultiStringStruct("dcc_arriving_request_text", "dcc_arriving_request_text_length")
}	

descriptor redistribution_control_descriptor
{
	descriptor_tag				8	uimsbf		eDecHex	0xAA;
	descriptor_length			8	uimsbf		eDecHex;
	loop data looplen(descriptor_length)
	{
		rc_information			8	uimsbf		eDecHex;
	}
}

descriptor ATSC_private_information_descriptor
{
	descriptor_tag				8	uimsbf		eDecHex	0xAD;
	descriptor_length			8	uimsbf		eDecHex;
	format_identifier			32	uimsbf		eStreamFormat;
	loop private_data looplen(descriptor_length - 4)
	{
		private_data_byte		8	uimsbf		eDecHex;
	}
}

descriptor genre_descriptor
{
	descriptor_tag				8	uimsbf		eDecHex	0xAB;
	descriptor_length			8	uimsbf		eDecHex;
	reserved					3	bslbf		eHidden	0x7;
	attribute_count				5	uimsbf		eDecHex	cNextByteLen;
	loop attributes looplen(attribute_count)
	{
		attribute				8	uimsbf		CategoricalGenreCode;
	}
}
