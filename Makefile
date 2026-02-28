BOFNAME		:= wpd_notify
CC_X64		:= x86_64-w64-mingw32-g++
CC_X86		:= i686-w64-mingw32-g++
STRIP_X64	:= x86_64-w64-mingw32-strip
STRIP_X86	:= i686-w64-mingw32-strip
CFLAGS		:= -fno-asynchronous-unwind-tables -Os -w -fno-ident

all: x64 x86

x64:
	@ $(CC_X64) -o /tmp/$(BOFNAME).x64.o -c src/$(BOFNAME).cpp $(CFLAGS)
	@ $(STRIP_X64) --strip-unneeded /tmp/$(BOFNAME).x64.o
	@ echo "[*] Compiled $(BOFNAME) (x64)"

x86:
	@ $(CC_X86) -o /tmp/$(BOFNAME).x86.o -c src/$(BOFNAME).cpp $(CFLAGS)
	@ $(STRIP_X86) --strip-unneeded /tmp/$(BOFNAME).x86.o
	@ echo "[*] Compiled $(BOFNAME) (x86)"