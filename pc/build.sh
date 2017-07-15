#!/bin/bash

g++ draw_main.cc -o draw -O3 -lSDL2 -lSDL2_gfx -lpthread -lboost_system
g++ snake_main.cc -o snake -O3 -lSDL2 -lSDL2_gfx -lpthread -lboost_system
