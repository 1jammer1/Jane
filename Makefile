# Makefile for HTTP Server/Client Audio Upload Program

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lmicrohttpd -lcurl
TARGET = uploader
SRC = main.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)
