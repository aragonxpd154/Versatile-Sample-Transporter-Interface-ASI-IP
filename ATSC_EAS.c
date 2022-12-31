
//  a. changed private_indicator to "zero" not hidden, default to value zero
//  b.  changed field names from "reserved" to "reserved<x>" where x is a number
//

// RGM 2007-11-02 Widened alert_text_length to 16 bits per
// ANSI_SCTE 18 2007

#include "ATSCEnums.scp"

enum EASTableId
{
    0xD8            "0xD8 Cable Emergency Alert"
}

section_id EAS
{
    "Version %d Section %d [Pid %u (0x%X)]",f7,f9,pid,pid
}

enum eCountrySub
{
    0x00            "All or an unspecified portion of a county"
    0x01            "Northwest"
    0x02            "North Central"
    0x03            "Northeast"
    0x04            "West Central"
    0x05            "Central"
    0x06            "East Central"
    0x07            "Southeast"
    0x08            "South Central"
    0x09            "Southeast"
    0x0A..0x0F      "Reserved"
}

enum ePriority
{
    0x00            "0x%02X) (Test message"
    0x01..0x02      "0x%02X) (Reserved"
    0x03            "0x%02X) (Low priority"
    0x04..0x06      "0x%02X) (Reserved"
    0x07            "0x%02X) (Medium priority"
    0x08..0x0A      "0x%02X) (Reserved"
    0x0B            "0x%02X) (High priority"
    0x0C..0x0E      "0x%02X) (Reserved"
    0x0F            "0x%02X) (Maximum priority"
}

enum eEASdescriptor
{
  0x00          "0x%02X) (in-band details channel descriptor"
  0x01          "0x%02X) (in-band exceptions channel descriptor"
  0x02          "0x%02X) (audio file descriptor"
  0xAD          "0x%02X) (ATSC private information descriptor"
  0xC0..0xFF    "0x%02X) (user private"
  default       "0x%02X) (reserved for future use"
}

enum eEASAudioFormat
{
  0x00          "0x%02X) (reserved"
  0x01          "0x%02X) (AIFF basic"
  0x02          "0x%02X) (AIFF extended"
  0x03          "0x%02X) (WAV basic"
  0x04          "0x%02X) (WAV extended"
  0x05          "0x%02X) (MP3 basic"
  0x06          "0x%02X) (MP3 extended"
  0x07..0x3F    "0x%02X) (reserved for future use"
  default       "0x%02X) (private use"
}

enum eEASAudioSource
{
  0x00          "0x%02X) (reserved"
  0x01          "0x%02X) (out-of-band DSM-CC object carousel"
  0x02          "0x%02X) (out-of-band DSM-CC data carousel"
  0x03..0x7F    "0x%02X) (reserved for future use"
  default       "0x%02X) (private use"  
}

enum eInband
{
    0x00    "out-of-band"
    0x01    "in-band"
}

macro in_band_details_channels_descriptor()
{
    details_RF_channel      8   uimsbf      eDecHex;
    details_program_number  16  uimsbf      eDecHex;
}

macro in_band_exception_channels_descriptor()
{
    exception_channel_count 8   uimsbf      eDecHex cIterations;
    loop exception_channel looplen(exception_channel_count) eIterations
    {
        exception_RF_channel        8   uimsbf      eDecHex;
        exception_program_number    16  uimsbf      eDecHex;
    }
}

macro audio_file_descriptor()
{
    number_of_audio_sources     8   uimsbf      eDecHex cIterations;
    loop audio_sources looplen(number_of_audio_sources) eIterations
    {
        loop_length             8   uimsbf      eDecHex cByteLen;
        file_name_present       1   bslbf       eDecHex;
        audio_format            7   uimsbf      eEASAudioFormat;
        if (file_name_present == 1)
        {
            file_name_length    8   uimsbf      eDecHex cTextLen;
            file_name           8   iso_latin   eNA     eNA file_name_length;
        }
        audio_source            8   uimsbf      eEASAudioSource;
        if (audio_source == 0x01)
        {
            program_number      16  uimsbf      eDecHex;
            carousel_id         32  uimsbf      eDecHex;
            application_id      16  uimsbf      eDecHex;
        }
        else if (audio_source == 0x02)
        {
            program_number      16  uimsbf      eDecHex;
            download_id         32  uimsbf      eDecHex;
            module_id           32  uimsbf      eDecHex;
            application_id      16  uimsbf      eDecHex;
        }
        else if (file_name_present == 1)
        {
            loop reserved_bytes looplen(loop_length - 3 - file_name_length)
            {
                reserved_byte   8   uimsbf      eDecHex;
            }
        }
        else
        {
            loop reserved_bytes looplen(loop_length - 2)
            {
                reserved_byte   8   uimsbf      eDecHex;
            }
        }
    }
}

macro user_private_descriptor
{
    company_ID              24  uimsbf      eDecHex;
    loop private_bytes looplen(eas_descriptor_length - 3)
    {
        private_data_byte   8   uimsbf      eDecHex;
    }
}

macro eas_ATSC_private_information_descriptor
{
    format_identifier           32  uimsbf      eStreamFormat;
    loop private_data looplen(eas_descriptor_length - 4)
    {
        private_data_byte   8   uimsbf      eDecHex;
    }
}

macro reserved_descriptor
{
    loop reserved_bytes looplen(eas_descriptor_length)
    {
        reserved_data_byte  8   uimsbf      eDecHex;
    }
}

table EAS
{
    group Table_Header
    {
        table_id                        8       uimsbf      AtscTableId 0xD8;
        section_syntax_indicator        1       bslbf       eDec        0x1     vSet;
        zero                            1       bslbf       eDec        0x0     vClear;
        reserved1                       2       bslbf       eHidden     0x3     vSet;
        section_length                  12      uimsbf      eDecHex     cByteLen;
        table_id_extension              16      uimsbf      eDec        0x0000  vClear;
        reserved2                       2       bslbf       eHidden     0x3     vSet;
        version_number                  5       uimsbf      eDec;
        current_next_indicator          1       bslbf       eDec        0x1     vSet;
        section_number                  8       uimsbf      eDec        0x00    vClear;
        last_section_number             8       uimsbf      eDec        0x00    vClear; 
    }

    protocol_version                    8       uimsbf      eDecHex     0x00;
    EAS_Event_ID                        16      uimsbf      eDecHex;
    EAS_originator_code                 24      bslbf       eISOLatin   eNA     eNA 0x45414E; // EAN
    EAS_event_code_length               8       uimsbf      eDec        cTextLen;
    EAS_event_code                      8       iso_latin   eNA         eNA     EAS_event_code_length;
    nature_of_activation_text_length    8       uimsbf      eDecHex     cNextByteLen;
    MultiStringStruct("nature_of_activation_text", "nature_of_activation_text_length")
    alert_message_time_remaining        8       uimsbf      eDec;
    event_start_time                    32      uimsbf      eATSCTime;
    event_duration                      16      uimsbf      eDecHex;
    reserved3                           12      bslbf       eHidden     0xFFF   vSet;
    alert_priority                      4       uimsbf      ePriority;
    details_OOB_source_ID               16      uimsbf      eDecHex;
    reserved4                           6       bslbf       eHidden     0x3F    vSet;
    details_major_channel_number        10      uimsbf      eDecHex;
    reserved5                           6       bslbf       eHidden     0x3F    vSet;
    details_minor_channel_number        10      uimsbf      eDecHex;
    audio_OOB_source_ID                 16      uimsbf      eDecHex;
    alert_text_length                   16      uimsbf      eDecHex     cNextByteLen;
    MultiStringStruct("alert_text", "alert_text_length")
    location_code_count                 8       uimsbf      eDec        cIterations;    
    loop locations looplen(location_code_count) eIterations
    {
        state_code                      8       uimsbf      eDec;
        county_subdivision              4       uimsbf      eCountrySub;
        reserved6                       2       bslbf       eHidden     0x3     vSet;
        county_code                     10      uimsbf      eDecHex;
    }       
    exception_count                     8       uimsbf      eDec        cIterations;    
    loop exceptions looplen(exception_count) eIterations
    {
        in_band_reference               1       uimsbf      eInband;
        reserved7                       7       bslbf       eHidden     0x7F    vSet;
        if (in_band_reference == 1)
        {
            reserved8                   6       bslbf       eHidden     0x3F    vSet;
            exception_major_channel_number  10  uimsbf      eDecHex;
            reserved9                   6       bslbf       eHidden     0x3F    vSet;
            exception_minor_channel_number  10  uimsbf      eDecHex;
        }
        else {
            reserved                    16      bslbf       eHidden     0xFFFF  vSet;
            exception_OOB_source_ID     16      uimsbf      eDecHex;
        }
    }       
    reserved10                          6       bslbf       eHidden     0x3F    vSet;
    descriptors_length                  10      uimsbf      eDec        cNextByteLen;
    loop eas_descriptors looplen(descriptors_length)
    {
        eas_descriptor_tag              8       uimsbf      eEASdescriptor;
        eas_descriptor_length           8       uimsbf      eDecHex     cByteLen;
        if (eas_descriptor_tag == 0)
        {
            in_band_details_channels_descriptor()
        }
        else if (eas_descriptor_tag == 1)
        {
            in_band_exception_channels_descriptor()
        }
        else if (eas_descriptor_tag == 2)
        {
            audio_file_descriptor()
        }
        else if (eas_descriptor_tag == 0xAD)
        {
            eas_ATSC_private_information_descriptor()
        }
        else if (eas_descriptor_tag >= 0xC0)
        {
            user_private_descriptor()
        }
        else
        {
            reserved_descriptor()
        }
    }
    CRC_32              32      rpchof      eHex;
}