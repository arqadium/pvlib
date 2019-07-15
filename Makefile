
.PHONY: all static shared install clean lint

PROJECT := pv

CFILES := src/pv.c
HFILES := src/pv.h
OFILES := $(CFILES:.c=.o)

LD := $(CC)
AR := ar
STRIP := strip
LIBS :=
LIBDIRS :=
INCLUDES :=
FWORKS :=

CFLAGS := -ansi
ARFLAGS := -rc

ifeq ($(OS),Windows_NT)
	## Windows specific libraries and flags
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		## Linux specific libraries and flags
		INCLUDES += /usr/include/SDL2
		UNAME_P := $(shell uname -p)
		ifeq ($(UNAME_P),x86_64)
			CFLAGS += -m64 -mcpu=nocona
		endif
		ifneq ($(filter %86,$(UNAME_P)),)
			CFLAGS += -m32 -mcpu=pentiumpro
		endif
		#ifneq ($(filter arm%,$(UNAME_P)),)
		#	CFLAGS += -DARCH_ARM
		#endif
	endif
	ifeq ($(UNAME_S),Darwin)
		## macOS specific libraries and flags
		CFLAGS += -m64 -mcpu=core2
	endif
endif

## Mostly no need to edit past this point

LIB := $(patsubst %,-l%,$(LIBS)) $(patsubst %,-L%,$(LIBDIRS))
INCLUDE := $(patsubst %,-I%,$(INCLUDES))
FRAMEWORKS := $(patsubst %,-framework %,$(FWORKS))

ifeq ($(NDEBUG),1)
	CFLAGS += -DNDEBUG=1 -O2 -Wall
else
	CFLAGS += -O0 -g -w
endif

all: static shared

static: $(PROJECT).a

shared: $(PROJECT).so

%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $(INCLUDE) $<

$(PROJECT).a: $(OFILES)
ifeq ($(NDEBUG),1)
	$(STRIP) -s $^
endif
	$(AR) $(ARFLAGS) $@ $^

$(PROJECT).so: $(OFILES)
	$(LD) $(LDFLAGS) -o $@ $^
ifeq ($(NDEBUG),1)
	$(STRIP) -s $@
endif

install:
	[ -f $(PROJECT).a ] && \
		install -Dm644 $(PROJECT).a /usr/local/lib/$(PROJECT).a
	[ -f $(PROJECT).so ] && \
		install -Dm644 $(PROJECT).so /usr/local/lib/$(PROJECT).so
	install -Dm644 src/pv.h \
		/usr/local/include/pv.h

clean:
	rm -f $(OFILES)
	rm -f $(PROJECT).a $(PROJECT).so

lint:
	for _file in $(CFILES) $(HFILES); do \
		clang-format -i -style=file $$_file ; \
	done
	unset _file
