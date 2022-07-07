#include <chrono>
#include <iostream>
#include <time.h>
#include <string>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>
#include <mpi.h>
#include <random>
#include "Settings.hpp"

// #define GRAPHIC_MODE
// #define PARALLEL_MODE

#define neighbour_radius settings->neighbour_radius
#define COLS settings->COLS
#define ROWS settings->ROWS
#define CELL_WIDTH settings->CELL_WIDTH
#define CELL_HEIGHT settings->CELL_HEIGHT

Settings* settings;

uint8_t* write_grid;
uint8_t* read_grid;

int max_neighbours, half_neighbours;
int my_rank, n_procs;


#ifdef GRAPHIC_MODE
ALLEGRO_FONT* font;
ALLEGRO_DISPLAY* display;
ALLEGRO_EVENT_QUEUE* queue;
ALLEGRO_TIMER* timer;

ALLEGRO_COLOR wall_color, floor_color;
int DISPLAY_WIDTH, DISPLAY_HEIGHT;
#endif // GRAPHIC_MODE



inline int at(int y, int x) {
	return y * COLS + x;
}

void initialize();
void initialize(std::string* settingsPath);
void initialize_random_grid();
void terminate();
void frame_update();
void update_grid();
void flip_grid();

#ifdef GRAPHIC_MODE
void draw_grid();
#endif // GRAPHIC_MODE

#ifdef PARALLEL_MODE
void send_grid();
void recv_grid();
#endif // PARALLEL_MODE


int main(int argc, char const* argv[])
{
	if(argc == 2) {
		std::string settingsPath = argv[1];
		initialize(&settingsPath);
	}
	else initialize();


	#ifndef PARALLEL_MODE
	initialize_random_grid();
	#else
	if(my_rank == 0) {
		initialize_random_grid();
		send_grid();
	}
	else {
		recv_grid();
	}

	#endif // PARALLEL_MODE


	auto t1 = std::chrono::high_resolution_clock::now();

	int generation = 0;
	bool running = true;
	while(running) {

		#ifdef GRAPHIC_MODE
		ALLEGRO_EVENT event;
		al_wait_for_event(queue, &event);
		if(event.type == ALLEGRO_EVENT_KEY_UP || event.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
			running = false;
		else if(event.type == ALLEGRO_EVENT_TIMER) {
			#endif // GRAPHIC_MODE

			frame_update();
			if(++generation == settings->last_generation) {
				running = false;
			}
			#ifdef GRAPHIC_MODE
		}
		#endif // GRAPHIC_MODE
	}
	auto t2 = std::chrono::high_resolution_clock::now();
	auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

	std::cout << ms_int.count() << std::endl;

	#ifdef GRAPHIC_MODE
	terminate();
	#endif // GRAPHIC_MODE

	return 0;
}

void initialize() {
	initialize(nullptr);
}

void initialize(std::string* settingsPath) {
	if(settingsPath)
		settings = new Settings(*settingsPath);
	else
		settings = new Settings();

	write_grid = new uint8_t[ROWS * COLS];
	read_grid = new uint8_t[ROWS * COLS];

	max_neighbours = (neighbour_radius * (neighbour_radius + 1) * 4);
	half_neighbours = max_neighbours / 2;
	#ifdef GRAPHIC_MODE
	if(COLS * ROWS > 1382400) {
		std::cout << "Grid is too large for graphic mode" << std::endl;
		exit(1);

	}
	DISPLAY_WIDTH = COLS * CELL_WIDTH;
	DISPLAY_HEIGHT = ROWS * CELL_HEIGHT;
	wall_color = al_map_rgb(settings->wall_color.r, settings->wall_color.g, settings->wall_color.b);
	floor_color = al_map_rgb(settings->floor_color.r, settings->floor_color.g, settings->floor_color.b);
	#endif // GRAPHIC_MODE

	if(!al_init()) fprintf(stderr, "Failed to initialize allegro.\n");
	if(!al_install_keyboard()) fprintf(stderr, "Failed to install keyboard.\n");

	#ifdef GRAPHIC_MODE
	if(!al_init_font_addon()) fprintf(stderr, "Failed to initialize font addon.\n");
	if(!al_init_ttf_addon()) fprintf(stderr, "Failed to initialize ttf addon.\n");
	if(!al_init_primitives_addon()) fprintf(stderr, "Failed to initialize primitives addon.\n");

	font = al_load_ttf_font("fonts/Roboto/Roboto-Medium.ttf", 20, 0);
	display = al_create_display(DISPLAY_WIDTH, DISPLAY_HEIGHT);
	queue = al_create_event_queue();
	timer = al_create_timer(1.0 / 60);

	if(!font) fprintf(stderr, "Failed load font.\n");
	if(!display) fprintf(stderr, "Failed initialize display.\n");
	if(!queue) fprintf(stderr, "Failed create queue.\n");
	if(!timer) fprintf(stderr, "Failed to create timer.\n");

	al_register_event_source(queue, al_get_display_event_source(display));
	al_register_event_source(queue, al_get_keyboard_event_source());
	al_register_event_source(queue, al_get_timer_event_source(timer));
	al_start_timer(timer);
	#endif

	#ifdef PARALLEL_MODE
	MPI_Init(NULL, NULL);

	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &n_procs);
	#endif // PARALLEL_MODE
}

void initialize_random_grid() {
	if(settings->rand_seed)
		srand(settings->rand_seed);
	else srand(time(0));

	for(int i = 0; i < ROWS; i++) {
		for(int j = 0; j < COLS; j++) {
			read_grid[at(i, j)] = (rand() % 100) < settings->initial_fill_perc;
		}
	}
	memccpy(write_grid, read_grid, ROWS * COLS, sizeof(read_grid[0]));
}

void terminate()
{
	al_uninstall_keyboard();
	#ifdef GRAPHIC_MODE
	al_destroy_event_queue(queue);
	al_destroy_display(display);
	al_destroy_font(font);
	#endif // GRAPHIC_MODE

	delete settings;
	delete[] read_grid;
	delete[] write_grid;
}

void frame_update() {
	#ifdef GRAPHIC_MODE
	al_flip_display();
	al_clear_to_color(floor_color);
	draw_grid();
	#endif // GRAPHIC_MODE

	update_grid();
	flip_grid();

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
			if(walls >= half_neighbours + settings->roughness)
				write_grid[at(i, j)] = 1;
			else if(walls <= half_neighbours - settings->roughness)
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

#ifdef GRAPHIC_MODE
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
#endif

void send_grid() {
	int rowsPerThread = ROWS / n_procs;

	for(int target = 1; target < n_procs - 1; target++) {
		MPI_Request req;


		int start_row = rowsPerThread * target - neighbour_radius;
		int count = (rowsPerThread + (neighbour_radius * 2)) * COLS;
		MPI_Isend(&read_grid[at(start_row, 0)], count, MPI_UINT8_T, target, 55, MPI_COMM_WORLD, &req);
	}

	// last thread send	
	int last_thread = n_procs - 1;
	// get the remaining rows + padding
	int rowsPerLastThread = ROWS - (rowsPerThread * last_thread);

	MPI_Request last_req;


	int start_row = rowsPerThread * last_thread;
	int count = (rowsPerLastThread + neighbour_radius) * COLS;
	MPI_Isend(&read_grid[at(start_row, 0)], count, MPI_UINT8_T, last_thread, 55, MPI_COMM_WORLD, &last_req);

}

void recv_grid() {

}


