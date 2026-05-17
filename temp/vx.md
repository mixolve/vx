.current paired device identifier from xcrun devicectl list devices: `CF070CDB-CA7C-5C56-A472-79E0C4E098D3`

.only this palette allowed in the project: white, black, gray shades, #9999FF, #FFCC99, no gradient colors

.only one font across the ui: Sometype Mono, size 22
.only lower or upper case text (except VX by MIXOLVE and module labels such as EQE)

.root build tree is `temp/build/`, with separate platform folders inside it:
.`temp/build/macos-vst3` for macOS VST3
.`temp/build/macos-au` for macOS AU
.`temp/build/ios-device` for iOS AUv3 on a physical device
.`temp/build/ios-simulator` for iOS AUv3 on a simulator

.store helper files, logs, temporary files, and scripts in `temp/`
