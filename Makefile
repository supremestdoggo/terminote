CFLAGS += -lncurses

# full installation
install:
	@echo "Building terminote..."
	@$(CC) main.c -O3 $(CFLAGS) -o terminote
	@echo "Preparing .terminote directory..."
	@mkdir -pv ~/.terminote/notes
	@echo "Installing terminote..."
	@mv -f terminote /usr/local/bin/terminote
	@echo "Done."

# builds and prepares directory, but doesn't install
dev:
	@echo "Building terminote..."
	@$(CC) main.c -lncurses -o terminote
	@echo "Preparing .terminote directory..."
	@mkdir -pv ~/.terminote/notes