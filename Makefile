CFLAGS=-Wall -pedantic -g 
LIBS= -L/usr/lib -lcurl 
PREFIX=/usr/local

crypto_watch: crypto_watch.c cJSON/cJSON.c
	$(CC) $(CFLAGS) $^ -O3 -o $@ $(LIBS)

.PHONY: clean
clean:
	rm crypto_watch 

.PHONY: install
install: crypto_watch 
	cp $< $(HOME)/.config/argos/crypto_watch.15m

.PHONY: uninstall 
uninstall:
	rm $(PREFIX)/bin/crypto_watch 
