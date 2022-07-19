#!/usr/bin/python3
import os

map_sizes = [(900, 900), (1500, 1500), (3000, 3000)]
axis_threads = [(1, 1),
                (1, 2), (2, 1),
                (1, 3), (3, 1),
                (1, 4), (4, 1), (2, 2),
                (1, 5), (5, 1),
                (1, 6), (6, 1), (2, 3), (3, 2),
                (1, 7), (7, 1),
                (1, 8), (8, 1), (2, 4), (4, 2)
                ]

tests_per_category = 20

for (cols, rows) in map_sizes:
    for (x_threads, y_threads) in axis_threads:
        if(cols % x_threads != 0 or rows % y_threads != 0):
            continue

        tot_threads = x_threads * y_threads

        command = "mpirun -np {} ./bin/cavegen -cols {} -rows {} -G -p -x {} -y {} -o ./benchmarks/nogfx_{}.csv"
        if tot_threads == 1:
            # serial
            command = "mpirun -np {} ./bin/cavegen -cols {} -rows {} -G -s -x {} -y {} -o ./benchmarks/nogfx_{}.csv"

        command = command.format(
            tot_threads, cols, rows, x_threads, y_threads, cols)

        print("_" * 80)
        for i in range(tests_per_category):
            print(
                f"cols: {cols}, rows: {rows};\t x: {x_threads}, y: {y_threads};\t [{i+1}/{tests_per_category}]")
            if(os.system(command) != 0):
                print("Error running command: " + command)
                exit(1)
