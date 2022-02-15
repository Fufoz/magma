## Magma
Magma is a polygon for graphics test projects using Vulkan API.

### Hail to the triangle!
<p align="center">
  <img src="/images/tri.png" />
</p>

### Animation
This example demonstrates a skeletal animation. Model of a fish and accompanying
skinned data is loaded from a gltf file. This data later gets passed to the vertex
shader to compute a final skin matrix.
<p align="center">
  <img src="/images/fish.gif" />
</p>

### Boids
Boids sample demonstrates boids simulation using many fishes that moves in a school-like behaivour. 
The rules for simulating scools of fish were taken from Craig Reynolds boids [paper](https://cs.stanford.edu/people/eroberts/courses/soco/projects/2008-09/modeling-natural-systems/boids.html).
Beside these rules boids also avoid collision with virtual tank borders via sphere tracing technique in order to find the most unobstructed direction.
<p align="center">
  <img src="/images/boidsComp.gif" />
</p>

### Fluid simulation
This sample implements a 2d fluid simulation by solving Navier-Stokes PDEs.<br />
Instead of keeping track of each fluid particle individually(Lagrangian viewpoint) the problem domain is discretized by using 2d grid of cells, where each cell has certain fluid characteristics like velocity or pressure. <br />
We then look at this fixed cells  and and observe how fluid quantities change over time (a.k.a Eulerian viepoint).<br />
At each time step we update these vector fields by solving aforementioned PDEs untill the fluid becomes divergent free.
Solving Navier-Stokes PDEs consists of the following parts:
  * fluid advection step: update velocity vector field based on the results of the previous step  
  * fluid viscocity step: apply viscocity to the fluid
  * force application step: generate force by dragging the mouse over the domain
  * pressure solver step: find pressure at every grid cell
  * pressure subtract step: make fluid divergent free again
  * force ink step: apply colored ink by dragging mouse over the domain
  * ink advection step: transport applied ink by velocity field
<p align="center">
  <img src="/images/girl.gif" />
</p>
<p align="center">
  <img src="/images/ink.gif" />
</p>

