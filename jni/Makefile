NDK_PATH=/opt/android-ndk-r9

APP=pts-multicall
BIN=../libs/armeabi/$(APP)
X86_PATH=x86_bin
X86_BIN=$(X86_PATH)/$(APP)
SRC=main.c pts-exec.c pts-wrap.c pts-daemon.c bcrypt.c \
	blowfish.c helpers.c pts-passwd.c pts-shell.c
UPDATE_ZIP=pts-multi_$(shell date +%Y%m%d)_tan-ce.zip

all : $(BIN) $(X86_BIN) zip
	@echo -e "\\n--- x86 and Android build complete ---"

x86 : $(X86_BIN)

$(BIN) : force-look
	@echo -e "\\n--- Starting NDK build ---"
	@$(NDK_PATH)/ndk-build
	@echo "--- Copying to update_zip ---"
	cp $(BIN) ../update_zip/

$(X86_BIN) : force-look
	@echo -e "\\n--- Starting x86 build ---"
	mkdir -p $(X86_PATH)
	gcc -Wall -D_X86 -D_POSIX_C_SOURCE=200809L -o $@ $(SRC)

clean:
	-rm -rf ../obj/*
	-rm -rf ../libs/*
	-rm -rf x86_bin/$(APP)
	-rm -f ../update_zip/$(APP)

zip : ../$(UPDATE_ZIP)

../$(UPDATE_ZIP): force-look
	@echo -e "\\n--- Starting update zip creation ---"
	-rm -f $@
	(cd ../update_zip; zip ../$(UPDATE_ZIP) `find -type f`)

force-look:
	@true
