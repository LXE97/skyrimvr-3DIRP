Scriptname _3DIRP_Settings Hidden

Float Function GetSetting(String settingName) Global Native
Bool Function SetSetting(String settingName, Float value) Global Native

Bool Function ReloadSettings() Global Native
Bool Function LoadProfile(String profile) Global Native
Bool Function CreateProfile(String profile) Global Native
Bool Function SaveSettings() Global Native

String[] Function GetProfiles() Global Native
String Function GetActiveProfile() Global Native
