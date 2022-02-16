
export COMPILER=${VULKAN_SDK}/bin/glslc
${COMPILER} -fshader-stage=vertex -g shaders/fluid_cube_vert.glsl -o shaders/spv/fluid_cube_vert.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_advect_quantity.glsl -o shaders/spv/fluid_advect_quantity.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_apply_force.glsl -o shaders/spv/fluid_apply_force.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_project_divergence.glsl -o shaders/spv/fluid_project_divergence.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_jacobi_solver.glsl -DPRESSURE_SOLVER=1 -o shaders/spv/fluid_jacobi_solver_pressure.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_jacobi_solver.glsl -o shaders/spv/fluid_jacobi_solver.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_project_gradient_subtract.glsl -o shaders/spv/fluid_project_gradient_subtract.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_ink_present.glsl -o shaders/spv/fluid_ink_present.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_vorticity_curl.glsl -o shaders/spv/fluid_vorticity_curl.spv
${COMPILER} -fshader-stage=fragment -g shaders/fluid_vorticity_force.glsl -o shaders/spv/fluid_vorticity_force.spv

${COMPILER} -fshader-stage=compute -g shaders/boids.comp -o shaders/spv/boids.spv
${COMPILER} -fshader-stage=vertex -g shaders/skyboxVert.glsl -o shaders/spv/skyboxVert.spv
${COMPILER} -fshader-stage=fragment -g shaders/skyboxFrag.glsl -o shaders/spv/skyboxFrag.spv
${COMPILER} -fshader-stage=vertex -g shaders/debugVert.glsl -o shaders/spv/debugVert.spv
${COMPILER} -fshader-stage=fragment -g shaders/debugFrag.glsl -o shaders/spv/debugFrag.spv
${COMPILER} -fshader-stage=vertex -g shaders/fishVert.glsl -o shaders/spv/fishVert.spv
${COMPILER} -fshader-stage=fragment -g shaders/fishFrag.glsl -o shaders/spv/fishFrag.spv
