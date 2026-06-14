class UMenuGameOptionsClientWindow extends UMenuPageWindow;

// Weapon Flash
var UWindowCheckbox WeaponFlashCheck;
var localized string WeaponFlashText;
var localized string WeaponFlashHelp;

// Weapon Hand
var UWindowComboControl WeaponHandCombo;
var localized string WeaponHandText;
var localized string WeaponHandHelp;

var localized string LeftName;
var localized string CenterName;
var localized string RightName;
var localized string HiddenName;

// Dodging
var UWindowCheckbox DodgingCheck;
var localized string DodgingText;
var localized string DodgingHelp;

// View Bob
var UWindowHSliderControl ViewBobSlider;
var localized string ViewBobText;
var localized string ViewBobHelp;

// Game Speed
var UWindowHSliderControl SpeedSlider;
var localized string SpeedText;

// Reduced Gore
var UWindowComboControl GoreCombo;
var localized string GoreText;
var localized string GoreHelp;
var localized string GoreLevels[3];

// Local Logging
var UWindowCheckbox LocalCheck;
var localized string LocalText;
var localized string LocalHelp;

// Android Touch Overlay
var UWindowCheckbox TouchOverlayCheck;
var localized string TouchOverlayText;
var localized string TouchOverlayHelp;
var globalconfig bool bTouchOverlay;

var globalconfig bool bShowGoreControl;

var float ControlOffset;

function Created()
{
	local int ControlWidth, ControlLeft, ControlRight;
	local int CenterWidth, CenterPos;
	local int I, S;

	Super.Created();

	ControlWidth = WinWidth/2.5;
	ControlLeft = (WinWidth/2 - ControlWidth)/2;
	ControlRight = WinWidth/2 + ControlLeft;

	CenterWidth = (WinWidth/4)*3;
	CenterPos = (WinWidth - CenterWidth)/2;

	// Weapon Hand
	WeaponHandCombo = UWindowComboControl(CreateControl(class'UWindowComboControl', CenterPos, ControlOffset, CenterWidth, 1));
	WeaponHandCombo.SetText(WeaponHandText);
	WeaponHandCombo.SetHelpText(WeaponHandHelp);
	WeaponHandCombo.SetFont(F_Normal);
	WeaponHandCombo.SetEditable(False);
	WeaponHandCombo.AddItem(LeftName, "Left");
	WeaponHandCombo.AddItem(CenterName, "Center");
	WeaponHandCombo.AddItem(RightName, "Right");
	WeaponHandCombo.AddItem(HiddenName, "Hidden");
	switch(GetPlayerOwner().Handedness)
	{
		case -1: WeaponHandCombo.SetSelectedIndex(2); break;
		case 0: WeaponHandCombo.SetSelectedIndex(1); break;
		case 1: WeaponHandCombo.SetSelectedIndex(0); break;
		case 2: WeaponHandCombo.SetSelectedIndex(3); break;
		default: WeaponHandCombo.SetSelectedIndex(2); break;
	}
	ControlOffset += 25;

	if ( class'GameInfo'.default.bAlternateMode )
		bShowGoreControl = false;

	if(bShowGoreControl)
	{
		// Reduced Gore
		GoreCombo = UWindowComboControl(CreateControl(class'UWindowComboControl', CenterPos, ControlOffset, CenterWidth, 1));
		GoreCombo.SetText(GoreText);
		GoreCombo.SetHelpText(GoreHelp);
		GoreCombo.SetFont(F_Normal);
		GoreCombo.SetEditable(False);
		GoreCombo.AddItem(GoreLevels[0]);
		GoreCombo.AddItem(GoreLevels[1]);
		GoreCombo.AddItem(GoreLevels[2]);

		if(class'GameInfo'.default.bVeryLowGore)
			GoreCombo.SetSelectedIndex(2);
		else
		if(class'GameInfo'.default.bLowGore)
			GoreCombo.SetSelectedIndex(1);
		else
			GoreCombo.SetSelectedIndex(0);
		ControlOffset += 25;
	}

	// View Bob
	ViewBobSlider = UWindowHSliderControl(CreateControl(class'UWindowHSliderControl', ControlRight, ControlOffset, ControlWidth, 1));
	ViewBobSlider.SetRange(0, 8, 1);
	ViewBobSlider.SetValue((GetPlayerOwner().Bob*1000) / 4);
	ViewBobSlider.SetText(ViewBobText);
	ViewBobSlider.SetHelpText(ViewBobHelp);
	ViewBobSlider.SetFont(F_Normal);
	ControlOffset += 25;

	// Game Speed
	if(GetLevel().Game != None)
	{
		SpeedSlider = UWindowHSliderControl(CreateControl(class'UWindowHSliderControl', CenterPos, ControlOffset, CenterWidth, 1));
		SpeedSlider.SetRange(50, 200, 5);
		S = GetLevel().Game.GameSpeed * 100.0;
		SpeedSlider.SetValue(S);
		SpeedSlider.SetText(SpeedText$" ["$S$"%]:");
		SpeedSlider.SetFont(F_Normal);
		ControlOffset += 25;
	}

	// Dodging
	DodgingCheck = UWindowCheckbox(CreateControl(class'UWindowCheckbox', ControlLeft, ControlOffset, ControlWidth, 1));
	DodgingCheck.bChecked = (GetPlayerOwner().DodgeClickTime > 0);
	DodgingCheck.SetText(DodgingText);
	DodgingCheck.SetHelpText(DodgingHelp);
	DodgingCheck.SetFont(F_Normal);
	DodgingCheck.Align = TA_Right;

	// Weapon Flash
	WeaponFlashCheck = UWindowCheckbox(CreateControl(class'UWindowCheckbox', ControlRight, ControlOffset, ControlWidth, 1));
	if (!GetPlayerOwner().bNoFlash)
		WeaponFlashCheck.bChecked = true;
	WeaponFlashCheck.SetText(WeaponFlashText);
	WeaponFlashCheck.SetHelpText(WeaponFlashHelp);
	WeaponFlashCheck.SetFont(F_Normal);
	WeaponFlashCheck.Align = TA_Right;
	ControlOffset += 25;

	// Android Touch Overlay
	TouchOverlayCheck = UWindowCheckbox(CreateControl(class'UWindowCheckbox', ControlLeft, ControlOffset, ControlWidth, 1));
	TouchOverlayCheck.bChecked = bTouchOverlay;
	TouchOverlayCheck.SetText(TouchOverlayText);
	TouchOverlayCheck.SetHelpText(TouchOverlayHelp);
	TouchOverlayCheck.SetFont(F_Normal);
	TouchOverlayCheck.Align = TA_Right;
	ControlOffset += 25;

	// UT99_ANDROID_V155_PREFERENCES_CLEANUP:
	// Hide obsolete ngStats local logging on Android.
}

function AfterCreate()
{
	Super.AfterCreate();
	DesiredWidth = 220;
	DesiredHeight = ControlOffset + 32;
}

function BeforePaint(Canvas C, float X, float Y)
{
	local int ControlWidth, ControlLeft, ControlRight;
	local int CenterWidth, CenterPos;

	ControlWidth = WinWidth/2.5;
	ControlLeft = (WinWidth/2 - ControlWidth)/2;
	ControlRight = WinWidth/2 + ControlLeft;

	CenterWidth = (WinWidth/4)*3;
	CenterPos = (WinWidth - CenterWidth)/2;

	WeaponFlashCheck.SetSize(ControlWidth, 1);
	WeaponFlashCheck.WinLeft = ControlRight;

	DodgingCheck.SetSize(ControlWidth, 1);
	DodgingCheck.WinLeft = ControlLeft;

	WeaponHandCombo.SetSize(CenterWidth, 1);
	WeaponHandCombo.WinLeft = CenterPos;
	WeaponHandCombo.EditBoxWidth = 90;

	ViewBobSlider.SetSize(CenterWidth, 1);
	ViewBobSlider.SliderWidth = 90;
	ViewBobSlider.WinLeft = CenterPos;

	if(SpeedSlider != None)
	{
		SpeedSlider.SetSize(CenterWidth, 1);
		SpeedSlider.SliderWidth = 90;
		SpeedSlider.WinLeft = CenterPos;
	}

	if(GoreCombo != None)
	{
		GoreCombo.SetSize(CenterWidth, 1);
		GoreCombo.WinLeft = CenterPos;
		GoreCombo.EditBoxWidth = 90;
	}

	if(TouchOverlayCheck != None)
	{
		TouchOverlayCheck.SetSize(ControlWidth, 1);
		TouchOverlayCheck.WinLeft = ControlLeft;
	}
}

function Notify(UWindowDialogControl C, byte E)
{
	switch(E)
	{
	case DE_Change:
		switch(C)
		{
		case WeaponFlashCheck:
			WeaponFlashChecked();
			break;
		case DodgingCheck:
			DodgingChecked();
			break;
		case WeaponHandCombo:
			WeaponHandChanged();
			break;
		case ViewBobSlider:
			ViewBobChanged();
			break;
		case SpeedSlider:
			SpeedChanged();
			break;
		case GoreCombo:
			GoreChanged();
			break;
		case TouchOverlayCheck:
			TouchOverlayChecked();
			break;
		}
	}
	Super.Notify(C, E);
}

function WeaponFlashChecked()
{
	GetPlayerOwner().bNoFlash = !WeaponFlashCheck.bChecked;
}

function DodgingChecked()
{
	if(DodgingCheck.bChecked)
		GetPlayerOwner().ChangeDodgeClickTime(0.25);
	else
		GetPlayerOwner().ChangeDodgeClickTime(-1.0);
}

function WeaponHandChanged()
{
	GetPlayerOwner().ChangeSetHand(WeaponHandCombo.GetValue2());
}

function ViewBobChanged()
{
	GetPlayerOwner().UpdateBob((ViewBobSlider.Value * 4) / 1000);
}

function SpeedChanged()
{
	local int S;

	S = SpeedSlider.GetValue();
	SpeedSlider.SetText(SpeedText$" ["$S$"%]:");
	if(GetLevel().Game != None)
		GetLevel().Game.SetGameSpeed(float(S) / 100.0);
}

function GoreChanged()
{
	local bool bLowGore, bVeryLowGore;

	switch(GoreCombo.GetSelectedIndex())
	{
	case 0:
		bLowGore = False;
		bVeryLowGore = False;
		break;
	case 1:
		bLowGore = True;
		bVeryLowGore = False;
		break;
	case 2:
		bLowGore = True;
		bVeryLowGore = True;
		break;
	}

	if (GetLevel().Game != None)
	{
		GetLevel().Game.bLowGore = bLowGore;
		GetLevel().Game.bVeryLowGore = bVeryLowGore;
	}

	class'GameInfo'.default.bLowGore = bLowGore;
	class'GameInfo'.default.bVeryLowGore = bVeryLowGore;
}

function SaveConfigs()
{
	GetPlayerOwner().SaveConfig();
	if ( GetLevel().Game != None )
	{
		GetLevel().Game.SaveConfig();
		GetLevel().Game.GameReplicationInfo.SaveConfig();
	}
	class'GameInfo'.static.StaticSaveConfig();
	Super.SaveConfigs();
}

function TouchOverlayChecked()
{
	bTouchOverlay = TouchOverlayCheck.bChecked;
	class'UMenuGameOptionsClientWindow'.default.bTouchOverlay = TouchOverlayCheck.bChecked;
	class'UMenuGameOptionsClientWindow'.static.StaticSaveConfig();
	SaveConfig();
}

function LocalChecked()
{
	if (GetLevel().Game != None)
		GetLevel().Game.bLocalLog = LocalCheck.bChecked;
}

defaultproperties
{
	SpeedText="Game Speed"
	GoreText="Gore Level"
	GoreHelp="Choose the level of gore you wish to see in the game."
	GoreLevels(0)="Normal"
	GoreLevels(1)="Reduced"
	GoreLevels(2)="Ultra-Low"
	WeaponFlashText="Weapon Flash"
	WeaponFlashHelp="If checked, your screen will flash when you fire your weapon."
	WeaponHandText="Weapon Hand"
	WeaponHandHelp="Select where your weapon will appear."
	LeftName="Left"
	CenterName="Center"
	RightName="Right"
	HiddenName="Hidden"
	DodgingText="Dodging"
	DodgingHelp="If checked, double tapping the movement keys (forward, back, and strafe left or right) will result in a fast dodge move."
	ViewBobText="View Bob"
	ViewBobHelp="Use the slider to adjust the amount your view will bob when moving."
	LocalText="ngStats Local Logging"
	LocalHelp="If checked, your system will log local botmatch and single player tournament games for stat compilation."
	TouchOverlayText="Touch Controls"
	TouchOverlayHelp="Shows the Android touch overlay for movement, looking, firing, jumping, crouching, weapon switching and menu access."
	ControlOffset=20
	bShowGoreControl=True
	bTouchOverlay=False
}