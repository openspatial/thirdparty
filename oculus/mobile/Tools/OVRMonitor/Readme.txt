Oculus Remote Monitor

-----------------------------------------------------------------------------------------------------------------------
Instructions
-----------------------------------------------------------------------------------------------------------------------

About:
    OVRMonitor      - Mac/Linux/Windows client that connects, captures and displays performance metrics on a remote
                      VRLib instance.
    OVRCapture      - Library built into LibOVR/VRLib capable of capturing and sending performance metrics to a
                      remote client. Its functionality is disabled unless OVRSettings enables it at launch.


Build OVRMonitor:
    On Mac/Linux simply use the Makefile in this directory. The Makefile has several rules to make life easier...
        all         - will build OVRMonitor.
        run         - will build and run OVRMonitor.
        run_debug   - will build and run OVRMonitor attached to the debugger.
        clean       - deletes all intermediate files so that next build is a total rebuild.
    So for example, you can quickly build and run like so...
        make run
    
    On Windows simply double click build.bat... This assumes you have Visual Studio 2013 installed.
    Currently the MSBuild files used for building on Windows don't work correctly inside the IDE, this will be
    fixed in the not too distant future. So for now build from command-line or use the convenient batch file.

    After building, executable can be found under "bin/<platform>/"

Enable VR Developer Mode:
    For security reasons the Capture Library won't accept connections unless VR Developer Mode is turned on and Capture 
    support is enabled in your device's Oculus Developer Preferences file.

    1. Go to “Application Manager→Gear VR Service→Manage Storage”
    2. Tap on the version number until the “Developer Mode” checkbox appears
    3. Tap the “Developer Mode” checkbox


Enable Capture in Developer Preferences:
    Append the line “dev_enableCapture 1” to the file “/sdcard/.oculusprefs” on your target device. A simple way to set
    the contents of this file is by issuing the following commands in your Terminal / Command Line...
        adb shell
        echo dev_enableCapture 1 > /sdcard/.oculusprefs

    It is always a good idea to delete the Developer Preferences file when done to prevent Capture support from turning 
    on unexpectedly. This can be done simply with...
        adb shell rm /sdcard/.oculusprefs


Connecting:
    Make sure your PC and target GearVR device are on the same network. Ideally a rather low traffic 802.11n/ac
    network as FrameBuffer capturing consumes a signifant amount of bandwidth.

    Launch the Application.

    If things work as planned, OVRMonitor should auto-discover your application and list it in the "Available Hosts"
    view. At this point you can simply select your host+application and click "Connect".

    Once connected, if you see "Send" events taking a long time in the Profile Data view when connected, this likely
    means you are network bandwidth bound. Over WiFi its normal for this to happen periodically, but if it happens
    for more than a few seconds at a time the Capture library internal buffer may fill up, and if this happens
    it may either drop data or stall the application. In either case it will disrupt the collection of accurate
    data. So it is recommended to use a dedicated network for capturing data. If you are unable to use a network
    with a high enough quality of service, you may have to disable Frame Buffer Capturing when you connect.



-----------------------------------------------------------------------------------------------------------------------
Notes
-----------------------------------------------------------------------------------------------------------------------

Overhead:
    It is very important that OVRCapture have minimal impact when OVRMonitor is not connected. So to that end
    it is gauranteed that if Init()/Shutdown() are never called, no memory allocations will take place and no
    sockets or threads will be created and every other call will do nothing more than a check against a bool
    and return.

    The usage intention is that you should only call Init()/Shutdown() when you are in profiling mode (by having
    a bit set in your Android Intent set).

    Once you do call Init(), several allocations are performed to create some thread saftey objects, sockets, and
    background threads. But until a connection is actually established the other API calls will return immediately.

    When a connection is established, great effort is taken to make sure the overhead is minimal. But you should
    expect that once a connection is formed each API call is potentially accessing some Thread Local Storage and
    doing some atomic operations.


Dependencies:
    There are intentially no dependencies on LibOVR for the time being as its not yet clear at what level this
    will be integrated into every application and what classes/functions will be exposed in the public API in
    the long term.

    While we should probably allow a custom allocator to be used by OVR::Capture, for the time being the assumption
    is OVR::Capture::Init() will never be called unless you are trying to attach to OVRMonitor during development
    and if Init() is never called, no memory allocations will ever take place inside OVRCapture.


TODO:
    The ZeroConfig mechanism in place right now will just start broadcasting endlessly once a second once Init() is
    made, which isn't ideal. In the future it will listen for a broadcast and reply with a UDP SendTo() directly
    to the source of the broadcast, which should keep the ZeroConfig thread asleep until someone "pings" it.

    Handle multiple connects/disconnects... pretty sure right now that once you disconnect reconnecting will likely
    hit loads of issues with any stack based packets (CPU/GPU Zones, etc).

    Currently logging into VPN breaks the UDP Broadcast mechanism... to fix this we should scan the interface list
    to detect the correct subnet...

    Experiment with doing on-device frame buffer compression (jpeg?)... need to do careful analysis of CPU and memory
    bandwidth overhead of doing this. But will likely do it and keep it as a togglable option.

    WiFi Direct support to bypass centralized routers, weak signals and hopefully improve quality of service.

    Someday in the future it would be nice to share the LibOVR Kernel helper classes/functions/etc... right now there
    is a bit of duplication, particularly in threading and file IO.


-----------------------------------------------------------------------------------------------------------------------
Third Party
-----------------------------------------------------------------------------------------------------------------------

wxWidgets:
    Cross platform UI based on wxWidgets - http://www.wxwidgets.org

Font Awesome:
    Some art inside OVRMonitor is based on Font Awesome by Dave Gandy - http://fontawesome.io

