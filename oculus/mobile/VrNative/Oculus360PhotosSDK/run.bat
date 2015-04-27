@rem Run.bat for Vr360
@if "%1" NEQ "debug" (
    @if "%1" NEQ "release" (
	    @if "%1" NEQ "clean" (
            @echo The first parameter to run.bat must be one of debug, release or clean.
            @goto:EOF
		)
	)
)
call ..\build.cmd com.oculus.oculus360photossdk Oculus360PhotosSDK-debug.apk com.oculus.oculus360photossdk.MainActivity %1 %2
