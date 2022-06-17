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
    int rand_seed = 0xCAFFE;

    int last_generation = 20;

    int initial_fill_perc = 51;

    uint8_t neighbour_radius = 3;
    int roughness = 4;

    int COLS = 500;
    int ROWS = 300;

    int CELL_WIDTH = 3;
    int CELL_HEIGHT = 3;

    rgb wall_color;
    rgb floor_color;


    Settings() : Settings("./settings/default.json") {}

    Settings(std::string settingsFile) {

        std::ifstream settings(settingsFile);
        json jsonSettings;
        settings >> jsonSettings;

        rand_seed = jsonSettings["rand_seed"];

        last_generation = jsonSettings["last_generation"];

        COLS = jsonSettings["cols"];
        ROWS = jsonSettings["rows"];
        CELL_WIDTH = jsonSettings["cell_width"];
        CELL_HEIGHT = jsonSettings["cell_height"];


        initial_fill_perc = jsonSettings["initial_fill_perc"];

        neighbour_radius = jsonSettings["neighbour_radius"];
        roughness = jsonSettings["roughness"];


        wall_color.r = jsonSettings["wall_color"][0];
        wall_color.g = jsonSettings["wall_color"][1];
        wall_color.b = jsonSettings["wall_color"][2];

        floor_color.r = jsonSettings["floor_color"][0];
        floor_color.g = jsonSettings["floor_color"][1];
        floor_color.b = jsonSettings["floor_color"][2];
    }

};
