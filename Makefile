main.exe: src/main.c
	x86_64-w64-mingw32-gcc -nostdlib -mwindows src/main.c -lkernel32 -lmsvcrt -luser32 -lshell32 -o main.exe -s -fno-ident -fno-asynchronous-unwind-tables
