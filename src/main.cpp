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

int fill_perc = 53;

const int COLS = 400;
const int ROWS = 300;

const int DISPLAY_WIDTH = 1200;
const int DISPLAY_HEIGHT = 900;

const int CELL_WIDTH = DISPLAY_WIDTH / COLS;
const int CELL_HEIGHT = DISPLAY_HEIGHT / ROWS;


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
void update_grid();
void flip_grid();
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
	al_flip_display();
	al_clear_to_color(floor_color);

	update_grid();

	draw_grid();


}

int get_neighbour_walls(size_t y, size_t x) {
	int walls = 0;
	for(size_t i = y - 1; i <= y + 1; i++)
		for(size_t j = x - 1; j <= x + 1; j++) {
			if(i <= 0 || i >= ROWS - 1 || j <= 0 || j >= COLS - 1) walls++;
			else walls += read_grid[at(i, j)];
		}

	walls -= read_grid[at(y, x)];
	return walls;
}

void update_grid() {
	for(size_t i = 0; i < ROWS; i++) {
		for(size_t j = 0; j < COLS; j++) {
			int walls = get_neighbour_walls(i, j);
			if(walls > 4) write_grid[at(i, j)] = 1;
			else if(walls < 4) write_grid[at(i, j)] = 0;
			else write_grid[at(i, j)] = read_grid[at(i, j)];
		}
	}
}

void flip_grid() {
	uint8_t* tmp = read_grid;
	read_grid = write_grid;
	write_grid = tmp;
}


void draw_grid() {
	flip_grid();
	for(int i = 0; i < ROWS; i++) {
		for(int j = 0; j < COLS; j++) {
			if(read_grid[at(i, j)]) {
				int y = i * CELL_HEIGHT;
				int x = j * CELL_WIDTH;
				al_draw_filled_rectangle(x, y, x + CELL_WIDTH, y + CELL_HEIGHT, wall_color);
			}

		}
	}

}



