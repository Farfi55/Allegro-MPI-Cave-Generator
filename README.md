# Allegro-MPI-Cave-Generator

Cellular automata project that procedurally generates a cave using simple rules and parallelization.

## Purpose of the project

This is a project for the *Parallel Algorithmsm* course at my university *(Unical)*.

It utilizes **mpi** to parallelize a **cellular automata algorithm** on a 2d cartesian grid.

## How to build

### dependencies

* [allegro5](https://github.com/liballeg/allegro5)
* [mpich](https://github.com/pmodels/mpich)

### building

```sh
git clone https://github.com/Farfi55/Allegro-MPI-Cave-Generator.git
cd ./Allegro-MPI-Cave-Generator
make
```

inside `src/main.cpp` there are some `#define` that deeply change how the software will be built

* `GRAPHIC_MODE`: utilizes [allegro5](https://github.com/liballeg/allegro5) to render.
* `PARALLEL_MODE`: utilizes [mpich](https://github.com/pmodels/mpich) to distribute the workload among a grid of threads
* `DEBUG_MODE`: displayes extra text i used to debug during development

## Running

if built with `PARALLEL_MODE`:

```sh
mpirun -np <n_procs> ./out/main [-c config_file] [-x x_threads] [-y y_threads]
```

if built without without `PARALLEL_MODE`:

just run it with `./out/main`

### example

`mpirun -np 6 ./out/main -c ./config/my_config.cfg -x 3 -y 2`

this will:

* run `./out/main` on 6 threads
* load the config file `./config/my_config.cfg`
* set 3 threads on the x axis
* set 2 threads on the y axis

## Configuration

when running you can set a custom configuration with the option `-c config_file`:

configuration options:

* "rand_seed": 1337
  * if `0` generate a pseudo-random grid each time
  * else use the number as a seed for the random-number generator
* "last_generation": **n**
  * if is set `0` the program will continue indefinately until stopped using **ctrl + c** in the terminal
  * program stops after **n** generations
* "cols": 480
* "rows": 360
  * number of columns and rows in the grid
* "x_threads": 1
  * number of threads on the x axis
* "y_threads": 1
  * number of threads on the y axis
* "initial_fill_perc": 51
  * chance (%) of a cell starting filled
* "cell_size": 3
  * size in pixel for each cell
* "draw_edges": true
  * include edges in the rendering
* "wall_color": [r, g, b]
* "floor_color": [r, g, b]
  * colors used for walls and floors, each value goes from 0-255
* "results_file_path": "./benchmarks/results_default_config.txt"
  * if defined, the time taken by the program will be appended to the file indicated
* "neighbour_radius": 1,
  * how many cells to consider in each direction when calculating the walls around a cell
* "roughness": 1,
  * threshold for the cell to become as it's neighbour majority

