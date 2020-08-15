all: mouse-guard-console.exe mouse-guard.exe

mouse-guard-console.exe: console.cpp
	cl /nologo /c /O2 /GS- /GL /arch:SSE2 /EHsc /MD /DUNICODE console.cpp
	link /opt:ref /opt:icf /debug:none /ltcg /out:mouse-guard-console.exe console.obj user32.lib
	del console.obj

mouse-guard.exe: gui.cpp resource.h gui.rc app.ico
	rc /nologo gui.rc
	cl /nologo /c /O2 /GS- /GL /arch:SSE2 /EHsc /Fe:mouse-guard.exe /MD /DUNICODE gui.cpp
	link /opt:ref /opt:icf /debug:none /ltcg /out:mouse-guard.exe gui.obj gui.res user32.lib comctl32.lib gdi32.lib shell32.lib
	del gui.obj gui.res
