
#include "DB_Common.scp"

// This file is dependent on the  DC_DESCRIPTOR macro being defined:
// Either ISDB_DSMCC_Macros.scp
// or MPEG_DSMCC_Macros.scp must precede this script

association PMT
{
	assocStreamType 	2	0x0B
	assocStreamType 	2	0x0D
}

//
// Enums
//

enum eMessageId
{
	0x1001	"DownloadInfoRequest"
	0x1002	"DownloadInfoIndication"
	0x1003	"DownloadDataBlock"
	0x1004	"DownloadDataRequest"
	0x1005	"DownloadCancel"
	0x1006	"DownloadServerInitiate"
	default eHex
}
enum eAdaptType
{
	0x01		"DSM-CC Conditional Access adaptation format"
	0x02		"DSM-CC User ID adaptation format"
	0x04		"DSM-CC Synchronised Download Protocol format"
	0x80..0xFF	"User Defined adaptation type"
	default		"ISO/IEC 13818-6 Reserved"
}

enum eDsmccType
{
	0x01		"U-N configuration message"
	0x02		"U-N session message"
	0x03		"Download message"
	0x04		"SDB channel change protocol message"
	0x05		"U-N pass-thru message"
	0x06		"SMPTE 325M opportunistic flow control protocol"
	0x80..0xFF	"User Defined message type"
	default		"ISO/IEC 13818-6 Reserved"
}
	
//
// Macros
//

macro CRC { CRC_32	32 	rpchof	eHex;}

// DDB messages call the fourth field downloadId,
// everything else calls it transactionId
macro DSMCC_MESSAGEHEADER(_MESSAGEID_)
{
    group dsmccMessageHeader
    {
        protocolDiscriminator           8   uimsbf;
        dsmccType                       8   uimsbf  eDsmccType;
        messageId                       16  uimsbf  eMessageId  eNA eNA _MESSAGEID_;

        if (messageId == 0x1003)
        {
            downloadId                  32  uimsbf  eDecHex;
        }
        else
        {
            transactionId               32  uimsbf  eDecHex;
        }
        reserved                        8   uimsbf;
        adaptationLength                8   uimsbf;
        messageLength                   16  uimsbf;

        if (adaptationLength > 0)
        {
            adaptationType              8   uimsbf	eAdaptType;

            loop adaptationHeader looplen(adaptationLength - 1)
            {
                adaptationHeaderByte    8   uimsbf  eDecHex;
            }
        }
    }
}


macro DSMCC_DDB_BODY
{
	moduleId			16	uimsbf	eHex;
	moduleVersion		8	uimsbf;
	reserved			8	uimsbf;
	blockNumber			16	uimsbf;
	
	rawbytes length(messageLength - adaptationLength - 6)
}


macro DSMCC_DSI_BODY
{
	// All set for data carousel, NSAP address for object carousels
	loop serverId looplen(5) eIterations
	{
		serverId_long		32	uimsbf		eHex;
	}
	COMPATIBILITYDESCRIPTOR()

	PRIVATEDATA("privateData","16")
}


