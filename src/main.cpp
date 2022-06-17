#include <chrono>
#include <iostream>
#include <time.h>
#include <string>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>

#define GRAPHIC_MODE

ALLEGRO_FONT* font;
ALLEGRO_DISPLAY* display;
ALLEGRO_EVENT_QUEUE* queue;
ALLEGRO_TIMER* timer;


int rand_seed = 0xCAFFE;


int initial_fill_perc = 51;

const int COLS = 500;
const int ROWS = 300;

const int CELL_WIDTH = 3;
const int CELL_HEIGHT = 3;

const int DISPLAY_WIDTH = COLS * CELL_WIDTH;
const int DISPLAY_HEIGHT = ROWS * CELL_HEIGHT;

int stopwatch_last_gen = 20;
int generation = 0;

const uint8_t neighbour_radius = 3;

const int max_neighbours = (neighbour_radius * (neighbour_radius + 1) * 4);
const int half_neighbours = max_neighbours / 2;

const int roughness = 4;


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
	#ifdef GRAPHIC_MODE
	initialize();
	#endif
	initialize_random_grid();

	auto t1 = std::chrono::high_resolution_clock::now();


	bool running = true;
	while(running) {
		#ifdef GRAPHIC_MODE
		ALLEGRO_EVENT event;
		al_wait_for_event(queue, &event);
		if(event.type == ALLEGRO_EVENT_KEY_UP || event.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
			running = false;
		else if(event.type == ALLEGRO_EVENT_TIMER)
			#endif
		{
			frame_update();
			if(generation++ == stopwatch_last_gen) {
				auto t2 = std::chrono::high_resolution_clock::now();

				/* Getting number of milliseconds as an integer. */
				auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
				std::cout << ms_int.count() << std::endl;
				running = false;
			}
		}
	}
	#ifdef GRAPHIC_MODE
	terminate();
	#endif

	return 0;
}

void initialize() {
	if(!al_init()) fprintf(stderr, "Failed to initialize allegro.\n");
	if(!al_install_keyboard()) fprintf(stderr, "Failed to install keyboard.\n");
	#ifdef GRAPHIC_MODE
	if(!al_init_font_addon()) fprintf(stderr, "Failed to initialize font addon.\n");
	if(!al_init_ttf_addon()) fprintf(stderr, "Failed to initialize ttf addon.\n");
	if(!al_init_primitives_addon()) fprintf(stderr, "Failed to initialize primitives addon.\n");

	font = al_load_ttf_font("fonts/Roboto/Roboto-Medium.ttf", 20, 0);
	display = al_create_display(DISPLAY_WIDTH, DISPLAY_HEIGHT);
	#endif
	queue = al_create_event_queue();
	timer = al_create_timer(1.0 / 60);

	#ifdef GRAPHIC_MODE
	if(!font) fprintf(stderr, "Failed load font.\n");
	if(!display) fprintf(stderr, "Failed initialize display.\n");
	#endif
	if(!queue) fprintf(stderr, "Failed create queue.\n");
	if(!timer) fprintf(stderr, "Failed to create timer.\n");

	#ifdef GRAPHIC_MODE
	al_register_event_source(queue, al_get_display_event_source(display));
	#endif
	al_register_event_source(queue, al_get_keyboard_event_source());
	al_register_event_source(queue, al_get_timer_event_source(timer));
	al_start_timer(timer);
}

void initialize_random_grid() {
	if(rand_seed)
		srand(rand_seed);
	else srand(time(0));

	for(int i = 0; i < ROWS; i++) {
		for(int j = 0; j < COLS; j++) {
			read_grid[at(i, j)] = (rand() % 100) < initial_fill_perc;
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
	#ifdef GRAPHIC_MODE
	al_flip_display();
	al_clear_to_color(floor_color);
	#endif

	update_grid();
	flip_grid();

	#ifdef GRAPHIC_MODE
	draw_grid();
	#endif
}

int get_neighbour_walls(int y, int x) {
	int walls = 0;
	for(int i = y - neighbour_radius; i <= y + neighbour_radius; i++)
		for(int j = x - neighbour_radius; j <= x + neighbour_radius; j++) {
			if(i <= neighbour_radius || i >= ROWS - neighbour_radius || j <= neighbour_radius || j >= COLS - neighbour_radius) walls++;
			else walls += read_grid[at(i, j)];
		}

	walls -= read_grid[at(y, x)];
	return walls;
}

void update_grid() {
	for(int i = 0; i < ROWS; i++) {
		for(int j = 0; j < COLS; j++) {
			int walls = get_neighbour_walls(i, j);
			if(walls >= half_neighbours + roughness)
				write_grid[at(i, j)] = 1;
			else if(walls <= half_neighbours - roughness)
				write_grid[at(i, j)] = 0;
			else
				write_grid[at(i, j)] = read_grid[at(i, j)];
		}
	}
}

void flip_grid() {
	uint8_t* tmp = read_grid;
	read_grid = write_grid;
	write_grid = tmp;
}


void draw_grid() {
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



