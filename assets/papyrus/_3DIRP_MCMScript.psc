Scriptname _3DIRP_MCMScript extends SKI_ConfigBase

String[] BookTypes
String[] MiscQuestOptions
String[] ControllerButtons
String[] BookButtons
String[] CloseActions
String[] Fonts
String[] Profiles

Int[] ToggleOptions
String[] ToggleKeys
Int ToggleCount

Int[] MenuOptions
String[] MenuKeys
Int[] MenuChoiceTypes
Int MenuCount

Int[] SliderOptions
String[] SliderKeys
Float[] SliderMinimums
Float[] SliderMaximums
Float[] SliderIntervals
Int SliderCount

Int ProfileOption
Int NewProfileOption
Int ResetHiddenQuestsOption
Bool SettingsLoaded
String SelectedProfile = "Default"

Int Function GetVersion()
	Return 1
EndFunction

Event OnConfigInit()
	Pages = New String[3]
	Pages[0] = "General"
	Pages[1] = "Holsters"
	Pages[2] = "Journal"

	InitializeMenuChoices()

	ToggleOptions = New Int[8]
	ToggleKeys = New String[8]
	MenuOptions = New Int[16]
	MenuKeys = New String[16]
	MenuChoiceTypes = New Int[16]
	SliderOptions = New Int[40]
	SliderKeys = New String[40]
	SliderMinimums = New Float[40]
	SliderMaximums = New Float[40]
	SliderIntervals = New Float[40]

	SettingsLoaded = _3DIRP_Settings.ReloadSettings()
	RefreshProfiles()
EndEvent

Function InitializeMenuChoices()
	BookTypes = New String[2]
	BookTypes[0] = "None"
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

	BookButtons = New String[3]
	BookButtons[0] = "Primary"
	BookButtons[1] = "Secondary"
	BookButtons[2] = "None"

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
			SettingsLoaded = _3DIRP_Settings.LoadProfile(SelectedProfile)
		EndIf
	EndIf
	RefreshProfiles()
EndEvent

Event OnConfigClose()
	_3DIRP_Settings.SaveSettings()
EndEvent

Event OnPageReset(String page)
	ToggleCount = 0
	MenuCount = 0
	MenuChoiceTypes = New Int[16]
	SliderCount = 0
	ProfileOption = -1
	NewProfileOption = -1
	ResetHiddenQuestsOption = -1

	If !SettingsLoaded
		SetCursorFillMode(TOP_TO_BOTTOM)
		AddTextOption("Settings file could not be loaded", "")
		AddTextOption("Data/SKSE/Plugins/3DIRP_VR_UI.json", "")
		Return
	EndIf

	If page == "" || page == "General"
		DrawGeneralPage()
	ElseIf page == "Holsters"
		DrawHolstersPage()
	ElseIf page == "Journal"
		DrawJournalPage()
	EndIf
EndEvent

Function DrawGeneralPage()
	SetCursorFillMode(TOP_TO_BOTTOM)
	AddMenuSetting("iPrimaryButton", "Primary Button", ControllerButtons)
	AddMenuSetting("iSecondaryButton", "Secondary Button", ControllerButtons)
	AddToggleSetting("bShowDebugSpheres", "Show Hitboxes")

	SetCursorPosition(1)
	ProfileOption = AddMenuOption("Profile", _3DIRP_Settings.GetActiveProfile())
	AddToggleSetting("bDebugLog", "Debug Log")
EndFunction

Function DrawHolstersPage()
	SetCursorFillMode(TOP_TO_BOTTOM)
	AddToggleSetting("bAllowEmptyArrowHand", "Allow Empty Arrow Hand")
	AddMenuSetting("iLeftShoulderPrimary", "Left Primary", BookTypes, 1)
	AddMenuSetting("iLeftShoulderSecondary", "Left Secondary", BookTypes, 1)
	AddMenuSetting("iLeftShoulderBoth", "Left Both", BookTypes, 1)
	AddMenuSetting("iRightShoulderPrimary", "Right Primary", BookTypes, 1)
	AddMenuSetting("iRightShoulderSecondary", "Right Secondary", BookTypes, 1)
	AddMenuSetting("iRightShoulderBoth", "Right Both", BookTypes, 1)
	AddMenuSetting("iBellyPrimary", "Belly Primary", BookTypes, 1)
	AddMenuSetting("iBellySecondary", "Belly Secondary", BookTypes, 1)
	AddMenuSetting("iBellyBoth", "Belly Both", BookTypes, 1)

	SetCursorPosition(1)
	AddSliderSetting("fShoulderRadius", "Shoulder Radius", 0.0, 50.0, 0.5)
	AddSliderSetting("fShoulderX", "Shoulder X", -20.0, 20.0, 0.5)
	AddSliderSetting("fShoulderY", "Shoulder Y", -20.0, 20.0, 0.5)
	AddSliderSetting("fShoulderZ", "Shoulder Z", -20.0, 20.0, 0.5)
	AddSliderSetting("fBellyRadius", "Belly Radius", 0.0, 50.0, 0.5)
	AddSliderSetting("fBellyX", "Belly X", -20.0, 20.0, 0.5)
	AddSliderSetting("fBellyY", "Belly Y", -20.0, 20.0, 0.5)
	AddSliderSetting("fBellyZ", "Belly Z", -20.0, 20.0, 0.5)
EndFunction

Function DrawJournalPage()
	SetCursorFillMode(TOP_TO_BOTTOM)
	AddMenuSetting("iShowMiscInAll", "Show Misc Quests", MiscQuestOptions)
	AddToggleSetting("bHighlightNewQuests", "Highlight New Quests")
		AddMenuSetting("iTrackButton", "Track Button", BookButtons)
	AddMenuSetting("iHideButton", "Hide Button", BookButtons)
	AddMenuSetting("iCloseButton", "Close Button", BookButtons)
	AddMenuSetting("iCloseAction", "Close Action", CloseActions)
	AddSliderSetting("fCloseTiming", "Close Timing", 0.2, 3.0, 0.1)
	ResetHiddenQuestsOption = AddTextOption("Reset Hidden Quest List", "Reset")

	SetCursorPosition(1)
	AddSliderSetting("fLightIntensity", "Light Intensity", 0.0, 3.0, 0.1)
	AddSliderSetting("fJournalScale", "Book Scale", 0.5, 2.0, 0.05)
	AddSliderSetting("fFontSize", "Font Scale", 0.5, 3.0, 0.05)
	AddSliderSetting("fTopMargin", "Top Margin", 0.0, 5.0, 0.05)
	AddSliderSetting("fHorizontalMargin", "Left Margin", 0.0, 5.0, 0.05)
	AddSliderSetting("fQuestLineSpacing", "Quest Spacing", 0.0, 3.0, 0.05)
	AddMenuSetting("iFont", "Font", Fonts)
EndFunction

Function AddToggleSetting(String settingName, String label)
	ToggleKeys[ToggleCount] = settingName
	ToggleOptions[ToggleCount] = AddToggleOption(label, _3DIRP_Settings.GetSetting(settingName) != 0.0)
	ToggleCount += 1
EndFunction

Function AddMenuSetting(String settingName, String label, String[] choices, Int choiceType = 0)
	Int index = _3DIRP_Settings.GetSetting(settingName) As Int
	If index < 0 || index >= choices.Length
		index = 0
		_3DIRP_Settings.SetSetting(settingName, 0.0)
	EndIf
	MenuKeys[MenuCount] = settingName
	MenuChoiceTypes[MenuCount] = choiceType
	MenuOptions[MenuCount] = AddMenuOption(label, choices[index])
	MenuCount += 1
EndFunction

Function AddSliderSetting(String settingName, String label, Float minimum, Float maximum, Float interval)
	SliderKeys[SliderCount] = settingName
	SliderMinimums[SliderCount] = minimum
	SliderMaximums[SliderCount] = maximum
	SliderIntervals[SliderCount] = interval
	SliderOptions[SliderCount] = AddSliderOption(label, _3DIRP_Settings.GetSetting(settingName), "{3}")
	SliderCount += 1
EndFunction

Event OnOptionSelect(Int option)
	If option == ResetHiddenQuestsOption
		_3DIRP_Settings.SetSetting("bResetHiddenQuests", 1.0)
		SetTextOptionValue(option, "Queued")
		Return
	EndIf

	Int index = FindOption(ToggleOptions, ToggleCount, option)
	If index >= 0
		Bool value = _3DIRP_Settings.GetSetting(ToggleKeys[index]) == 0.0
		If value
			_3DIRP_Settings.SetSetting(ToggleKeys[index], 1.0)
		Else
			_3DIRP_Settings.SetSetting(ToggleKeys[index], 0.0)
		EndIf
		SetToggleOptionValue(option, value)
	EndIf
EndEvent

Event OnOptionMenuOpen(Int option)
	If option == ProfileOption
		SetMenuDialogOptions(Profiles)
		SetMenuDialogStartIndex(FindProfile(_3DIRP_Settings.GetActiveProfile()))
		SetMenuDialogDefaultIndex(0)
		Return
	EndIf

	Int index = FindOption(MenuOptions, MenuCount, option)
	If index >= 0
		String settingName = MenuKeys[index]
		If MenuChoiceTypes[index] == 1
			String[] bookChoices = New String[2]
			bookChoices[0] = "None"
			bookChoices[1] = "Journal"
			Int bookValue = _3DIRP_Settings.GetSetting(settingName) As Int
			If bookValue < 0 || bookValue >= bookChoices.Length
				bookValue = 0
			EndIf
			SetMenuDialogOptions(bookChoices)
			SetMenuDialogStartIndex(bookValue)
			SetMenuDialogDefaultIndex(bookValue)
			Return
		EndIf

		String[] choices = GetMenuChoices(MenuKeys[index])
		Int value = _3DIRP_Settings.GetSetting(MenuKeys[index]) As Int
		If value < 0 || value >= choices.Length
			value = 0
		EndIf
		SetMenuDialogOptions(choices)
		SetMenuDialogStartIndex(value)
		SetMenuDialogDefaultIndex(value)
	EndIf
EndEvent

Event OnOptionMenuAccept(Int option, Int value)
	If option == ProfileOption
		If value >= 0 && value < Profiles.Length && _3DIRP_Settings.LoadProfile(Profiles[value])
			SelectedProfile = Profiles[value]
			SetMenuOptionValue(option, Profiles[value])
			ForcePageReset()
		EndIf
		Return
	EndIf

	Int index = FindOption(MenuOptions, MenuCount, option)
	If index >= 0
		String settingName = MenuKeys[index]
		If MenuChoiceTypes[index] == 1
			If value == 0
				_3DIRP_Settings.SetSetting(settingName, 0.0)
				SetMenuOptionValue(option, "None")
			ElseIf value == 1
				_3DIRP_Settings.SetSetting(settingName, 1.0)
				SetMenuOptionValue(option, "Journal")
			EndIf
			Return
		EndIf

		String[] choices = GetMenuChoices(MenuKeys[index])
		If value >= 0 && value < choices.Length
			_3DIRP_Settings.SetSetting(MenuKeys[index], value As Float)
			SetMenuOptionValue(option, choices[value])
		EndIf
	EndIf
EndEvent

Event OnOptionSliderOpen(Int option)
	Int index = FindOption(SliderOptions, SliderCount, option)
	If index >= 0
		Float value = _3DIRP_Settings.GetSetting(SliderKeys[index])
		SetSliderDialogStartValue(value)
		SetSliderDialogDefaultValue(value)
		SetSliderDialogRange(SliderMinimums[index], SliderMaximums[index])
		SetSliderDialogInterval(SliderIntervals[index])
	EndIf
EndEvent

Event OnOptionSliderAccept(Int option, Float value)
	Int index = FindOption(SliderOptions, SliderCount, option)
	If index >= 0
		_3DIRP_Settings.SetSetting(SliderKeys[index], value)
		SetSliderOptionValue(option, value, "{3}")
	EndIf
EndEvent

Event OnOptionInputOpen(Int option)
	If option == NewProfileOption
		SetInputDialogStartText("")
	EndIf
EndEvent

Event OnOptionInputAccept(Int option, String value)
	If option == NewProfileOption && value != "" && _3DIRP_Settings.CreateProfile(value)
		SelectedProfile = value
		_3DIRP_Settings.SaveSettings()
		RefreshProfiles()
		SetInputOptionValue(option, value)
		SetMenuOptionValue(ProfileOption, value)
		ForcePageReset()
	EndIf
EndEvent

String[] Function GetMenuChoices(String settingName)
	If settingName == "iShowMiscInAll"
		Return MiscQuestOptions
	ElseIf settingName == "iPrimaryButton" || settingName == "iSecondaryButton"
		Return ControllerButtons
	ElseIf settingName == "iHideButton" || settingName == "iCloseButton" || settingName == "iTrackButton"
		Return BookButtons
	ElseIf settingName == "iCloseAction"
		Return CloseActions
	ElseIf settingName == "iFont"
		Return Fonts
	EndIf
	String[] noChoices = New String[1]
	noChoices[0] = "Invalid setting"
	Return noChoices
EndFunction

Int Function FindOption(Int[] options, Int count, Int option)
	Int index = 0
	While index < count
		If options[index] == option
			Return index
		EndIf
		index += 1
	EndWhile
	Return -1
EndFunction

Int Function FindProfile(String profile)
	Int index = 0
	While index < Profiles.Length
		If Profiles[index] == profile
			Return index
		EndIf
		index += 1
	EndWhile
	Return 0
EndFunction

Function RefreshProfiles()
	Profiles = _3DIRP_Settings.GetProfiles()
EndFunction
