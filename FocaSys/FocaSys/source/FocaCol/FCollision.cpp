#include<FocaCol/FCollision.h>
FocaCol::CollisionBase::CollisionBase() : Id(0), Center(0.0f), Size(1.0f)
{

}
FocaCol::CollisionBase::CollisionBase(u32 Id, const glm::vec3& Center, const glm::vec3& Size) : Id(Id), Center(Center), Size(Size)
{

}
FocaCol::CollisionBase::~CollisionBase()
{

}
u32 FocaCol::CollisionBase::GetId() const
{
	return Id;
}
const glm::vec3& FocaCol::CollisionBase::GetCenter() const
{
	return Center;
}
const glm::vec3& FocaCol::CollisionBase::GetSize() const
{
	return Size;
}