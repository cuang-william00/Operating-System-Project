
assignment3: assignment3.c
	gcc -o assignment3 assignment3.c
all: assignment3

debug: assignment3.c
	gcc -o assignment3 assignment3.c -g -O0

run:
	./assignment3 -l 25565 -p asdffdsa &

send_all_files:
	./runtestclients.sh

test: assignment3 run send_all_files

clean:
	rm -f assignment3
	rm -f *.txt
