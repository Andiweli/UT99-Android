class UBrowserIRCUserList expands UWindowListBoxItem;

var string NickName;
var bool bChOp;
var bool bVoice;

function int Compare(UWindowList T, UWindowList B)
{
	local UBrowserIRCUserList UT, UB;

	UT = UBrowserIRCUserList(T);
	UB = UBrowserIRCUserList(B);

	if(UT.bChOp && !UB.bChOp)
		return -1;

	if(!UT.bChOp && UB.bChOp)
		return 1;

	if(UT.bVoice && !UB.bVoice)
		return -1;

	if(!UT.bVoice && UB.bVoice)
		return 1;

	if(Caps(UT.NickName) < Caps(UB.NickName))
		return -1;

	return 1;
}