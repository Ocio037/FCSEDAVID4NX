#include "FocaCol/FColAABB.h"
#include <cmath>
FocaCol::ColAABB::ColAABB() : CollisionBase(), Kind(ColKind::Solid)
{ 

}
FocaCol::ColAABB::ColAABB(u32 Id, const glm::vec3& Center, const glm::vec3& Size, ColKind Kind) : CollisionBase(Id, Center, Size), Kind(Kind)
{

}
FocaCol::ColKind FocaCol::ColAABB::GetKind() const
{
	return Kind;
}
glm::vec3 FocaCol::ColAABB::GetMinimum() const
{
	return Center - Size * 0.5f;
}
glm::vec3 FocaCol::ColAABB::GetMaximum() const
{
	return Center + Size * 0.5f;
}
bool FocaCol::ColAABB::IsValid() const
{
	return static_cast<u32>(Kind) <= static_cast<u32>(ColKind::Trigger) && std::isfinite(Center.x) && std::isfinite(Center.y) && std::isfinite(Center.z) && std::isfinite(Size.x) && std::isfinite(Size.y) && std::isfinite(Size.z) && Size.x > 0.0f && Size.y > 0.0f && Size.z > 0.0f;
}
