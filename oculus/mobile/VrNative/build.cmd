@if "%OCULUS_SDK_PATH%"=="" (
	@set OCULUS_SDK_PATH="..\.."
)
@set OVR_RUN_OLDDIR=%CD%
@set PACKAGE_NAME=%1
@for %%* in (.) do set CURRENT_DIR_NAME=%%~n*
@REM Note that parameter #2, APK_NAME is no longer used ... we need to clean up all run.bat files to remove it
@set APK_NAME=%2
@set ACTIVITY_NAME=%3
@if "%4"=="debug" (
	@set DEBUG=1
	@set RETAIL=0
	@echo Selecting DEBUG.
) else (
	@if "%4"=="release" (
		@set DEBUG=0
		@set RETAIL=0
		@echo Selecting RELEASE.
	) else (
		@if "%4"=="retail" (
			@set DEBUG=0
			@set RETAIL=1
			@echo Selecting RETAIL.
		) else (
			if "%4"=="clean" (
				@goto :CleanProjects
			) else (
				@echo Unknown paramter "%4"! Aborting!
				@goto:EOF
			)
		)
	)
)
@if "%5"=="-n" (
	@set INSTALL_TO_PHONE=0
	@echo Skipping installation and launch.
) else (
	@set INSTALL_TO_PHONE=1
)

@set DEBUG_APK_NAME=%CURRENT_DIR_NAME%-debug.apk
@set RELEASE_APK_NAME=%CURRENT_DIR_NAME%-release-unsigned.apk
@set RETAIL_APK_NAME=%CURRENT_DIR_NAME%-release-signed.apk
@echo 1
@if %DEBUG%==1 (
	@set FINAL_APK_NAME=%DEBUG_APK_NAME%
) else (
	@echo 2
	@if %RETAIL%==1 (
		@set FINAL_APK_NAME=%RETAIL_APK_NAME%
	) else (
		@set FINAL_APK_NAME=%RELEASE_APK_NAME%
	)
)
@echo ActivityName: %ACTIVITY_NAME%
@echo Outputting to package: bin\%FINAL_APK_NAME%

@rem Check that Android projects are udpated, so that ndk-build doesn't fail after clean repository get
@if exist local.properties goto NoProjectUpdate

@echo ========================== Update Project ===========================
call android update project -t android-19 -p . -s

:NoProjectUpdate
@if exist %OCULUS_SDK_PATH%\VRLib\local.properties goto NoVRLibUpdate
@cd %OCULUS_SDK_PATH%\VRLib
rem Updating %OCULUS_SDK_PATH%\VRLib android project...

call android update project -t android-19 -p . -s
:NoVRLibUpdate

@if exist %OCULUS_SDK_PATH%\Tools\BreakpadClient\local.properties goto NoBreakpadUpdate
@cd %OCULUS_SDK_PATH%\Tools\BreakpadClient
rem Updating %OCULUS_SDK_PATH%\Tools\BreakpadClient android project...

call android update project -t android-19 -p . -s

@cd /d %OVR_RUN_OLDDIR%
:NoBreakpadUpdate
@echo =============================== Build VRLib =================================
@ver > nul &rem Resets ERRORLEVEL to 0.
@cd %OCULUS_SDK_PATH%\VRLib
call ndk-build V=0 -j10 NDK_DEBUG=0 OVR_DEBUG=%DEBUG%
if %ERRORLEVEL% NEQ 0 (
	@echo *****************************************************************************
	@echo ERROR: ndk-build had errors building VRLib. Terminating batch file.
	@echo *****************************************************************************
	goto :End
)
@echo Nuking VRLib/bin/res/crunch...
rd /S /Q bin\res\crunch
@rem @echo Deleting VRLib/libs/armeabi-v7a/gdbserver...
@rem del /Q libs\armeabi-v7a\gdb*.*

@echo ========================== Native Build Project =============================
@cd /d %OVR_RUN_OLDDIR%
call ndk-build V=0 -j10 NDK_DEBUG=%DEBUG% OVR_DEBUG=%DEBUG%
if %ERRORLEVEL% NEQ 0 (
	@echo *****************************************************************************
	@echo ERROR: ndk-build had errors. Terminating batch file.
	@echo *****************************************************************************
	goto :End
)
@echo Nuking bin/res/crunch...
rd /S /Q bin\res\crunch

@echo =========================== Ant Build Project ===============================
@echo 3
@if %DEBUG%==1 (
	@echo Building DEBUG apk.
	call ant -quiet debug
) else (
	@echo 4
	@if %RETAIL%==1 (
		@echo Building Signed RETAIL apk for alias %CURRENT_DIR_NAME%
		call ant -quiet clean
		call ant -quiet release
		del bin\%FINAL_APK_NAME%
		@echo signing
		@rem sign the app with the private key modifies apk in-place
		call jarsigner -verbose -sigalg MD5withRSA -digestalg SHA1 -keystore %OCULUS_SDK_PATH%\bin\Signing\Release\oculusvr-release-key.keystore bin\%RELEASE_APK_NAME% oculusvr
		@echo verifying
		@rem verify apk is signed
		@rem call jarsigner -verify -verbose -certs bin\%RELEASE_APK_NAME%
		@rem align apk
		call zipalign -v 4 bin\%RELEASE_APK_NAME% bin\%FINAL_APK_NAME%
	) else (
		@echo Building RELEASE apk.
		@rem For the time being, just build debug releases until we have a keystore for signing
		@rem We do the rename to avoid confusion, since ant will name this as -debug.apk but
		@rem the native code is all built with release optimizations.
		@rem	call ant -quiet release
		call ant -quiet debug
		if %ERRORLEVEL% NEQ 0 (
			@echo *****************************************************************************
			@echo ERROR: ant had errors. Terminating batch file.
			@echo *****************************************************************************
			goto :End
		)
		del bin\%FINAL_APK_NAME%
		ren bin\%DEBUG_APK_NAME% %FINAL_APK_NAME%
		if %ERRORLEVEL% NEQ 0 (
			@echo *****************************************************************************
			@echo ERROR: ren bin\%DEBUG_APK_NAME% %FINAL_APK_NAME% failed
			@echo *****************************************************************************
			goto :End
		)
	)
)
if %ERRORLEVEL% NEQ 0 (
	@echo *****************************************************************************
	@echo ERROR: ant had errors. Terminating batch file.
	@echo *****************************************************************************
	goto :End
)

@echo 5
@if %INSTALL_TO_PHONE%==1 goto DoUninstall2
@echo -n: skipping apk installation.
@goto End
:DoUninstall2
@echo =========================== Uninstall Project ===============================
@echo Uninstall may report failure if the package doesn't exist. This is not fatal.
adb uninstall %PACKAGE_NAME%

:DoInstall
@echo ============================ Install Project ================================
adb install bin\%FINAL_APK_NAME%
if %ERRORLEVEL% EQU 0 goto DoStart
@echo *****************************************************************************
@echo ERROR: Failed to install apk. If the remove failed above, you may need to remove the package manually.
@echo *****************************************************************************
goto :End

:DoStart
@echo ============================= Start Activity ================================
@echo Starting %PACKAGE_NAME%/%ACTIVITY_NAME%
adb shell am start %PACKAGE_NAME%/%ACTIVITY_NAME%
if %ERRORLEVEL% EQU 0 goto End
@echo *****************************************************************************
@echo ERROR: Failed to start activity is the PACKAGE_NAME variable set correctly and is the activity name correct?
@echo *****************************************************************************
goto :End

:CleanProjects
cd %OCULUS_SDK_PATH%\VRLib
call ndk-build clean NDK_DEBUG=1
call ndk-build clean NDK_DEBUG=0
call ant clean
del /S /Q obj\*.* > NUL
rmdir /S /Q obj
del /S /Q bin\*.* > NUL
rmdir /S /Q bin
cd /d %OVR_RUN_OLDDIR%
call ndk-build clean NDK_DEBUG=0
call ndk-build clean NDK_DEBUG=1
call ant clean
del /S /Q obj\*.* > NUL
rmdir /S /Q obj
del /S /Q bin\*.* > NUL
rmdir /S /Q bin
goto :End

:End
@cd /d %OVR_RUN_OLDDIR%
@echo Done.
