ScriptName _3DIRP_MCMScript extends SKI_ConfigBase

String[] BookTypes
String[] MiscQuestOptions
String[] ControllerButtons
String[] BookButtons
String[] CloseActions
String[] Fonts
String[] Profiles

Bool SettingsLoaded
String kNewProfileCommand = "Write to New"
String SelectedProfile = "Default"

Int Function GetVersion()
	Return 1
EndFunction

; -------------------------------------------------------------------------------------------------
; MAIN EVENTS
; -------------------------------------------------------------------------------------------------
Event OnVersionUpdate(Int a_version)
	If a_version > CurrentVersion
		Debug.Trace(ModName + " update from " + CurrentVersion + " to " + a_version)
		OnConfigInit()
	EndIf
EndEvent

Event OnConfigInit()
	Pages = New String[4]
	Pages[0] = "Mod Settings"
	Pages[1] = "Holsters"
	Pages[2] = "Journal"
	Pages[3] = "Floating Book"

	InitializeMenuChoices()
EndEvent

Function InitializeMenuChoices()
	BookTypes = New String[2]
	BookTypes[0] = "Disabled"
	BookTypes[1] = "Journal"

	MiscQuestOptions = New String[4]
	MiscQuestOptions[0] = "Misc Tab Only"
	MiscQuestOptions[1] = "Favorites"
	MiscQuestOptions[2] = "All Quests"
	MiscQuestOptions[3] = "Favorites and All Quests"

	ControllerButtons = New String[4]
	ControllerButtons[0] = "Trigger"
	ControllerButtons[1] = "Grip"
	ControllerButtons[2] = "A"
	ControllerButtons[3] = "B"

	BookButtons = New String[2]
	BookButtons[0] = "Primary"
	BookButtons[1] = "Secondary"

	CloseActions = New String[2]
	CloseActions[0] = "Hold"
	CloseActions[1] = "Double Tap"

	Fonts = New String[1]
	Fonts[0] = "Default"
EndFunction

Event OnConfigOpen()
	InitializeMenuChoices()
	SettingsLoaded = _3DIRP_Settings.ReloadSettings()
	If SettingsLoaded
		If !_3DIRP_Settings.LoadProfile(SelectedProfile)
			SelectedProfile = "Default"
			_3DIRP_Settings.LoadProfile(SelectedProfile)
		EndIf
	EndIf
	RefreshProfiles()
EndEvent

Event OnConfigClose()
	_3DIRP_Settings.SaveSettings()
EndEvent

; -------------------------------------------------------------------------------------------------
; PAGE DEFINITIONS
; -------------------------------------------------------------------------------------------------
Event OnPageReset(String a_page)
	If !SettingsLoaded
		Return
	EndIf

	If a_page == "" || a_page == "Mod Settings"
		DrawGeneralPage()
	ElseIf a_page == "Holsters"
		DrawHolstersPage()
	ElseIf a_page == "Journal"
		DrawJournalPage()
	ElseIf a_page == "Floating Book"
		DrawFloatingBookPage()
	EndIf
EndEvent

Function DrawGeneralPage()
	SetCursorFillMode(TOP_TO_BOTTOM)
	AddMenuOptionST("iPrimaryButton", "Primary Button", ControllerButtons[GetMenuIndex("iPrimaryButton", ControllerButtons)])
	AddMenuOptionST("iSecondaryButton", "Secondary Button", ControllerButtons[GetMenuIndex("iSecondaryButton", ControllerButtons)])
	AddToggleOptionST("bShowDebugSpheres", "Show Hitboxes", GetToggleSetting("bShowDebugSpheres"))

	SetCursorPosition(1)
	AddMenuOptionST("Profile", "Profile", _3DIRP_Settings.GetActiveProfile())
	AddToggleOptionST("bDebugLog", "Debug Log", GetToggleSetting("bDebugLog"))
EndFunction

Function DrawHolstersPage()
	SetCursorFillMode(TOP_TO_BOTTOM)
	AddMenuOptionST("iLeftShoulderPrimary", "Left Primary", BookTypes[GetMenuIndex("iLeftShoulderPrimary", BookTypes)])
	AddMenuOptionST("iLeftShoulderSecondary", "Left Secondary", BookTypes[GetMenuIndex("iLeftShoulderSecondary", BookTypes)])
	AddMenuOptionST("iLeftShoulderBoth", "Left Both", BookTypes[GetMenuIndex("iLeftShoulderBoth", BookTypes)])
	AddMenuOptionST("iRightShoulderPrimary", "Right Primary", BookTypes[GetMenuIndex("iRightShoulderPrimary", BookTypes)])
	AddMenuOptionST("iRightShoulderSecondary", "Right Secondary", BookTypes[GetMenuIndex("iRightShoulderSecondary", BookTypes)])
	AddMenuOptionST("iRightShoulderBoth", "Right Both", BookTypes[GetMenuIndex("iRightShoulderBoth", BookTypes)])
	AddMenuOptionST("iBellyPrimary", "Belly Primary", BookTypes[GetMenuIndex("iBellyPrimary", BookTypes)])
	AddMenuOptionST("iBellySecondary", "Belly Secondary", BookTypes[GetMenuIndex("iBellySecondary", BookTypes)])
	AddMenuOptionST("iBellyBoth", "Belly Both", BookTypes[GetMenuIndex("iBellyBoth", BookTypes)])

	SetCursorPosition(1)
	AddToggleOptionST("bAllowEmptyArrowHand", "Allow Empty Arrow Hand", GetToggleSetting("bAllowEmptyArrowHand"))
	AddSliderOptionST("fShoulderRadius", "Shoulder Radius", _3DIRP_Settings.GetSetting("fShoulderRadius"), "{1}")
	AddSliderOptionST("fShoulderX", "Shoulder X", _3DIRP_Settings.GetSetting("fShoulderX"), "{1}")
	AddSliderOptionST("fShoulderY", "Shoulder Y", _3DIRP_Settings.GetSetting("fShoulderY"), "{1}")
	AddSliderOptionST("fShoulderZ", "Shoulder Z", _3DIRP_Settings.GetSetting("fShoulderZ"), "{1}")
	AddSliderOptionST("fBellyRadius", "Belly Radius", _3DIRP_Settings.GetSetting("fBellyRadius"), "{1}")
	AddSliderOptionST("fBellyX", "Belly X", _3DIRP_Settings.GetSetting("fBellyX"), "{1}")
	AddSliderOptionST("fBellyY", "Belly Y", _3DIRP_Settings.GetSetting("fBellyY"), "{1}")
	AddSliderOptionST("fBellyZ", "Belly Z", _3DIRP_Settings.GetSetting("fBellyZ"), "{1}")
EndFunction

Function DrawJournalPage()
	SetCursorFillMode(TOP_TO_BOTTOM)
	AddMenuOptionST("iShowMiscInAll", "Show Misc Quests", MiscQuestOptions[GetMenuIndex("iShowMiscInAll", MiscQuestOptions)])
	AddToggleOptionST("bHighlightNewQuests", "Highlight New Quests", GetToggleSetting("bHighlightNewQuests"))
	AddMenuOptionST("iHideButton", "Hide Button", BookButtons[GetMenuIndex("iHideButton", BookButtons)])
	AddMenuOptionST("iCloseButton", "Close Button", BookButtons[GetMenuIndex("iCloseButton", BookButtons)])
	AddMenuOptionST("iCloseAction", "Close Action", CloseActions[GetMenuIndex("iCloseAction", CloseActions)])
	AddSliderOptionST("fCloseTiming", "Close Timing", _3DIRP_Settings.GetSetting("fCloseTiming"), "{1}")
	AddEmptyOption()
	AddSliderOptionST("fLightIntensity", "Light Intensity", _3DIRP_Settings.GetSetting("fLightIntensity"), "{2}")
	AddSliderOptionST("fPageBrightness", "Page Brightness", _3DIRP_Settings.GetSetting("fPageBrightness"), "{2}")
	AddEmptyOption()
	AddTextOptionST("ResetHiddenQuests", "Reset Hidden Quest List", "Reset")

	SetCursorPosition(1)
	AddMenuOptionST("iFont", "Font", Fonts[GetMenuIndex("iFont", Fonts)])
	AddSliderOptionST("fJournalScale", "Book Scale", _3DIRP_Settings.GetSetting("fJournalScale"), "{2}")
	AddSliderOptionST("fFontSize", "Font Scale", _3DIRP_Settings.GetSetting("fFontSize"), "{2}")
	AddSliderOptionST("fTopMargin", "Top Margin", _3DIRP_Settings.GetSetting("fTopMargin"), "{2}")
	AddSliderOptionST("fHorizontalMargin", "Left Margin", _3DIRP_Settings.GetSetting("fHorizontalMargin"), "{2}")
	AddSliderOptionST("fQuestLineSpacing", "Quest Spacing", _3DIRP_Settings.GetSetting("fQuestLineSpacing"), "{2}")
	AddSliderOptionST("fLeftPageTextZOffset", "Left Page Text Z offset", _3DIRP_Settings.GetSetting("fLeftPageTextZOffset"), "{3}")
	AddSliderOptionST("fRightPageTextZOffset", "Right Page Text Z offset", _3DIRP_Settings.GetSetting("fRightPageTextZOffset"), "{3}")
	AddSliderOptionST("fLeftPageTextZOffsetRight", "Left Page Text Z offset (Rhand)", _3DIRP_Settings.GetSetting("fLeftPageTextZOffsetRight"), "{3}")
	AddSliderOptionST("fRightPageTextZOffsetRight", "Right Page Text Z offset (Rhand)", _3DIRP_Settings.GetSetting("fRightPageTextZOffsetRight"), "{3}")
EndFunction

Function DrawFloatingBookPage()
	SetCursorFillMode(TOP_TO_BOTTOM)
	AddSliderOptionST("fFloatingJournalScale", "Book Scale", _3DIRP_Settings.GetSetting("fFloatingJournalScale"), "{2}")
	AddSliderOptionST("fFloatingFollowSpeed", "Follow Speed", _3DIRP_Settings.GetSetting("fFloatingFollowSpeed"), "{2}")
	AddToggleOptionST("bFollowWhileHovered", "Follow While Hovered", GetToggleSetting("bFollowWhileHovered"))
	AddSliderOptionST("fFloatingDespawnDistance", "Auto Close Distance", _3DIRP_Settings.GetSetting("fFloatingDespawnDistance"), "{0}")
	

EndFunction

; -------------------------------------------------------------------------------------------------
; STATE OPTION EVENTS - GENERAL
; -------------------------------------------------------------------------------------------------
State iPrimaryButton
	Event OnMenuOpenST()
		OpenMenuSetting("iPrimaryButton", ControllerButtons)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iPrimaryButton", ControllerButtons, a_index)
	EndEvent
EndState

State iSecondaryButton
	Event OnMenuOpenST()
		OpenMenuSetting("iSecondaryButton", ControllerButtons)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iSecondaryButton", ControllerButtons, a_index)
	EndEvent
EndState

State bShowDebugSpheres
	Event OnSelectST()
		ToggleSetting("bShowDebugSpheres")
	EndEvent
EndState

State bDebugLog
	Event OnSelectST()
		ToggleSetting("bDebugLog")
	EndEvent
EndState

State Profile
	Event OnMenuOpenST()
		Int index
		RefreshProfiles()
		index = FindProfile(_3DIRP_Settings.GetActiveProfile())
		SetMenuDialogStartIndex(index)
		SetMenuDialogDefaultIndex(index)
		SetMenuDialogOptions(Profiles)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		If a_index >= 0 && a_index < Profiles.Length
			If Profiles[a_index] == kNewProfileCommand
				CreateNextProfile()
			ElseIf _3DIRP_Settings.LoadProfile(Profiles[a_index])
				SelectedProfile = Profiles[a_index]
				SetMenuOptionValueST(SelectedProfile)
				ForcePageReset()
			EndIf
		EndIf
	EndEvent
EndState

; -------------------------------------------------------------------------------------------------
; STATE OPTION EVENTS - HOLSTERS
; -------------------------------------------------------------------------------------------------
State iLeftShoulderPrimary
	Event OnMenuOpenST()
		OpenMenuSetting("iLeftShoulderPrimary", BookTypes)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iLeftShoulderPrimary", BookTypes, a_index)
	EndEvent
EndState

State iLeftShoulderSecondary
	Event OnMenuOpenST()
		OpenMenuSetting("iLeftShoulderSecondary", BookTypes)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iLeftShoulderSecondary", BookTypes, a_index)
	EndEvent
EndState

State iLeftShoulderBoth
	Event OnMenuOpenST()
		OpenMenuSetting("iLeftShoulderBoth", BookTypes)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iLeftShoulderBoth", BookTypes, a_index)
	EndEvent
EndState

State iRightShoulderPrimary
	Event OnMenuOpenST()
		OpenMenuSetting("iRightShoulderPrimary", BookTypes)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iRightShoulderPrimary", BookTypes, a_index)
	EndEvent
EndState

State iRightShoulderSecondary
	Event OnMenuOpenST()
		OpenMenuSetting("iRightShoulderSecondary", BookTypes)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iRightShoulderSecondary", BookTypes, a_index)
	EndEvent
EndState

State iRightShoulderBoth
	Event OnMenuOpenST()
		OpenMenuSetting("iRightShoulderBoth", BookTypes)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iRightShoulderBoth", BookTypes, a_index)
	EndEvent
EndState

State iBellyPrimary
	Event OnMenuOpenST()
		OpenMenuSetting("iBellyPrimary", BookTypes)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iBellyPrimary", BookTypes, a_index)
	EndEvent
EndState

State iBellySecondary
	Event OnMenuOpenST()
		OpenMenuSetting("iBellySecondary", BookTypes)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iBellySecondary", BookTypes, a_index)
	EndEvent
EndState

State iBellyBoth
	Event OnMenuOpenST()
		OpenMenuSetting("iBellyBoth", BookTypes)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iBellyBoth", BookTypes, a_index)
	EndEvent
EndState

State bAllowEmptyArrowHand
	event OnHighlightST()
		SetInfoText("Enable the holster when you have a bow in the other hand")
	endevent
	Event OnSelectST()
		ToggleSetting("bAllowEmptyArrowHand")
	EndEvent
EndState

State fShoulderRadius
	Event OnSliderOpenST()
		OpenSliderSetting("fShoulderRadius", 0.0, 50.0, 0.5)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fShoulderRadius", a_value, "{1}")
	EndEvent
EndState

State fShoulderX
	Event OnSliderOpenST()
		OpenSliderSetting("fShoulderX", -20.0, 20.0, 0.5)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fShoulderX", a_value, "{1}")
	EndEvent
EndState

State fShoulderY
	Event OnSliderOpenST()
		OpenSliderSetting("fShoulderY", -20.0, 20.0, 0.5)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fShoulderY", a_value, "{1}")
	EndEvent
EndState

State fShoulderZ
	Event OnSliderOpenST()
		OpenSliderSetting("fShoulderZ", -20.0, 20.0, 0.5)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fShoulderZ", a_value, "{1}")
	EndEvent
EndState

State fBellyRadius
	Event OnSliderOpenST()
		OpenSliderSetting("fBellyRadius", 0.0, 50.0, 0.5)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fBellyRadius", a_value, "{1}")
	EndEvent
EndState

State fBellyX
	Event OnSliderOpenST()
		OpenSliderSetting("fBellyX", -20.0, 20.0, 0.5)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fBellyX", a_value, "{1}")
	EndEvent
EndState

State fBellyY
	Event OnSliderOpenST()
		OpenSliderSetting("fBellyY", -20.0, 20.0, 0.5)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fBellyY", a_value, "{1}")
	EndEvent
EndState

State fBellyZ
	Event OnSliderOpenST()
		OpenSliderSetting("fBellyZ", -20.0, 20.0, 0.5)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fBellyZ", a_value, "{1}")
	EndEvent
EndState

; -------------------------------------------------------------------------------------------------
; STATE OPTION EVENTS - JOURNAL
; -------------------------------------------------------------------------------------------------
State iShowMiscInAll
	Event OnMenuOpenST()
		OpenMenuSetting("iShowMiscInAll", MiscQuestOptions)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iShowMiscInAll", MiscQuestOptions, a_index)
	EndEvent
EndState

State bHighlightNewQuests
	event OnHighlightST()
		SetInfoText("Sort newly-started quests to the top of their page until you view them")
	endevent
	Event OnSelectST()
		ToggleSetting("bHighlightNewQuests")
	EndEvent
EndState

State iHideButton
	event OnHighlightST()
		SetInfoText("Hold this button on the quest description page to hide that quest")
	endevent
	Event OnMenuOpenST()
		OpenMenuSetting("iHideButton", BookButtons)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iHideButton", BookButtons, a_index)
	EndEvent
EndState

State iCloseButton
	Event OnMenuOpenST()
		OpenMenuSetting("iCloseButton", BookButtons)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iCloseButton", BookButtons, a_index)
	EndEvent
EndState

State iCloseAction
	Event OnMenuOpenST()
		OpenMenuSetting("iCloseAction", CloseActions)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iCloseAction", CloseActions, a_index)
	EndEvent
EndState

State fCloseTiming
	Event OnSliderOpenST()
		OpenSliderSetting("fCloseTiming", 0.2, 3.0, 0.1)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fCloseTiming", a_value, "{1}")
	EndEvent
EndState

State ResetHiddenQuests
	Event OnSelectST()
		If _3DIRP_Settings.ResetHiddenQuests()
			SetTextOptionValueST("Reset")
		EndIf
	EndEvent
EndState

State fLightIntensity
	Event OnSliderOpenST()
		OpenSliderSetting("fLightIntensity", 0.0, 3.0, 0.05)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fLightIntensity", a_value, "{1}")
	EndEvent
EndState

State fPageBrightness
	Event OnSliderOpenST()
		OpenSliderSetting("fPageBrightness", 0.3, 1.0, 0.05)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fPageBrightness", a_value, "{1}")
	EndEvent
EndState

State fJournalScale
	Event OnSliderOpenST()
		OpenSliderSetting("fJournalScale", 0.5, 2.0, 0.05)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fJournalScale", a_value, "{2}")
	EndEvent
EndState

State fFloatingJournalScale
	Event OnSliderOpenST()
		OpenSliderSetting("fFloatingJournalScale", 0.5, 2.0, 0.05)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fFloatingJournalScale", a_value, "{2}")
	EndEvent
EndState

State fFloatingDespawnDistance
	event OnHighlightST()
		SetInfoText("Floating book will be dismissed when you move this far away")
	endevent
	Event OnSliderOpenST()
		OpenSliderSetting("fFloatingDespawnDistance", 0.0, 300.0, 10.0)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fFloatingDespawnDistance", a_value, "{0}")
	EndEvent
EndState

State fFloatingFollowSpeed
	Event OnSliderOpenST()
		OpenSliderSetting("fFloatingFollowSpeed", 0.0, 20.0, 0.25)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fFloatingFollowSpeed", a_value, "{2}")
	EndEvent
EndState

State bFollowWhileHovered
	event OnHighlightST()
		SetInfoText("Freezes the book in place when your hand is interacting with it")
	endevent
	Event OnSelectST()
		ToggleSetting("bFollowWhileHovered")
	EndEvent
EndState

State fFontSize
	Event OnSliderOpenST()
		OpenSliderSetting("fFontSize", 0.5, 3.0, 0.05)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fFontSize", a_value, "{2}")
	EndEvent
EndState

State fTopMargin
	Event OnSliderOpenST()
		OpenSliderSetting("fTopMargin", 0.0, 5.0, 0.05)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fTopMargin", a_value, "{2}")
	EndEvent
EndState

State fHorizontalMargin
	Event OnSliderOpenST()
		OpenSliderSetting("fHorizontalMargin", 0.0, 5.0, 0.05)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fHorizontalMargin", a_value, "{2}")
	EndEvent
EndState

State fQuestLineSpacing
	Event OnSliderOpenST()
		OpenSliderSetting("fQuestLineSpacing", 0.0, 3.0, 0.05)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fQuestLineSpacing", a_value, "{2}")
	EndEvent
EndState

State fLeftPageTextZOffset
	Event OnSliderOpenST()
		OpenSliderSetting("fLeftPageTextZOffset", 0.0, 3.0, 0.001)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fLeftPageTextZOffset", a_value, "{3}")
	EndEvent
EndState

State fRightPageTextZOffset
	Event OnSliderOpenST()
		OpenSliderSetting("fRightPageTextZOffset", 0.0, 3.0, 0.001)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fRightPageTextZOffset", a_value, "{3}")
	EndEvent
EndState

State fLeftPageTextZOffsetRight
	Event OnSliderOpenST()
		OpenSliderSetting("fLeftPageTextZOffsetRight", 0.0, 3.0, 0.001)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fLeftPageTextZOffsetRight", a_value, "{3}")
	EndEvent
EndState

State fRightPageTextZOffsetRight
	Event OnSliderOpenST()
		OpenSliderSetting("fRightPageTextZOffsetRight", 0.0, 3.0, 0.001)
	EndEvent
	Event OnSliderAcceptST(Float a_value)
		AcceptSliderSetting("fRightPageTextZOffsetRight", a_value, "{3}")
	EndEvent
EndState

State iFont
	Event OnMenuOpenST()
		OpenMenuSetting("iFont", Fonts)
	EndEvent
	Event OnMenuAcceptST(Int a_index)
		AcceptMenuSetting("iFont", Fonts, a_index)
	EndEvent
EndState

; -------------------------------------------------------------------------------------------------
; HELPERS
; -------------------------------------------------------------------------------------------------
Bool Function GetToggleSetting(String a_setting)
	Return _3DIRP_Settings.GetSetting(a_setting) != 0.0
EndFunction

Function ToggleSetting(String a_setting)
	Bool value = !GetToggleSetting(a_setting)
	If value
		_3DIRP_Settings.SetSetting(a_setting, 1.0)
	Else
		_3DIRP_Settings.SetSetting(a_setting, 0.0)
	EndIf
	SetToggleOptionValueST(value)
EndFunction

Int Function GetMenuIndex(String a_setting, String[] a_choices)
	Int index = _3DIRP_Settings.GetSetting(a_setting) As Int
	If index < 0 || index >= a_choices.Length
		index = 0
	EndIf
	Return index
EndFunction

Function OpenMenuSetting(String a_setting, String[] a_choices)
	Int index = GetMenuIndex(a_setting, a_choices)
	SetMenuDialogStartIndex(index)
	SetMenuDialogDefaultIndex(index)
	SetMenuDialogOptions(a_choices)
EndFunction

Function AcceptMenuSetting(String a_setting, String[] a_choices, Int a_index)
	If a_index >= 0 && a_index < a_choices.Length
		_3DIRP_Settings.SetSetting(a_setting, a_index As Float)
		SetMenuOptionValueST(a_choices[a_index])
	EndIf
EndFunction

Function OpenSliderSetting(String a_setting, Float a_minimum, Float a_maximum, Float a_interval)
	Float value = _3DIRP_Settings.GetSetting(a_setting)
	SetSliderDialogStartValue(value)
	SetSliderDialogDefaultValue(value)
	SetSliderDialogRange(a_minimum, a_maximum)
	SetSliderDialogInterval(a_interval)
EndFunction

Function AcceptSliderSetting(String a_setting, Float a_value, String a_format)
	_3DIRP_Settings.SetSetting(a_setting, a_value)
	SetSliderOptionValueST(a_value, a_format)
EndFunction

Int Function FindProfile(String a_profile)
	Int index = 0
	While index < Profiles.Length
		If Profiles[index] == a_profile
			Return index
		EndIf
		index += 1
	EndWhile
	Return 0
EndFunction

Function RefreshProfiles()
	String[] storedProfiles = _3DIRP_Settings.GetProfiles()
	Profiles = Utility.CreateStringArray(storedProfiles.Length + 1)
	Int index = 0
	While index < storedProfiles.Length
		Profiles[index] = storedProfiles[index]
		index += 1
	EndWhile
	Profiles[index] = kNewProfileCommand
EndFunction

Function CreateNextProfile()
	Int suffix = 1
	String profileName = "Profile " + suffix
	While ProfileExists(profileName)
		suffix += 1
		profileName = "Profile " + suffix
	EndWhile

	If _3DIRP_Settings.CreateProfile(profileName)
		SelectedProfile = profileName
		RefreshProfiles()
		SetMenuOptionValueST(SelectedProfile)
		ForcePageReset()
	EndIf
EndFunction

Bool Function ProfileExists(String a_profile)
	Int index = 0
	While index < Profiles.Length - 1
		If Profiles[index] == a_profile
			Return True
		EndIf
		index += 1
	EndWhile
	Return False
EndFunction
