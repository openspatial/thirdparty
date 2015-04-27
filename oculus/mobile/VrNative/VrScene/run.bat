@rem run.bat for VrScene
@if "%1" NEQ "debug" (
    @if "%1" NEQ "release" (
	    @if "%1" NEQ "clean" (
		@if "%1" NEQ "retail" (
            		@echo The first parameter to run.bat must be one of debug, release or clean.
            		@goto:EOF
		)
	    )
	)
)
call ..\build.cmd com.oculusvr.vrscene bin/MainActivity-debug.apk com.oculusvr.vrscene.MainActivity %1 %2

