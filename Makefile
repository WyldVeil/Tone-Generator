CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c99 -Isrc
LDFLAGS := -mwindows
LIBS    := -lwinmm -lole32 -lgdi32 -luser32 -lcomctl32

SRC := src/main.c src/audio.c src/synth.c src/gui.c src/presets.c
OUT := ToneGen.exe

TESTS := test_synth.exe test_presets.exe

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS) $(LIBS)

test_synth.exe: tests/test_synth.c src/synth.c src/synth.h
	$(CC) -O0 -g -Wall -Wextra -std=c99 -Isrc tests/test_synth.c src/synth.c -o test_synth.exe -lm

test_presets.exe: tests/test_presets.c src/presets.c src/presets.h
	$(CC) -O0 -g -Wall -Wextra -std=c99 -Isrc tests/test_presets.c src/presets.c -o test_presets.exe

test: $(TESTS)
	./test_synth.exe
	./test_presets.exe

clean:
	rm -f $(OUT) test_synth.exe test_presets.exe

.PHONY: test clean
