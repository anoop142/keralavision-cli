CC=gcc
CFLAGS=-Wall -O2 
LIBS=-lcurl
LOCATION=/usr/local/bin
SRC= keralavision-cli.c
TARGET=kv-cli


all: $(TARGET)  


$(TARGET): $(SRC)
	$(CC) -o $(TARGET) $(CFLAGS) $(SRC) $(LIBS)

install: all
	strip $(TARGET)
	cp $(TARGET) $(LOCATION)

clean:
	rm -rf $(TARGET)
