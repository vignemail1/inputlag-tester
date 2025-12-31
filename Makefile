# Makefile pour inputlag-tester (C++ uniquement)

.PHONY: all build clean help

CL = cl
CPPFLAGS = /std:c++17 /W4 /O2 /EHsc
LDLIBS = dxgi.lib d3d11.lib kernel32.lib user32.lib advapi32.lib gdi32.lib

CPP_SRC = inputlag-tester.cpp
CPP_EXE = inputlag-tester.exe

all: build
	@echo [OK] Build complet termine

build: $(CPP_EXE)
	@echo [OK] inputlag-tester compile avec succes

$(CPP_EXE): $(CPP_SRC)
	@echo [*] Compilation C++...
	$(CL) $(CPPFLAGS) $(CPP_SRC) /link $(LDLIBS) /OUT:$(CPP_EXE)
	@echo [OK] $(CPP_EXE) pret

clean:
	@echo [*] Nettoyage...
	@if exist $(CPP_EXE) del /Q $(CPP_EXE)
	@if exist *.obj del /Q *.obj
	@if exist *.lib del /Q *.lib
	@if exist *.ilk del /Q *.ilk
	@if exist *.pdb del /Q *.pdb
	@echo [OK] Nettoyage termine

help:
	@echo.
	@echo ========================================
	@echo   inputlag-tester Build System
	@echo ========================================
	@echo.
	@echo Usage:
	@echo   make           - Build le programme
	@echo   make clean     - Nettoie les binaires
	@echo   make help      - Affiche cette aide
	@echo.
	@echo Quick Start:
	@echo   1. Ouvrir "Developer Command Prompt for VS"
	@echo   2. cd C:\path\to\inputlag-tester
	@echo   3. make
	@echo   4. .\inputlag-tester.exe -n 100 -interval 200
	@echo.
	@echo Flags disponibles:
	@echo   -x int         Coord X region de capture
	@echo   -y int         Coord Y region de capture
	@echo   -w int         Largeur region (defaut: 200)
	@echo   -h int         Hauteur region (defaut: 200)
	@echo   -n int         Nombre d'echantillons (defaut: 100)
	@echo   -warmup int    Echantillons de chauffe (defaut: 10)
	@echo   -interval int  Intervalle en ms entre mouvements (defaut: 200)
	@echo   -dx int        Deplacement X en pixels (defaut: 20)
	@echo.
	@echo Exemples:
	@echo   .\inputlag-tester.exe -n 50 -interval 150
	@echo   .\inputlag-tester.exe -n 100 -interval 200 -warmup 5
	@echo.
