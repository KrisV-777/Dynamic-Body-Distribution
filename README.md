# Dynamic Body Distribution ([Nexus Mods Mirror](https://www.nexusmods.com/skyrimspecialedition/mods/157779))

SKSE plugin that allows for runtime replacement of body textures and morphs.

The entire plugin operates on `.yml` config files to apply replacements at runtime as actors load. How to write these configs and setup the necessary folders can be read up on in the [wiki](https://github.com/KrisV-777/Dynamic-Body-Distribution/wiki).

## Requirements
* [xmake](https://xmake.io/#/)
	* Add this to your `PATH`
* [PowerShell](https://github.com/PowerShell/PowerShell/releases/latest)
* [Visual Studio Community 2022](https://visualstudio.microsoft.com/)
	* Desktop development with C++
* [CommonLibSSE](https://github.com/powerof3/CommonLibSSE/tree/dev)
	* You need to build from the powerof3/dev branch
* Create Environment Variables:
  * `XSE_TES5_MODS_PATH`: Path to your MO2/Vortex `mods` folder
  * `XSE_TES5_GAME_PATH`: Path to your SSE install directory

## Building
```
git clone https://github.com/KrisV-777/Dynamic-Body-Distribution.git
cd Dynamic-Body-Distribution
git submodule update --init --recursive
xmake f -m release [
	--copy_to_papyrus=(y/n)		# create/update a mod instance
]
xmake
```
