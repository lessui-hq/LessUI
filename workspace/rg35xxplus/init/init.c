#include <SDL/SDL.h>
int main(void) {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_SetVideoMode(0,0,0,0);
	SDL_Quit();
	return 0;
}
