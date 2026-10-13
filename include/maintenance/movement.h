#pragma once

namespace maintenance {

/**
 * Endstone only raises a PlayerMoveEvent for position changes above this squared distance (or for a large camera
 * turn). Using the same value means "did the player really move?" agrees with the server's own definition.
 */
inline constexpr float kMoveThresholdSquared = 1.0F / 256.0F;

/**
 * True if a displacement is a real position change. Rotation-only events (displacement below the threshold) are not:
 * cancelling those would make Endstone teleport the player back for every camera turn without stopping any movement.
 */
constexpr bool isPositionalMove(float dx, float dy, float dz)
{
    return dx * dx + dy * dy + dz * dz > kMoveThresholdSquared;
}

} // namespace maintenance
