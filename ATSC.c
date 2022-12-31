

macro MultiStringStruct( __mssfld, __msslen)
{
    // This strange construct allows the Multiplexer to start with an
    // update_data_length of zero and still have the ability to add in MMS later.
    //
    // It uses strange cheats in the way that the Script Parser works, and my
    // advice is to leave it alone. MJW.

    if ( __msslen == 0)
    {
        loop __mssfld looplen ( __msslen > 0) eIterations
        {
            multi_string_struct			__mssfld
        }
    }
    else
    {
        multi_string_struct			__mssfld
    }
}

macro MultiStringStruct2( __mssfld, __msslen, __msspre)
{
    // This strange construct allows the Multiplexer to start with an
    // update_data_length of zero and still have the ability to add in MMS later.
    //
    // It uses strange cheats in the way that the Script Parser works, and my
    // advice is to leave it alone. MJW.

    if ( __msslen == __msspre)
    {
        loop __mssfld looplen ( __msslen > __msspre) eIterations
        {
            multi_string_struct			__mssfld
        }
    }
    else
    {
        multi_string_struct			__mssfld
    }
}

#include "ATSCEnums.scp"
#include "ATSCDescriptors.scp"
#include "SCTE127.scp"
#include "ATSC_PIT.SCP"

valid_descriptors PMT
{
    0x81,0x86,0x87,0xA3, 0xAA, 0xAD
}


/////////////////////////////////////////////////////////////////////
//
// ATSC System Time Table (STT)
//

valid_descriptors STT
{
     5, 0xAD
}

validation vSTTLength
{
    (section_length == fnBITSUM(fNext) / 8) && (section_length < 1022) && (section_length > 16)
}

association STT
{
    assocPid		0xCD		0x1FFB
    assocCycleTime	0xCD		1000
}

section_id STT
{
    "Version %3d Section %3d [Pid %u (0x%X)]",f7,f9,pid,pid
}

format eGPS_UTC_Offset
{
    default "%0.0f seconds", fThis;
}

table STT
{
    group table_header
    {
        table_id 			8    	uimsbf		AtscTableId	0xCD;
        section_syntax_indicator 	1    	bslbf		eDec		0x1		vSet;
        private_indicator	        	1    	bslbf		eHidden	0x1		vSet;
        reserved			2    	bslbf		eHidden	0x3		vSet;
        section_length			12    	uimsbf		eDecHex	cByteLen	vSTTLength;
        table_id_extension		16    	uimsbf		eDec		0x0000	vClear;
        reserved			2    	bslbf		eHidden	0x3		vSet;
        version_number		5    	uimsbf		eDec		0x00		vClear;
        current_next_indicator	1    	bslbf	    	eDec		0x1  		vSet;
        section_number		8    	uimsbf		eDec		0x00		vClear;
        last_section_number		8    	uimsbf		eDec		0x00		vClear;
    }

    protocol_version                		8       	uimsbf		eDecHex	0x00;
        system_time                     		32      	uimsbf		eATSCTime;
        GPS_UTC_offset                  		8	uimsbf		eGPS_UTC_Offset;

    group daylight_savings
    {
        DS_status			1	bslbf		DaylightSavingsStatus;
        reserved			2	bslbf		eHidden	0x3		vSet;
        DS_day_of_month		5	uimsbf		eDecHex;
        DS_hour			8	uimsbf		eDecHex;
    }

        loop descriptors looplen(section_length - 17)

    CRC_32				32    	rpchof		eHex;
}


/////////////////////////////////////////////////////////////////////
//
// ATSC Master Guide Table (MGT)

valid_descriptors MGT
{
     5, 0x80, 0xAD
}

validation vMGTLength
{
    (section_length == fnBITSUM(fNext) / 8) && (section_length < 4094) && (section_length > 13)
}

association MGT
{
    assocPid		0xC7		0x1FFB
    assocCycleTime	0xC7		150
}

section_id MGT
{
    "Version %3d Section %3d [Pid %u (0x%X)]",f7,f9,pid,pid
}

table MGT
{
        group table_header
    {
        table_id 			8    	uimsbf		AtscTableId	0xC7;
        section_syntax_indicator 	1    	bslbf		eDec		0x1		vSet;
        private_indicator	        	1    	bslbf		eHidden	0x1		vSet;
        reserved			2    	bslbf		eHidden	0x3		vSet;
        section_length			12    	uimsbf		eDec		cByteLen	vMGTLength;
        table_id_extension		16    	uimsbf		eDec		0x0000	vClear;
        reserved			2    	bslbf		eHidden	0x3		vSet;
        version_number		5    	uimsbf		eDecHex	0x00;
        current_next_indicator	1    	bslbf	    	eDec		0x1 	 	vSet;
        section_number		8    	uimsbf		eDec		0x00		vClear;
        last_section_number		8    	uimsbf		eDec		0x00		vClear;
    }

        protocol_version               		8       	uimsbf		eDecHex	0x00;
        tables_defined                  		16      	uimsbf		eDecHex	cIterations;

    loop tables looplen(tables_defined) eIterations
        {
                table_type                  		16     	uimsbf		TableType;
                reserved                    		3       	uimsbf      	eHidden     	0x7     		vSet;
                table_type_PID              	13      	uimsbf		eDecHex;
                reserved                    		3       	uimsbf      	eHidden     	0x7     		vSet;
            table_type_version_number   5       	uimsbf		eDecHex;
                number_bytes                	32      	uimsbf		eDecHex;
                reserved                    		4       	uimsbf		eHidden     	0xF     		vSet;
                table_type_descriptors_len  	12      	uimsbf		eDecHex	cDescriptors;

                loop descriptors looplen(table_type_descriptors_len)
        }

        reserved                        		4       	uimsbf      	eHidden     	0xF     		vSet;
        descriptors_length              		12      	uimsbf		eDecHex	cDescriptors;

        loop descriptors looplen(descriptors_length)

    CRC_32				32    	rpchof		eHex;
}


////////////////////////////////////////////////////////////////////
//
// ATSC Terrestrial Virtual Channel Table (TVCT)

valid_descriptors TVCT
{
    5, 0x80, 0xA0..0xA2, 0xAD
}

validation vTVCTLength
{
    (section_length == fnBITSUM(fNext) / 8) && (section_length < 1022) && (section_length > 12)
}

association TVCT
{
    assocPid		0xC8		0x1FFB
    assocCycleTime	0xC8		400
}

section_id TVCT
{
    "Version %3d Section %3d Transport Stream Id %5d [Pid %u (0x%X)]",f7,f9,f5,pid,pid
}

validation ShortNameLen
{
    7
}

table TVCT
{
    group table_header
    {
        table_id 			8    	uimsbf		AtscTableId		0xC8;
        section_syntax_indicator 	1    	bslbf		eDec			0x1		vSet;
        private_indicator	        	1    	bslbf		eHidden		0x1		vSet;
        reserved			2    	bslbf		eHidden		0x3		vSet;
        section_length			12 	uimsbf		eDecHex		cByteLen	vTVCTLength;
        transport_stream_id		16    	uimsbf		eDec			0x0000;
        reserved			2    	bslbf		eHidden		0x3		vSet;
        version_number		5    	uimsbf		eDecHex		0x00;
        current_next_indicator	1    	bslbf	    	eDec			0x1;
        section_number		8    	uimsbf		eDecHex		0x00;
        last_section_number		8    	uimsbf		eDecHex		0x00;
    }
    protocol_version			8	uimsbf		eDecHex		0x00;
    num_channels_in_section		8	uimsbf		eDecHex		cIterations;

    loop channels looplen(num_channels_in_section) eIterations
    {
        short_name			16	unicode	eNA			eNA		ShortNameLen	"       ";
        reserved			4	bslbf		eHidden		0xF		vSet;
        major_channel_number	10	uimsbf		eDecHex;
        minor_channel_number	10	uimsbf		eDecHex;
        modulation_mode		8	uimsbf		ModulationMode;
        carrier_frequency		32	uimsbf		eDecHex;
        channel_TSID			16	uimsbf		eDecHex;
        program_number		16	uimsbf		eDecHex;
        ETM_location			2	uimsbf		EtmLocation;
        access_controlled		1	bslbf		eDec;
        hidden				1	bslbf		eDec;
        reserved			2	bslbf		eHidden		0x03		vSet;
        hide_guide			1	bslbf		eDec;
        reserved			3	bslbf		eHidden		0x07		vSet;
        service_type			6	uimsbf		ServiceType;
        source_id			16	uimsbf		eDecHex;
        reserved			6	bslbf		eHidden		0x3F		vSet;
        descriptors_length		10	uimsbf		eDecHex		cDescriptors;

        loop descriptors looplen(descriptors_length)
    }

    reserved				6	bslbf		eHidden		0x3F	vSet;
    additional_descriptors_length	10	uimsbf		eDecHex		cDescriptors;

    loop descriptors looplen(additional_descriptors_length)

    CRC_32				32    	rpchof		eHex;
}

////////////////////////////////////////////////////////////////////
//
// ATSC Cable Virtual Channel Table (CVCT)

valid_descriptors CVCT
{
    5, 0x80, 0xA0..0xA2, 0xAD
}

validation vCVCTLength
{
    (section_length == fnBITSUM(fNext) / 8) && (section_length < 1022) && (section_length > 12)
}

association CVCT
{
    assocPid		0xC9		0x1FFB
    assocCycleTime	0xC9		400
}

section_id CVCT
{
    "Version %3d Section %3d Transport Stream Id %5d [Pid %u (0x%X)]",f7,f9,f5,pid,pid
}

table CVCT
{
    group table_header
    {
        table_id 			8    	uimsbf		AtscTableId	0xC9;
        section_syntax_indicator 	1    	bslbf		eDec		0x1		vSet;
        private_indicator	        	1    	bslbf		eHidden	0x1		vSet;
        reserved			2    	bslbf		eHidden	0x3		vSet;
        section_length			12    	uimsbf		eDecHex	cByteLen	vCVCTLength;
        transport_stream_id		16    	uimsbf		eDecHex	0x0000;
        reserved			2    	bslbf		eHidden	0x3		vSet;
        version_number		5    	uimsbf		eDecHex	0x00;
        current_next_indicator	1    	bslbf	    	eDec		0x01;
        section_number		8    	uimsbf		eDecHex	0x00;
        last_section_number		8    	uimsbf		eDecHex	0x00;
    }
    protocol_version			8	uimsbf		eDecHex	0x00;
    num_channels_in_section		8	uimsbf		eDecHex	cIterations;

    loop channels looplen(num_channels_in_section) eIterations
    {
        short_name			16	unicode	eNA		eNA		ShortNameLen	"       ";
        reserved			4	bslbf		eHidden	0xF		vSet;
        major_channel_number	10	uimsbf		eDecHex;
        minor_channel_number	10	uimsbf		eDecHex;
        modulation_mode		8	uimsbf		ModulationMode;
        carrier_frequency		32	uimsbf		eDecHex;
        channel_TSID			16	uimsbf		eDecHex;
        program_number		16	uimsbf		eDecHex;
        ETM_location			2	uimsbf		EtmLocation;
        access_controlled		1	bslbf		eDec;
        hidden				1	bslbf		eDec;
        path_select			1	uimsbf		PathSelect;
        out_of_band			1	bslbf		eDec;
        hide_guide			1	bslbf		eDec;
        reserved			3	bslbf		eHidden	0x07		vSet;
        service_type			6	uimsbf		ServiceType;
        source_id			16	uimsbf		eDecHex;
        reserved			6	bslbf		eHidden	0x3F		vSet;
        descriptors_length		10	uimsbf		eDecHex	cDescriptors;

        loop descriptors looplen(descriptors_length)
    }
    reserved				6	bslbf		eHidden	0x3F		vSet;
    additional_descriptors_length	10	uimsbf		eDecHex	cDescriptors;

    loop descriptors looplen(additional_descriptors_length)

    CRC_32				32    	rpchof		eHex;
}


////////////////////////////////////////////////////////////////////
//
// ATSC Rating Region Table (RRT)

validation vRRTLength
{
    (section_length == fnBITSUM(fNext) / 8) && (section_length < 1022) && (section_length > 13)
}

valid_descriptors RRT
{
    5, 0xAD
}

association RRT
{
    assocPid		0xCA		0x1FFB
    assocCycleTime	0xCA		60000
}

section_id RRT
{
    "Version %3d Section %3d [Pid %u (0x%X)]",f8,f10,pid,pid
}

table RRT
{
    group table_header
    {
        table_id 			8    	uimsbf		AtscTableId		0xCA;
        section_syntax_indicator 	1    	bslbf		eDec			0x1		vSet;
        private_indicator	        	1    	bslbf		eHidden		0x1		vSet;
        reserved			2    	bslbf		eHidden		0x3		vSet;
        section_length			12    	uimsbf		eDec			cByteLen	vRRTLength;
        reserved			8	bslbf		eHidden		0xFF		vSet;
        rating_region			8    	uimsbf		RatingRegion;
        reserved			2    	bslbf		eHidden		0x3		vSet;
        version_number		5    	uimsbf		eDecHex		0x00;
        current_next_indicator	1	bslbf	    	eDec			0x1  		vSet;
        section_number		8    	uimsbf		eDec			0x00		vClear;
        last_section_number		8    	uimsbf		eDec			0x00		vClear;
    }

    protocol_version			8	uimsbf		eDecHex		0x00;
    rating_region_name_length		8	uimsbf		eDecHex		cNextByteLen;
    MultiStringStruct("rating_region_name_text", "rating_region_name_length")
    dimensions_defined			8	uimsbf		eDecHex		cIterations;

    loop dimensions looplen(dimensions_defined) eIterations
    {
        dimension_name_length	8	uimsbf		eDecHex		cNextByteLen;
        MultiStringStruct("dimension_name_text", "dimension_name_length")
        reserved			3	bslbf		eHidden		0x7		vSet;
        graduated_scale		1	bslbf		eDec;
        values_defined		4	uimsbf		eDecHex		cIterations;

        loop values looplen(values_defined) eIterations
        {
            abbrev_rating_value_length	8	uimsbf		eDecHex	cNextByteLen;
            MultiStringStruct("abbrev_rating_value_text", "abbrev_rating_value_length")


            rating_value_length		8	uimsbf		eDecHex	cNextByteLen;
            MultiStringStruct("rating_value_text", "rating_value_length")
        }
    }
    reserved				6	bslbf		eHidden		0x3F		vSet;
    descriptors_length			10	uimsbf		eDec			cDescriptors;

    loop descriptors looplen(descriptors_length)

    CRC_32				32    	rpchof		eHex;
}

/////////////////////////////////////////////////////////////////////////////////
//
// ATSC Event Information Table (EIT)

valid_descriptors EIT
{
    5, 0x80, 0x81, 0x86, 0x87, 0xAA, 0xAB, 0xAD
}

validation vATSC_EITLength
{
    (section_length == fnBITSUM(fNext) / 8) && (section_length < 4094) && (section_length > 10)
}

section_id EIT
{
    "Version %3d Section %3d Source Id %5d [Pid %u (0x%X)]",f7,f9,f5,pid,pid
}

access_key EIT
{
    pid,f0,f5,f7,f9
}

table EIT
{
    group table_header
    {
        table_id 			8    	uimsbf		AtscTableId	0xCB;
        section_syntax_indicator 	1    	bslbf		eDec		0x1		vSet;
        private_indicator	        	1    	bslbf		eHidden	0x1		vSet;
        reserved			2    	bslbf		eHidden	0x3		vSet;
        section_length			12    	uimsbf		eDec		cByteLen	vATSC_EITLength;
        source_id			16	uimsbf;
        reserved				2	bslbf		eHidden	0x3		vSet;
        version_number		5	uimsbf;
        current_next_indicator	1	bslbf		eDec		1		vSet;
        section_number		8	uimsbf;
        last_section_number		8	uimsbf;
    }
    protocol_version			8	uimsbf;
    num_events_in_section		8	uimsbf		eDec		cIterations;

    loop event_information looplen(num_events_in_section) eIterations
    {
        reserved			2		uimsbf		eHidden	0x3	vSet;
        event_id			14		uimsbf;
        start_time			32		uimsbf		eATSCTime;
        reserved			2		uimsbf		eHidden	0x3	vSet;
        ETM_Location			2		uimsbf		EtmLocation;
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
// ATSC Extended Text Table (ETT)

validation vETTLength
{
    (section_length == fnBITSUM(fNext) / 8) && (section_length < 4094) && (section_length > 13)
}

access_key ETT
{
    pid,f0,f7,f9,f12 // PID + table_id + version_number + ETM_id
}

section_id ETT
{
    "Version %3d Section %3d ETM_Id 0x%8X [Pid %u (0x%X)]",f7,f9,f12,pid,pid
}

table ETT
{
    group table_header
    {
        table_id 			8    	uimsbf		AtscTableId	0xCC;
        section_syntax_indicator 	1    	bslbf		eDec		0x1		vSet;
        private_indicator	        	1    	bslbf		eHidden	0x1		vSet;
        reserved			2    	bslbf		eHidden	0x3		vSet;
        section_length			12    	uimsbf		eDecHex	cByteLen	vETTLength;
        table_id_extension		16    	uimsbf		eDec;
        reserved			2    	bslbf		eHidden	0x3		vSet;
        version_number		5    	uimsbf		eDecHex	0x00;
        current_next_indicator	1    	bslbf	    	eDec		0x1  		vSet;
        section_number		8    	uimsbf		eDecHex	0x00		vClear;
        last_section_number		8    	uimsbf		eDecHex	0x00		vClear;
    }

    protocol_version			8	uimsbf		eDecHex	0x00;
    ETM_id				32	uimsbf		eDecHex;
    multi_string_struct	extended_text_message
    CRC_32				32	rpchof		eHex;
}


/////////////////////////////////////////////////////////////////////////////////
//
// ATSC Directed Channel Change Table (DCCT)
access_key DCCT
{
    pid,f0,f5,f6,f8 // PID + table_id + dcc_subtype + dcc_id + version_number
}

valid_descriptors DCCT
{
    5, 0x80, 0xA8, 0xA9, 0xAD
}

validation vDCCTLength
{
    (section_length == fnBITSUM(fNext) / 8) && (section_length < 4094) && (section_length > 11)
}

association DCCT
{
    assocPid		0xD3		0x1FFB
    assocCycleTime	0xD3		150
}

section_id DCCT
{
    "Version %3d Section %3d ID %3d [Pid %u (0x%X)]",f8,f10,f6,pid,pid
}


table DCCT
{
    group table_header
    {
        table_id 				8    	uimsbf		AtscTableId		0xD3;
        section_syntax_indicator 		1    	bslbf		eDec			0x1		vSet;
        private_indicator			1    	bslbf		eHidden		0x1		vSet;
        reserved				2    	bslbf		eHidden		0x3		vSet;
        section_length				12    	uimsbf		eDec			cByteLen	vDCCTLength;
        dcc_subtype				8	uimsbf		eDec			0;
        dcc_id					8    	uimsbf		eDec;
        reserved				2    	bslbf		eHidden		0x3		vSet;
        version_number			5    	uimsbf		eDec		0x00;
        current_next_indicator		1    	bslbf	    	eDec		0x1  vSet;
        section_number			8    	uimsbf		eDec		0x00;
        last_section_number			8    	uimsbf		eDec		0x00;
    }

    protocol_version				8	uimsbf;
    dcc_test_count				8	uimsbf		eDec		cIterations;
    loop dcc_test looplen(dcc_test_count) eIterations
    {
        dcc_context				1	uimsbf;
        reserved				3	bslbf		eHidden		0x7		vSet;
        dcc_from_major_channel_number	10	uimsbf;
        dcc_from_minor_channel_number	10	uimsbf;
        reserved				4	bslbf		eHidden		0xF		vSet;
        dcc_to_major_channel_number	10	uimsbf;
        dcc_to_minor_channel_number	10	uimsbf;
        dcc_start_time				32	uimsbf		eATSCTime;
        dcc_end_time				32	uimsbf		eATSCTime;
        dcc_term_count			8	uimsbf		eDec		cIterations;

        loop term looplen(dcc_term_count) eIterations
        {
            dcc_selection_type		8	uimsbf		DccSelectionType;
            dcc_selection_id		64	uimsbf;
            reserved			6	bslbf		eHidden		0x3F		vSet;
            dcc_term_descriptors_length	10	uimsbf		eDec		cDescriptors;

            loop descriptors looplen(dcc_term_descriptors_length)
        }
        reserved				6	bslbf		eHidden		0x3F		vSet;
        dcc_test_descriptors_length			10	uimsbf		eDec		cDescriptors;

        loop descriptors looplen(dcc_test_descriptors_length)
    }
    reserved					6	bslbf		eHidden		0x3F		vSet;
    dcc_additional_descriptors_length		10	uimsbf		eDec		cDescriptors;

    loop descriptors looplen(dcc_additional_descriptors_length)
    CRC_32					32	rpchof		eHex;
}


/////////////////////////////////////////////////////////////////////////////////
//
// ATSC DCC Selection Code Table (DCCSCT)

valid_descriptors DCCSCT
{
    5, 0x80, 0xAB, 0xAD
}

validation vDCCSCTLength
{
    (section_length == fnBITSUM(fNext) / 8) && (section_length < 4094) && (section_length > 11)
}

validation vdcc_state
{
    (dcc_state_location_code >= 79) && (dcc_state_location_code <=99)
}

association DCCSCT
{
    assocPid		0xD4		0x1FFB
    assocCycleTime	0xD4		3600000
}

section_id DCCSCT
{
    "Version %3d Section %3d [Pid %u (0x%X)]",f7,f9,pid,pid
}

calculation cupdatelen
{
    (fnBITSUM(fNext) / 8) - 2 - dccsct_descriptors_length
}

table DCCSCT
{
    group table_header
    {
        table_id 					8    	uimsbf		AtscTableId		0xD4;
        section_syntax_indicator	 	1    	bslbf		eDec			0x1		vSet;
        private_indicator				1    	bslbf		eHidden		0x1		vSet;
        reserved					2    	bslbf		eHidden		0x3		vSet;
        section_length				12    	uimsbf		eDecHex		cByteLen	vDCCSCTLength;
        dccsct_type				16    	uimsbf		eDecHex		0x0000;
        reserved					2    	bslbf		eHidden		0x3		vSet;
        version_number				5    	uimsbf		eDecHex		0x00;
        current_next_indicator			1    	bslbf	    	eDec			0x1  		vSet;
        section_number				8    	uimsbf		eDecHex		0x00		vClear;
        last_section_number			8    	uimsbf		eDecHex		0x00		vClear;
    }

    protocol_version				8	uimsbf		eDecHex		0x00;
    updates_defined				8	uimsbf		eDecHex		cIterations;

    loop updates looplen (updates_defined) eIterations
    {
        update_type				8	uimsbf		UpdateTypeCoding;
        update_data_length			8	uimsbf		eDecHex		cupdatelen;

//        {
            if (update_type == 0x01)
            {
                genre_category_location_code	8	uimsbf		CategoricalGenreCode;
                MultiStringStruct2("genre_category_name_text", "update_data_length", "1")
            }

            if (update_type == 0x02)
            {
                dcc_state_location_code		8	uimsbf		eDecHex		eNA		vdcc_state;
                MultiStringStruct2("dcc_state_location_code_text", "update_data_length", "1")
            }

            if (update_type == 0x03)
            {
                state_code				8	uimsbf		eDecHex;
                reserved				6	bslbf		eHidden		0x3F		vSet;
                dcc_county_location_code		10	uimsbf		eDecHex;
                MultiStringStruct2("dcc_county_location_code_text", "update_data_length", "3")
            }
  //      }

        reserved					6	bslbf		eHidden		0x3F		vSet;
        dccsct_descriptors_length		10	uimsbf		eDecHex		cDescriptors;

        loop descriptors looplen(dccsct_descriptors_length)
    }

    reserved					6	bslbf		eHidden		0x3F		vSet;
    dccsct_additional_descriptors_length	10	uimsbf		eDecHex		cDescriptors;

    loop descriptors looplen(dccsct_additional_descriptors_length)

    CRC_32					32	rpchof		eHex;
}
