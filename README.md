* Builds for x86 Windows (Visual Studio 2013), x86 Linux and Raspberry Pi; follow Urho3D build instructions to set up your environment, then run either cmake_vs2013_q2.bat or cmake_gcc_q2.sh (these helper scripts create a minimal Urho3D config which compiles faster; the Urho3D scripts also work however)
* Build the 'Q2App' target
* Copy 'maps.lst', 'pak0.pak', 'pak1.pak', 'pak2.pak' and 'video' directory from 'baseq2' in your Quake 2 installation directory into 'Bin/baseq2' (alternatively, at least under Visual Studio, you can set debug working directory to your Quake 2 installation directory)
* Run 'Bin/Q2App' (command line arguments for Q2App are passed to Urho3D; Quake 2 command line arguments can be changed by editing Bin/Quake2Data/CommandLine.txt)
* 'Q2App -w' will start Urho3D windowed, 'Q2App -v' will start fullscreen with vsync
