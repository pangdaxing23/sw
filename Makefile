CC := gcc
BIN := ./bin
SRC := ./src
ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif

$(BIN)/sw: $(SRC)/sw.c
	mkdir -p $(BIN)
	$(CC) $^ -o $@

install: $(BIN)/sw
	install -d $(PREFIX)/bin
	install $(BIN)/sw $(PREFIX)/bin

uninstall: $(PREFIX)/bin/sw
	rm -f $(PREFIX)/bin/sw

clean:
	rm -rf $(BIN)

.PHONY: clean install uninstall
