CFLAGS=-I../src
LDFLAGS=-L../lib -lvosk -lportaudio -ldl -lpthread -Wl,-rpath,../lib

all: main

main: main.o
	gcc $^ -o $@ $(LDFLAGS)

#test_vosk_speaker: test_vosk_speaker.o
#	gcc $^ -o $@ $(LDFLAGS)

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o *.a main
