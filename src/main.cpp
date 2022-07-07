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

#define GRAPHIC_MODE
// #define PARALLEL_MODE
#define DEGUB_MODE

Settings* settings;

uint8_t* write_grid;
uint8_t* read_grid;
uint8_t* full_grid; // full grid used for drawing


/**
 * how many cells to consider in each direction.
 * example with radius: 1
 * _ _ _ _ _ _ _ _ _
 * _ _ _ x x x _ _ _
 * _ _ _ x o x _ _ _
 * _ _ _ x x x _ _ _
 * _ _ _ _ _ _ _ _ _
 * example with radius: 2
 * _ _ x x x x x _ _
 * _ _ x x x x x _ _
 * _ _ x x o x x _ _
 * _ _ x x x x x _ _
 * _ _ x x x x x _ _
 */
int radius;

// maxiumum number of wall neightbours around any cell
int max_neighbours;
// half the maximun number of wall neighbours around any cell
int half_neighbours;

int my_rank = 0; // MPI rank
int n_procs = 1; // MPI size

int my_rows, my_inner_rows, tot_inner_rows;
int my_cols, my_inner_cols, tot_inner_cols;

#ifdef PARALLEL_MODE
int neighbours_ranks[3][3];

MPI_Datatype column_type; // for sending/receiving left and right columns
MPI_Datatype row_type; // for sending/receiving top and bottom rows
MPI_Datatype angle_type; // for sending/receiving corners
MPI_Datatype inner_grid_type;
MPI_Comm cave_comm;
#endif // PARALLEL_MODE

#ifdef GRAPHIC_MODE

ALLEGRO_FONT* font;
ALLEGRO_DISPLAY* display;
ALLEGRO_EVENT_QUEUE* queue;
ALLEGRO_TIMER* timer;

ALLEGRO_COLOR wall_color, floor_color;
int DISPLAY_WIDTH, DISPLAY_HEIGHT;
#endif // GRAPHIC_MODE


inline int at(int y, int x) {
	return y * my_cols + x;
}

void initialize();
void initialize(std::string* settingsPath);
void serial_initialize_random_grid();
void terminate();
void frame_update();
void update_grid();
void flip_grid();

void check_generic_settings();

#ifdef GRAPHIC_MODE
void graphic_initialize();
void serial_draw_grid();
void check_graphic_settings();

#ifdef PARALLEL_MODE
void parallel_draw_grid();
#endif // PARALLEL_MODE

#endif // GRAPHIC_MODE

#ifdef PARALLEL_MODE
void parallel_initialize();
void check_parallel_settings();
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


	auto start_time = std::chrono::high_resolution_clock::now();

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
	auto end_time = std::chrono::high_resolution_clock::now();
	auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

	std::cout << ms_int.count() << std::endl;

	terminate();

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

	tot_inner_rows = settings->rows;
	tot_inner_cols = settings->cols;
	radius = settings->neighbour_radius;
	max_neighbours = (radius * (radius + 1) * 4);
	half_neighbours = max_neighbours / 2;



	// checkGeneralSettings();

	#ifdef PARALLEL_MODE
	my_inner_rows = settings->rows / settings->threads_per_col;
	my_inner_cols = settings->cols / settings->threads_per_row;
	#else 
	my_inner_rows = settings->rows;
	my_inner_cols = tot_inner_cols;
	#endif // PARALLEL_MODE

	my_rows = my_inner_rows + 2 * radius;
	my_cols = my_inner_cols + 2 * radius;


	#ifdef PARALLEL_MODE
	parallel_initialize();
	#endif // PARALLEL_MODE


	#ifdef GRAPHIC_MODE
	graphic_initialize();
	#endif // GRAPHIC_MODE

	#ifdef DEGUB_MODE
	std::cout << "my_rows: " << my_rows << std::endl;
	std::cout << "my_cols: " << my_cols << std::endl;
	std::cout << "my_inner_rows: " << my_inner_rows << std::endl;
	std::cout << "my_inner_cols: " << my_inner_cols << std::endl;
	std::cout << "draw_edges: " << settings->draw_edges << std::endl;
	#endif // DEGUB_MODE

	// create grid and set to 0 every element
	write_grid = new uint8_t[my_rows * my_cols]{ };
	read_grid = new uint8_t[my_rows * my_cols]{ };


	#ifdef PARALLEL_MODE
	if(my_rank == 0) {
		initialize_random_grid();
		send_grid();
	}
	else {
		recv_grid();
	}
	#else
	serial_initialize_random_grid();
	#endif // PARALLEL_MODE

}

#ifdef GRAPHIC_MODE
void graphic_initialize() {
	if(my_rank == 0) {
		check_graphic_settings();
		DISPLAY_WIDTH = settings->cols * settings->cell_width;
		DISPLAY_HEIGHT = settings->rows * settings->cell_height;
		if(settings->draw_edges) {
			DISPLAY_WIDTH += 2 * radius * settings->cell_width;
			DISPLAY_HEIGHT += 2 * radius * settings->cell_height;
		}

		wall_color = al_map_rgb(settings->wall_color.r, settings->wall_color.g, settings->wall_color.b);
		floor_color = al_map_rgb(settings->floor_color.r, settings->floor_color.g, settings->floor_color.b);
		if(!al_init()) fprintf(stderr, "Failed to initialize allegro.\n");
		if(!al_install_keyboard()) fprintf(stderr, "Failed to install keyboard.\n");
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

		al_set_app_name("cave generator");
	}
}
#endif


#ifdef PARALLEL_MODE
void parallel_initialize() {
	MPI_Init(NULL, NULL);

	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &n_procs);

	check_parallel_settings();

	int dims[2] = { settings->threads_per_row, settings->threads_per_col };
	int periods[2] = { 0, 0 };


	MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &cave_comm);

	int my_coords[2];
	MPI_Cart_coords(cave_comm, my_rank, 2, my_coords);

	for(int i = 0; i < 3; i++) {
		for(int j = 0; j < 3; j++) {
			const int coords[] = { my_coords[0] + i - 1,my_coords[1] + j - 1 };
			MPI_Cart_rank(cave_comm, coords, &neighbours_ranks[i][j]);
		}
	}


	MPI_Type_vector(radius, my_cols, radius * 2, MPI_UINT8_T, &column_type);

}
#endif // PARALLEL_MODE

#ifdef PARALLEL_MODE

void parallel_initialize_random_grid() {
	if(settings->rand_seed)
		srand(settings->rand_seed);
	else srand(time(0));

	full_grid = new uint8_t[tot_inner_rows * tot_inner_cols];

	for(int i = 0; i < tot_inner_rows; i++) {
		for(int j = 0; j < tot_inner_cols; j++) {
			full_grid[i * tot_inner_cols + j] = (rand() % 100) < settings->initial_fill_perc;
		}
	}
}

#else

void serial_initialize_random_grid() {
	if(settings->rand_seed)
		srand(settings->rand_seed);
	else srand(time(0));

	int fill_perc = settings->initial_fill_perc;
	for(int i = 0; i < my_rows; i++) {
		for(int j = 0; j < my_cols; j++) {
			if(i <= radius || i >= my_rows - radius || j <= radius || j >= my_cols - radius)
				write_grid[at(i, j)] = read_grid[at(i, j)] = 1;
			else write_grid[at(i, j)] = read_grid[at(i, j)] = ((rand() % 100) < fill_perc);
		}
	}
	// memccpy(write_grid, read_grid, my_rows * my_cols, sizeof(write_grid[0]));
}

#endif // PARALLEL_MODE


void terminate()
{
	#ifdef GRAPHIC_MODE
	al_uninstall_keyboard();
	al_destroy_event_queue(queue);
	al_destroy_display(display);
	al_destroy_font(font);
	#endif // GRAPHIC_MODE

	delete settings;
	delete[] read_grid;
	delete[] write_grid;

	#ifdef PARALLEL_MODE
	if(my_rank == 0) {
		delete[] full_grid;
	}
	#endif // PARALLEL_MODE
}


void check_general_settings() {



}



#ifdef PARALLEL_MODE

void check_parallel_settings() {
	if(settings->threads_per_col <= 0) {
		std::cout << "threads_per_col must be greater than 0" << std::endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
		exit(1);
	}
	if(settings->threads_per_col <= 0) {
		std::cout << "threads_per_col must be greater than 0" << std::endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
		exit(1);
	}
	if(settings->rows % settings->threads_per_col != 0) {
		std::cout << "Rows have to be divisible by the number of threads per column" << std::endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
		exit(1);
	}
	if(settings->cols % settings->threads_per_row != 0) {
		std::cout << "Cols have to be divisible by the number of threads per row" << std::endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
		exit(1);
	}
	if(settings->threads_per_row * settings->threads_per_col != n_procs) {
			// make sure the product of dims is equal to the number of processes
		std::cout << "Error: number of processes does not match the number of threads" << std::endl;
		std::cout << "number of processes: " << n_procs << std::endl;
		std::cout << "number of threads per row: " << settings->threads_per_row << std::endl;
		std::cout << "number of threads per column: " << settings->threads_per_col << std::endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
		exit(1);
	}
}
#endif // PARALLEL_MODE


#ifdef GRAPHIC_MODE

void check_graphic_settings() {
	if(settings->cols * settings->rows > 1382400) {
		std::cout << "Grid is too large for graphic mode" << std::endl;
		exit(1);

	}
}
#endif // GRAPHIC_MODE



void frame_update() {
	#ifdef GRAPHIC_MODE
	if(my_rank == 0) {
		al_flip_display();
		al_clear_to_color(wall_color);

		#ifdef PARALLEL_MODE 
		parallel_draw_grid();
		#else  
		serial_draw_grid();
		#endif // PARALLEL_MODE
	}
	#endif // GRAPHIC_MODE

	update_grid();
	flip_grid();

}

int get_neighbour_walls(int y, int x) {
	int walls = 0;
	for(int i = y - radius; i <= y + radius; i++)
		for(int j = x - radius; j <= x + radius; j++) {
			walls += read_grid[at(i, j)];
		}

	walls -= read_grid[at(y, x)];
	return walls;
}

void update_grid() {
	for(int i = radius; i < my_rows - radius; i++) {
		for(int j = radius; j < my_cols - radius; j++) {
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
#ifdef PARALLEL_MODE
void parallel_draw_grid() {
	for(int i = 0; i < tot_inner_rows; i++) {
		for(int j = 0; j < tot_inner_cols; j++) {
			if(full_grid[i * tot_inner_cols + j] == 0) {

				int y = i * settings->CELL_HEIGHT;
				int x = j * settings->CELL_WIDTH;
				al_draw_filled_rectangle(x, y, x + settings->CELL_WIDTH, y + settings->CELL_HEIGHT, floor_color);

			}
		}
	}
}
#endif // PARALLEL_MODE


void serial_draw_grid() {

	for(int i = radius; i < my_rows - radius; i++) {
		for(int j = radius; j < my_cols - radius; j++) {
			if(read_grid[at(i, j)] == 0) {

				int y = (i - (!settings->draw_edges * radius)) * settings->cell_height;
				int x = (j - (!settings->draw_edges * radius)) * settings->cell_width;

				al_draw_filled_rectangle(x, y, x + settings->cell_width, y + settings->cell_height, floor_color);

			}
		}
	}
}
#endif


#ifdef PARALLEL_MODE
void send_grid() {
	// int rowsPerThread = ROWS / n_procs;

	// for(int target = 1; target < n_procs - 1; target++) {
	// 	MPI_Request req;


	// 	int start_row = rowsPerThread * target - radius;
	// 	int count = (rowsPerThread + (radius * 2)) * COLS;
	// 	MPI_Isend(&read_grid[at(start_row, 0)], count, MPI_UINT8_T, target, 55, MPI_COMM_WORLD, &req);
	// }

	// // last thread send
	// int last_thread = n_procs - 1;
	// // get the remaining rows + padding
	// int rowsPerLastThread = ROWS - (rowsPerThread * last_thread);

	// MPI_Request last_req;


	// int start_row = rowsPerThread * last_thread;
	// int count = (rowsPerLastThread + radius) * COLS;
	// MPI_Isend(&read_grid[at(start_row, 0)], count, MPI_UINT8_T, last_thread, 55, MPI_COMM_WORLD, &last_req);

}

void recv_grid() {

}


#endif // PARALLEL_MODE

