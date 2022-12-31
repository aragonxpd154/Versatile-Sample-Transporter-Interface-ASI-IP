

valid_descriptors BAT
{
	0x83
}

enum eLogicalChannel
{
	0		"Undefined"
	1000..1023	"Reserved for future use"
	default		eDecHex
}

descriptor logical_channel_descriptor
{
	descriptor_tag		8	uimsbf		eDecHex	0x83;
	descriptor_length	8	uimsbf		eDecHex;

	loop channels looplen(descriptor_length)
	{
		service_id			16	uimsbf	eDecHex;
		reserved			6	bslbf	eHidden		0x3F	vSet;
		logical_channel_number	10	uimsbf	eLogicalChannel;
	}
}
