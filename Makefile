CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = group28_manager
SRC = group28_manager.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)