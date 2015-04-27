#!/bin/bash
if [ "$1" == "" ] ; then
	echo "Usage: make_new_project [project name] [company name (no spaces or punctuation)]"
	exit
fi

mkdir -p ../$1
cp -r jni ../$1
cp -r res ../$1
cp -r src ../$1
cp AndroidManifest.xml ../$1
cp build.xml ../$1
cp .classpath ../$1
cp .cproject ../$1
cp pom.xml ../$1
cp .project ../$1
cp project.properties ../$1
cp run.bat ../$1
cp run.sh ../$1
cp build.sh ../$1

sed -i.deleteme "s/vrtemplate/$1/g"      ../$1/AndroidManifest.xml
sed -i.deleteme "s/VRTemplate/$1/g"      ../$1/build.xml
sed -i.deleteme "s/VRTemplate/$1/g"      ../$1/pom.xml
sed -i.deleteme "s/VRTemplate/$1/g"      ../$1/run.bat
sed -i.deleteme "s/vrtemplate/$1/g"      ../$1/run.bat
sed -i.deleteme "s/VRTemplate/$1/g"      ../$1/run.sh
sed -i.deleteme "s/vrtemplate/$1/g"      ../$1/run.sh
sed -i.deleteme "s/VRTemplate/$1/g"      ../$1/build.sh
sed -i.deleteme "s/VRTemplate/$1/g"      ../$1/.project
sed -i.deleteme "s/VR\ Template/$1/g"    ../$1/res/values/strings.xml

if [ "$2" != "" ] ; then
	sed -i.deleteme "s/yourcompany/$2/g" ../$1/AndroidManifest.xml
	sed -i.deleteme "s/yourcompany/$2/g" ../$1/pom.xml
	sed -i.deleteme "s/yourcompany/$2/g" ../$1/run.bat
	sed -i.deleteme "s/yourcompany/$2/g" ../$1/run.sh
fi

# Cleanup...
find ../$1 -name "*.deleteme" | xargs rm
