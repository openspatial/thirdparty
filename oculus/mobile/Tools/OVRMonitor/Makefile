help:
	@echo Usage....
	@echo "make help           - displays this text"
	@echo "make clean          - delete intermediate files"
	@echo "make all -j20       - build OVRMonitor"
	@echo "make run -j20       - build and run OVRMonitor"
	@echo "make run_debug -j20 - build and run inside debugger"


HOST_OS          = $(shell uname -s | tr \'[:upper:]\' \'[:lower:]\')
HOST_ARCH        = $(shell uname -m | tr \'[:upper:]\' \'[:lower:]\')
HOST_PLATFORM    = $(HOST_OS)_$(HOST_ARCH)

PROJECT_ROOT     = $(realpath .)
VENDOR_ROOT      = $(realpath ../..)/3rdParty
LIBOVRCAP_ROOT   = $(realpath ../..)/VRLib/jni/LibOVR/Src/Capture
THIRDPARTY_ROOT  = $(PROJECT_ROOT)/thirdparty

WXWIDGETS_URL    = https://sourceforge.net/projects/wxwindows/files/3.0.2/wxWidgets-3.0.2.tar.bz2
WXWIDGETS_PAK    = $(VENDOR_ROOT)/wxWidgets-3.0.2.tar.bz2
WXWIDGETS_SHA1   = 6461eab4428c0a8b9e41781b8787510484dea800
WXWIDGETS_SRC    = $(VENDOR_ROOT)/wxWidgets-3.0.2
WXWIDGETS_BUILD  = $(THIRDPARTY_ROOT)/wxwidgets/build_$(HOST_PLATFORM)
WXWIDGETS_ROOT   = $(THIRDPARTY_ROOT)/wxwidgets/$(HOST_PLATFORM)
WX_CONFIG        = $(WXWIDGETS_ROOT)/bin/wx-config

SRC_ROOT         = $(PROJECT_ROOT)/src
BIN_ROOT         = $(PROJECT_ROOT)/bin/$(HOST_PLATFORM)
OBJ_ROOT         = $(PROJECT_ROOT)/obj/$(HOST_PLATFORM)
ICONS_ROOT       = $(PROJECT_ROOT)/icons
RESOURCES_ROOT   = $(PROJECT_ROOT)/resources

CPP_SRC_FILES    = $(wildcard $(SRC_ROOT)/*.cpp)
CPP_OBJ_FILES    = $(patsubst $(SRC_ROOT)/%.cpp,$(OBJ_ROOT)/%.cpp.o,$(CPP_SRC_FILES))

ifeq ($(HOST_OS),darwin)
	MM_SRC_FILES = $(wildcard $(SRC_ROOT)/*.mm)
	MM_OBJ_FILES = $(patsubst $(SRC_ROOT)/%.mm,$(OBJ_ROOT)/%.mm.o,$(MM_SRC_FILES))
endif

SRC_FILES        = $(CPP_SRC_FILES) $(MM_SRC_FILES)
OBJ_FILES        = $(CPP_OBJ_FILES) $(MM_OBJ_FILES)

ICON_PNG_FILES   = $(wildcard $(ICONS_ROOT)/*.png)
ICON_PNG_H_FILES = $(patsubst $(ICONS_ROOT)/%.png,$(SRC_ROOT)/icons/%.png.h,$(ICON_PNG_FILES))

OVRMONITOR_PATH  = $(BIN_ROOT)/OVRMonitor

BIN2C            = $(PROJECT_ROOT)/tools/bin2c.pl
CURL             = $(shell which curl)
WGET             = $(shell which wget)
SHASUM           = $(shell which shasum)

CPPFLAGS         = -I$(LIBOVRCAP_ROOT)/include

ifeq ($(HOST_OS),darwin)
	export CXX             = clang++
	export CC              = clang
	DEBUGGER               = lldb
	INFO_PLIST             = $(RESOURCES_ROOT)/Info.plist
	APP_ICON_PNG           = $(RESOURCES_ROOT)/AppIcon.png
	APP_ICON_ICONSET       = $(OBJ_ROOT)/resources/AppIcon.iconset
	APP_ICON_ICNS          = $(OBJ_ROOT)/resources/AppIcon.icns
	OVRMONITOR_BUNDLE_PATH = $(BIN_ROOT)/OVRMonitor.app
else
	export CXX = g++
	export CC  = gcc
	DEBUGGER   = gdb
endif

ifeq ($(HOST_OS),darwin)
	HOST_CFLAGS  = -mmacosx-version-min=10.9
	HOST_LFLAGS  = -mmacosx-version-min=10.9
	HOST_WXFLAGS = --with-macosx-version-min=10.9
endif

$(WXWIDGETS_PAK):
	@mkdir -p $(dir $(WXWIDGETS_PAK))
ifneq ($(CURL),)
	@$(CURL) -o $(WXWIDGETS_PAK) -L $(WXWIDGETS_URL) && touch -c $(WXWIDGETS_PAK)
else ifneq ($(WGET),)
	@$(WGET) -O $(WXWIDGETS_PAK) $(WXWIDGETS_URL) && touch -c $(WXWIDGETS_PAK)
else
	@$(error curl or wget not found. One of which is required for downloading wxWidgets)
endif

$(WXWIDGETS_SRC): $(WXWIDGETS_PAK)
ifneq ($(SHASUM),)
	@if [ "$(word 1,$(shell $(SHASUM) $(WXWIDGETS_PAK)))" != "$(WXWIDGETS_SHA1)" ]; then echo wxWidgets SHA mismatch... run again to attempt another download && rm $(WXWIDGETS_PAK) && false; fi
else
	@$(error shasum not installed... required for verification of wxWidgets package)
endif
	@cd $(dir $(WXWIDGETS_PAK)) && rm -rf $(WXWIDGETS_SRC) && tar jxf $(WXWIDGETS_PAK) && touch -c $(WXWIDGETS_SRC)

$(WXWIDGETS_ROOT): $(WXWIDGETS_SRC)
	@echo Building WxWidgets...
	@rm -rf $(WXWIDGETS_BUILD) $(WXWIDGETS_ROOT)
	@mkdir -p $(WXWIDGETS_BUILD)
	@cd $(WXWIDGETS_BUILD) && $(WXWIDGETS_SRC)/configure --disable-shared --disable-webviewwebkit --disable-stc $(HOST_WXFLAGS) --prefix="$(WXWIDGETS_ROOT)" && make && make install && touch -c $(WXWIDGETS_ROOT)

external: $(WXWIDGETS_ROOT)
	@echo Thirdparty Build Complete...

$(ICON_PNG_H_FILES): $(ICON_PNG_FILES)
	@mkdir -p $(dir $@)
	@$(BIN2C) $(patsubst $(SRC_ROOT)/icons/%.png.h,$(ICONS_ROOT)/%.png,$@) $@

$(CPP_OBJ_FILES): $(CPP_SRC_FILES) $(WXWIDGETS_ROOT) $(ICON_PNG_H_FILES)
	@mkdir -p $(dir $@)
	@echo Compiling $(patsubst $(OBJ_ROOT)/%.cpp.o,$(SRC_ROOT)/%.cpp,$@)...
	@$(CXX) -g -c $(CPPFLAGS) $(HOST_CFLAGS) $(shell $(WX_CONFIG) --cxxflags) -o $@ $(patsubst $(OBJ_ROOT)/%.cpp.o,$(SRC_ROOT)/%.cpp,$@)

$(MM_OBJ_FILES): $(MM_SRC_FILES) $(WXWIDGETS_ROOT) $(ICON_PNG_H_FILES)
	@mkdir -p $(dir $@)
	@echo Compiling $(patsubst $(OBJ_ROOT)/%.mm.o,$(SRC_ROOT)/%.mm,$@)...
	@$(CXX) -g -c $(CPPFLAGS) $(HOST_CFLAGS) $(shell $(WX_CONFIG) --cxxflags) -o $@ $(patsubst $(OBJ_ROOT)/%.mm.o,$(SRC_ROOT)/%.mm,$@)

$(APP_ICON_ICONSET): $(APP_ICON_PNG)
	@rm -rf $(APP_ICON_ICONSET)
	@mkdir -p $(APP_ICON_ICONSET)
	@echo Creating $(APP_ICON_ICONSET)...
	@sips -z 16 16     $(APP_ICON_PNG) --out $(APP_ICON_ICONSET)/icon_16x16.png
	@sips -z 32 32     $(APP_ICON_PNG) --out $(APP_ICON_ICONSET)/icon_16x16@2x.png
	@sips -z 32 32     $(APP_ICON_PNG) --out $(APP_ICON_ICONSET)/icon_32x32.png
	@sips -z 64 64     $(APP_ICON_PNG) --out $(APP_ICON_ICONSET)/icon_32x32@2x.png
	@sips -z 128 128   $(APP_ICON_PNG) --out $(APP_ICON_ICONSET)/icon_128x128.png
	@sips -z 256 256   $(APP_ICON_PNG) --out $(APP_ICON_ICONSET)/icon_128x128@2x.png
	@sips -z 256 256   $(APP_ICON_PNG) --out $(APP_ICON_ICONSET)/icon_256x256.png
	@sips -z 512 512   $(APP_ICON_PNG) --out $(APP_ICON_ICONSET)/icon_256x256@2x.png
	@sips -z 512 512   $(APP_ICON_PNG) --out $(APP_ICON_ICONSET)/icon_512x512.png

$(APP_ICON_ICNS): $(APP_ICON_ICONSET)
	@mkdir -p $(dir $@)
	@echo Creating $(APP_ICON_ICNS)...
	@iconutil -c icns $(APP_ICON_ICONSET) -o $(APP_ICON_ICNS)

$(OVRMONITOR_PATH): $(OBJ_FILES) $(WXWIDGETS_ROOT) $(APP_ICON_ICNS)
	@mkdir -p $(dir $@)
	@echo Linking $(OVRMONITOR_PATH)...
	@$(CXX) -g $(OBJ_FILES) $(HOST_LFLAGS) $(shell $(WX_CONFIG) --libs) -o $(OVRMONITOR_PATH)
ifneq ($(OVRMONITOR_BUNDLE_PATH),)
	@echo Creating MacOS Bundle...
	@mkdir -p                 $(OVRMONITOR_BUNDLE_PATH)/Contents/MacOS
	@mkdir -p                 $(OVRMONITOR_BUNDLE_PATH)/Contents/Resources
	@echo "APPL????" >        $(OVRMONITOR_BUNDLE_PATH)/Contents/PkgInfo
	@cp -f $(INFO_PLIST)      $(OVRMONITOR_BUNDLE_PATH)/Contents/Info.plist
	@cp -f $(OVRMONITOR_PATH) $(OVRMONITOR_BUNDLE_PATH)/Contents/MacOS/OVRMonitor
	@cp -f $(APP_ICON_ICNS)   $(OVRMONITOR_BUNDLE_PATH)/Contents/Resources/AppIcon.icns
endif

all: $(OVRMONITOR_PATH)
	@echo Done!

run: $(OVRMONITOR_PATH)
ifneq ($(OVRMONITOR_BUNDLE_PATH),)
	@open $(OVRMONITOR_BUNDLE_PATH)
else
	@LD_LIBRARY_PATH=$(WXWIDGETS_ROOT)/lib/ $(OVRMONITOR_PATH)
endif

run_debug: $(OVRMONITOR_PATH)
ifneq ($(OVRMONITOR_BUNDLE_PATH),)
	@$(DEBUGGER) $(OVRMONITOR_BUNDLE_PATH)
else
	@$(DEBUGGER) $(OVRMONITOR_PATH)
endif

clean:
	@rm -rf $(WXWIDGETS_BUILD) $(WXWIDGETS_ROOT) $(BIN_ROOT) $(OBJ_ROOT) $(SRC_ROOT)/icons
