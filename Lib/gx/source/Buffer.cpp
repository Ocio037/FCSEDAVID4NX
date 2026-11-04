#include<Buffer.h>
#include"Data/BufferData.h"
#include"GxUtil.h"
#include<Display.h>
gx::Buffer::Buffer() : Data(nullptr)
{

}
gx::Buffer::~Buffer()
{
    Finalize();
}
void gx::Buffer::Initialize(gx::Display& display)
{
    Finalize();
    Data = Mem::MakeUnique<BufferData>();
    if (!Data) return;
    const internal::Device* Device = display.GetDeviceInfo();
    if (!Device || !Device->DeviceDat.Device)
    {
        Data.reset();
        return;
    }
    Data->Device = Device->DeviceDat.Device;
    Data->PhyDevice = Device->PhyDeviceDat.PhyDevice;
}
void gx::Buffer::Finalize()
{
    if (!Data) return;
    if (Data->Device)
    {
        Data->Device.waitIdle();
        if (Data->HasIndex)
        {
            if (Data->IndexBuffer)
            {
                Data->Device.destroyBuffer(Data->IndexBuffer);
            }
            if (Data->IndexMemory)
            {
                Data->Device.freeMemory(Data->IndexMemory);
            }
        }
        if (Data->HasVertex)
        {
            if (Data->VertexBuffer)
            {
                Data->Device.destroyBuffer(Data->VertexBuffer);
            }
            if (Data->VertexMemory)
            {
                Data->Device.freeMemory(Data->VertexMemory);
            }
        }
    }
    Data.reset();
}
bool gx::Buffer::CreateVertexBuffer(const void* data, u64 sizeInBytes)
{
    if (!Data || !Data->Device || !data || sizeInBytes == 0) return false;
    if (Data->HasVertex)
    {
        if (Data->VertexBuffer)
        {
            Data->Device.destroyBuffer(Data->VertexBuffer);
        }
        if (Data->VertexMemory)
        {
            Data->Device.freeMemory(Data->VertexMemory);
        }
        Data->HasVertex = false;
    }
    vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
        .setSize(sizeInBytes)
        .setUsage(vk::BufferUsageFlagBits::eVertexBuffer)
        .setSharingMode(vk::SharingMode::eExclusive);
    Data->VertexBuffer = Data->Device.createBuffer(bufferInfo);
    vk::MemoryRequirements memReqs = Data->Device.getBufferMemoryRequirements(Data->VertexBuffer);
    u32 memTypeIdx = internal::FindMemoryType(Data->PhyDevice, memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    if (memTypeIdx == UINT32_MAX)
    {
        Data->Device.destroyBuffer(Data->VertexBuffer);
        return false;
    }
    vk::MemoryAllocateInfo allocInfo = vk::MemoryAllocateInfo()
        .setAllocationSize(memReqs.size)
        .setMemoryTypeIndex(memTypeIdx);
    Data->VertexMemory = Data->Device.allocateMemory(allocInfo);
    Data->Device.bindBufferMemory(Data->VertexBuffer, Data->VertexMemory, 0);
    void* mapped = Data->Device.mapMemory(Data->VertexMemory, 0, sizeInBytes);
    std::memcpy(mapped, data, sizeInBytes);
    Data->Device.unmapMemory(Data->VertexMemory);
    Data->HasVertex = true;
    Data->VertexSize = sizeInBytes;
    return true;
}
bool gx::Buffer::UpdateVertexBuffer(const void* data, u64 sizeInBytes, u64 offsetInBytes)
{
	if (!Data || !Data->Device || !Data->HasVertex || !Data->VertexMemory || !data || sizeInBytes == 0 ||
		offsetInBytes > Data->VertexSize || sizeInBytes > Data->VertexSize - offsetInBytes)
    {
        return false;
    }
	void* mapped = Data->Device.mapMemory(Data->VertexMemory, 0, Data->VertexSize);
    if (!mapped)
    {
        return false;
    }
	std::memcpy(static_cast<u8*>(mapped) + offsetInBytes, data, static_cast<size_t>(sizeInBytes));
    Data->Device.unmapMemory(Data->VertexMemory);
    return true;
}
bool gx::Buffer::UpdateVertexBuffer(const void* data, u64 sizeInBytes)
{
	return UpdateVertexBuffer(data, sizeInBytes, 0);
}
bool gx::Buffer::CreateIndexBuffer(const u32* data, u32 indexCount)
{
    if (!Data || !Data->Device || !data || indexCount == 0) return false;
    if (Data->HasIndex)
    {
        if (Data->IndexBuffer)
        {
            Data->Device.destroyBuffer(Data->IndexBuffer);
        }
        if (Data->IndexMemory)
        {
            Data->Device.freeMemory(Data->IndexMemory);
        }
        Data->HasIndex = false;
    }
    u64 sizeInBytes = static_cast<u64>(indexCount) * sizeof(u32);
    vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
        .setSize(sizeInBytes)
        .setUsage(vk::BufferUsageFlagBits::eIndexBuffer)
        .setSharingMode(vk::SharingMode::eExclusive);
    Data->IndexBuffer = Data->Device.createBuffer(bufferInfo);
    vk::MemoryRequirements memReqs = Data->Device.getBufferMemoryRequirements(Data->IndexBuffer);
    u32 memTypeIdx = internal::FindMemoryType(Data->PhyDevice, memReqs.memoryTypeBits,vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    if (memTypeIdx == UINT32_MAX)
    {
        Data->Device.destroyBuffer(Data->IndexBuffer);
        return false;
    }
    vk::MemoryAllocateInfo allocInfo = vk::MemoryAllocateInfo()
        .setAllocationSize(memReqs.size)
        .setMemoryTypeIndex(memTypeIdx);
    Data->IndexMemory = Data->Device.allocateMemory(allocInfo);
    Data->Device.bindBufferMemory(Data->IndexBuffer, Data->IndexMemory, 0);
    void* mapped = Data->Device.mapMemory(Data->IndexMemory, 0, sizeInBytes);
    std::memcpy(mapped, data, sizeInBytes);
    Data->Device.unmapMemory(Data->IndexMemory);
    Data->IndexCount = indexCount;
    Data->HasIndex = true;
    return true;
}
u32 gx::Buffer::GetIndexCount() const
{
    if (!Data) return 0;
    return Data->IndexCount;
}
bool gx::Buffer::IsValid() const
{
    return Data && Data->Device && Data->HasVertex;
}
