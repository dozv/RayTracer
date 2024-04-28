# Ray Tracer

[Imgur link to animated GIF](https://imgur.com/xragOqD)

Imagine a virtual camera shooting camera rays pixel by pixel into a scene.
If a camera ray hits an object like a triangle (a cube is made out of 12 triangles),
color the pixel accordingly; otherwise, use a background color.

From the intersection point on, you can also shoot more rays, like shadow rays.
Check if the shadow ray intersects with another object on the way to your light source.
If yes, darken the pixel; if not, continue.
This was ray tracing in a nutshell for (non-technical) people.


Features:
- Camera rays
- Shadow rays
- Reflection rays
- Lambertian (diffuse) illumination model and shading
- First-person camera controls (WASD, arrow keys)
