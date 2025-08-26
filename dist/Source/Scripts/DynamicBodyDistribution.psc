ScriptName DynamicBodyDistribution Hidden

; Reset this actors 3d, allows you to manually force-apply profiles to this actor
Function Reset3D(Actor akActor) global native

; Get all profiles that this actor is compatible with. If akActor == none, return all profiles
String[] Function GetTextureProfiles(Actor akActor) global native
String[] Function GetSliderProfiles(Actor akActor) global native

; Apply and set the given profile to this actor
bool Function ApplyTextureProfile(Actor akActor, String a_profile) global native
bool Function ApplySliderProfile(Actor akActor, String a_profile) global native

; Get the profiles currently used by this actor, formatted as [TextureProfile, SliderProfile]
String[] Function GetProfiles(Actor akActor) global native
; Delete the cache entries for this actor and optionally disable them from being processed by DBD
; if not excluded, profiles will be randomly re-assigned the next time their are loaded/their 3D is reset
Function ClearProfiles(Actor akActor, bool abAndExclude) native global
