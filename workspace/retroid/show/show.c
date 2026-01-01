// rgb30 - Display an image on screen during boot/install/update
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

SDL_Window* window;
SDL_Surface* screen;

int main(int argc, char* argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: show.elf image.png [delay]\n");
		return 1;
	}

	char path[256];
	snprintf(path, sizeof(path), "%s", argv[1]);
	if (access(path, F_OK) != 0) {
		fprintf(stderr, "show.elf: Image not found: %s\n", path);
		return 1;
	}

	int delay = argc > 2 ? atoi(argv[2]) : 2;

	fprintf(stderr, "show.elf: Initializing SDL2...\n");
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "show.elf: SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	SDL_ShowCursor(0);

	// Use 0,0 to let SDL auto-detect display size (like tg5040)
	fprintf(stderr, "show.elf: Creating window...\n");
	window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_SHOWN);
	if (!window) {
		fprintf(stderr, "show.elf: SDL_CreateWindow failed: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	screen = SDL_GetWindowSurface(window);
	if (!screen) {
		fprintf(stderr, "show.elf: SDL_GetWindowSurface failed: %s\n", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	fprintf(stderr, "show.elf: Window size: %dx%d\n", screen->w, screen->h);
	SDL_FillRect(screen, NULL, 0);

	fprintf(stderr, "show.elf: Loading image: %s\n", path);
	SDL_Surface* img = IMG_Load(path);
	if (!img) {
		fprintf(stderr, "show.elf: IMG_Load failed: %s\n", IMG_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	fprintf(stderr, "show.elf: Image size: %dx%d\n", img->w, img->h);

	// Center the image on screen
	SDL_Rect dst = {(screen->w - img->w) / 2, (screen->h - img->h) / 2, img->w, img->h};
	SDL_BlitSurface(img, NULL, screen, &dst);

	if (SDL_UpdateWindowSurface(window) < 0) {
		fprintf(stderr, "show.elf: SDL_UpdateWindowSurface failed: %s\n", SDL_GetError());
	}

	fprintf(stderr, "show.elf: Displaying for %d seconds...\n", delay);
	sleep(delay);

	SDL_FreeSurface(img);
	SDL_DestroyWindow(window);
	SDL_Quit();

	fprintf(stderr, "show.elf: Done\n");
	return 0;
}
