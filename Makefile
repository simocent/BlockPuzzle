CFLAGS = -O2
LDFLAGS = -lraylib -lglfw -lGL -lopenal -lm -pthread -ldl
FILES = main.c
a: 
	$(CC) $(FILES) -o blockpuzzle $(LDFLAGS) $(CFLAGS)
