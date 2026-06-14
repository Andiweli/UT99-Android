class UMenuCustomizeClientWindow extends UMenuPageWindow;

var localized string LocalizedKeyName[255];
var string RealKeyName[255];
var int BoundKey1[70];
var int BoundKey2[70];
var UMenuLabelControl KeyNames[70];
var UMenuRaisedButton KeyButtons[70];
var UMenuRaisedButton SelectedButton;
var localized string LabelList[70];
var string AliasNames[70];
var int Selection;
var bool bPolling;
var localized string OrString;
var localized string CustomizeHelp;

var UWindowSmallButton DefaultsButton;
var localized string DefaultsText;
var localized string DefaultsHelp;

var UMenuLabelControl JoystickHeading;
var localized string JoystickText;

var UWindowComboControl JoyXCombo;
var localized string JoyXText;
var localized string JoyXHelp;
var localized string JoyXOptions[2];
var string JoyXBinding[2];

var UWindowComboControl JoyYCombo;
var localized string JoyYText;
var localized string JoyYHelp;
var localized string JoyYOptions[2];
var string JoyYBinding[2];

var int AliasCount;
var bool bLoadedExisting;
var bool bJoystick;
var float JoyDesiredHeight, NoJoyDesiredHeight;

function Created()
{
	local int ButtonWidth, ButtonLeft, ButtonTop, I, J, pos;
	local int LabelWidth, LabelLeft;
	local UMenuLabelControl Heading;
	local bool bTop;

	bIgnoreLDoubleClick = True;
	bIgnoreMDoubleClick = True;
	bIgnoreRDoubleClick = True;

	bJoystick =	bool(GetPlayerOwner().ConsoleCommand("get windrv.windowsclient usejoystick"));

	Super.Created();

	SetAcceptsFocus();

	ButtonWidth = WinWidth - 140;
	ButtonLeft = WinWidth - ButtonWidth - 40;

	LabelWidth = WinWidth - 100;
	LabelLeft = 20;

	// Defaults Button
	DefaultsButton = UWindowSmallButton(CreateControl(class'UWindowSmallButton', 30, 10, 48, 16));
	DefaultsButton.SetText(DefaultsText);
	DefaultsButton.SetFont(F_Normal);
	DefaultsButton.SetHelpText(DefaultsHelp);
	
	ButtonTop = 25;
	bTop = True;
	for (I=0; I<ArrayCount(AliasNames); I++)
	{
		if(AliasNames[I] == "")
			break;

		j = InStr(LabelList[I], ",");
		if(j != -1)
		{
			if(!bTop)
				ButtonTop += 10;
			Heading = UMenuLabelControl(CreateControl(class'UMenuLabelControl', LabelLeft-10, ButtonTop+3, WinWidth, 1));
			Heading.SetText(Left(LabelList[I], j));
			Heading.SetFont(F_Bold);
			LabelList[I] = Mid(LabelList[I], j+1);
			ButtonTop += 19;
		}
		bTop = False;

		KeyNames[I] = UMenuLabelControl(CreateControl(class'UMenuLabelControl', LabelLeft, ButtonTop+3, LabelWidth, 1));
		KeyNames[I].SetText(LabelList[I]);
		KeyNames[I].SetHelpText(CustomizeHelp);
		KeyNames[I].SetFont(F_Normal);
		KeyButtons[I] = UMenuRaisedButton(CreateControl(class'UMenuRaisedButton', ButtonLeft, ButtonTop, ButtonWidth, 1));
		KeyButtons[I].SetHelpText(CustomizeHelp);
		KeyButtons[I].bAcceptsFocus = False;
		KeyButtons[I].bIgnoreLDoubleClick = True;
		KeyButtons[I].bIgnoreMDoubleClick = True;
		KeyButtons[I].bIgnoreRDoubleClick = True;
		ButtonTop += 19;
	}
	AliasCount = I;

	NoJoyDesiredHeight = ButtonTop + 10;

	// Joystick
	ButtonTop += 10;
	JoystickHeading = UMenuLabelControl(CreateControl(class'UMenuLabelControl', LabelLeft-10, ButtonTop+3, WinWidth, 1));
	JoystickHeading.SetText(JoystickText);
	JoystickHeading.SetFont(F_Bold);
	LabelList[I] = Mid(LabelList[I], j+1);
	ButtonTop += 19;

	JoyXCombo = UWindowComboControl(CreateControl(class'UWindowComboControl', 20, ButtonTop, WinWidth - 40, 1));
	JoyXCombo.CancelAcceptsFocus();
	JoyXCombo.SetText(JoyXText);
	JoyXCombo.SetHelpText(JoyXHelp);
	JoyXCombo.SetFont(F_Normal);
	JoyXCombo.SetEditable(False);
	JoyXCombo.AddItem(JoyXOptions[0]);
	JoyXCombo.AddItem(JoyXOptions[1]);
	JoyXCombo.EditBoxWidth = ButtonWidth;
	ButtonTop += 20;

	JoyYCombo = UWindowComboControl(CreateControl(class'UWindowComboControl', 20, ButtonTop, WinWidth - 40, 1));
	JoyYCombo.CancelAcceptsFocus();
	JoyYCombo.SetText(JoyYText);
	JoyYCombo.SetHelpText(JoyYHelp);
	JoyYCombo.SetFont(F_Normal);
	JoyYCombo.SetEditable(False);
	JoyYCombo.AddItem(JoyYOptions[0]);
	JoyYCombo.AddItem(JoyYOptions[1]);
	JoyYCombo.EditBoxWidth = ButtonWidth;
	ButtonTop += 20;

	LoadExistingKeys();

	DesiredWidth = 220;
	JoyDesiredHeight = ButtonTop + 10;
	DesiredHeight = JoyDesiredHeight;
}

function WindowShown()
{
	Super.WindowShown();
	bJoystick =	bool(GetPlayerOwner().ConsoleCommand("get windrv.windowsclient usejoystick"));
}

function LoadExistingKeys()
{
	local int I, J, pos;
	local string KeyName;
	local string Alias;

	for (I=0; I<AliasCount; I++)
	{
		BoundKey1[I] = 0;
		BoundKey2[I] = 0;
	}

	for (I=0; I<255; I++)
	{
		KeyName = GetPlayerOwner().ConsoleCommand( "KEYNAME "$i );
		RealKeyName[i] = KeyName;
		if ( KeyName != "" )
		{
			Alias = GetPlayerOwner().ConsoleCommand( "KEYBINDING "$KeyName );
			if ( Alias != "" )
			{
				pos = InStr(Alias, " ");
				if ( pos != -1 )
				{
					if( !(Left(Alias, pos) ~= "taunt") &&
						!(Left(Alias, pos) ~= "getweapon") &&
						!(Left(Alias, pos) ~= "viewplayernum"))
						Alias = Left(Alias, pos);
				}
				for (J=0; J<AliasCount; J++)
				{
					if ( AliasNames[J] ~= Alias && AliasNames[J] != "None" )
					{
						if ( BoundKey1[J] == 0 )
							BoundKey1[J] = i;
						else
						if ( BoundKey2[J] == 0)
							BoundKey2[J] = i;
					}
				}
			}
		}
	}

	bLoadedExisting = False;
	Alias = GetPlayerOwner().ConsoleCommand( "KEYBINDING JoyX" );
	if(Alias ~= JoyXBinding[0])
		JoyXCombo.SetSelectedIndex(0);
	if(Alias ~= JoyXBinding[1])
		JoyXCombo.SetSelectedIndex(1);

	Alias = GetPlayerOwner().ConsoleCommand( "KEYBINDING JoyY" );
	if(Alias ~= JoyYBinding[0])
		JoyYCombo.SetSelectedIndex(0);
	if(Alias ~= JoyYBinding[1])
		JoyYCombo.SetSelectedIndex(1);
	bLoadedExisting = True;
}

function BeforePaint(Canvas C, float X, float Y)
{
	local int ButtonWidth, ButtonLeft, I;
	local int LabelWidth, LabelLeft;

	ButtonWidth = WinWidth - 135;
	ButtonLeft = WinWidth - ButtonWidth - 20;

	DefaultsButton.AutoWidth(C);
	DefaultsButton.WinLeft = ButtonLeft + ButtonWidth - DefaultsButton.WinWidth;

	LabelWidth = WinWidth - 100;
	LabelLeft = 20;

	if(bJoystick)
	{
		DesiredHeight = JoyDesiredHeight;

		JoystickHeading.ShowWindow();
		JoyXCombo.ShowWindow();
		JoyYCombo.ShowWindow();

		JoyXCombo.SetSize(WinWidth - 40, 1);
		JoyXCombo.EditBoxWidth = ButtonWidth;

		JoyYCombo.SetSize(WinWidth - 40, 1);
		JoyYCombo.EditBoxWidth = ButtonWidth;
	}
	else
	{
		DesiredHeight = NoJoyDesiredHeight;

		JoystickHeading.HideWindow();
		JoyXCombo.HideWindow();
		JoyYCombo.HideWindow();
	}

	for (I=0; I<AliasCount; I++)
	{
		KeyButtons[I].SetSize(ButtonWidth, 1);
		KeyButtons[I].WinLeft = ButtonLeft;

		KeyNames[I].SetSize(LabelWidth, 1);
		KeyNames[I].WinLeft = LabelLeft;
	}

	for (I=0; I<AliasCount; I++ )
	{
		if ( BoundKey1[I] == 0 )
			KeyButtons[I].SetText("");
		else
		if ( BoundKey2[I] == 0 )
			KeyButtons[I].SetText(LocalizedKeyName[BoundKey1[I]]);
		else
			KeyButtons[I].SetText(LocalizedKeyName[BoundKey1[I]]$OrString$LocalizedKeyName[BoundKey2[I]]);
	}
}

function KeyDown( int Key, float X, float Y )
{
	if (bPolling)
	{
		ProcessMenuKey(Key, RealKeyName[Key]);
		bPolling = False;
		SelectedButton.bDisabled = False;
	}
}

function RemoveExistingKey(int KeyNo, string KeyName)
{
	local int I;

	// Remove this key from any existing binding display
	for ( I=0; I<AliasCount; I++ )
	{
		if(I != Selection)
		{
			if ( BoundKey2[I] == KeyNo )
				BoundKey2[I] = 0;

			if ( BoundKey1[I] == KeyNo )
			{
				BoundKey1[I] = BoundKey2[I];
				BoundKey2[I] = 0;
			}
		}
	}
}

function SetKey(int KeyNo, string KeyName)
{
	if ( BoundKey1[Selection] != 0 )
	{

		// if this key is already chosen, just clear out other slot
		if(KeyNo == BoundKey1[Selection])
		{
			// if 2 exists, remove it it.
			if(BoundKey2[Selection] != 0)
			{
				GetPlayerOwner().ConsoleCommand("SET Input "$RealKeyName[BoundKey2[Selection]]);
				BoundKey2[Selection] = 0;
			}
		}
		else 
		if(KeyNo == BoundKey2[Selection])
		{
			// Remove slot 1
			GetPlayerOwner().ConsoleCommand("SET Input "$RealKeyName[BoundKey1[Selection]]);
			BoundKey1[Selection] = BoundKey2[Selection];
			BoundKey2[Selection] = 0;
		}
		else
		{
			// Clear out old slot 2 if it exists
			if(BoundKey2[Selection] != 0)
			{
				GetPlayerOwner().ConsoleCommand("SET Input "$RealKeyName[BoundKey2[Selection]]);
				BoundKey2[Selection] = 0;
			}

			// move key 1 to key 2, and set ourselves in 1.
			BoundKey2[Selection] = BoundKey1[Selection];
			BoundKey1[Selection] = KeyNo;
			GetPlayerOwner().ConsoleCommand("SET Input"@KeyName@AliasNames[Selection]);		
		}
	}
	else
	{
		BoundKey1[Selection] = KeyNo;
		GetPlayerOwner().ConsoleCommand("SET Input"@KeyName@AliasNames[Selection]);		
	}
}

function ProcessMenuKey( int KeyNo, string KeyName )
{
	if ( (KeyName == "") || (KeyName == "Escape")  
		|| ((KeyNo >= 0x70 ) && (KeyNo <= 0x79)) // function keys
		|| ((KeyNo >= 0x30 ) && (KeyNo <= 0x39))) // number keys
		return;

	RemoveExistingKey(KeyNo, KeyName);
	SetKey(KeyNo, KeyName);
}

function Notify(UWindowDialogControl C, byte E)
{
	local int I;

	Super.Notify(C, E);

	if(C == DefaultsButton && E == DE_Click)
	{
		GetPlayerOwner().ResetKeyboard();
		LoadExistingKeys();
		return;
	} 

	switch(E)
	{
	case DE_Change:
		switch(C)
		{
		case JoyXCombo:
			if(bLoadedExisting)
				GetPlayerOwner().ConsoleCommand("SET Input JoyX "$JoyXBinding[JoyXCombo.GetSelectedIndex()]);
			break;
		case JoyYCombo:
			if(bLoadedExisting)
				GetPlayerOwner().ConsoleCommand("SET Input JoyY "$JoyYBinding[JoyYCombo.GetSelectedIndex()]);
			break;
		}
		break;
	case DE_Click:
		if (bPolling)
		{
			bPolling = False;
			SelectedButton.bDisabled = False;

			if(C == SelectedButton)
			{
				ProcessMenuKey(1, RealKeyName[1]);
				return;
			}
		}

		if (UMenuRaisedButton(C) != None)
		{
			SelectedButton = UMenuRaisedButton(C);
			for ( I=0; I<AliasCount; I++ )
			{
				if (KeyButtons[I] == C)
					Selection = I;
			}
			bPolling = True;
			SelectedButton.bDisabled = True;
		}
		break;
	case DE_RClick:
		if (bPolling)
			{
				bPolling = False;
				SelectedButton.bDisabled = False;

				if(C == SelectedButton)
				{
					ProcessMenuKey(2, RealKeyName[2]);
					return;
				}
			}
		break;
	case DE_MClick:
		if (bPolling)
			{
				bPolling = False;
				SelectedButton.bDisabled = False;

				if(C == SelectedButton)
				{
					ProcessMenuKey(4, RealKeyName[4]);
					return;
				}			
			}
		break;
	}
}

function GetDesiredDimensions(out float W, out float H)
{	
	Super.GetDesiredDimensions(W, H);
	H = 200;
}

defaultproperties
{
	OrString=" or "
	AliasNames(0)="Fire"
	AliasNames(1)="AltFire"
	AliasNames(2)="MoveForward"
	AliasNames(3)="MoveBackward"
	AliasNames(4)="TurnLeft"
	AliasNames(5)="TurnRight"
	AliasNames(6)="StrafeLeft"
	AliasNames(7)="StrafeRight"
	AliasNames(8)="Jump"
	AliasNames(9)="Duck"
	AliasNames(10)="Look"
	AliasNames(11)="InventoryActivate"
	AliasNames(12)="InventoryNext"
	AliasNames(13)="InventoryPrevious"
	AliasNames(14)="LookUp"
	AliasNames(15)="LookDown"
	AliasNames(16)="CenterView"
	AliasNames(17)="Walking"
	AliasNames(18)="Strafe"
	AliasNames(19)="NextWeapon"
	AliasNames(20)="ThrowWeapon"
	AliasNames(21)="FeignDeath"
	LabelList(0)="Fire"
	LabelList(1)="Alternate Fire"
	LabelList(2)="Move Forward"
	LabelList(3)="Move Backward"
	LabelList(4)="Turn Left"
	LabelList(5)="Turn Right"
	LabelList(6)="Strafe Left"
	LabelList(7)="Strafe Right"
	LabelList(8)="Jump/Up"
	LabelList(9)="Crouch/Down"
	LabelList(10)="Mouse Look"
	LabelList(11)="Activate Item"
	LabelList(12)="Next Item"
	LabelList(13)="Previous Item"
	LabelList(14)="Look Up"
	LabelList(15)="Look Down"
	LabelList(16)="Center View"
	LabelList(17)="Walk"
	LabelList(18)="Strafe"
	LabelList(19)="Next Weapon"
	LabelList(20)="Throw Weapon"
	LabelList(21)="Feign Death"
	CustomizeHelp="Click the blue rectangle and then press the key to bind to this control."
	DefaultsText="Reset"
	DefaultsHelp="Reset all controls to their default settings."
	JoystickText="Joystick"
	JoyXText="X Axis"
	JoyXHelp="Select the behavior for the left-right axis of your joystick."
	JoyXOptions(0)="Strafe Left/Right"
	JoyXOptions(1)="Turn Left/Right"
	JoyXBinding(0)="Axis aStrafe speed=2"
	JoyXBinding(1)="Axis aBaseX speed=0.7"
	JoyYText="Y Axis"
	JoyYHelp="Select the behavior for the up-down axis of your joystick."
	JoyYOptions(0)="Move Forward/Back"
	JoyYOptions(1)="Look Up/Down"
	JoyYBinding(0)="Axis aBaseY speed=2"
	JoyYBinding(1)="Axis aLookup speed=-0.4"
	LocalizedKeyName(0)=""
	LocalizedKeyName(1)="LeftMouse"
	LocalizedKeyName(2)="RightMouse"
	LocalizedKeyName(3)="Cancel"
	LocalizedKeyName(4)="MiddleMouse"
	LocalizedKeyName(5)="Unknown05"
	LocalizedKeyName(6)="Unknown06"
	LocalizedKeyName(7)="Unknown07"
	LocalizedKeyName(8)="Backspace"
	LocalizedKeyName(9)="Tab"
	LocalizedKeyName(10)="Unknown0A"
	LocalizedKeyName(11)="Unknown0B"
	LocalizedKeyName(12)="Unknown0C"
	LocalizedKeyName(13)="Enter"
	LocalizedKeyName(14)="Unknown0E"
	LocalizedKeyName(15)="Unknown0F"
	LocalizedKeyName(16)="Shift"
	LocalizedKeyName(17)="Ctrl"
	LocalizedKeyName(18)="Alt"
	LocalizedKeyName(19)="Pause"
	LocalizedKeyName(20)="CapsLock"
	LocalizedKeyName(21)="Unknown15"
	LocalizedKeyName(22)="Unknown16"
	LocalizedKeyName(23)="Unknown17"
	LocalizedKeyName(24)="Unknown18"
	LocalizedKeyName(25)="Unknown19"
	LocalizedKeyName(26)="Unknown1A"
	LocalizedKeyName(27)="Escape"
	LocalizedKeyName(28)="Unknown1C"
	LocalizedKeyName(29)="Unknown1D"
	LocalizedKeyName(30)="Unknown1E"
	LocalizedKeyName(31)="Unknown1F"
	LocalizedKeyName(32)="Space"
	LocalizedKeyName(33)="PageUp"
	LocalizedKeyName(34)="PageDown"
	LocalizedKeyName(35)="End"
	LocalizedKeyName(36)="Home"
	LocalizedKeyName(37)="Left"
	LocalizedKeyName(38)="Up"
	LocalizedKeyName(39)="Right"
	LocalizedKeyName(40)="Down"
	LocalizedKeyName(41)="Select"
	LocalizedKeyName(42)="Print"
	LocalizedKeyName(43)="Execute"
	LocalizedKeyName(44)="PrintScrn"
	LocalizedKeyName(45)="Insert"
	LocalizedKeyName(46)="Delete"
	LocalizedKeyName(47)="Help"
	LocalizedKeyName(48)="0"
	LocalizedKeyName(49)="1"
	LocalizedKeyName(50)="2"
	LocalizedKeyName(51)="3"
	LocalizedKeyName(52)="4"
	LocalizedKeyName(53)="5"
	LocalizedKeyName(54)="6"
	LocalizedKeyName(55)="7"
	LocalizedKeyName(56)="8"
	LocalizedKeyName(57)="9"
	LocalizedKeyName(58)="Unknown3A"
	LocalizedKeyName(59)="Unknown3B"
	LocalizedKeyName(60)="Unknown3C"
	LocalizedKeyName(61)="Unknown3D"
	LocalizedKeyName(62)="Unknown3E"
	LocalizedKeyName(63)="Unknown3F"
	LocalizedKeyName(64)="Unknown40"
	LocalizedKeyName(65)="A"
	LocalizedKeyName(66)="B"
	LocalizedKeyName(67)="C"
	LocalizedKeyName(68)="D"
	LocalizedKeyName(69)="E"
	LocalizedKeyName(70)="F"
	LocalizedKeyName(71)="G"
	LocalizedKeyName(72)="H"
	LocalizedKeyName(73)="I"
	LocalizedKeyName(74)="J"
	LocalizedKeyName(75)="K"
	LocalizedKeyName(76)="L"
	LocalizedKeyName(77)="M"
	LocalizedKeyName(78)="N"
	LocalizedKeyName(79)="O"
	LocalizedKeyName(80)="P"
	LocalizedKeyName(81)="Q"
	LocalizedKeyName(82)="R"
	LocalizedKeyName(83)="S"
	LocalizedKeyName(84)="T"
	LocalizedKeyName(85)="U"
	LocalizedKeyName(86)="V"
	LocalizedKeyName(87)="W"
	LocalizedKeyName(88)="X"
	LocalizedKeyName(89)="Y"
	LocalizedKeyName(90)="Z"
	LocalizedKeyName(91)="Unknown5B"
	LocalizedKeyName(92)="Unknown5C"
	LocalizedKeyName(93)="Unknown5D"
	LocalizedKeyName(94)="Unknown5E"
	LocalizedKeyName(95)="Unknown5F"
	LocalizedKeyName(96)="NumPad0"
	LocalizedKeyName(97)="NumPad1"
	LocalizedKeyName(98)="NumPad2"
	LocalizedKeyName(99)="NumPad3"
	LocalizedKeyName(100)="NumPad4"
	LocalizedKeyName(101)="NumPad5"
	LocalizedKeyName(102)="NumPad6"
	LocalizedKeyName(103)="NumPad7"
	LocalizedKeyName(104)="NumPad8"
	LocalizedKeyName(105)="NumPad9"
	LocalizedKeyName(106)="GreyStar"
	LocalizedKeyName(107)="GreyPlus"
	LocalizedKeyName(108)="Separator"
	LocalizedKeyName(109)="GreyMinus"
	LocalizedKeyName(110)="NumPadPeriod"
	LocalizedKeyName(111)="GreySlash"
	LocalizedKeyName(112)="F1"
	LocalizedKeyName(113)="F2"
	LocalizedKeyName(114)="F3"
	LocalizedKeyName(115)="F4"
	LocalizedKeyName(116)="F5"
	LocalizedKeyName(117)="F6"
	LocalizedKeyName(118)="F7"
	LocalizedKeyName(119)="F8"
	LocalizedKeyName(120)="F9"
	LocalizedKeyName(121)="F10"
	LocalizedKeyName(122)="F11"
	LocalizedKeyName(123)="F12"
	LocalizedKeyName(124)="F13"
	LocalizedKeyName(125)="F14"
	LocalizedKeyName(126)="F15"
	LocalizedKeyName(127)="F16"
	LocalizedKeyName(128)="F17"
	LocalizedKeyName(129)="F18"
	LocalizedKeyName(130)="F19"
	LocalizedKeyName(131)="F20"
	LocalizedKeyName(132)="F21"
	LocalizedKeyName(133)="F22"
	LocalizedKeyName(134)="F23"
	LocalizedKeyName(135)="F24"
	LocalizedKeyName(136)="Unknown88"
	LocalizedKeyName(137)="Unknown89"
	LocalizedKeyName(138)="Unknown8A"
	LocalizedKeyName(139)="Unknown8B"
	LocalizedKeyName(140)="Unknown8C"
	LocalizedKeyName(141)="Unknown8D"
	LocalizedKeyName(142)="Unknown8E"
	LocalizedKeyName(143)="Unknown8F"
	LocalizedKeyName(144)="NumLock"
	LocalizedKeyName(145)="ScrollLock"
	LocalizedKeyName(146)="Unknown92"
	LocalizedKeyName(147)="Unknown93"
	LocalizedKeyName(148)="Unknown94"
	LocalizedKeyName(149)="Unknown95"
	LocalizedKeyName(150)="Unknown96"
	LocalizedKeyName(151)="Unknown97"
	LocalizedKeyName(152)="Unknown98"
	LocalizedKeyName(153)="Unknown99"
	LocalizedKeyName(154)="Unknown9A"
	LocalizedKeyName(155)="Unknown9B"
	LocalizedKeyName(156)="Unknown9C"
	LocalizedKeyName(157)="Unknown9D"
	LocalizedKeyName(158)="Unknown9E"
	LocalizedKeyName(159)="Unknown9F"
	LocalizedKeyName(160)="LShift"
	LocalizedKeyName(161)="RShift"
	LocalizedKeyName(162)="LControl"
	LocalizedKeyName(163)="RControl"
	LocalizedKeyName(164)="UnknownA4"
	LocalizedKeyName(165)="UnknownA5"
	LocalizedKeyName(166)="UnknownA6"
	LocalizedKeyName(167)="UnknownA7"
	LocalizedKeyName(168)="UnknownA8"
	LocalizedKeyName(169)="UnknownA9"
	LocalizedKeyName(170)="UnknownAA"
	LocalizedKeyName(171)="UnknownAB"
	LocalizedKeyName(172)="UnknownAC"
	LocalizedKeyName(173)="UnknownAD"
	LocalizedKeyName(174)="UnknownAE"
	LocalizedKeyName(175)="UnknownAF"
	LocalizedKeyName(176)="UnknownB0"
	LocalizedKeyName(177)="UnknownB1"
	LocalizedKeyName(178)="UnknownB2"
	LocalizedKeyName(179)="UnknownB3"
	LocalizedKeyName(180)="UnknownB4"
	LocalizedKeyName(181)="UnknownB5"
	LocalizedKeyName(182)="UnknownB6"
	LocalizedKeyName(183)="UnknownB7"
	LocalizedKeyName(184)="UnknownB8"
	LocalizedKeyName(185)="UnknownB9"
	LocalizedKeyName(186)="Semicolon"
	LocalizedKeyName(187)="Equals"
	LocalizedKeyName(188)="Comma"
	LocalizedKeyName(189)="Minus"
	LocalizedKeyName(190)="Period"
	LocalizedKeyName(191)="Slash"
	LocalizedKeyName(192)="Tilde"
	LocalizedKeyName(193)="UnknownC1"
	LocalizedKeyName(194)="UnknownC2"
	LocalizedKeyName(195)="UnknownC3"
	LocalizedKeyName(196)="UnknownC4"
	LocalizedKeyName(197)="UnknownC5"
	LocalizedKeyName(198)="UnknownC6"
	LocalizedKeyName(199)="UnknownC7"
	LocalizedKeyName(200)="A"
	LocalizedKeyName(201)="B"
	LocalizedKeyName(202)="X"
	LocalizedKeyName(203)="Y"
	LocalizedKeyName(204)="Back"
	LocalizedKeyName(205)="Guide"
	LocalizedKeyName(206)="Start"
	// UT99_ANDROID_CONTROLLER_FULL_REMAP_V120
	LocalizedKeyName(207)="LJoyPress"
	LocalizedKeyName(208)="RJoyPress"
	LocalizedKeyName(209)="ShoulderL"
	LocalizedKeyName(210)="ShoulderR"
	LocalizedKeyName(211)="TriggerL"
	LocalizedKeyName(212)="TriggerR"
	LocalizedKeyName(213)="RJoyLeft"
	LocalizedKeyName(214)="RJoyRight"
	LocalizedKeyName(215)="RJoyUp"
	LocalizedKeyName(216)="LJoyLeft"
	LocalizedKeyName(217)="LJoyRight"
	LocalizedKeyName(218)="LJoyUp"
	LocalizedKeyName(219)="LeftBracket"
	LocalizedKeyName(220)="Backslash"
	LocalizedKeyName(221)="RightBracket"
	LocalizedKeyName(222)="SingleQuote"
	LocalizedKeyName(223)="LJoyDown"
	LocalizedKeyName(224)="LJoyAxisX"
	LocalizedKeyName(225)="LJoyAxisY"
	LocalizedKeyName(226)="RJoyAxisX"
	LocalizedKeyName(227)="RJoyAxisY"
	LocalizedKeyName(228)="MouseX"
	LocalizedKeyName(229)="MouseY"
	LocalizedKeyName(230)="MouseZ"
	LocalizedKeyName(231)="MouseW"
	LocalizedKeyName(232)="RJoyX"
	LocalizedKeyName(233)="RJoyY"
	LocalizedKeyName(234)="RJoyDown"
	LocalizedKeyName(235)="UnknownEB"
	LocalizedKeyName(236)="MouseWheelUp"
	LocalizedKeyName(237)="MouseWheelDown"
	LocalizedKeyName(238)="Unknown10E"
	LocalizedKeyName(239)="Unknown10F"
	LocalizedKeyName(240)="DPadUp"
	LocalizedKeyName(241)="DPadDown"
	LocalizedKeyName(242)="DPadLeft"
	LocalizedKeyName(243)="DPadRight"
	LocalizedKeyName(244)="UnknownF4"
	LocalizedKeyName(245)="UnknownF5"
	LocalizedKeyName(246)="Attn"
	LocalizedKeyName(247)="CrSel"
	LocalizedKeyName(248)="ExSel"
	LocalizedKeyName(249)="ErEof"
	LocalizedKeyName(250)="Play"
	LocalizedKeyName(251)="Zoom"
	LocalizedKeyName(252)="NoName"
	LocalizedKeyName(253)="PA1"
	LocalizedKeyName(254)="OEMClear"
}
