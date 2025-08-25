ScriptName DBDConsole Hidden

String Function GetArrayString(String[] arr) global
  String result = "[\n"
  int i = 0
  While (i < arr.Length)
    result += "\t" + arr[i] + "\n"
    i += 1
  EndWhile
  result += "]"
  return result
EndFunction

Function Reset3D(Actor akActor) global
  If (!akActor)
    ConsoleUtil.PrintConsole("Invalid actor reference.")
    return
  EndIf
  DynamicBodyDistribution.Reset3D(akActor)
EndFunction

Function PrintTextureProfiles(Actor akActor) global
  String[] profiles = DynamicBodyDistribution.GetTextureProfiles(akActor)
  ConsoleUtil.PrintConsole("Texture Profiles for " + akActor + ":")
  ConsoleUtil.PrintConsole(GetArrayString(profiles))
EndFunction

Function PrintSliderProfiles(Actor akActor) global
  String[] profiles = DynamicBodyDistribution.GetSliderProfiles(akActor)
  ConsoleUtil.PrintConsole("Slider Profiles for " + akActor + ":")
  ConsoleUtil.PrintConsole(GetArrayString(profiles))
EndFunction

Function ApplyTextureProfileConsole(Actor akActor, String a_profile) global
  If (!akActor)
    ConsoleUtil.PrintConsole("Invalid actor reference.")
    return
  EndIf
  bool result = DynamicBodyDistribution.ApplyTextureProfile(akActor, a_profile)
  If result
    ConsoleUtil.PrintConsole("Applied texture profile: " + a_profile)
  Else
    ConsoleUtil.PrintConsole("Failed to apply texture profile: " + a_profile)
  EndIf
EndFunction

Function ApplySliderProfileConsole(Actor akActor, String a_profile) global
  If (!akActor)
    ConsoleUtil.PrintConsole("Invalid actor reference.")
    return
  EndIf
  bool result = DynamicBodyDistribution.ApplySliderProfile(akActor, a_profile)
  If result
    ConsoleUtil.PrintConsole("Applied slider profile: " + a_profile)
  Else
    ConsoleUtil.PrintConsole("Failed to apply slider profile: " + a_profile)
  EndIf
EndFunction

Function PrintCurrentProfiles(Actor akActor) global
  String[] profiles = DynamicBodyDistribution.GetProfiles(akActor)
  ConsoleUtil.PrintConsole("Texture Profile: " + profiles[0])
  ConsoleUtil.PrintConsole("Slider Profile: " + profiles[1])
EndFunction
