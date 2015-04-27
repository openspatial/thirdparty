@if "%1"=="" (
	@echo Usage: make_new_project ^[project name^] ^[company name ^(no spaces or punctuation^)^]
	@goto End
)

mkdir ..\%1
xcopy /e /s /i jni ..\%1\jni\
xcopy /e /s /i res ..\%1\res\
xcopy /e /s /i src ..\%1\src\
copy AndroidManifest.xml ..\%1
copy build.xml ..\%1
copy .classpath ..\%1
copy .cproject ..\%1
copy pom.xml ..\%1
copy .project ..\%1
copy project.properties ..\%1
copy run.bat ..\%1
copy run.sh ..\%1
copy build.sh ..\%1

sed -i s/vrtemplate/%1/g ..\%1\AndroidManifest.xml
sed -i s/VRTemplate/%1/g ..\%1\build.xml
sed -i s/VRTemplate/%1/g ..\%1\pom.xml
sed -i s/VRTemplate/%1/g ..\%1\run.bat
sed -i s/vrtemplate/%1/g ..\%1\run.bat
sed -i s/VRTemplate/%1/g ..\%1\run.sh
sed -i s/VRTemplate/%1/g ..\%1\build.sh
sed -i s/vrtemplate/%1/g ..\%1\run.sh
sed -i s/VRTemplate/%1/g ..\%1\.project
sed -i s/"VR Template"/%1/g ..\%1\res\values\strings.xml

if NOT "%2"=="" (
	sed -i s/yourcompany/%2/g ..\%1\AndroidManifest.xml
	sed -i s/yourcompany/%2/g ..\%1\pom.xml
	sed -i s/yourcompany/%2/g ..\%1\run.bat
	sed -i s/yourcompany/%2/g ..\%1\run.sh
)

@echo Done.
:End
