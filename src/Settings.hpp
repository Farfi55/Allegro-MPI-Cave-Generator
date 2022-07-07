#pragma once

#include <allegro5/allegro5.h>
#include <fstream> //ifstream
#include <iostream> //cout
#include <assert.h>

#include "json.hpp"
using json = nlohmann::json;


struct rgb { int r; int g; int b; };

struct Settings
{
    // seed used for random number generation
    // 0 means random seed
    int rand_seed = 0;

    // program stops after this many generations
    // 0 means no limit
    int last_generation = 0;

    // % of cells to be walls (0-100) (default: 51)
    int initial_fill_perc = 51;

    uint8_t neighbour_radius = 1;
    int roughness = 1;

    int cols = 500;
    int rows = 300;

    // PARALLEL ONLY
    // number of threads per column
    int threads_per_col = 2;

    // PARALLEL ONLY
    // number of threads per row
    int threads_per_row = 2;

    // GRAPHIC ONLY
    int cell_width = 3;

    // GRAPHIC ONLY
    int cell_height = 3;

    // GRAPHIC ONLY
    bool draw_edges = true;

    // GRAPHIC ONLY
    int max_frame_rate = 24;

    // GRAPHIC ONLY
    rgb wall_color{ 0, 0, 0 };

    // GRAPHIC ONLY
    rgb floor_color{ 255, 255, 255 };

    std::string results_file_path = "";


    Settings() : Settings("./settings/default.json") {}

    Settings(std::string settingsFile) {

        std::ifstream settings(settingsFile);
        if(!settings.is_open()) {
            std::cout << "Failed to open settings file: " << settingsFile << std::endl;
            std::cout << "Continue with default settings? (y/n)" << std::endl;
            char c;
            std::cin >> c;
            if(c == 'y') {
                return;
            }
            else {
                std::cout << "Exiting..." << std::endl;
                exit(1);
            }
        }

        json jsonSettings;
        settings >> jsonSettings;

        if(jsonSettings.contains("rand_seed")) rand_seed = jsonSettings["rand_seed"];
        if(jsonSettings.contains("last_generation")) last_generation = jsonSettings["last_generation"];

        if(jsonSettings.contains("cols")) cols = jsonSettings["cols"];
        if(jsonSettings.contains("rows")) rows = jsonSettings["rows"];

        if(jsonSettings.contains("threads_per_col")) threads_per_col = jsonSettings["threads_per_col"];
        if(jsonSettings.contains("threads_per_row")) threads_per_row = jsonSettings["threads_per_row"];

        if(jsonSettings.contains("cell_size")) cell_width = cell_height = jsonSettings["cell_size"];
        if(jsonSettings.contains("cell_width")) cell_width = jsonSettings["cell_width"];
        if(jsonSettings.contains("cell_height")) cell_height = jsonSettings["cell_height"];

        if(jsonSettings.contains("draw_edges")) draw_edges = jsonSettings["draw_edges"];
        if(jsonSettings.contains("max_frame_rate")) max_frame_rate = jsonSettings["max_frame_rate"];

        if(jsonSettings.contains("initial_fill_perc")) initial_fill_perc = jsonSettings["initial_fill_perc"];

        if(jsonSettings.contains("neighbour_radius")) neighbour_radius = jsonSettings["neighbour_radius"];
        if(jsonSettings.contains("roughness")) roughness = jsonSettings["roughness"];

        if(jsonSettings.contains("wall_color")) {
            wall_color.r = jsonSettings["wall_color"][0];
            wall_color.g = jsonSettings["wall_color"][1];
            wall_color.b = jsonSettings["wall_color"][2];
        }

        if(jsonSettings.contains("floor_color")) {
            floor_color.r = jsonSettings["floor_color"][0];
            floor_color.g = jsonSettings["floor_color"][1];
            floor_color.b = jsonSettings["floor_color"][2];
        }
        if(jsonSettings.contains("results_file_path")) results_file_path = jsonSettings["results_file_path"];

    }
};
