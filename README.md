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
