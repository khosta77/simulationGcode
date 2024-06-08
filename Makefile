TARGET=./a.out
CC=g++
SRCS=\
	./main.cpp
STD=-std=c++20

all: clean $(TARGET)

LIBS=\
    -ljpeg # Добавить  -lmmal -lmmal_core -lmmal_util, если не будет работать
JPEGLIB=-I /opt/homebrew/Cellar/jpeg-turbo/*/include -L /opt/homebrew/Cellar/jpeg-turbo/*/lib

$(TARGET):
	$(CC) $(STD) $(JPEGLIB) $(LIBS) -O3 -lm -x c++ $(SRCS) -o $(TARGET)

build: $(TARGET)

clean:
	rm -rf $(TARGET)
