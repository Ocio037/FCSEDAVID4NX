#include "FocaCol/FColOBB.h"
#include <cmath>
FocaCol::ColOBB::ColOBB() : CollisionBase(), Kind(ColKind::Solid), RotationDegrees(0.0f)
{

}
FocaCol::ColOBB::ColOBB(u32 Id, const glm::vec3& Center, const glm::vec3& Size, const glm::vec3& RotationDegrees, ColKind Kind) : CollisionBase(Id, Center, Size), Kind(Kind), RotationDegrees(RotationDegrees)
{

}
FocaCol::ColKind FocaCol::ColOBB::GetKind() const
{
	return Kind;
}
const glm::vec3& FocaCol::ColOBB::GetRotationDegrees() const
{
	return RotationDegrees;
}
bool FocaCol::ColOBB::IsValid() const
{
	return static_cast<u32>(Kind) <= static_cast<u32>(ColKind::Trigger) && std::isfinite(Center.x) && std::isfinite(Center.y) && std::isfinite(Center.z) && std::isfinite(Size.x) && std::isfinite(Size.y) && std::isfinite(Size.z) && Size.x > 0.0f && Size.y > 0.0f && Size.z > 0.0f && std::isfinite(RotationDegrees.x) && std::isfinite(RotationDegrees.y) && std::isfinite(RotationDegrees.z);
}