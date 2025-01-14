# texture_packer

don't forget to the include the stb header in your `main.cpp`

```cpp
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
```

## texture coordinates 
For the sake of brevity denote texture coordinate as tc. Normally if you want a texture to tile over your geometry you simply make the tc's outside of the [0, 1]x[0, 1] range. By doing that and setting specific opengl options the texture will automatically tile on that geometry.

When using a texture container, recall that this allows for different geometry to use different images by sampling those images from the container, this is facilitated by allowing the geometry to use regular tc's as if they were on its own specific image, and then in the code there is a conversion done which transforms the regular tcs to "packed" tcs, these packed ones correctly specify where the texture resides within the context of the texture container.

Now a problem arises if your regular tcs are outside of [0,1]x[0,1] then that means that in the context of the packed texture those ptcs will probably be sampling neighboring images causing your geometry to look like a patchwork of different textures which is obviously a big problem.

In general the way to fix this is to manually tile the texture coordinates by using the modulo operation to force texture coordinates back inside the texture it should exist in.

Now we need to figure out at which stage in the rendering pipeline this needs to be fixed at, if we tried to correct for this problem before we upload our data to the graphics card, we'll have a problem, because the rasterizer will interpolate the passed in texture coordinates, if we forced the texture coordinates back inside the texture before uploading the geometry data then we would get some strange texture data on the inside of the texture without any tiling.

This tells us that we must allow these out of range tcs to go through the rasterization phase first, and catch them after that. This implies that we need to handle this problem in our fragment shader. In the fragment shader we would be doing something like this: 

```
  (0, 0)                                                    (1, 0)
    -----------------------------------------------------------
    |                                                         |
    |               ................W................         |
    |                                                         |
    |            (tlx, tly)                    (trx, try)     |
    |               ---------------------------------         |
    |         .     |                               |         |
    |         .     |                               |         |
    |         .     |                               |         |
    |         H     |                               |         |
    |         .     |               ?               |         |
    |         .     |              (fx, fy)         |         |
    |               ---------------------------------         |
    |            (blx, bly)                    (brx, bry)     |
    |                                                         |
    |                                                         |
    |                                                         |
    |                                                         |
    |                               x                         |
    |                           (ptcx, ptcy)                  |
    |                                                         |
    |                                                         |
    |                                                         |
    |                                                         |
    -----------------------------------------------------------
  (0, 1)                                                     (1, 1)
```

And now we would need to know where (tcx, tcy) would end up if it was put back into the bounding box of our selected texture. To do this we can derive the following equation. First we focus on putting tcy back into the box, as tcx is already within the x bounds. 

So given tcy we measure its delta y from the top left y value, that is we set dy = tcy - tly we then need to get the remainder of dy with respect to H = (bly - tly), and thus we have that fy = dy % H + tly, similary we deduce that fx = dx % W + tlx.

Now that we've solved the correction of the texture coordinate, we can now deduce that the information that is required in the fragment shader:
 - the bounding box of the current texture used for this fragment 
 - the packed texture coordinate and container index for the current fragment

 in order to get the bounding box we will be using an array of the bounding boxes for each image, so we will have to construct a mapping of an integer to the a particular texture in the c++ code upload that array by uniform to the shader program, then pass this integer along with the geometry when uploading to VBO's and also be sure to make it a flat out so that that number does not get interpolated before it gets to the fragment shader, then in the fragment shader we can access the correct bounding box for that image to use in the calculation.

Note: Something I messed up was giving the bounding box in terms of pixels, this is wrong since you must normalize them to be in the [0, 1] range
