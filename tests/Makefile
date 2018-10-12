banchmark: banchmark.c gen.c
	./config.sh
	gcc banchmark.c gen.c -L/usr/local/lib -lzmq -lczmq -g -o banchmark

clean:
	rm -f conf.h
	rm -f banchmark
	rm -rf banchmark.dSYM
