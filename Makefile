CC=gcc
CFLAGS=-Wall -O2 
LIBS=-lcurl
INSTALL_DIR=/usr/local/bin
SRC= keralavision-cli.c
TARGET=kv-cli


all: $(TARGET)  


$(TARGET): $(SRC)
	$(CC) -o $(TARGET) $(CFLAGS) $(SRC) $(LIBS)

install: all
	install -s $(TARGET) $(INSTALL_DIR)

clean:
	rm -f $(TARGET)
