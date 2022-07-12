# Allegro-MPI-Cave-Generator

Cellular automata project that procedurally generates a cave using simple rules and parallelization.

## Purpose of the project

This is a project for the *Parallel Algorithmsm* course at my university *(Unical)*.

It utilizes **mpi** to parallelize a **cellular automata algorithm** on a 2d cartesian grid.

Inspired by this [video](https://youtu.be/v7yyZZjF1z4)

## How to build

### dependencies

* c++ 17
* makefile
* [allegro5](https://github.com/liballeg/allegro5) for rendering
* [mpich](https://github.com/pmodels/mpich) or [open-mpi](https://github.com/open-mpi/ompi) for parellization

### building

```sh
git clone https://github.com/Farfi55/Allegro-MPI-Cave-Generator.git
cd ./Allegro-MPI-Cave-Generator
make
```

## Running

```sh
mpirun -np <n_procs> ./bin/cavegen [options]
```

for more info use: `./bin/cavegen -h`

### example

`mpirun -np 6 bin/cavegen -c ./config/my_config.cfg -p -x 3 -y 2`

* `mpirun -np 6 bin/cavegen` run program on 6 threads
* `-c ./config/my_config.cfg` load the specified config file
* `-p`: run in parallel mode
* `-x 3`: set 3 threads on the x axis
* `-y 2`: set 2 threads on the y axis

## Configuration

when running you can set a custom configuration with the option `-c config_file`

for more info on the configuration options use `cavegen -hc`

<!-- 
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
 -->

## showcase

![cavegen showcase](videos/cavegen.gif)
