set BIN="../../bin/FbxConvert/Win64/"
%BIN%FbxConvertx64.exe -o home_theater -pack -cinema -stripModoNumbers -rotate 180 -scale 0.01 -translate 3 0 0.45 -flipv -attrib position uv0 -sort origin -tag screen -render home_theater\home_theater.fbx -raytrace screen -include home_theater\icon.png %1 %2 %3 %4
