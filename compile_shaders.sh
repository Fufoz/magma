
export COMPILER=${VULKAN_SDK}/bin/glslc
${COMPILER} -fshader-stage=vertex -g shaders/fluid_cube_vert.glsl -o shaders/spv/fluid_cube_vert.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_advect_quantity.glsl -o shaders/spv/fluid_advect_quantity.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_apply_force.glsl -o shaders/spv/fluid_apply_force.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_project_divergence.glsl -o shaders/spv/fluid_project_divergence.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_jacobi_solver.glsl -DPRESSURE_SOLVER=1 -o shaders/spv/fluid_jacobi_solver_pressure.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_jacobi_solver.glsl -o shaders/spv/fluid_jacobi_solver.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_project_gradient_subtract.glsl -o shaders/spv/fluid_project_gradient_subtract.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_ink_present.glsl -o shaders/spv/fluid_ink_present.spv