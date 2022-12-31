

enum eStreamFormat
{
	0x43554549	"0x%08X) (CUEI SCTE35 DPI"
}

enum eStreamType
{
	0x86		"0x%02X) (SCTE35 DPI Cueing"
}

enum eEncrypt_algorithms
{
	0x00		"0x%02X) (No Encyption"
	0x01		"0x%02X) (DES-ECB mode"
	0x02		"0x%02X) (DES-CBC mode"
	0x03		"0x%02X) (Triple DES EDE3-ECB mode"
       	0x04..0x1f       "0x%02X) (Reserved"
            0x20..0xff        "0x%02X) (User private"
	default		eDecHex
}
	
enum eCUEI
 {
	0x43554549	"0x%X) (CUEI"  // CUE INSERT FLAG
 }

enum eSpliceCommandType
{
	0x00		"0x%02X) (Null"
	0x04		"0x%02X) (Schedule"
	0x05		"0x%02X) (Insert"
	0x06		"0x%02X) (Time Signal"
	0x07		"0x%02X) (Bandwidth reservation"
	0xff		"0x%02X) (Private"
	default		"0x%02X) (Undefined"
}


enum eSpecificSpliceDescriptors
{
	0x00		"0x%02X) (avail_descriptor"
	0x01		"0x%02X) (DTMF_descriptor"
	0x02		"0x%02X) (segmentation_descriptor"
	default		"0x%02X) (Reserved"
}

enum eCueType
{
	0x0		"0x%02X) (splice_insert/null/schedule"
	0x1		"0x%02X) (All commands"
	0x2		"0x%02X) (Segmentation"
	0x3		"0x%02X) (Tiered splicing"
	0x4		"0x%02X) (Tiered segmentation"
	0x5..0x7F	"0x%02X) (Reserved"
	default		"0x%02X) (User defined"
}

enum eSegmentationUpidType
{
	0x00		"0x%02X) (Not present"
	0x01		"0x%02X) (User defined"
	0x02		"0x%02X) (ISCII"
	0x03		"0x%02X) (Ad-ID"
	0x04		"0x%02X) (UMID"
	0x05		"0x%02X) (ISAN"
	0x06		"0x%02X) (V-ISAN"
	0x07		"0x%02X) (TID"
	0x08		"0x%02X) (TI."
	0x09		"0x%02X) (ADI"
	0x0A..0x1F	"0x%02X) (Reserved"
}

enum eSegmentationType
{
	0x00		"0x%02X) (Not indicated"
	0x01		"0x%02X) [Content Identification"
	0x10		"0x%02X) (Program start"
	0x11		"0x%02X) (Program end"
	0x12		"0x%02X) (Program early termination"
	0x13		"0x%02X) (Program breakaway"
	0x14		"0x%02X) (Program resumption"
	0x15		"0x%02X) (Program runover planned"
	0x16		"0x%02X) (Program runover unplanned"
	0x20		"0x%02X) (Chapter start"
	0x21		"0x%02X) (Chapter end"
	0x30		"0x%02X) (Provider advertisement start"
	0x31		"0x%02X) (Provider advertisement end"
	0x32		"0x%02X) (Distributor advertisement start"
	0x33		"0x%02X) (Distributor advertisement end"
	0x40		"0x%02X) (Unscheduled event start"
	0x41		"0x%02X) (Unscheduled event end"
	default		"0x%02X) (Reserved"
}

descriptor cue_identifier_descriptor
{
	descriptor_tag			8		uimsbf			eDecHex		0x8A;
	descriptor_length		8		uimsbf			eDecHex;
	cue_stream_type		8		uimsbf			eCueType;
}

format ePrerollTime
{
	default 	"%02.1fs (%0.0f)", fThis / 10, fThis;
}

valid_descriptors PMT
{
	0x8A
}

/////////////////////////////////////////////////////////////////////////////////
// Tables
/////////////////////////////////////////////////////////////////////////////////

//table id fc, stream type 86 in PMT

association PMT
{
	assocStreamType	0x2		0x86
}



/////////////////////////////////////////////////////////////////////////////////
//
// macros
//
/////////////////////////////////////////////////////////////////////////////////



macro BREAK_DURATION()
{
	auto_return			            1    	bslbf		eDec		eNA		eNA		0x1;
	reserved				6    	bslbf		eHidden	0x3F		vSet;
	duration	                                    33	uimsbf    	eDecHex;					 // 33-bit  90kHZ CLOCK SAFETY FIELD

}

macro SPLICE_TIME()
{
	time_specified_flag		            1    	bslbf		eDec		eNA		eNA		0x1;
	if (time_specified_flag)
	{
		reserved			6    	bslbf		eHidden	0x3F	vSet;
		pts_time			33	uimsbf    	eDecHex;	// 33-bit  90kHZ CLOCK SAFETY FIELD
	}
	else
	{
		reserved			7	bslbf		eHidden	0x7F	vSet;
	}

}

macro SEGMENTATION_DURATION()
{
	reserved				7	bslbf		eHidden	0x7F		vSet;
	duration				33	uimsbf		eDecHex;
}

macro SPLICE_NULL()
{
	//no data, as in spec
}

macro 	SPLICE_SCHEDULE()
{
	splice_count					8    	uimsbf		eHex		cIterations;    
	loop splices looplen(splice_count) eIterations
	 {
		splice_event_id                                  32	bslbf    	eDecHex; // 32-bit splice ident
		splice_event_cancel_indicator	1    	bslbf		eDec;
		reserved				7    	bslbf		eHidden	0x7F		vSet;
		if  ( splice_event_cancel_indicator  == 0x00 )
   	                     {
			  out_of_network_indicator	1    	bslbf		eDec;
			  program_splice_flag		1    	bslbf		eDec;
			  duration_flag		  	1    	bslbf		eDec;	
			  reserved			5    	bslbf		eHidden	0x1F		vSet;
			if (program_splice_flag == 1)
			{	
			   	utc_splice_time  	32      	uimsbf		eATSCTime;//inbuilt, sys time UTC=intg secs since Jan6th 1980
			}					

			if (program_splice_flag == 0)
			{
			   	component_count 	8    	uimsbf		eDecHex	cIterations;    
				loop components looplen(component_count ) eIterations
					{
					  component_tag	8    	uimsbf		eDecHex;    
					  utc_splice_time  	32      	uimsbf		eATSCTime;
					}
			}
			if (duration_flag == 1)
				{
				  BREAK_DURATION()
				}
			unique_program_id			16		uimsbf		eDecHex;//as event_id dvb
			avail					  8		uimsbf		eDecHex;//incr each event
			avail_count				  8		uimsbf		eDecHex;	
		         }//sp-evt-cancel
		} //splices
 }//splice schedule


macro SPLICE_INSERT()
{
		splice_event_id                                  32	bslbf    	eDecHex; // 32-bit splice ident
		splice_event_cancel_indicator	1    	bslbf		eDec;
		reserved				7    	bslbf		eHidden	0x7F		vSet;
		if  ( splice_event_cancel_indicator  == 0x00 )
   	                     {
			  out_of_network_indicator	1    	bslbf		eDec;
			  program_splice_flag		1    	bslbf		eDec;
			  duration_flag		  	1    	bslbf		eDec;
			  splice_immediate_flag	1    	bslbf		eDec;
			  reserved			4    	bslbf		eHidden	0xF		vSet;
			if ((program_splice_flag == 1) && (splice_immediate_flag == 0))
				
			     {	
			   	SPLICE_TIME()
			     }					

			if (program_splice_flag == 0)
			    {
			       	component_count 	8    	uimsbf		eDecHex	cIterations;    
				loop components looplen(component_count ) eIterations
					{
					  component_tag	8    	uimsbf		eDecHex;    
					  utc_splice_time  	32      	uimsbf		eATSCTime;
					}
			    }
			if (duration_flag == 1)
				{
				  BREAK_DURATION()
				}
			unique_program_id			16		uimsbf		eDecHex;//as event_id dvb
			avail_num				  8		uimsbf		eDecHex;//incr each event
			avails_expected			  8		uimsbf		eDecHex;//was avail_count		
		         }//sp-evt-cancel
 }//splice insert

macro TIME_SIGNAL()
{
		SPLICE_TIME()
}//time signal

macro BANDWIDTH_RESERVATION()
{
}//bandwidth reservation

macro AVAIL_DESCRIPTOR()   
{
		provider_avail_id				32		uimsbf		eDecHex; //network code to direct affiliate to blank
}

calculation cDtmfCount
{
	fnSTRLEN(DTMF_char)
}

validation vLength8
{
	8
}

validation vLength12
{
	12
}

macro DTMF_DESCRIPTOR()
{
	preroll				8		uimsbf		ePrerollTime;
	dtmf_count			3		uimsbf		eDec		cDtmfCount;
	reserved			5		uimsbf		eHidden	0x1F			vSet;
	DTMF_char			8		iso_latin	eNA		eNA			dtmf_count;
}

macro SEGMENTATION_DESCRIPTOR()
{
	segmentation_event_id					32	uimsbf		eDecHex;
	segmentation_event_cancel_indicator			1	bslbf		eDec;
	reserved							7	uimsbf		eHidden	0x7F		vSet;
	if (segmentation_event_cancel_indicator == 0)
	{
		program_segmentation_flag				1	bslbf		eDec;
		segmentation_duration_flag				1	bslbf		eDec;
		reserved						6	uimsbf		eHidden	0x3F		vSet;
		if (program_segmentation_flag == 0)
		{
			component_count				8	uimsbf		eDecHex	cIterations;

			loop components looplen(component_count) eIterations
			{
				component_tag			8	uimsbf		eDecHex;
				reserved				7	uimsbf		eHidden	0x7F		vSet;
				pts_offset				33	uimsbf		eDecHex;
			}
		}
		if (segmentation_duration_flag == 1)
		{
			SEGMENTATION_DURATION()
		}
		segmentation_upid_type				8	uimsbf		eSegmentationUpidType;	
		segmentation_upid_length				8	uimsbf		eDecHex	cNextByteLen;
		if (segmentation_upid_type == 0x02)
		{
			segmentation_upid				8	iso_latin	eNA	eNA	vLength8	"        ";
		}
		else if (segmentation_upid_type == 0x03)
		{
			segmentation_upid				8	iso_latin	eNA	eNA	vLength12	"            ";
		}
		else if (segmentation_upid_type == 0x05)
		{
			segmentation_upid				64	uimsbf		eHex;
		}
		else if (segmentation_upid_type == 0x07)
		{
			segmentation_upid				8	iso_latin	eNA	eNA	vLength12	"            ";
		}
		else
		{
			loop segmentation_upid looplen(segmentation_upid_length)
			{	
				segmentation_upid_byte		8	uimsbf		eDecHex;
			}
		}
		segmentation_type_id				8	uimsbf		eSegmentationType;
		chapter						8	uimsbf		eDecHex;
		chapter_count						8	uimsbf		eDecHex;
	}
}

macro	SPLICE_DESCRIPTOR()

{
	splice_descriptor_tag 			8    	uimsbf		eSpecificSpliceDescriptors;
	descriptor_length				8	uimsbf		eDecHex;
	if (descriptor_length >= 4)
	{
		identifier					32	uimsbf		eCUEI			eNA		eNA		0x43554549;

		if  (splice_descriptor_tag == 0x00 && identifier == 0x43554549) 
		{
			AVAIL_DESCRIPTOR()   	
		}
		else if (splice_descriptor_tag == 0x01 && identifier == 0x43554549)
		{
			DTMF_DESCRIPTOR()
		}
		else if (splice_descriptor_tag == 0x02 && identifier == 0x43554549)
		{
			SEGMENTATION_DESCRIPTOR()
		}
		else	//add more tests here as descriptors added to spec
		{
			rawbytes length (descriptor_length - 4)
		}
	}
	else
	{
		rawbytes length (descriptor_length)
	}
}



/////////////////////////////////////////////////////////////////////////////////
//
// DPI SPLICE TABLE
//
/////////////////////////////////////////////////////////////////////////////////
calculation cDPI_descriptors_len { fnBITLEN(dpi_descriptors) / 8 }

validation vDPI_splice_infoLength
{
	(fThis == fnBITSUM(fNext) / 8) && (fThis < 4094) && (fThis > 9)
}

section_id DPI_splice_info
{
	"Pid %u (0x%X)",pid,pid
}

access_key DPI_splice_info 
{
	pid,f0
}

table DPI_splice_info
{
    	group TableHeader
    	{
		table_id 				8    	uimsbf		eHex    	0xFC;
		section_syntax_indicator 		1    	bslbf		eDec		0x0		vClear;
		private_indicator			1    	bslbf		eHidden	0x0		vClear;
		reserved				2    	bslbf		eHidden	0x3		vSet;
		section_length			12  	uimsbf		eDecHex	cByteLen	vDPI_splice_infoLength	17;
    	}

	protocol_version				8    	uimsbf		eDecHex	0x0		vClear;
	encypted_packet				1    	bslbf		eDecHex	0x0		vClear;
	encryption_algorithm				6	uimsbf		eEncrypt_algorithms;
	pts_adjustment                                            	33	bslbf		eDecHex;			// 33-bit display in hex norm zero
	cw_index					8    	uimsbf		eDecHex;			//8 bit decrypt key
	reserved					12    	bslbf		eHidden	0xFFF		vSet;
	splice_command_length			12    	uimsbf	      	eDecHex; 			//New 2004 SCTE35 spec, 0FFF = undefined

	loop SpliceCommands looplen(1) eIterations
	{
		splice_command_type		8	uimsbf		eSpliceCommandType; //4 sched,5 is insert or edecHex number

		if  ( splice_command_type == 0x00 )
		{
			SPLICE_NULL()
		}

		if ( splice_command_type == 0x04 )
		{
			SPLICE_SCHEDULE()
		}

		if ( splice_command_type == 0x05 )
		{
			SPLICE_INSERT()
		}

		if (splice_command_type == 0x06 )
		{
			TIME_SIGNAL()
		}

		if (splice_command_type == 0x07 )
		{
			BANDWIDTH_RESERVATION()
		}
	}

	descriptor_loop_length		16    	uimsbf		eDec		cDPI_descriptors_len;
		
	loop dpi_descriptors looplen(descriptor_loop_length)	
	{
		SPLICE_DESCRIPTOR()
	}

	group alignment_stuffing
	{
		rawbytes length (section_length - 10 - (fnBITLEN(SpliceCommands)/8) - 2 - descriptor_loop_length - 4*encypted_packet - 4)
	}

	if ( encypted_packet == 0x01)
	{
		E_CRC_32				32   	rpchof		eHex;
	}

	CRC_32					32    	rpchof		eHex;
} //end table






	
// End of File /////////////////
