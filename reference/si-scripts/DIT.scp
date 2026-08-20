

validation vDITLength
{
	section_length == 0x1
}

section_id DIT
{
	"[Pid %4u (0x%X)]",pid,pid
}	

access_key DIT
{
	f0
}

table DIT
{
	__tableHeaderShort1("0x7E", "vDITLength")

	transition_flag				1   	uimsbf		eDec;
	reserved_future_use			7   	bslbf		eHidden		0x7F		vSet;
}		
