# Lecture command cheat sheet

## Build and validate

```bat
configure_ninja_cuda128.bat
execute_all_tests.bat
```

## Short curated demo

```bat
execute_lecture_demo_core.bat
```

## Full phase runners

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
execute_graph_connected_components_all_sweeps_and_analyze.bat
execute_graph_weighted_relaxation_all_sweeps_and_analyze.bat
execute_constraint_network_all_sweeps_and_analyze.bat
execute_combination_finder_all_sweeps_and_analyze.bat
execute_assignment_preprocessing_all_sweeps_and_analyze.bat
execute_local_search_moves_all_sweeps_and_analyze.bat
execute_scenario_simulation_all_sweeps_and_analyze.bat
```

## Plot-only runners

```bat
execute_assignment_preprocessing_plots.bat
execute_local_search_moves_plots.bat
execute_scenario_simulation_plots.bat
```

## Export/visualize problem definitions

Use the phase-specific exporters when you want to explain the generated problem instance, not only the timing result:

```bat
build-cuda-ninja\export_cost_matrix.exe --preset tiny --output-dir results\cost_matrix_tiny
build-cuda-ninja\export_spatial_events.exe --output-dir results\spatial_events_demo
build-cuda-ninja\export_graph_foundation.exe --output-dir results\graph_foundation_demo
build-cuda-ninja\export_constraint_network.exe --tasks 32 --resources 16 --candidates 512 --output-dir results\constraint_network_problem_demo
build-cuda-ninja\export_assignment_preprocessing.exe --tasks 32 --resources 24 --top-k 4 --output-dir resultsssignment_preprocessing_problem_demo
build-cuda-ninja\export_local_search_moves.exe --tasks 32 --resources 24 --moves 1024 --output-dir results\local_search_moves_problem_demo
build-cuda-ninja\export_scenario_simulation.exe --tasks 32 --resources 16 --scenarios 4096 --output-dir results\scenario_simulation_problem_demo
```
