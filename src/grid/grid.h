//
// Created by sachetto on 29/09/17.
//

#ifndef MONOALG3D_GRID_H
#define MONOALG3D_GRID_H

#include "cell/cell.h"

#include <stdlib.h>
#include <stdio.h>

struct grid {

    struct cell_node *first_cell;     // First cell of grid.
    Real side_length;        // Length of cube grid. Default = 1.0.
    uint64_t number_of_cells;  // Number of cells of grid.

    uint64_t original_num_cells;
    uint64_t num_active_cells;

    //TODO: @Incomplete
    /*
    vector <int> freeSVPositions;

    vector<int> refinedThisStep;
*/
    struct cell_node* *active_cells;

    bool init_ode;

};



void initialize_grid(struct grid *the_grid, Real side_length);
void clean_and_free_grid(struct grid* the_grid);
void construct_grid(struct grid *the_grid);
void initialize_and_construct_grid(struct grid *the_grid, Real side_length);

void print_grid(struct grid* the_grid, FILE *output_file);
bool print_grid_and_check_for_activity (struct grid *the_grid, FILE *output_file, int count);

void clean_grid(struct grid *the_grid);
void order_grid_cells (struct grid *the_grid);

void set_grid_flux(struct grid *the_grid);

bool refine_grid_with_bound(struct grid* the_grid, Real min_h, Real refinement_bound);
void refine_grid(struct grid* the_grid, int num_steps);
void refine_grid_cell_at(struct grid* the_grid, uint64_t cell_number );

bool derefine_grid_with_bound(struct grid *the_grid, Real derefinement_bound, Real max_h);
void derefine_all_grid (struct grid* the_grid);
void derefine_grid_inactive_cells (struct grid* the_grid);

void initialize_grid_with_mouse_mesh (struct grid *the_grid, const char *mesh_file);
void initialize_grid_with_rabbit_mesh (struct grid *the_grid, const char *mesh_file);
void initialize_grid_with_benchmark_mesh (struct grid *the_grid, Real start_h);

void initialize_grid_with_plain_mesh (struct grid *the_grid, Real desired_side_lenght, Real start_h, int num_layers);
void initialize_grid_with_plain_fibrotic_mesh (struct grid *the_grid, Real side_length, Real start_h, Real num_layers, Real phi);
void initialize_grid_with_plain_and_sphere_fibrotic_mesh (struct grid *the_grid, Real side_length,
                                                          Real start_h, Real num_layers, Real phi,
                                                          Real plain_center, Real sphere_radius, Real bz_size,
                                                          Real bz_radius);
void set_plain_sphere_fibrosis(struct grid* the_grid, Real phi,  Real plain_center, Real sphere_radius, Real bz_size,
                               Real bz_radius);
void set_plain_fibrosis(struct grid* the_grid, Real phi);

#endif //MONOALG3D_GRID_H