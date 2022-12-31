
#include "DB_Common.scp"

association PMT
{
	assocStreamType		2	0x0B
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

macro DSMCC_MODULEINFO
{
	loop DSMCC_moduleInfo looplen(numberOfModules) eIterations
	{
		moduleId			16	uimsbf	eHex;
		moduleSize			32	uimsbf;
		moduleVersion		8	uimsbf;
		moduleInfoLength	8	uimsbf;
		loop moduleInfo looplen(moduleInfoLength)
		{
			moduleInfoByte	8	uimsbf	eHex;
		}
	}
}


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
// NOTE: If only two-layer object carousels are being used and more
// detail is required then uncomment the following lines and use them
// to replace the PRIVATEDATA macro statement above.
//    privateDataLength               16          uimsbf;
//    loop ServiceGatewayInfo looplen(privateDataLength)
//    {
//        IOR()
//
//        downloadTapsCount           8           uimsbf;
//        serviceContextListLength    8           uimsbf;
//        userInfoLength              16          uimsbf;
//    }


macro DSMCC_DII_BODY
{
    downloadId              32      uimsbf;
    blockSize               16      uimsbf;
    windowSize              8       uimsbf;
    ackPeriod               8       uimsbf;
    tCDownloadWindow        32      uimsbf;
    tCDownloadScenario      32      uimsbf;

    COMPATIBILITYDESCRIPTOR()

    numberOfModules         16      uimsbf;

    loop modules looplen(numberOfModules) eIterations
    {
        moduleId            16      uimsbf      eHex;
        moduleSize          32      uimsbf;
        moduleVersion       8       uimsbf;
        PRIVATEDATA("moduleInfo", "8")
    }

    PRIVATEDATA("privateData", "16")
}
// NOTE: If only two-layer object carousels are being used and more
// detail is required then uncomment the following lines and use them
// to replace the PRIVATEDATA("moduleInfo","8") macro statement above.
//        moduleInfoLength    8       uimsbf;
//
//        moduleTimeOut       32      uimsbf      eHex;
//        blockTimeOut        32      uimsbf      eHex;
//        minBlockTime        32      uimsbf;
//		
//        BIOP_CONNBINDER()
//
//        userInfoLength      8   uimsbf;
//        loop userInfo looplen(userInfoLength)
//        {
//            userInfoByte    8   uimsbf	eHex;
//        }
