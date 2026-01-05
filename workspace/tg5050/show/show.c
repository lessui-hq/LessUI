// show - Display an image on screen during boot/install/update
// Uses SDL_Renderer for GLES compatibility
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FIXED_BPP 2
#define FIXED_DEPTH (FIXED_BPP * 8)
#define RGBA_MASK_565 0xF800, 0x07E0, 0x001F, 0x0000

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

	// Get display mode for dimensions and rotation detection
	SDL_DisplayMode mode;
	if (SDL_GetCurrentDisplayMode(0, &mode) < 0) {
		fprintf(stderr, "show.elf: SDL_GetCurrentDisplayMode failed: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	int w = mode.w;
	int h = mode.h;
	int p = w * FIXED_BPP;

	// Detect if rotation is needed (portrait panel showing landscape content)
	int rotate = (h > w) ? 3 : 0; // 3 = 270 degrees CCW
	fprintf(stderr, "show.elf: Display mode: %dx%d, rotate=%d\n", w, h, rotate);

	// Create fullscreen window
	fprintf(stderr, "show.elf: Creating window...\n");
	SDL_Window* window =
	    SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h,
	                     SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
	if (!window) {
		fprintf(stderr, "show.elf: SDL_CreateWindow failed: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	// Create renderer with hardware acceleration
	SDL_Renderer* renderer =
	    SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer) {
		fprintf(stderr, "show.elf: SDL_CreateRenderer failed: %s\n", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	// Create streaming texture for our content
	SDL_Texture* texture =
	    SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);
	if (!texture) {
		fprintf(stderr, "show.elf: SDL_CreateTexture failed: %s\n", SDL_GetError());
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	// Create surface to draw on (will be backed by texture pixels)
	SDL_Surface* screen = SDL_CreateRGBSurfaceFrom(NULL, w, h, FIXED_DEPTH, p, RGBA_MASK_565);
	if (!screen) {
		fprintf(stderr, "show.elf: SDL_CreateRGBSurfaceFrom failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(texture);
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	// Lock texture and draw to it
	SDL_LockTexture(texture, NULL, &screen->pixels, &screen->pitch);
	SDL_FillRect(screen, NULL, 0);

	fprintf(stderr, "show.elf: Loading image: %s\n", path);
	SDL_Surface* img = IMG_Load(path);
	if (!img) {
		fprintf(stderr, "show.elf: IMG_Load failed: %s\n", IMG_GetError());
		SDL_UnlockTexture(texture);
		SDL_FreeSurface(screen);
		SDL_DestroyTexture(texture);
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	fprintf(stderr, "show.elf: Image size: %dx%d\n", img->w, img->h);

	// Center the image on screen
	SDL_Rect dst = {(screen->w - img->w) / 2, (screen->h - img->h) / 2, img->w, img->h};
	SDL_BlitSurface(img, NULL, screen, &dst);
	SDL_FreeSurface(img);
	SDL_UnlockTexture(texture);

	// Render with rotation if needed
	SDL_RenderClear(renderer);
	if (rotate) {
		// Rotate around center, fill screen
		SDL_RenderCopyEx(renderer, texture, NULL, NULL, rotate * 90, NULL, SDL_FLIP_NONE);
	} else {
		SDL_RenderCopy(renderer, texture, NULL, NULL);
	}
	SDL_RenderPresent(renderer);

	fprintf(stderr, "show.elf: Displaying for %d seconds...\n", delay);
	sleep(delay);

	// Cleanup
	SDL_FreeSurface(screen);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	fprintf(stderr, "show.elf: Done\n");
	return 0;
}
