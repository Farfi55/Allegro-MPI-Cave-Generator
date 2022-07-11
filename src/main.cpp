#include <chrono>
#include <iostream>
#include <time.h>
#include <string>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>
#include <algorithm>
#include <mpi.h>
#include <random>
#include "Config.hpp"

#define GRAPHIC_MODE
// #define PARALLEL_MODE
// #define DEBUG_MODE

#define ROOT_RANK 0

Config* cfg;
std::string config_file_path = "./config/default.cfg";

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
int outer_grid_size;

int generation = 0;
bool is_running = true;

double total_time = 0;
double communication_time = 0;
double generation_time = 0;
double draw_time = 0;
double start_time, end_time;
double* frame_times;

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

ALLEGRO_COLOR threads_grid_color;
#endif // GRAPHIC_MODE


inline int at(int y, int x) {
	return y * my_cols + x;
}

inline bool isValidCoord(int y, int x) {
	return y >= 0 && y < cfg->y_threads&& x >= 0 && x < cfg->x_threads;
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

void initialize(std::string configPath, int x_threads, int y_threads);
void serial_initialize_random_grid();
void terminate();
void frame_update();
void update_grid();
void flip_grid();
void check_generic_settings();
void no_graphic_loop();
void write_header(std::ofstream& file);
void write_result(std::ofstream& file);




// graphic only
void graphic_initialize();
void serial_draw_grid();
void check_graphic_settings();
void graphic_serial_loop();
void graphic_parallel_loop();

// parallel only
void parallel_draw_grid();
void parallel_initialize_random_grid();
void parallel_initialize();
void check_parallel_settings();

void scatter_initial_grid();
void gather_grid();

void send_columns();
void send_rows();
void send_corners();

void receive_columns();
void receive_rows();
void receive_corners();

/*
 * ==================================================================================
 *  --------------------------------------------------------------------------------
 *  									MAIN
 *  --------------------------------------------------------------------------------
 * ==================================================================================
 */

int main(int argc, char const* argv[])
{

	int x_threads = 0, y_threads = 0;
	for(int i = 0; i < argc; i++) {
		if(argv[i] == std::string("-c") && i + 1 < argc) {
			config_file_path = argv[++i];
		}
		else if(argv[i] == std::string("-x") && i + 1 < argc) {
			x_threads = std::stoi(argv[++i]);
		}
		else if(argv[i] == std::string("-y") && i + 1 < argc) {
			y_threads = std::stoi(argv[++i]);
		}
		// else if(argv[i] == std::string("-g")) {
		// 	cfg->graphic_mode = true;
		// }
		// else if(argv[i] == std::string("-p")) {
		// 	cfg->parallel_mode = true;
		// }
	}


	initialize(config_file_path, x_threads, y_threads);

	start_time = MPI_Wtime();

	while(is_running) {
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

	end_time = MPI_Wtime();
	total_time = end_time - start_time;
	// auto end_time = std::chrono::high_resolution_clock::now();
	// auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

	if(my_rank == ROOT_RANK) {
		std::cout << std::endl;
		std::cout << "Total time:         " << total_time << " s" << std::endl;
		std::cout << "Communication time: " << communication_time << " s" << std::endl;
		std::cout << "Generation time:    " << generation_time << " s" << std::endl;
		std::cout << "Draw time:          " << draw_time << " s" << std::endl;

		if(cfg->results_file_path.empty()) {
			std::cout << "No results file path specified" << std::endl;
		}
		else {
			std::ofstream file;
			if(std::filesystem::exists(cfg->results_file_path)) {
				file.open(cfg->results_file_path, std::ios::app);
			}
			else {
				std::cout << "Creating results file" << std::endl;
				file.open(cfg->results_file_path);
				write_header(file);
			}
			write_result(file);
			file.close();

			std::cout << "Results written to " << cfg->results_file_path << std::endl;
		}
	}



	terminate();

	return 0;
}

/*
 * ==================================================================================
 *  --------------------------------------------------------------------------------
 *  								INITIALIZE
 *  --------------------------------------------------------------------------------
 * ==================================================================================
 */

void initialize(std::string configPath, int x_threads, int y_threads) {
	if(!configPath.empty())
		cfg = new Config(configPath);
	else
		cfg = new Config();

	tot_inner_rows = cfg->rows;
	tot_inner_cols = cfg->cols;
	radius = cfg->neighbour_radius;
	max_neighbours = (radius * (radius + 1) * 4);
	half_neighbours = max_neighbours / 2;

	if(x_threads) cfg->x_threads = x_threads;
	if(y_threads) cfg->y_threads = y_threads;

	frame_times = new double[cfg->last_generation];

	// checkGeneralSettings();

	#ifdef PARALLEL_MODE
	my_inner_rows = cfg->rows / cfg->y_threads;
	my_inner_cols = cfg->cols / cfg->x_threads;
	#else 
	my_inner_rows = cfg->rows;
	my_inner_cols = cfg->cols;
	#endif // PARALLEL_MODE

	my_rows = my_inner_rows + 2 * radius;
	my_cols = my_inner_cols + 2 * radius;
	inner_grid_size = my_inner_rows * my_inner_cols;
	outer_grid_size = my_rows * my_cols;

	MPI_Init(NULL, NULL);
	#ifdef PARALLEL_MODE
	parallel_initialize();
	#endif // PARALLEL_MODE

	#ifdef GRAPHIC_MODE
	graphic_initialize();
	#endif // GRAPHIC_MODE

	// create grid and set to 1 every element
	write_grid = new uint8_t[outer_grid_size];
	read_grid = new uint8_t[outer_grid_size];
	std::fill_n(read_grid, outer_grid_size, 1);


	#ifdef PARALLEL_MODE
	if(my_rank == ROOT_RANK) {
		parallel_initialize_random_grid();
	}
	scatter_initial_grid();
	#else
	serial_initialize_random_grid();
	#endif // PARALLEL_MODE
	std::copy_n(read_grid, outer_grid_size, write_grid);

}

#ifdef GRAPHIC_MODE
void graphic_initialize() {
	if(my_rank == ROOT_RANK) {
		check_graphic_settings();
		DISPLAY_WIDTH = cfg->cols * cfg->cell_width;
		DISPLAY_HEIGHT = cfg->rows * cfg->cell_height;
		if(cfg->draw_edges) {
			DISPLAY_WIDTH += 2 * radius * cfg->cell_width;
			DISPLAY_HEIGHT += 2 * radius * cfg->cell_height;
		}

		wall_color = al_map_rgb(cfg->wall_color.r, cfg->wall_color.g, cfg->wall_color.b);
		floor_color = al_map_rgb(cfg->floor_color.r, cfg->floor_color.g, cfg->floor_color.b);
		threads_grid_color = al_map_rgb(cfg->threads_grid_color.r, cfg->threads_grid_color.g, cfg->threads_grid_color.b);
		if(!al_init()) fprintf(stderr, "Failed to initialize allegro.\n");
		if(!al_install_keyboard()) fprintf(stderr, "Failed to install keyboard.\n");
		if(!al_init_font_addon()) fprintf(stderr, "Failed to initialize font addon.\n");
		if(!al_init_ttf_addon()) fprintf(stderr, "Failed to initialize ttf addon.\n");
		if(!al_init_primitives_addon()) fprintf(stderr, "Failed to initialize primitives addon.\n");

		font = al_load_ttf_font("fonts/Roboto/Roboto-Medium.ttf", 20, 0);
		display = al_create_display(DISPLAY_WIDTH, DISPLAY_HEIGHT);
		queue = al_create_event_queue();
		timer = al_create_timer(1.0 / cfg->max_frame_rate);

		if(!font) fprintf(stderr, "Failed load font.\n");
		if(!display) fprintf(stderr, "Failed initialize display.\n");
		if(!queue) fprintf(stderr, "Failed create queue.\n");
		if(!timer) fprintf(stderr, "Failed to create timer.\n");

		al_register_event_source(queue, al_get_display_event_source(display));
		al_register_event_source(queue, al_get_keyboard_event_source());
		al_register_event_source(queue, al_get_timer_event_source(timer));
		al_start_timer(timer);
		al_start_timer(timer);

		al_set_app_name("cave generator");
	}
}
#endif // GRAPHIC_MODE



/*
 * ==================================================================================
 *  --------------------------------------------------------------------------------
 *  							  PARALLEL INITIALIZE
 *  --------------------------------------------------------------------------------
 * ==================================================================================
 */
#ifdef PARALLEL_MODE
void parallel_initialize() {


	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &n_procs);

	check_parallel_settings();

	int dims[2] = { cfg->y_threads, cfg->x_threads };
	int periods[2] = { 0, 0 };


	MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &cave_comm);

	int my_coords[2];
	MPI_Cart_coords(cave_comm, my_rank, 2, my_coords);
	// std::cout << "rank: " << my_rank << " coords: " << my_coords[0] << " " << my_coords[1] << std::endl;

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
	const int starts[] = { 0, 0 };
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
	if(cfg->rand_seed)
		srand(cfg->rand_seed);
	else {
		int seed = time(NULL);
		srand(seed);
		std::cout << "Random seed: " << seed << std::endl;
	}


	int fill_perc = cfg->initial_fill_perc;
	for(int i = 0; i < my_rows; i++) {
		for(int j = 0; j < my_cols; j++) {
			if(i <= radius || i >= my_rows - radius || j <= radius || j >= my_cols - radius)
				write_grid[at(i, j)] = read_grid[at(i, j)] = 1;
			else write_grid[at(i, j)] = read_grid[at(i, j)] = ((rand() % 100) < fill_perc);
		}
	}
}

#endif // PARALLEL_MODE


void terminate()
{
	#ifdef GRAPHIC_MODE
	if(my_rank == ROOT_RANK) {
		al_uninstall_keyboard();
		al_destroy_event_queue(queue);
		al_destroy_display(display);
		al_destroy_font(font);
	}
	#endif // GRAPHIC_MODE

	delete cfg;
	delete[] read_grid;
	delete[] write_grid;
	delete[] frame_times;

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

	#endif // PARALLEL_MODE
	MPI_Finalize();
}


void check_general_settings() {



}



#ifdef PARALLEL_MODE

void check_parallel_settings() {
	if(cfg->x_threads < 1 || cfg->y_threads < 1) {
		std::cout << "x_threads and y_threads must be at least 1" << std::endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
		exit(1);
	}
	if(cfg->rows % cfg->y_threads != 0) {
		std::cout << "Rows have to be divisible by the y_threads" << std::endl;
		std::cout << "rows: " << cfg->rows << " y_threads: " << cfg->y_threads << std::endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
		exit(1);
	}
	if(cfg->cols % cfg->x_threads != 0) {
		std::cout << "Cols have to be divisible by the x_threads" << std::endl;
		std::cout << "cols: " << cfg->cols << " x_threads: " << cfg->x_threads << std::endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
		exit(1);
	}
	if(cfg->y_threads * cfg->x_threads != n_procs) {
			// make sure the product of dims is equal to the number of processes
		std::cout << "Error: number of processes does not match the number of threads" << std::endl;
		std::cout << "number of processes: " << n_procs << std::endl;
		std::cout << "number of threads per row: " << cfg->y_threads << std::endl;
		std::cout << "number of threads per column: " << cfg->x_threads << std::endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
		exit(1);
	}
}
#endif // PARALLEL_MODE


#ifdef GRAPHIC_MODE

void check_graphic_settings() {
	if(cfg->cols * cfg->rows > 1382400) {
		std::cout << "Grid is too large for graphic mode" << std::endl;

		#ifdef PARALLEL_MODE
		MPI_Abort(MPI_COMM_WORLD, 1);
		#endif // PARALLEL_MODE
		exit(1);

	}
}
#endif // GRAPHIC_MODE



/*
 * ==================================================================================
 *  --------------------------------------------------------------------------------
 *  									MAIN LOOP
 *  --------------------------------------------------------------------------------
 * ==================================================================================
 */
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
			frame_update();
			if(++generation == cfg->last_generation) {
				is_running = false;
			}
		}
	}
	else {
		frame_update();
		if(++generation == cfg->last_generation) {
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
		if(++generation == cfg->last_generation) {
			is_running = false;
		}
	}
}

#endif // PARALLEL_MODE

#else // NO_GRAPHIC_MODE

void no_graphic_loop() {
	frame_update();
	if(++generation == cfg->last_generation) {
		is_running = false;
	}
}

#endif // GRAPHIC_MODE


void frame_update() {
	double frame_start_time = MPI_Wtime();
	#ifdef GRAPHIC_MODE
	if(my_rank == ROOT_RANK) {
		double start_draw_time = MPI_Wtime();
		al_flip_display();
		al_clear_to_color(wall_color);

		#ifdef PARALLEL_MODE 
		//receive the grid from the other processes
		double receive_start_time = MPI_Wtime();
		gather_grid();
		communication_time += MPI_Wtime() - receive_start_time;

		parallel_draw_grid();
		#else  
		serial_draw_grid();
		#endif // PARALLEL_MODE

		double end_draw_time = MPI_Wtime();
		draw_time += end_draw_time - start_draw_time;
	}
	else {
		// send read_grid to root
		#ifdef PARALLEL_MODE
		double receive_start_time = MPI_Wtime();
		gather_grid();
		communication_time += MPI_Wtime() - receive_start_time;
		#endif // PARALLEL_MODE

	}
	#endif // GRAPHIC_MODE

	#ifdef PARALLEL_MODE
	// send columns to other processes
	double comms_start_time = MPI_Wtime();
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
	communication_time += MPI_Wtime() - comms_start_time;

	#endif // PARALLEL_MODE

	double generation_start_time = MPI_Wtime();
	update_grid();
	generation_time += MPI_Wtime() - generation_start_time;
	std::swap(read_grid, write_grid);

	// #ifdef PARALLEL_MODE
	// MPI_Barrier(cave_comm);
	// #endif // PARALLEL_MODE	

	double frame_end_time = MPI_Wtime();
	frame_times[generation] = frame_end_time - frame_start_time;
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

			if(walls >= half_neighbours + cfg->roughness)
				write_grid[at(i, j)] = 1;
			else if(walls <= half_neighbours - cfg->roughness)
				write_grid[at(i, j)] = 0;
			else
				write_grid[at(i, j)] = read_grid[at(i, j)];
		}
	}
}



/*
 * ==================================================================================
 *  --------------------------------------------------------------------------------
 *  									DRAWING
 *  --------------------------------------------------------------------------------
 * ==================================================================================
 */
#ifdef GRAPHIC_MODE
#ifdef PARALLEL_MODE
void parallel_draw_grid() {
	// offset in cells, not pixels
	int edge_offset = edge_offset = cfg->draw_edges ? radius : 0;

	for(int proc = 0; proc < n_procs; proc++) {

		int proc_x = (proc % cfg->x_threads) * my_inner_cols + edge_offset;
		int proc_y = (proc / cfg->x_threads) * my_inner_rows + edge_offset;

		#ifdef DEBUG_MODE
		int proc_x2 = ((proc % cfg->x_threads) + 1) * my_inner_cols + edge_offset;
		int proc_y2 = ((proc / cfg->x_threads) + 1) * my_inner_rows + edge_offset;
		#endif // DEBUG_MODE

		for(int i = 0; i < my_inner_rows; i++) {

			int y = (i + proc_y) * cfg->cell_height;

			for(int j = 0; j < my_inner_cols; j++) {

				int idx = (proc * inner_grid_size) + (i * my_inner_cols) + j;
				if(root_grid[idx] == 0) {

					int x = (j + proc_x) * cfg->cell_width;
					al_draw_filled_rectangle(x, y, x + cfg->cell_width, y + cfg->cell_height, floor_color);

				}
			}
		}
		#ifdef DEBUG_MODE
		if(cfg->draw_threads_grid) {
			al_draw_rectangle(proc_x * cfg->cell_width, proc_y * cfg->cell_height, proc_x2 * cfg->cell_width, proc_y2 * cfg->cell_height, threads_grid_color, 1);
		}
		#endif // DEBUG_MODE
	}
}
#endif // PARALLEL_MODE


void serial_draw_grid() {

	for(int i = radius; i < my_rows - radius; i++) {
		for(int j = radius; j < my_cols - radius; j++) {
			if(read_grid[at(i, j)] == 0) {

				int y = (i - (!cfg->draw_edges * radius)) * cfg->cell_height;
				int x = (j - (!cfg->draw_edges * radius)) * cfg->cell_width;

				al_draw_filled_rectangle(x, y, x + cfg->cell_width, y + cfg->cell_height, floor_color);

			}
		}
	}
}
#endif


#ifdef PARALLEL_MODE
/*
 * ==================================================================================
 *  --------------------------------------------------------------------------------
 *  							PARALLEL COMUNICATION
 *  --------------------------------------------------------------------------------
 * ==================================================================================
 */


void parallel_initialize_random_grid() {
	if(cfg->rand_seed)
		srand(cfg->rand_seed);
	else {
		int seed = time(NULL);
		srand(seed);
		std::cout << "Random seed: " << seed << std::endl;
	}


	root_grid = new uint8_t[inner_grid_size * n_procs];

	// for(int i = 0; i < tot_inner_rows; i++) {
	// 	for(int j = 0; j < tot_inner_cols; j++) {
	// 		root_grid[i * tot_inner_cols + j] = (rand() % 100) < settings->initial_fill_perc;
	// 	}
	// }

	for(int proc = 0; proc < n_procs; proc++) {
		#ifdef DEBUG_MODE
		// std::cout << "proc: " << proc << " starts at: " << (proc * inner_grid_size) << std::endl;
		#endif // DEBUG_MODE

		for(int i = 0; i < my_inner_rows; i++) {
			for(int j = 0; j < my_inner_cols; j++) {
				int idx = (proc * inner_grid_size) + (i * my_inner_cols) + j;
				root_grid[idx] = (rand() % 100) < cfg->initial_fill_perc;
			}
		}
	}


}

void scatter_initial_grid() {
	// root sends initial grid to all other processes
	uint8_t* dest_buff = &read_grid[(my_cols * radius) + radius];
	MPI_Scatter(root_grid, 1, contiguous_grid_t, dest_buff, 1, inner_grid_t, ROOT_RANK, cave_comm);
}

void gather_grid() {
	uint8_t* send_buff = &read_grid[(my_cols * radius) + radius];
	MPI_Gather(send_buff, 1, inner_grid_t, root_grid, 1, contiguous_grid_t, ROOT_RANK, cave_comm);
}


#define TOP 0
#define MIDDLE 1
#define BOTTOM 2
#define LEFT 0
#define RIGHT 2


void send_columns() {
	if(neighbours_ranks[MIDDLE][LEFT] != MPI_PROC_NULL) {
		MPI_Request req;
		int start_idx = my_cols * radius + radius;
		MPI_Isend(&read_grid[start_idx], 1, column_t, neighbours_ranks[MIDDLE][LEFT], 1001, cave_comm, &req);
		MPI_Request_free(&req);

	}

	if(neighbours_ranks[MIDDLE][RIGHT] != MPI_PROC_NULL) {
		MPI_Request req;
		int start_idx = my_cols * radius + my_inner_cols;
		MPI_Isend(&read_grid[start_idx], 1, column_t, neighbours_ranks[MIDDLE][RIGHT], 1002, cave_comm, &req);
		MPI_Request_free(&req);
	}
	// #ifdef DEBUG_MODE
	// std::cout << "[" << my_rank << "]: columns sent" << std::endl;
	// #endif // DEBUG_MODE
}

void send_rows() {
	if(neighbours_ranks[TOP][MIDDLE] != MPI_PROC_NULL) {
		MPI_Request req;
		int start_idx = my_cols * radius + radius;
		MPI_Isend(&read_grid[start_idx], 1, row_t, neighbours_ranks[TOP][MIDDLE], 1003, cave_comm, &req);
		MPI_Request_free(&req);
	}

	if(neighbours_ranks[BOTTOM][MIDDLE] != MPI_PROC_NULL) {
		MPI_Request req;
		int start_idx = my_cols * my_inner_rows + radius;
		MPI_Isend(&read_grid[start_idx], 1, row_t, neighbours_ranks[BOTTOM][MIDDLE], 1004, cave_comm, &req);
		MPI_Request_free(&req);
	}

}

void send_corners() {
	if(neighbours_ranks[TOP][LEFT] != MPI_PROC_NULL) {
		MPI_Request req;
		int start_idx = my_cols * radius + radius;
		MPI_Isend(&read_grid[start_idx], 1, corner_t, neighbours_ranks[TOP][LEFT], 1005, cave_comm, &req);
		MPI_Request_free(&req);
	}

	if(neighbours_ranks[TOP][RIGHT] != MPI_PROC_NULL) {
		MPI_Request req;
		int start_idx = my_cols * radius + my_inner_cols;
		MPI_Isend(&read_grid[start_idx], 1, corner_t, neighbours_ranks[TOP][RIGHT], 1006, cave_comm, &req);
		MPI_Request_free(&req);
	}

	if(neighbours_ranks[BOTTOM][LEFT] != MPI_PROC_NULL) {
		MPI_Request req;
		int start_idx = my_cols * my_inner_rows + radius;
		MPI_Isend(&read_grid[start_idx], 1, corner_t, neighbours_ranks[BOTTOM][LEFT], 1007, cave_comm, &req);
		MPI_Request_free(&req);
	}

	if(neighbours_ranks[BOTTOM][RIGHT] != MPI_PROC_NULL) {
		MPI_Request req;
		int start_idx = my_cols * my_inner_rows + my_inner_cols;
		MPI_Isend(&read_grid[start_idx], 1, corner_t, neighbours_ranks[BOTTOM][RIGHT], 1008, cave_comm, &req);
		MPI_Request_free(&req);
	}
}



void receive_columns() {

	// std::cout << "[" << my_rank << "]: receiving columns" << std::endl;
	if(neighbours_ranks[MIDDLE][RIGHT] != MPI_PROC_NULL) {
		int start_idx = my_cols * radius + my_inner_cols + radius;
		MPI_Recv(&read_grid[start_idx], 1, column_t, neighbours_ranks[MIDDLE][RIGHT], 1001, cave_comm, MPI_STATUS_IGNORE);
	}
	if(neighbours_ranks[MIDDLE][LEFT] != MPI_PROC_NULL) {
		int start_idx = my_cols * radius;
		MPI_Recv(&read_grid[start_idx], 1, column_t, neighbours_ranks[MIDDLE][LEFT], 1002, cave_comm, MPI_STATUS_IGNORE);

	}
	// std::cout << "[" << my_rank << "]: columns received" << std::endl;

}

void receive_rows() {

	if(neighbours_ranks[BOTTOM][MIDDLE] != MPI_PROC_NULL) {
		int start_idx = my_cols * (my_rows - radius) + radius;
		MPI_Recv(&read_grid[start_idx], 1, row_t, neighbours_ranks[BOTTOM][MIDDLE], 1003, cave_comm, MPI_STATUS_IGNORE);
	}

	if(neighbours_ranks[TOP][MIDDLE] != MPI_PROC_NULL) {
		int start_idx = radius;
		MPI_Recv(&read_grid[start_idx], 1, row_t, neighbours_ranks[TOP][MIDDLE], 1004, cave_comm, MPI_STATUS_IGNORE);
	}
}

void receive_corners() {
	if(neighbours_ranks[BOTTOM][RIGHT] != MPI_PROC_NULL) {
		int start_idx = my_cols * (my_inner_rows + radius) + my_inner_cols + radius;
		MPI_Recv(&read_grid[start_idx], 1, corner_t, neighbours_ranks[BOTTOM][RIGHT], 1005, cave_comm, MPI_STATUS_IGNORE);
	}
	if(neighbours_ranks[BOTTOM][LEFT] != MPI_PROC_NULL) {
		int start_idx = my_cols * (my_inner_rows + radius);
		MPI_Recv(&read_grid[start_idx], 1, corner_t, neighbours_ranks[BOTTOM][LEFT], 1006, cave_comm, MPI_STATUS_IGNORE);
	}
	if(neighbours_ranks[TOP][RIGHT] != MPI_PROC_NULL) {
		int start_idx = my_inner_cols + radius;
		MPI_Recv(&read_grid[start_idx], 1, corner_t, neighbours_ranks[TOP][RIGHT], 1007, cave_comm, MPI_STATUS_IGNORE);
	}
	if(neighbours_ranks[TOP][LEFT] != MPI_PROC_NULL) {
		int start_idx = 0;
		MPI_Recv(&read_grid[start_idx], 1, corner_t, neighbours_ranks[TOP][LEFT], 1008, cave_comm, MPI_STATUS_IGNORE);
	}
}



#endif // PARALLEL_MODE




void write_header(std::ofstream& file) {
	std::string separator = ",";
	file << "total_time" << separator
		<< "communication_time" << separator
		<< "generation_time" << separator
		<< "draw_time" << separator
		<< "start_time" << separator
		<< "end_time" << separator
		<< "n_procs" << separator
		<< "x_threads" << separator
		<< "y_threads" << separator
		<< "config_file_path" << separator
		<< "frame_timings" << std::endl;
}


void print_frame_times(std::ostream& os) {
	os << '[';
	for(int i = 0; i < cfg->last_generation; i++) {
		os << frame_times[i] << ' ';
	}
	os << ']';
}

void write_result(std::ofstream& file) {
	std::string separator = ",";
	file << total_time << separator
		<< communication_time << separator
		<< generation_time << separator
		<< draw_time << separator
		<< start_time << separator
		<< end_time << separator
		<< n_procs << separator
		<< cfg->x_threads << separator
		<< cfg->y_threads << separator
		<< config_file_path << separator;
	print_frame_times(file);

	file << std::endl;
}
