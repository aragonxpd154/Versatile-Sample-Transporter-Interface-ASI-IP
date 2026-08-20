

enum ePID
{
    0x1FFB			"PSIP"
}

enum eStreamType
{
    0x95			"0x%2X) (ATSC A90 Service Description"
}

enum AtscTableId
{
    0xC7			"0xC7 MGT"
    0xC8	 		"0xC8 TVCT"
    0xC9	 		"0xC9 CVCT"
    0xCA			"0xCA RRT"
    0xCB			"0xCB EIT"
    0xCC			"0xCC ETT"
    0xCD			"0xCD STT"
    0xCE			"0xCE DET"
    0xCF			"0xCF DST"
    0xD1			"0xD1 NRT"
    0xD2			"0xD2 LTST"
    0xD3			"0xD3 DCCT"
    0xD4			"0xD4 DCCSCT"
	0xD8			"0xD8 EAS"
}

enum TableType
{
    0x0000 		"Terrestrial VCT with current_next_indicator = 1"
    0x0001		"Terrestrail VCT with current_next_indicator = 0"
    0x0002 		"Cable VCT with current_next_indicator = 1"
    0x0003 		"Cable VCT with current_next_indicator = 0"
    0x0004 		"channel ETT"
    0x0005		"DCCSCT"
    0x0006..0x00FF  	"Reserved for future ATSC use"
    0x0100..0x017F  	"EIT-0 to EIT-127"
    0x0180..0x01FF  	"Reserved for future ATSC use"
    0x0200..0x027F  	"event ETT-0 to event ETT-127"
    0x0280..0x0300  	"Reserved for future ATSC use"
    0x0301..0x03FF  	"RRT with rating region 1-255"
    0x0400..0x0FFF  	"User private"
    0x1180		"Long Term Service Table (LTST)"
    0x1200..0x127F	"ETT-0 to ETT-127 associated with DET"
    0x1300..0x137F  	"DET-0 to DET-127"
    0x1400..0x14FF	"DCCT with dcc_id 0-255"
    default		"Reserved for future ATSC use"
}

enum eStreamFormat
{
	0x47413934		"0x%08X) (GA94 - ATSC A53"	// A53/D
	0x41432D33		"0x%08X) (AC-3"		// A53/D
}	


enum DaylightSavingsStatus
{
    0			"Not in daylight savings time"
    1			"In daylight savings time"
}

enum ModulationMode
{
    0x00			"Reserved"
    0x01			"analog"
    0x02			"SCTE_mode_1"
    0x03			"SCTE_mode_2"
    0x04			"ATSC (8 VSB)"
    0x05			"ATSC (16 VSB)"
    0x06..0x7F		"Reserved for future use by ATSC"
    0x80			"Modulation parameters are defined by a private descriptor"
    0x81..0xFF		"User private"
}

enum EtmLocation
{
    0x00			"No ETM"
    0x01			"ETM located in the PTC carrying this PSIP"
    0x02			"ETM located in the PTC carrying this event"
    0x03			"Reserved for future ATSC use"
}

enum ServiceType
{
    0x00			"Reserved"
    0x01			"analog_television"
    0x02			"ATSC_digital_television"
    0x03			"ATSC_audio_only"
    0x04			"ATSC_data_broadcast_service"
    0x05..0x3F		"Reserved for future ATSC use"
}

enum PathSelect
{
	0                          			"path 1"
	1                          			"path 2"
}

enum RatingRegion
{
    0x00			"Forbidden"
    0x01			"US (50 states + possessions)"
    0x02			"Canada"
    0x03			"Taiwan"
    0x04			"South Korea"
    0x05..0xFF		"Reserved"
}

enum esample_rate_code
{
    0x0           	"48kHz"
    0x1           	"44.1kHz"
    0x2           	"32kHz"
    0x3           	"reserved"
    0x4		"48kHz or 44.1kHz"
    0x5		"48kHz or 32kHz"
    0x6		"44.1kHz or 32kHz"
    0x7		"48kHz or 44.1kHz or 32kHz"
}

enum ebit_rate_code
{
    0x000           	"32 kbps (exact)"
    0x001           	"40 kbps (exact)"
    0x002           	"48 kbps (exact)"
    0x003           	"56 kbps (exact)"
    0x004           	"64 kbps (exact)"
    0x005           	"80 kbps (exact)"
    0x006           	"96 kbps (exact)"
    0x007           	"112 kbps (exact)"
    0x008           	"128 kbps (exact)"
    0x009           	"160 kbps (exact)"
    0x00A          	"192kbps (exact)"
    0x00B           	"224 kbps (exact)"
    0x00C           	"256 kbps (exact)"
    0x00D           	"320 kbps (exact)"
    0x00E           	"384 kbps (exact)"
    0x00F           	"448 kbps (exact)"
    0x010           	"512 kbps (exact)"
    0x011           	"576 kbps (exact)"
    0x012           	"640 kbps (exact)"
    0x20           	"32 kbps (Upper limit)"
    0x21           	"40 kbps (Upper limit)"
    0x22           	"48 kbps (Upper limit)"
    0x23           	"56 kbps (Upper limit)"
    0x24           	"64 kbps (Upper limit)"
    0x25           	"80 kbps (Upper limit)"
    0x26           	"96 kbps (Upper limit)"
    0x27           	"112 kbps (Upper limit)"
    0x28           	"128 kbps (Upper limit)"
    0x29           	"160 kbps (Upper limit)"
    0x2A          	"192kbps (Upper limit)"
    0x2B           	"224 kbps (Upper limit)"
    0x2C           	"256 kbps (Upper limit)"
    0x2D           	"320 kbps (Upper limit)"
    0x2E           	"384 kbps (Upper limit)"
    0x2F           	"448 kbps (Upper limit)"
    0x30		"512 kbps (Upper limit)"
    0x31           	"576 kbps (Upper limit)"
    0x32           	"640 kbps (Upper limit)"
    default		"Undefined"
}

enum esurround_mode
{
    0x0           	"Not indicated"
    0x1           	"Not Dolby Surround encoded"
    0x2           	"Dolby Surround encoded"
    0x3           	"reserved"
}

enum echannels
{
    0x0           	"1+1"
    0x1           	"1/0"
    0x2           	"2/0"
    0x3           	"3/0"
    0x4           	"2/1"
    0x5           	"3/1"
    0x6           	"2/2"
    0x7           	"3/2"
    0x8           	"1"
    0x9           	"<= 2"
    0xA           	"<= 3"
    0xB           	"<= 4"
    0xC           	"<= 5"
    0xD           	"<= 6"
    0xE           	"reserved"
    0xF           	"reserved"
}

enum AtscStreamType
{
    0x00		"Reserved"
    0x01		"MPEG-1 Video"
    0x02		"MPEG-2 Video"
    0x03 		"MPEG-1 Audio"
    0x04 		"MPEG-2 Audio"
    0x05 		"Private_sections"
    0x06		"PES private data"
    0x07		"MHEG"
    0x08		"Annex A DSM-CC"
    0x09		"ITU-T Rec. H.222.1"
    0x0A		"MPE"
    0x0B		"DSM-CC U-N messages"
    0x0C		"DSM-CC descriptors"
    0x0D		"DSM-CC sections"
    0x0E		"MPEG-2 auxiliary"
    0x0F		"Advanced audio (AAC)"
    0x10..0x13	"Reserved"
    0x14		"Non-streaming synchronized data"
    0x15..0x7F	"Reserved"
    0x80		"MPEG-2 Video"
    0x81		"ATSC A/53 Audio"
    0x82		"Standard subtitle"
    0x83		"Isochronous data"
    0x84		"Asynchronous data"
    0x85		"UPID"
    0x86..0x94	"Reserved by ATSC and SCTE"
    0x95		"Data Service Table/Network Resources Table"
    0x96..0xC1	"Reserved by ATSC and SCTE"
    0xC2		"Streaming Synchronized Data Stream"
    0xC3..0xFF	"User Private"
    default		"Unknown"
}

enum EasyReader
{
	0                      			"not tailored to the needs of beginning readers"
	1                      			"tailored to the needs of beginning readers"
}

enum WideAspectRatio
{
	0                      			"formatted for 4:3 aspect ratio"
	1                      			"formatted for 16:9 aspect ratio"
}

enum DccSelectionType
{
    0x00		"An unconditional channel change is requested. Value is not used"
    0x01		"Inclusion test on numeric postal codes"
    0x02		"Inclusion test on alphanumeric postal codes"
    0x03..0x04	"Reserved"
    0x05		"Test for membership in at least one indicated demographic category"
    0x06		"Test for membership of all indicated demographic categories"
    0x07		"Test for an interest in at least one indicated genre category"
    0x08		"Test for a interest in all indicated genre categories"
    0x09		"Cannot be authorized"
    0x0A..0x0B	"Reserved"
    0x0C		"Geographic location inclusion"
    0x0D		"Rating Blocked"
    0x0E		"Reserved"
    0x0F		"A return to previous Virtual Channel is requested if engaged in a DCC request"
    0x10		"Reserved"
    0x11		"Exclusion test on numeric postal codes"
    0x12		"Exclusion test on 8-character alphanumeric postal codes"
    0x13..0x14	"Reserved"
    0x15		"Test for non-membership in at least one indicated demographic category"
    0x16		"Test for non-membership in all the indicated demographic categories"
    0x17		"Test for non-interest in at least one indicated genre category"
    0x18		"Test for non-interest in all the indicated genre categories"
    0x19..0x1B	"Reserved"
    0x1C		"Geographic Location Exclusion"
    0x1D..0x1F	"Reserved"
    0x20..0x23	"Viewer Direct Select A-D"
    0x24..0xFF	"Reserved"
}

enum UpdateTypeCoding
{
    0x00	"Reserved"
    0x01	"new_genre_category - Genre table update"
    0x02	"new_state - Addition to state code data"
    0x03	"new_country - Addition to country code data"
    default	"Reserved for future use"
}

enum CategoricalGenreCode
{
    0x00	"Not Available"
    0x01..0x1F	"Reserved"
    0x20	"Education"
    0x21	"Entertainment"
    0x22	"Movie"
    0x23	"News"
    0x24	"Religious"
    0x25	"Sports"
    0x26	"Other"
    0x27	"Action"
    0x28	"Advertisement"
    0x29	"Animated"
    0x2A	"Anthology"
    0x2B	"Automobile"
    0x2C	"Awards"
    0x2D	"Baseball"
    0x2E	"Basketball"
    0x2F	"Bulletin"
    0x30	"Business"
    0x31	"Classical"
    0x32	"College"
    0x33	"Combat"
    0x34	"Comedy"
    0x35	"Commentary"
    0x36	"Concert"
    0x37	"Consumer"
    0x38	"Contemporary"
    0x39	"Crime"
    0x3A	"Dance"
    0x3B	"Documentary"
    0x3C	"Drama"
    0x3D	"Elementary"
    0x3E	"Erotica"
    0x3F	"Exercise"
    0x40	"Fantasy"
    0x41	"Farm"
    0x42	"Fashion"
    0x43	"Fiction"
    0x44	"Food"
    0x45	"Football"
    0x46	"Foreign"
    0x47	"Fund Raiser"
    0x48	"Game/Quiz"
    0x49	"Garden"
    0x4A	"Golf"
    0x4B	"Government"
    0x4C	"Health"
    0x4D	"High School"
    0x4E	"History"
    0x4F	"Hobby"
    0x50	"Hockey"
    0x51	"Home"
    0x52	"Horror"
    0x53	"Information"
    0x54	"Instruction"
    0x55	"International"
    0x56	"Interview"
    0x57	"Language"
    0x58	"Legal"
    0x59	"Live"
    0x5A	"Local"
    0x5B	"Math"
    0x5C	"Medical"
    0x5D	"Meeting"
    0x5E	"Military"
    0x5F	"Miniseries"
    0x60	"Music"
    0x61	"Mystery"
    0x62	"National"
    0x63	"Nature"
    0x64	"Police"
    0x65	"Politics"
    0x66	"Premier"
    0x67	"Prerecorded"
    0x68	"Product"
    0x69	"Professional"
    0x6A	"Public"
    0x6B	"Racing"
    0x6C	"Reading"
    0x6D	"Repair"
    0x6E	"Repeat"
    0x6F	"Review"
    0x70	"Romance"
    0x71	"Science"
    0x72	"Series"
    0x73	"Service"
    0x74	"Shopping"
    0x75	"Soap Opera"
    0x76	"Special"
    0x77	"Suspense"
    0x78	"Talk"
    0x79	"Technical"
    0x7A	"Tennis"
    0x7B	"Travel"
    0x7C	"Variety"
    0x7D	"Video"
    0x7E	"Weather"
    0x7F	"Western"
    0x80	"Auto Racing"
    0x82	"Aviation"
    0x83	"Biography"
    0x84	"Boating"
    0x85	"Bowling"
    0x86	"Boxing"
    0x87	"Cartoon"
    0x88	"Children"
    0x89	"Classic Film"
    0x8A	"Community"
    0x8B	"Computers"
    0x8C	"Country Music"
    0x8D	"Court"
    0x8E	"Extreme Sports"
    0x8F	"Family"
    0x90	"Financial"
    0x91	"Gymnastics"
    0x92	"Headlines"
    0x93	"Horse Racing"
    0x94	"Hunting/Fishing/Outdoors"
    0x95	"Independent"
    0x96	"Jazz"
    0x97	"Magazine"
    0x98	"Motorcycle Racing"
    0x99	"Music/Film/Books"
    0x9A	"News-International"
    0x9B	"News-Local"
    0x9C	"News-National"
    0x9D	"News-Regional"
    0x9E	"Olympics"
    0x9F	"Original"
    0xA0	"Performing Arts"
    0xA1	"Pets/Animals"
    0xA2	"Pop"
    0xA3	"Rock & Roll"
    0xA4	"Sci-Fi"
    0xA5	"Self Improvement"
    0xA6	"Sitcom"
    0xA7	"Skating"
    0xA8	"Skiing"
    0xA9	"Soccer"
    0xAA	"Track/Field"
    0xAB	"True"
    0xAC	"Volleyball"
    0xAD	"Wrestling"
    0xAE..0xFE	"Reserved (Detailed)"
    0xFF	"Null (not a category)"
}

enum DccDepartingRequestType
{
    0x00		"Reserved"
    0x01		"Cancel any outstanding departing request type and immediately perform a channel change"
    0x02		"Display departing request text in a centered window for a minimum of 10 seconds"
    0x03		"Display departing request text in a centered window indefinitely"
    0x04..0xFF	"Reserved"
}

enum DccArrivingRequestType
{
    0x00		"Reserved"
    0x01		"Display arriving request text in a centered window for a minimum of 10 seconds"
    0x02		"Display arriving request text in a centered window indefinitely"
    0x03..0xFF	"Reserved"
}

enum eCPCM_information
{
    0x00		"Retransmission outside the home not permitted"
    0x01		"Retransmission outside the home is permitted"
    default		"Reserved"
}

enum eCPCM_type
{
    0x00		"Basic CPCM Information"
    default		"Reserved"
}
