#include "view.h"


XMMATRIX Viewport::target_pixel_to_projective() const
{
    const float w = size.x;
    const float h = size.y;

    const float sx = (w != 0.0f) ? 2.0f / w : 0.0f;  
    const float sy = (h != 0.0f) ? -2.0f / h : 0.0f;  
    const float tx = -1.0f;                           
    const float ty = 1.0f;                           

    
    
    return XMMatrixSet(
        sx, 0.0f, 0.0f, tx,
        0.0f, sy, 0.0f, ty,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}