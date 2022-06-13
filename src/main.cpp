#include <iostream>
#include <time.h>
#include <string>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>


ALLEGRO_FONT* font;
ALLEGRO_DISPLAY* display;
ALLEGRO_EVENT_QUEUE* queue;
ALLEGRO_TIMER* timer;

const int ROWS = 100;
const int COLS = 100;

const int DISPLAY_WIDTH = 800;
const int DISPLAY_HEIGHT = 800;

const int CELL_WIDTH = DISPLAY_WIDTH / ROWS;
const int CELL_HEIGHT = DISPLAY_HEIGHT / COLS;

int fill_perc = 60;

uint8_t* write_grid = new uint8_t[ROWS * COLS];
uint8_t* read_grid = new uint8_t[ROWS * COLS];

ALLEGRO_COLOR wall_color = al_map_rgb(66, 58, 25);
ALLEGRO_COLOR floor_color = al_map_rgb(201, 162, 95);


inline int at(int y, int x) {
	return y * COLS + x;
}

void initialize();
void initialize_random_grid();
void terminate();
void frame_update();
void draw_grid();

int main(int argc, char const* argv[])
{
	initialize();
	initialize_random_grid();

	bool running = true;
	while(running) {
		ALLEGRO_EVENT event;
		al_wait_for_event(queue, &event);
		if(event.type == ALLEGRO_EVENT_KEY_UP || event.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
			running = false;
		else if(event.type == ALLEGRO_EVENT_TIMER) {
			frame_update();
		}

	}


	terminate();

	return 0;
}

void initialize() {
	if(!al_init()) fprintf(stderr, "Failed to initialize allegro.\n");
	if(!al_init_font_addon()) fprintf(stderr, "Failed to initialize font addon.\n");
	if(!al_init_ttf_addon()) fprintf(stderr, "Failed to initialize ttf addon.\n");
	if(!al_init_primitives_addon()) fprintf(stderr, "Failed to initialize primitives addon.\n");
	if(!al_install_keyboard()) fprintf(stderr, "Failed to install keyboard.\n");

	font = al_load_ttf_font("fonts/Roboto/Roboto-Medium.ttf", 20, 0);
	display = al_create_display(DISPLAY_WIDTH, DISPLAY_HEIGHT);
	queue = al_create_event_queue();
	timer = al_create_timer(1.0 / 60);

	if(!font) fprintf(stderr, "Failed load font.\n");
	if(!display) fprintf(stderr, "Failed initialize display.\n");
	if(!queue) fprintf(stderr, "Failed create queue.\n");
	if(!timer) fprintf(stderr, "Failed to create timer.\n");

	al_register_event_source(queue, al_get_keyboard_event_source());
	al_register_event_source(queue, al_get_display_event_source(display));
	al_register_event_source(queue, al_get_timer_event_source(timer));
	al_start_timer(timer);
}

void initialize_random_grid() {
	srand(time(0));
	for(int i = 0; i < ROWS; i++) {
		for(int j = 0; j < COLS; j++) {
			read_grid[at(i, j)] = (rand() % 100) < fill_perc;
		}
	}
	memccpy(write_grid, read_grid, ROWS * COLS, sizeof(read_grid[0]));
}

void terminate()
{
	al_uninstall_keyboard();
	al_destroy_event_queue(queue);
	al_destroy_display(display);
	al_destroy_font(font);
}

void frame_update() {
	draw_grid();
	al_flip_display();
}

void draw_grid() {
	for(int i = 0; i < ROWS; i++) {
		for(int j = 0; j < COLS; j++) {
			int y = i * CELL_HEIGHT;
			int x = j * CELL_WIDTH;

			if(read_grid[at(i, j)]) al_draw_filled_rectangle(x, y, x + CELL_WIDTH, y + CELL_HEIGHT, wall_color);
			else al_draw_filled_rectangle(x, y, x + CELL_WIDTH, y + CELL_HEIGHT, floor_color);
		}
	}

}


