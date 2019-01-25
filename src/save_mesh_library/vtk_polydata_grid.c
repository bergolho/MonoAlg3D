//
// Created by bergolho on 25/01/19.
//

#include "vtk_polydata_grid.h"
#include "../alg/cell/cell.h"
#include "../string/sds.h"
#include "data_utils.h"
#include <inttypes.h>
#include <math.h>
#include <stdint.h>

struct vtk_polydata_grid *new_vtk_polydata_grid ()
{
    struct vtk_polydata_grid *grid = (struct vtk_polydata_grid *)malloc(sizeof(struct vtk_polydata_grid));

    grid->values = NULL;
    grid->points = NULL;
    grid->lines = NULL;

    grid->num_points = 0;
    grid->num_lines = 0;

    return grid;
}

void new_vtk_polydata_grid_from_purkinje_grid(struct vtk_polydata_grid **vtk_grid, struct grid *grid, bool clip_with_plain,
                                                                     float *plain_coordinates, bool clip_with_bounds,
                                                                     float *bounds, bool read_only_values)
{
    static bool mesh_already_loaded =  false;

    if(grid == NULL) 
    {
        return;
    }

    if(!read_only_values) 
    {
        *vtk_grid = new_vtk_polydata_grid();
    }
    else 
    {
        if(!(*vtk_grid) && mesh_already_loaded) 
        {
            fprintf(stderr,
                    "Function new_vtk_polydata_grid_from_purkinje_grid can only be called with read_only_values if the grid is already loaded");
            exit(EXIT_FAILURE);
        }

        if(mesh_already_loaded) 
        {
            assert(*vtk_grid);
            sb_free((*vtk_grid)->values);
            (*vtk_grid)->values = NULL;
        }
        else 
        {
            *vtk_grid = new_vtk_polydata_grid();
        }
    }

    struct cell_node *grid_cell = grid->first_cell;
    struct node *u = grid->the_purkinje_network->list_nodes;

    struct point_3d aux;
    struct line auxl;
    float center_x, center_y, center_z;

    uint32_t id = 0;
    uint32_t num_cells = 0;

    struct point_hash *hash = point_hash_create();

    while (grid_cell != NULL)
    {
        if (grid_cell->active)
        {
            center_x = grid_cell->center_x;
            center_y = grid_cell->center_y;
            center_z = grid_cell->center_z;

            sb_push((*vtk_grid)->values, grid_cell->v);

            // This 'if' statement do not let us re-insert points and lines to the arrays ... =)
            if(mesh_already_loaded && read_only_values) 
            {
                grid_cell = grid_cell->next;
                u = u->next;
                continue;
            }

            // Insert the point to the array of points
            aux.x = center_x;
            aux.y = center_y;
            aux.z = center_z;

            // Search for duplicates
            if(point_hash_search(hash, aux) == -1) 
            {
                sb_push((*vtk_grid)->points, aux);
                point_hash_insert(hash, aux, id);
                id++;
            }

            // Insert the edge to the array of lines
            struct edge *v = u->list_edges;
            while (v != NULL)
            {
                auxl.source = u->id;
                auxl.destination = v->id;
                
                sb_push((*vtk_grid)->lines, auxl);

                v = v->next;
            }

        }
        grid_cell = grid_cell->next;
        u = u->next;
    }

    if(!mesh_already_loaded) 
    {
        (*vtk_grid)->num_points = id;
        (*vtk_grid)->num_lines = grid->the_purkinje_network->total_edges;

        if(read_only_values)
            mesh_already_loaded = true;
    }

}

// TODO: Implement this function
void save_vtk_polydata_grid_as_legacy_vtk(struct vtk_polydata_grid *vtk_grid, char *filename, bool binary)
{

}