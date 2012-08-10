all: build schemeken

SOURCES= \
Slip/SlipCompile.c \
Slip/SlipCache.c \
Slip/SlipDictionary.c \
Slip/SlipEnvironment.c \
Slip/SlipEvaluate.c \
Slip/SlipGrammar.c \
Slip/SlipMain.c \
Slip/SlipMemory.c \
Slip/SlipNative.c \
Slip/SlipPool.c \
Slip/SlipPrint.c \
Slip/SlipRead.c \
Slip/SlipScan.c \
Slip/SlipStack.c \
Slip/SlipThread.c \
Slip/SlipPersist.c \
Slip/SlipKen.c \
Ken/ken.c \
Ken/kencom.c \
Ken/kencrc.c \
Ken/kenext.c \
Ken/kenpat.c \
Ken/kenvar.c

DEBUG_FLAGS=-g

CFLAGS=$(DEBUG_FLAGS) -I Ken -I Slip
LDFLAGS=-Xlinker -no_pie

CFLAGS+=-std=gnu99
OBJECTS=$(addprefix build/,$(notdir $(SOURCES:.c=.o)))

build/%.o: Slip/%.c
	gcc -c -o $@ $(CFLAGS) $^

build/%.o: Ken/%.c
	gcc -c -o $@ $(CFLAGS) $^

build:
	mkdir build build/Slip build/Ken

schemeken: $(OBJECTS)
	gcc $(DEBUG_FLAGS) $(LDFLAGS) -o $@ $^

clean:
	-rm -r build
	-rm schemeken
