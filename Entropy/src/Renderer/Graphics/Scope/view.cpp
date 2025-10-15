#include "view.h"


XMMATRIX Viewport::target_pixel_to_projective() const
{
    const float w = size.x;
    const float h = size.y;

    const float sx = (w != 0.0f) ? 2.0f / w : 0.0f;  // scale X to [-1, 1]
    const float sy = (h != 0.0f) ? -2.0f / h : 0.0f;  // flip Y (top-left ? clip)
    const float tx = -1.0f;                           // shift X by -1
    const float ty = 1.0f;                           // shift Y by +1

    // Row-major matrix that turns [px, py, z, 1] (pixels) into clip coordinates:
    // x' = sx*px + tx,  y' = sy*py + ty,  z' = z,  w' = 1
    return XMMatrixSet(
        sx, 0.0f, 0.0f, tx,
        0.0f, sy, 0.0f, ty,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}