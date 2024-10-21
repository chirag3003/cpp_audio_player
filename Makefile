build:
	g++ -std=c++17 -o omniplay main.cpp -lSDL2 -lSDL2_mixer -lsqlite3

run:
	./omniplay

clean:
	rm omniplay

watch:
	ls *.cpp | entr make build

.PHONY: build run clean watch
