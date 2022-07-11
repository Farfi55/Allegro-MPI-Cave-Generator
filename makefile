CC = mpiCC
FLAGS = -O2 -I/usr/include/allegro5 -L/usr/lib -lallegro -lallegro_font -lallegro_ttf -lallegro_primitives
SOURCE = src/main.cpp
BIN = bin/cavegen

all:
	$(CC) $(SOURCE) $(FLAGS) -o $(BIN)
