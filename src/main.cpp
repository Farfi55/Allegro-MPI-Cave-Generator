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
#define PARALLEL_MODE
#define DEBUG_MODE

#define ROOT_RANK 0

Settings* settings;

uint8_t* write_grid;
uint8_t* read_grid;
uint8_t* root_grid; // full grid used for drawing


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

int inner_grid_size;

int generation = 0;
bool is_running = true;

#ifdef PARALLEL_MODE
int neighbours_ranks[3][3];

MPI_Datatype inner_grid_t;
MPI_Datatype contiguous_grid_t;
MPI_Datatype column_t; // for sending/receiving left and right columns
MPI_Datatype row_t; // for sending/receiving top and bottom rows
MPI_Datatype corner_t; // for sending/receiving corners
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

inline bool isValidCoord(int y, int x) {
	return y >= 0 && y < settings->y_threads&& x >= 0 && x < settings->x_threads;
}
inline bool isValidCoord(const int coords[]) {
	return isValidCoord(coords[0], coords[1]);
}
inline bool isValidCoord(int coords[]) {
	return isValidCoord(coords[0], coords[1]);
}

inline bool isCoordsEqual(const int coords1[], const int coords2[]) {
	return coords1[0] == coords2[0] && coords1[1] == coords2[1];
}
inline bool isCoordsEqual(int coords1[], int coords2[]) {
	return coords1[0] == coords2[0] && coords1[1] == coords2[1];
}

void initialize();
void initialize(std::string* settingsPath);
void serial_initialize_random_grid();
void terminate();
void frame_update();
void update_grid();
void flip_grid();
void check_generic_settings();
void no_graphic_loop();

// graphic only
void graphic_initialize();
void serial_draw_grid();
void check_graphic_settings();
void graphic_parallel_loop();

// parallel only
void parallel_draw_grid();
void parallel_initialize_random_grid();
void parallel_initialize();
void check_parallel_settings();

void send_initial_grid();
void receive_initial_grid();

void send_columns();
void send_rows();
void send_corners();

void receive_columns();
void receive_rows();
void receive_corners();



int main(int argc, char const* argv[])
{
	if(argc == 2) {
		std::string settingsPath = argv[1];
		initialize(&settingsPath);
	}
	else initialize();


	auto start_time = std::chrono::high_resolution_clock::now();


	while(is_running) {
		std::cout << "gen: " << generation << std::endl;
		#ifdef GRAPHIC_MODE
		#ifdef PARALLEL_MODE
		graphic_parallel_loop();
		#else // NO_PARALLEL_MODE
		graphic_serial_loop();
		#endif // PARALLEL_MODE
		#else // NO_GRAPHIC_MODE
		no_graphic_loop();
		#endif // GRAPHIC_MODE
	}
	auto end_time = std::chrono::high_resolution_clock::now();
	auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

	std::cout << ms_int.count() << std::endl;
	if(!(settings->results_file_path.empty())) {
		std::ofstream results_file;
		results_file.open(settings->results_file_path, std::ios::ate);
		results_file << ms_int.count() << std::endl;
		results_file.close();
	}

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
	my_inner_rows = settings->rows / settings->x_threads;
	my_inner_cols = settings->cols / settings->y_threads;
	#else 
	my_inner_rows = settings->rows;
	my_inner_cols = settings->cols;
	#endif // PARALLEL_MODE

	my_rows = my_inner_rows + 2 * radius;
	my_cols = my_inner_cols + 2 * radius;
	inner_grid_size = my_inner_rows * my_inner_cols;


	#ifdef PARALLEL_MODE
	parallel_initialize();
	#endif // PARALLEL_MODE


	#ifdef GRAPHIC_MODE
	graphic_initialize();
	#endif // GRAPHIC_MODE

	#ifdef DEBUG_MODE
	// std::cout << "my_rows: " << my_rows << std::endl;
	// std::cout << "my_cols: " << my_cols << std::endl;
	// std::cout << "my_inner_rows: " << my_inner_rows << std::endl;
	// std::cout << "my_inner_cols: " << my_inner_cols << std::endl;
	// std::cout << "draw_edges: " << settings->draw_edges << std::endl << std::endl;
	#endif // DEBUG_MODE

	// create grid and set to 0 every element
	write_grid = new uint8_t[my_rows * my_cols]{ };
	read_grid = new uint8_t[my_rows * my_cols]{ };


	#ifdef PARALLEL_MODE
	if(my_rank == ROOT_RANK) {
		parallel_initialize_random_grid();
		send_initial_grid();
	}
	else {
		receive_initial_grid();
	}

	#else
	serial_initialize_random_grid();
	#endif // PARALLEL_MODE

}

#ifdef GRAPHIC_MODE
void graphic_initialize() {
	if(my_rank == ROOT_RANK) {
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
		timer = al_create_timer(1.0 / settings->max_frame_rate);

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
#endif // GRAPHIC_MODE


#ifdef PARALLEL_MODE
void parallel_initialize() {

	MPI_Init(NULL, NULL);

	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &n_procs);

	check_parallel_settings();

	int dims[2] = { settings->y_threads, settings->x_threads };
	int periods[2] = { 0, 0 };


	MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &cave_comm);

	int my_coords[2];
	MPI_Cart_coords(cave_comm, my_rank, 2, my_coords);
	std::cout << "rank: " << my_rank << " coords: " << my_coords[0] << " " << my_coords[1] << std::endl;

	for(int i = 0; i < 3; i++) {
		for(int j = 0; j < 3; j++) {
			const int coords[] = { my_coords[0] + i - 1,my_coords[1] + j - 1 };
			if(isValidCoord(coords)) {
				MPI_Cart_rank(cave_comm, coords, &neighbours_ranks[i][j]);
				#ifdef DEBUG_MODE
				std::printf("my_rank %d, neighbours_ranks[%d][%d]: %d\n", my_rank, i, j, neighbours_ranks[i][j]);
				#endif // DEBUG_MODE
			}
			else {
				neighbours_ranks[i][j] = MPI_PROC_NULL; // -1
			}

		}
	}

	const int outer_sizes[] = { my_rows, my_cols };
	const int inner_sizes[] = { my_inner_rows, my_inner_cols };
	const int starts[] = { radius, radius };
	MPI_Type_create_subarray(2, outer_sizes, inner_sizes, starts, MPI_ORDER_C, MPI_UINT8_T, &inner_grid_t);
	MPI_Type_contiguous(inner_grid_size, MPI_UINT8_T, &contiguous_grid_t);

	MPI_Type_vector(my_inner_rows, radius, my_cols, MPI_UINT8_T, &column_t);
	MPI_Type_vector(radius, my_inner_cols, my_cols, MPI_UINT8_T, &row_t);
	MPI_Type_vector(radius, radius, my_cols, MPI_UINT8_T, &corner_t);



	MPI_Type_commit(&inner_grid_t);
	MPI_Type_commit(&contiguous_grid_t);
	MPI_Type_commit(&column_t);
	MPI_Type_commit(&row_t);
	MPI_Type_commit(&corner_t);

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
	if(my_rank == ROOT_RANK) {
		delete[] root_grid;
	}


	MPI_Type_free(&inner_grid_t);
	MPI_Type_free(&contiguous_grid_t);
	MPI_Type_free(&column_t);
	MPI_Type_free(&row_t);
	MPI_Type_free(&corner_t);

	MPI_Comm_free(&cave_comm);

	MPI_Finalize();
	#endif // PARALLEL_MODE
}


void check_general_settings() {



}



#ifdef PARALLEL_MODE

void check_parallel_settings() {
	if(settings->x_threads <= 0) {
		std::cout << "x_threads must be greater than 0" << std::endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
		exit(1);
	}
	if(settings->x_threads <= 0) {
		std::cout << "x_threads must be greater than 0" << std::endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
		exit(1);
	}
	if(settings->rows % settings->x_threads != 0) {
		std::cout << "Rows have to be divisible by the number of threads per column" << std::endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
		exit(1);
	}
	if(settings->cols % settings->y_threads != 0) {
		std::cout << "Cols have to be divisible by the number of threads per row" << std::endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
		exit(1);
	}
	if(settings->y_threads * settings->x_threads != n_procs) {
			// make sure the product of dims is equal to the number of processes
		std::cout << "Error: number of processes does not match the number of threads" << std::endl;
		std::cout << "number of processes: " << n_procs << std::endl;
		std::cout << "number of threads per row: " << settings->y_threads << std::endl;
		std::cout << "number of threads per column: " << settings->x_threads << std::endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
		exit(1);
	}
}
#endif // PARALLEL_MODE


#ifdef GRAPHIC_MODE

void check_graphic_settings() {
	if(settings->cols * settings->rows > 1382400) {
		std::cout << "Grid is too large for graphic mode" << std::endl;

		#ifdef PARALLEL_MODE
		MPI_Abort(MPI_COMM_WORLD, 1);
		#endif // PARALLEL_MODE
		exit(1);

	}
}
#endif // GRAPHIC_MODE


#ifdef GRAPHIC_MODE

#ifdef PARALLEL_MODE
void graphic_parallel_loop() {
	if(my_rank == ROOT_RANK) {
		ALLEGRO_EVENT event;
		al_wait_for_event(queue, &event);
		if(event.type == ALLEGRO_EVENT_KEY_UP || event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
			is_running = false;
			std::cout << "Abortings..." << std::endl;
			MPI_Abort(MPI_COMM_WORLD, 0);
		}
		else if(event.type == ALLEGRO_EVENT_TIMER) {
			std::cout << "root looping..." << std::endl;

			frame_update();
			if(++generation == settings->last_generation) {
				is_running = false;
			}
		}
	}
	else {
		std::cout << "others looping..." << std::endl;
		frame_update();
		if(++generation == settings->last_generation) {
			is_running = false;
		}
	}
}

#else // NO_PARALLEL_MODE

void graphic_serial_loop() {
	ALLEGRO_EVENT event;
	al_wait_for_event(queue, &event);
	if(event.type == ALLEGRO_EVENT_KEY_UP || event.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
		is_running = false;
	else if(event.type == ALLEGRO_EVENT_TIMER) {
		frame_update();
		if(++generation == settings->last_generation) {
			is_running = false;
		}
	}
}

#endif // PARALLEL_MODE

#else // NO_GRAPHIC_MODE

void no_graphic_loop() {
	frame_update();
	if(++generation == settings->last_generation) {
		is_running = false;
	}
}

#endif // GRAPHIC_MODE


void frame_update() {
	std::cout << "Generation: " << generation << std::endl;
	#if defined(PARALLEL_MODE) && defined(DEBUG_MODE)
	MPI_Barrier(cave_comm);
	#endif // PARALLEL_MODE	


	#ifdef GRAPHIC_MODE
	if(my_rank == ROOT_RANK) {
		al_flip_display();
		al_clear_to_color(wall_color);

		#ifdef PARALLEL_MODE 
		//receive the grid from the other processes
		// MPI_Gather(read_grid, 1, inner_grid_t, root_grid, n_procs, inner_grid_t, ROOT_RANK, cave_comm);

		parallel_draw_grid();
		#else  
		serial_draw_grid();
		#endif // PARALLEL_MODE
	}
	else {
		// send read_grid to root
		#ifdef PARALLEL_MODE
		// MPI_Gather(read_grid, 1, inner_grid_t, root_grid, n_procs, inner_grid_t, ROOT_RANK, cave_comm);
		#endif // PARALLEL_MODE

	}
	#endif // GRAPHIC_MODE

	#ifdef PARALLEL_MODE
	// send columns to other processes
	send_columns();

	// send rows to other processes
	send_rows();

	// send corners to other processes
	send_corners();

	// receive columns from other processes
	receive_columns();

	// receive rows from other processes
	receive_rows();

	// receive corners from other processes
	receive_corners();


	#endif // PARALLEL_MODE
	update_grid();
	flip_grid();

	#ifdef PARALLEL_MODE
	MPI_Barrier(cave_comm);
	#endif // PARALLEL_MODE	
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
	// offset in cells, not pixels
	int edge_offset = edge_offset = settings->draw_edges ? radius : 0;

	for(int proc = 0; proc < n_procs; proc++) {
		int proc_x = (proc % settings->x_threads) * my_cols + edge_offset;
		int proc_y = (proc / settings->x_threads) * my_rows + edge_offset;
		for(int i = 0; i < my_inner_rows; i++) {
			int y = (i + proc_y) * settings->cell_height;
			for(int j = 0; j < my_inner_cols; j++) {
				int idx = (proc * inner_grid_size) + (i * tot_inner_cols) + j;
				if(root_grid[idx] == 0) {

					int x = (j + proc_x) * settings->cell_width;
					al_draw_filled_rectangle(x, y, x + settings->cell_width, y + settings->cell_height, floor_color);

				}
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

void parallel_initialize_random_grid() {
	if(settings->rand_seed)
		srand(settings->rand_seed);
	else srand(time(0));

	root_grid = new uint8_t[inner_grid_size * n_procs];

	// for(int i = 0; i < tot_inner_rows; i++) {
	// 	for(int j = 0; j < tot_inner_cols; j++) {
	// 		root_grid[i * tot_inner_cols + j] = (rand() % 100) < settings->initial_fill_perc;
	// 	}
	// }

	for(int proc = 0; proc < n_procs; proc++) {
		for(int i = 0; i < my_inner_rows; i++) {
			for(int j = 0; j < my_inner_cols; j++) {
				int idx = (proc * inner_grid_size) + (i * my_inner_cols) + j;
				root_grid[idx] = (rand() % 100) < settings->initial_fill_perc;
			}
		}
	}


}

void send_initial_grid() {
	// root sends initial grid to all other processes
	uint8_t* dest_buff = read_grid + (my_cols * radius) + radius;
	MPI_Scatter(root_grid, 1, contiguous_grid_t, dest_buff, 1, inner_grid_t, ROOT_RANK, cave_comm);

}

void receive_initial_grid() {
	// destination: read_grid with (radius) padding
	uint8_t* dest_buff = read_grid + (my_cols * radius) + radius;
	MPI_Scatter(root_grid, 1, contiguous_grid_t, dest_buff, 1, inner_grid_t, ROOT_RANK, cave_comm);

}



void send_columns() {
}

void send_rows() {

}

void send_corners() {

}



void receive_columns() {

}

void receive_rows() {

}

void receive_corners() {

}



#endif // PARALLEL_MODE

