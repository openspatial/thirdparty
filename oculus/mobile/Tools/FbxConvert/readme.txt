

MSVC 2013
---------

Before compiling, download and install the Autodesk FBX SDK 2015.1

Copy the SDK from:

C:\Program Files\Autodesk\FBX\FBX SDK\2015.1\

to a folder:

\3rdParty\FBXSDK\2015.1\

relative to where the Oculus Mobile SDK is installed.

Load the FbxConvert.sln to compile the source.


Mac OSX
-------

Before compiling download and install the Autodesk FBX SDK 2015.1 to:

/Applications/Autodesk/FBXSDK/2015.1

Note that by default it will install to a folder with a space
between FBX and SDK.

Compile the source with make:

make -f Makefile_Mac


Linux
-----

Before compiling download and install the Autodesk FBX SDK 2015.1

Compile the source with make:

make -f Makefile_Linux
