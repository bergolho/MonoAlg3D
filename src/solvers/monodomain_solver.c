//
// Created by sachetto on 03/10/17.
//

#include "monodomain_solver.h"
#include "../gpu/gpu_common.h"
#include "../utils/constants.h"
#include "../utils/stop_watch.h"
#include "linear_system_solver.h"
#include <inttypes.h>
#include <omp.h>
#include <sys/stat.h>

static inline Real ALPHA (Real beta, Real cm, Real dt, Real h) {
    return (((beta * cm) / dt) * UM2_TO_CM2) * powf (h, 3.0f);
}

// TODO: make proper initialization and new functions
struct monodomain_solver *new_monodomain_solver (int num_threads, Real beta, Real cm, Real dt, Real sigma_x,
                                                 Real sigma_y, Real sigma_z) {

    struct monodomain_solver *result = (struct monodomain_solver *)malloc (sizeof (struct monodomain_solver));

    result->beta = beta;
    result->cm = 1.0;
    result->dt = dt;

    result->sigma_x = sigma_x;
    result->sigma_y = sigma_y;
    result->sigma_z = sigma_z;
    result->num_threads = num_threads;

    return result;
}

void init_solver (struct monodomain_solver *the_solver) {

    the_solver->beta = 0.14;
    the_solver->cm = 1.0;

    the_solver->sigma_y = 0.0001334f;
    the_solver->sigma_x = 0.0000176;
    the_solver->sigma_z = the_solver->sigma_x;

    the_solver->refine_each = 1;
    the_solver->derefine_each = 1;

    the_solver->tolerance = 1e-16;
    the_solver->use_jacobi = true;

    //    the_solver->sigma_y = 0.0001334f / 2.5f;
    //    the_solver->sigma_x = the_solver->sigma_y;
    //    the_solver->sigma_z = the_solver->sigma_y;
}

void solve_monodomain (struct grid *the_grid, struct monodomain_solver *the_monodomain_solver,
                       struct ode_solver *the_ode_solver, struct output_utils *output_info) {

    int refine_each = the_monodomain_solver->refine_each;
    int derefine_each = the_monodomain_solver->derefine_each;

    Real cg_tol = the_monodomain_solver->tolerance;

    int np = the_monodomain_solver->num_threads;

    if (np == 0)
        np = 1;

#if defined(_OPENMP)
    omp_set_num_threads (np);
#endif

    Real initial_v;
    bool redo_matrix = false;
//    Real *sv = NULL;
//    size_t pitch = 0;

    bool activity = false;

    int max_its;

    // TODO: I dont think we need this flag anymore
    bool parallel = (np != 0);
    bool gpu = the_ode_solver->gpu;
    bool jacobi = the_monodomain_solver->use_jacobi;

    int count = 0;

    Real refinement_bound = the_monodomain_solver->refinement_bound;
    Real derefinement_bound = the_monodomain_solver->derefinement_bound;

    Real min_h = the_monodomain_solver->min_h;
    Real max_h = the_monodomain_solver->max_h;

    bool adaptive = the_monodomain_solver->adaptive;
    bool save_to_file = (output_info->output_dir_name != NULL);

    Real h;
    int initRef;

    Real dt_edp = the_monodomain_solver->dt;
    Real dt_edo = the_ode_solver->min_dt;
    Real finalT = the_monodomain_solver->final_time;

    // TODO: create a file for stimulus definition!!
    Real stim_dur = the_ode_solver->stim_duration;
    int edo_method = the_ode_solver->method;

    if (gpu) {
        init_cuda_device (the_ode_solver->gpu_id);
    }

    order_grid_cells (the_grid);
    // TODO: check. I thing we can do this always
    save_old_cell_positions (the_grid);
    update_cells_to_solve (the_grid, the_ode_solver);

    printf ("Setting ODE's initial conditions\n");
    set_ode_initial_conditions_for_all_volumes (the_ode_solver, the_grid->num_active_cells);
    initial_v = the_ode_solver->model_data.initial_v;

    if (the_monodomain_solver->max_iterations > 0) {
        max_its = the_monodomain_solver->max_iterations;
    } else {
        max_its = (int)the_grid->number_of_cells;
    }

    print_solver_info (the_monodomain_solver, the_ode_solver, the_grid, output_info);

    int ode_step = 1;
    if (edo_method == EULER_METHOD) {
        if (dt_edp >= dt_edo) {
            ode_step = (int)(dt_edp / dt_edo);
            printf ("Solving EDO %d times before solving PDE\n", ode_step);
        } else {
            printf ("WARNING: EDO time step is greater than PDE time step. Adjusting to EDO time "
                    "step: %lf\n",
                    dt_edo);
            dt_edp = dt_edo;
        }
    }

    fflush (stdout);

    long ode_total_time = 0, cg_total_time = 0, total_write_time = 0, total_mat_time = 0, total_ref_time = 0,
         total_deref_time = 0;
    uint64_t total_cg_it = 0;
    struct stop_watch solver_time, ode_time, cg_time, part_solver, part_mat, write_time, ref_time, deref_time;

    init_stop_watch (&solver_time);
    init_stop_watch (&ode_time);
    init_stop_watch (&cg_time);
    init_stop_watch (&part_solver);
    init_stop_watch (&part_mat);
    init_stop_watch (&write_time);
    init_stop_watch (&ref_time);
    init_stop_watch (&deref_time);

    printf ("Assembling Monodomain Matrix Begin\n");
    start_stop_watch (&part_mat);
    set_initial_conditions (the_monodomain_solver, the_grid, the_ode_solver->model_data.initial_v);
    set_discretization_matrix (the_monodomain_solver, the_grid);
    total_mat_time = stop_stop_watch (&part_mat);
    printf ("Assembling Monodomain Matrix End\n");

    // TODO: @DEBUG: remove
    // print_grid_matrix(the_grid, stderr);
    // exit(0);

    start_stop_watch (&solver_time);

    Real cur_time = 0.0;
    int print_rate = output_info->print_rate;
    bool abort_on_no_activity = the_monodomain_solver->abort_on_no_activity;
    Real cg_error;
    uint64_t cg_iterations;

    // TODO: we need to handle stim on diferent edp times (pulses)
    set_spatial_stim (the_grid, the_ode_solver);
    set_ode_extra_data (the_grid, the_ode_solver);

    while (cur_time < finalT) {

        if (save_to_file) {

            if (count % print_rate == 0) {
                start_stop_watch (&write_time);

                sds tmp;
                sds c = sdsfromlonglong (count);
                tmp = sdscat (sdsdup (output_info->output_dir_name), "/V_t_");
                tmp = sdscat (tmp, c);
                FILE *f1 = fopen (tmp, "w");
                activity = print_grid_and_check_for_activity (the_grid, f1, count);
                fclose (f1);
                sdsfree (tmp);
                sdsfree (c);

                total_write_time += stop_stop_watch (&write_time);

                if (abort_on_no_activity) {
                    if (!activity) {
                        printf ("No activity, aborting simulation\n");
                        break;
                    }
                }
            }
        }

        count++;

        // TODO: @Incomplete: handle with the ODE and GPU stuff
        if (cur_time > 0.0) {
            update_ode_state_vector (the_ode_solver, the_grid, adaptive);
        }

        start_stop_watch (&ode_time);

        // TODO: remover (espiral)
        // if(t > 2.0) {
        //    stim_start = second_stim;
        //}

        solve_odes (the_ode_solver, the_grid, cur_time, ode_step, adaptive);

        ode_total_time += stop_stop_watch (&ode_time);

        start_stop_watch (&cg_time);

        cg_iterations = conjugate_gradient (the_grid, max_its, cg_tol, jacobi, &cg_error);

        cg_total_time += stop_stop_watch (&cg_time);
        total_cg_it += cg_iterations;

        if (count % print_rate == 0) {
            printf ("t = %lf, Iterations ="
                    "%" PRIu64 ", Error Norm = %e, Number of Cells:"
                    "%" PRIu64 "\n",
                    cur_time, cg_iterations, cg_error, the_grid->num_active_cells);
        }

        if (adaptive) {
            redo_matrix = false;
            if (cur_time > (stim_dur / 2.0)) {

                if (count % refine_each == 0) {
                    start_stop_watch (&ref_time);
                    redo_matrix = refine_grid_with_bound (the_grid, min_h, refinement_bound);
                    total_ref_time += stop_stop_watch (&ref_time);
                }

                if (count % derefine_each == 0) {
                    start_stop_watch (&deref_time);
                    redo_matrix |= derefine_grid_with_bound (the_grid, derefinement_bound, max_h);
                    total_deref_time += stop_stop_watch (&deref_time);
                }
            }

            if (redo_matrix) {
                order_grid_cells (the_grid);

                // TODO: we need to handle stim on diferent edp times (pulses)
                set_spatial_stim (the_grid, the_ode_solver);
                set_ode_extra_data (the_grid, the_ode_solver);

                if (gpu) {
                    update_cells_to_solve (the_grid, the_ode_solver);

                    // TODO: We need the decide how to handle this function
                    // if (refinedThisStep.size () > 0) {
                    //    updateAfterRefinement (sv, refinedThisStep.size (), &refinedThisStep[0],
                    // pitch, edo_method);
                    //}
                }
                start_stop_watch (&part_mat);

                set_discretization_matrix (the_monodomain_solver, the_grid);

                total_mat_time += stop_stop_watch (&part_mat);
            }
        }

        cur_time += dt_edp;
    }

    printf ("Resolution Time: %ld us\n", stop_stop_watch (&solver_time));
    printf ("ODE Total Time: %ld us\n", ode_total_time);
    printf ("CG Total Time: %ld us\n", cg_total_time);
    printf ("Mat time: %ld us \n", total_mat_time);
    printf ("Refine time: %ld us\n", total_ref_time);
    printf ("Derefine time: %ld us\n", total_deref_time);
    printf ("Write time: %ld us\n", total_write_time);
    printf ("CG Total Iterations: %ld\n", total_cg_it);
}

// TODO: this should be handled by a user provided library
void set_spatial_stim (struct grid *the_grid, struct ode_solver *the_ode_solver) {

    uint64_t n_active = the_grid->num_active_cells;
    struct cell_node **ac = the_grid->active_cells;
    Real side_length = the_grid->side_length;
    Real i_stim = the_ode_solver->stim_current;

    if (the_ode_solver->stim_currents != NULL) {
        free (the_ode_solver->stim_currents);
    }

    the_ode_solver->stim_currents = (Real *)malloc (sizeof (Real) * n_active);
    Real *stims = the_ode_solver->stim_currents;

#pragma omp parallel for
    for (int i = 0; i < n_active; i++) {

        bool stim;

        stim = (ac[i]->center_x > 5500.0) && ((ac[i]->center_x < 7000.0));
        stim &= (ac[i]->center_y < 1500.0);
        stim &= (ac[i]->center_z < 1500.0);

        if (stim) {
            stims[i] = i_stim;
        } else {
            stims[i] = 0.0;
        }
    }
}

// TODO: this should be handled by a user provided library
void set_ode_extra_data (struct grid *the_grid, struct ode_solver *the_ode_solver) {

    //    uint64_t n_active = the_grid->num_active_cells;
    //    struct cell_node** ac = the_grid->active_cells;
    //    Real side_length = the_grid->side_length;
    //
    //    if(the_ode_solver->stim_currents != NULL) {
    //        free(the_ode_solver->stim_currents);
    //    }
    //
    //    the_ode_solver->edo_extra_data = malloc(sizeof(Real)*n_active);
    //    Real *fibs = (Real*)the_ode_solver->edo_extra_data;
    //
    //
    //    #pragma omp parallel for
    //    for (int i = 0; i < n_active; i++) {
    //
    //        if(ac[i]->fibrotic) {
    //            fibs[i] = 0;
    //        }
    //        else if(ac[i]->borderZone) {
    //
    //            Real center_x = ac[i]->center_x;
    //            Real center_y = ac[i]->center_y;
    //            Real center_z = ac[i]->center_z;
    //
    //            if(globalArgs.use_plain_with_sphere) {
    //                Real distanceFromCenter = sqrt((center_x - plainCenter)*(center_x - plainCenter) + (center_y -
    //                plainCenter)*(center_y - plainCenter)); distanceFromCenter = (distanceFromCenter -
    //                fibRadius)/bz_size; fibs[i] = distanceFromCenter;
    //            }
    //
    //            else if(globalArgs.use_human) {
    //                Real distanceFromCenter;
    //
    //                if(ac[i]->scarType == 's') {
    //                    scarcenter_x = 52469;
    //                    scarcenter_y = 83225;
    //                    scarcenter_z = 24791;
    //                    distanceFromCenter = sqrt((center_x - scarcenter_x)*(center_x - scarcenter_x) + (center_y -
    //                    scarcenter_y)*(center_y - scarcenter_y)  + (center_z - scarcenter_z)*(center_z -
    //                    scarcenter_z)); distanceFromCenter = distanceFromCenter/bz_size_small;
    //                }
    //                else if(ac[i]->scarType == 'b') {
    //                    scarcenter_x = 95300;
    //                    scarcenter_y = 81600;
    //                    scarcenter_z = 36800;
    //                    distanceFromCenter = sqrt((center_x - scarcenter_x)*(center_x - scarcenter_x) + (center_y -
    //                    scarcenter_y)*(center_y - scarcenter_y)  + (center_z - scarcenter_z)*(center_z -
    //                    scarcenter_z)); distanceFromCenter = distanceFromCenter/bz_size_big;
    //                }
    //                else {
    //                    distanceFromCenter = 1;
    //                }
    //
    //                fibs[i] = distanceFromCenter;
    //            }
    //
    //        }
    //        else {
    //            fibs[i] = 1;
    //        }
    //
    //    }
}

void solve_odes (struct ode_solver *the_ode_solver, struct grid *the_grid, Real cur_time, int num_steps,
                 bool adaptive) {

    uint8_t edo_method = the_ode_solver->method;
    bool gpu = the_ode_solver->gpu;
    uint64_t n_active = the_grid->num_active_cells;

    // TODO: @Incomplete: handle with the ODE and GPU stuff
    if (edo_method == EULER_METHOD) {

        if (gpu) {
            if (adaptive) {
                // solveODEsDxAdptGPU (sv, cur_time, beta, cm, dt_edo, dt_edp, i_stim,
                //                   stim_start, stim_dur, pitch, ode_step);
                // updateMonodomainDxAdpt (originalNumCells, numActiveCells,
                //                        activeCells.data (), beta, cm, dt_edp, sv);
            } else {
                // solveODEsGPU (sv, cur_time, beta, cm, dt_edo, dt_edp, i_stim, stim_start,
                //             stim_dur, pitch, ode_step);
                // updateMonodomain (numActiveCells, activeCells.data (), beta, cm, dt_edp,
                //                  sv);
            }
        } else {
            solve_odes_cpu (the_ode_solver, n_active, cur_time, num_steps);
            // parallelSolveODEs (cur_time, beta, cm, dt_edo, dt_edp, i_stim, ode_step);
        }

        // TODO: @Incomplete: handle with the ODE and GPU stuff
    } else if (edo_method == EULER_METHOD_ADPT) {

        if (gpu) {
            if (adaptive) {
                // solveODEsDtDxAdptGPU (sv, cur_time, beta, cm, dt_edp, i_stim, stim_start,
                //                      stim_dur, globalArgs.min_dt_edo,
                //                      globalArgs.max_dt_edo, pitch);

                // updateMonodomainDxAdpt (originalNumCells, numActiveCells,
                //                       activeCells.data (), beta, cm, dt_edp, sv);

            } else {
                // solveODEsDtAdptGPU (sv, cur_time, beta, cm, dt_edp, i_stim, stim_start,
                //                    stim_dur, globalArgs.min_dt_edo,
                //                    globalArgs.max_dt_edo, pitch);
                // updateMonodomain (numActiveCells, activeCells.data (), beta, cm, dt_edp,
                //                  sv);
            }

        } else {
            // parallelSolveODEsAdpt (cur_time, beta, cm, dt_edp, i_stim);
        }
    }
}

void update_ode_state_vector (struct ode_solver *the_ode_solver, struct grid *the_grid, bool adaptive) {

    struct cell_node *grid_cell;
    grid_cell = the_grid->first_cell;

    int n_edos = the_ode_solver->model_data.number_of_ode_equations;

    if (the_ode_solver->gpu) {
        if (adaptive) {
            // updateSvGPUDxAdpt (originalNumCells, numActiveCells, activeCells.data (), sv);
        } else {
            // updateSvGPU (numActiveCells, activeCells.data (), sv);
        }
    } else {
        while (grid_cell != 0) {
            if (grid_cell->active) {
                the_ode_solver->sv[grid_cell->grid_position * n_edos] = grid_cell->v;
            }
            grid_cell = grid_cell->next;
        }
    }
}

void save_old_cell_positions (struct grid *the_grid) {

    uint64_t n_active = the_grid->num_active_cells;
    struct cell_node **ac = the_grid->active_cells;

#pragma omp parallel for
    for (uint64_t i = 0; i < n_active; i++) {
        ac[i]->sv_position = the_grid->active_cells[i]->grid_position;
    }
}

void update_cells_to_solve (struct grid *the_grid, struct ode_solver *solver) {

    uint64_t n_active = the_grid->num_active_cells;

    if (solver->cells_to_solve) {
        free (solver->cells_to_solve);
    }

    solver->cells_to_solve = (uint64_t *)malloc (the_grid->num_active_cells * sizeof (uint64_t));

#pragma omp parallel for
    for (uint64_t i = 0; i < n_active; i++) {
        solver->cells_to_solve[i] = the_grid->active_cells[i]->sv_position;
    }
}

void set_initial_conditions (struct monodomain_solver *the_solver, struct grid *the_grid, Real initial_v) {

    Real alpha, h;
    struct cell_node **ac = the_grid->active_cells;
    uint64_t active_cells = the_grid->num_active_cells;
    Real beta = the_solver->beta;
    Real cm = the_solver->cm;
    Real dt = the_solver->dt;

#pragma omp parallel for private(alpha, h)
    for (int i = 0; i < active_cells; i++) {
        h = ac[i]->face_length;
        alpha = ALPHA (beta, cm, dt, h);
        ac[i]->v = initial_v;
        ac[i]->b = initial_v * alpha;
    }
}

void set_discretization_matrix (struct monodomain_solver *the_solver, struct grid *the_grid) {

    uint64_t num_active_cells = the_grid->num_active_cells;
    struct cell_node **ac = the_grid->active_cells;

    initialize_diagonal_elements (the_solver, the_grid);

#pragma omp parallel for
    for (int i = 0; i < num_active_cells; i++) {

        // Computes and designates the flux due to south cells.
        fill_discretization_matrix_elements (the_solver, ac[i], ac[i]->south, 's');

        // Computes and designates the flux due to north cells.
        fill_discretization_matrix_elements (the_solver, ac[i], ac[i]->north, 'n');

        // Computes and designates the flux due to east cells.
        fill_discretization_matrix_elements (the_solver, ac[i], ac[i]->east, 'e');

        // Computes and designates the flux due to west cells.
        fill_discretization_matrix_elements (the_solver, ac[i], ac[i]->west, 'w');

        // Computes and designates the flux due to front cells.
        fill_discretization_matrix_elements (the_solver, ac[i], ac[i]->front, 'f');

        // Computes and designates the flux due to back cells.
        fill_discretization_matrix_elements (the_solver, ac[i], ac[i]->back, 'b');
    }
}

void initialize_diagonal_elements (struct monodomain_solver *the_solver, struct grid *the_grid) {

    Real alpha, h;
    uint64_t num_active_cells = the_grid->num_active_cells;
    struct cell_node **ac = the_grid->active_cells;
    Real beta = the_solver->beta;
    Real cm = the_solver->cm;
    Real dt = the_solver->dt;

#pragma omp parallel for private(alpha, h)
    for (int i = 0; i < num_active_cells; i++) {
        h = ac[i]->face_length;
        alpha = ALPHA (beta, cm, dt, h);

        struct element element;
        element.column = ac[i]->grid_position;
        element.cell = ac[i];
        element.value = alpha;

        if (ac[i]->elements != NULL) {
            free (ac[i]->elements);
        }

        ac[i]->elements = new_element_array ();

        ac[i]->elements[0] = element;
    }
}

void fill_discretization_matrix_elements (struct monodomain_solver *the_solver, struct cell_node *grid_cell,
                                          void *neighbour_grid_cell, char direction) {

    uint64_t position;
    bool has_found;
    Real h;

    struct transition_node *white_neighbor_cell;
    struct cell_node *black_neighbor_cell;

    Real sigmaX = the_solver->sigma_x;
    Real sigmaY = the_solver->sigma_y;
    Real sigmaZ = the_solver->sigma_z;

    Real sigmaX1 = (2.0f * sigmaX * sigmaX) / (sigmaX + sigmaX);
    Real sigmaX2 = (2.0f * sigmaX * sigmaX) / (sigmaX + sigmaX);
    Real sigmaY1 = (2.0f * sigmaY * sigmaY) / (sigmaY + sigmaY);
    Real sigmaY2 = (2.0f * sigmaY * sigmaY) / (sigmaY + sigmaY);
    Real sigmaZ1 = (2.0f * sigmaZ * sigmaZ) / (sigmaZ + sigmaZ);
    Real sigmaZ2 = (2.0f * sigmaZ * sigmaZ) / (sigmaZ + sigmaZ);

    /* When neighbour_grid_cell is a transition node, looks for the next neighbor
     * cell which is a cell node. */
    // Acha uma célula real que está no caixo enviado como vizinho
    uint16_t neighbour_grid_cell_level = ((struct basic_cell_data *)(neighbour_grid_cell))->level;
    char neighbour_grid_cell_type = ((struct basic_cell_data *)(neighbour_grid_cell))->type;

    if (neighbour_grid_cell_level > grid_cell->cell_data.level) {
        if ((neighbour_grid_cell_type == 'w')) {
            has_found = false;
            while (!has_found) {
                if (neighbour_grid_cell_type == 'w') {
                    white_neighbor_cell = (struct transition_node *)neighbour_grid_cell;
                    if (white_neighbor_cell->single_connector == NULL) {
                        has_found = true;
                    } else {
                        neighbour_grid_cell = white_neighbor_cell->quadruple_connector1;
                        neighbour_grid_cell_type = ((struct basic_cell_data *)(neighbour_grid_cell))->type;
                    }
                } else {
                    break;
                }
            }
        }
    }
    // Aqui, a célula vizinha tem um nivel de refinamento menor, entao eh mais simples.
    else {
        if (neighbour_grid_cell_level <= grid_cell->cell_data.level && (neighbour_grid_cell_type == 'w')) {
            has_found = false;
            while (!has_found) {
                if (neighbour_grid_cell_type == 'w') {
                    white_neighbor_cell = (struct transition_node *)(neighbour_grid_cell);
                    if (white_neighbor_cell->single_connector == 0) {
                        has_found = true;
                    } else {
                        neighbour_grid_cell = white_neighbor_cell->single_connector;
                        neighbour_grid_cell_type = ((struct basic_cell_data *)(neighbour_grid_cell))->type;
                    }
                } else {
                    break;
                }
            }
        }
    }

    // Tratamos somente os pontos interiores da malha.
    if (neighbour_grid_cell_type == 'b') {

        black_neighbor_cell = (struct cell_node *)(neighbour_grid_cell);

        if (black_neighbor_cell->active) {

            if (black_neighbor_cell->cell_data.level > grid_cell->cell_data.level) {
                h = black_neighbor_cell->face_length;
            } else {
                h = grid_cell->face_length;
            }

            struct element *cell_elements = grid_cell->elements;
            // Descobrimos a coluna que temos que preencher com o vizinho
            struct element element;

            position = black_neighbor_cell->grid_position;

            lock_cell_node (grid_cell);

            int el_counter = 1;
            element = cell_elements[el_counter];

            // TODO: maybe we should check for the el_counter size here
            while (el_counter < MAX_ELEMENTS_PER_MATRIX_LINE && element.cell != NULL && element.column != position) {
                element = cell_elements[++el_counter];
            }

            if (el_counter == MAX_ELEMENTS_PER_MATRIX_LINE) {
                printf ("%d %d\n ", el_counter, position);
            }

            // TODO: Cada elemento pode ter um sigma diferente
            if (element.cell == NULL) {

                struct element new_element;
                new_element.column = position;
                if (direction == 'n') { // Z direction
                    new_element.value = -sigmaZ1 * h;
                    cell_elements[0].value += (sigmaZ1 * h);
                } else if (direction == 's') { // Z direction
                    new_element.value = -sigmaZ2 * h;
                    cell_elements[0].value += (sigmaZ2 * h);
                } else if (direction == 'e') { // Y direction
                    new_element.value = -sigmaY1 * h;
                    cell_elements[0].value += (sigmaY1 * h);
                } else if (direction == 'w') { // Y direction
                    new_element.value = -sigmaY2 * h;
                    cell_elements[0].value += (sigmaY2 * h);
                } else if (direction == 'f') { // X direction
                    new_element.value = -sigmaX1 * h;
                    cell_elements[0].value += (sigmaX1 * h);
                } else if (direction == 'b') { // X direction
                    new_element.value = -sigmaX2 * h;
                    cell_elements[0].value += (sigmaX2 * h);
                }

                new_element.cell = black_neighbor_cell;
                cell_elements[el_counter] = new_element;
            }
            unlock_cell_node (grid_cell);

            // preenchemos a outra parte (a matrix é simetrica)

            cell_elements = black_neighbor_cell->elements;
            position = grid_cell->grid_position;

            lock_cell_node (black_neighbor_cell);

            el_counter = 1;
            element = cell_elements[el_counter];

            while (el_counter < MAX_ELEMENTS_PER_MATRIX_LINE && element.cell != NULL && element.column != position) {
                element = cell_elements[++el_counter];
            }

            if (element.cell == NULL) {

                struct element new_element;
                new_element.column = position;

                if (direction == 'n') { // Z direction
                    new_element.value = -sigmaZ1 * h;
                    cell_elements[0].value += (sigmaZ1 * h);
                } else if (direction == 's') { // Z direction
                    new_element.value = -sigmaZ2 * h;
                    cell_elements[0].value += (sigmaZ2 * h);
                } else if (direction == 'e') { // Y direction
                    new_element.value = -sigmaY1 * h;
                    cell_elements[0].value += (sigmaY1 * h);
                } else if (direction == 'w') { // Y direction
                    new_element.value = -sigmaY2 * h;
                    cell_elements[0].value += (sigmaY2 * h);
                } else if (direction == 'f') { // X direction
                    new_element.value = -sigmaX1 * h;
                    cell_elements[0].value += (sigmaX1 * h);
                } else if (direction == 'b') { // X direction
                    new_element.value = -sigmaX2 * h;
                    cell_elements[0].value += (sigmaX2 * h);
                }

                new_element.cell = grid_cell;
                cell_elements[el_counter] = new_element;
            }

            unlock_cell_node (black_neighbor_cell);
        }
    }
}

void print_solver_info (struct monodomain_solver *the_monodomain_solver, struct ode_solver *the_ode_solver,
                        struct grid *the_grid, struct output_utils *output_info) {
    printf ("System parameters: \n");
#if defined(_OPENMP)
    printf ("Using OpenMP with %d threads\n", omp_get_num_threads ());
#endif
    if (the_ode_solver->gpu) {
        printf ("Using GPU to solve ODEs\n");
    }

    printf ("Time discretization: %lf\n", the_monodomain_solver->dt);
    printf ("Initial V: %lf\n", the_ode_solver->model_data.initial_v);
    printf ("Initial Space Discretization: %lf um\n", the_monodomain_solver->start_h);

    if (the_monodomain_solver->adaptive) {
        printf ("Minimum Space Discretization: %lf um\n", the_monodomain_solver->min_h);
        printf ("Maximum Space Discretization: %lf um\n", the_monodomain_solver->max_h);
    }

    printf ("Width = %lf um, height = %lf um, Depth = %lf \n", the_grid->side_length, the_grid->side_length,
            the_grid->side_length);
    printf ("Initial N. of Elements = "
            "%" PRIu64 "\n",
            the_grid->num_active_cells);
    printf ("PDE time step = %lf\n", the_monodomain_solver->dt);
    printf ("ODE solver edo_method: %s\n", get_ode_method_name (the_ode_solver->method));
    printf ("ODE min time step = %lf\n", the_ode_solver->min_dt);
    if ((the_ode_solver->method == EULER_METHOD_ADPT)) {
        printf ("ODE max time step = %lf\n", the_ode_solver->max_dt);
        printf ("Absolute tolerance for edo = %lf\n", the_ode_solver->abs_tol);
        printf ("Relative tolerance for edo = %lf\n", the_ode_solver->rel_tol);
    }

    printf ("Simulation Final Time = %lf\n", the_monodomain_solver->final_time);
    printf ("Print Rate = %d\n", output_info->print_rate);
    printf ("Stimulus start = %lf\n", the_ode_solver->stim_start);
    printf ("Stimulus duration = %lf\n", the_ode_solver->stim_duration);
    printf ("Stimulus value = %lf\n", the_ode_solver->stim_current);
    printf ("Maximum CG iterations = %d\n", the_monodomain_solver->max_iterations);
    printf ("CG tolerance = %e\n", the_monodomain_solver->tolerance);
    if (the_monodomain_solver->use_jacobi) {
        printf ("Using Jacobi preconditioner\n");
    }
    if (the_monodomain_solver->adaptive) {
        printf ("Using adaptativity\n");
        printf ("Refinement Bound = %lf\n", the_monodomain_solver->refinement_bound);
        printf ("Derefinement Bound = %lf\n", the_monodomain_solver->derefinement_bound);
        printf ("Refining each %d time steps\n", the_monodomain_solver->refine_each);
        printf ("Derefining each %d time steps\n", the_monodomain_solver->derefine_each);
    }
    if (output_info->output_dir_name != NULL) {
        char *out_dir = output_info->output_dir_name;
        if (!dir_exists (out_dir)) {
            printf ("%s does not exist! Creating\n", out_dir);

            if (mkdir (out_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
                fprintf (stderr, "Error creating directory %s. Exiting!", out_dir);
                exit (10);
            }
        }

        printf ("Saving to plain text output in %s dir\n", out_dir);

    } else {
        printf ("The solution will not be saved\n");
    }
}

// Prints grid discretization matrix.
void print_grid_matrix (struct grid *the_grid, FILE *output_file) {
    if (!output_file) {
        fprintf (stderr, "print_grid_matrix: output_file is NULL! Open it first!");
        return;
    }

    struct cell_node *grid_cell;
    grid_cell = the_grid->first_cell;
    struct element element;
    while (grid_cell != 0) {
        if (grid_cell->active) {
            fprintf (output_file,
                     "Row "
                     "%" PRIu64 ": ",
                     grid_cell->grid_position + 1);

            element = grid_cell->elements[0];
            int el_count = 1;

            while ((el_count < MAX_ELEMENTS_PER_MATRIX_LINE) && (element.cell != NULL)) {

                fprintf (output_file,
                         " %.6lf ("
                         "%" PRIu64 ","
                         "%" PRIu64 ") ",
                         element.value, grid_cell->grid_position + 1, (element.column) + 1);

                element = grid_cell->elements[el_count];
                el_count++;
            }
            fprintf (output_file, "\n");
        }
        grid_cell = grid_cell->next;
    }
    fprintf (output_file, "________________________________________________________________________\n");
}