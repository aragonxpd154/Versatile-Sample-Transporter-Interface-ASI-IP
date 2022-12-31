
#include "MPEG_DSMCC_Macros.scp"
#include "DSMCC_Descriptors.scp"

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

    loop moduleInfo looplen(numberOfModules) eIterations
    {
	moduleId			16	uimsbf	eHex;
	moduleSize			32	uimsbf;
	moduleVersion		8	uimsbf;
	moduleInfoLength		8	uimsbf;

	rawbytes length (moduleInfoLength)

     }
    
     PRIVATEDATA("privateData","16")
}

#include "DSMCCv2.scp"
