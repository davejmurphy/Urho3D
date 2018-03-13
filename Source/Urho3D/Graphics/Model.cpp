//
// Copyright (c) 2008-2017 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../Core/Profiler.h"
#include "../Graphics/Geometry.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/Model.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/VertexBuffer.h"
#include "../IO/Log.h"
#include "../IO/File.h"
#include "../IO/FileSystem.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/XMLFile.h"

#include "../DebugNew.h"

#include "../Gltf/GltfHelper.h"

#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

namespace Urho3D
{

unsigned LookupVertexBuffer(VertexBuffer* buffer, const Vector<SharedPtr<VertexBuffer> >& buffers)
{
    for (unsigned i = 0; i < buffers.Size(); ++i)
    {
        if (buffers[i] == buffer)
            return i;
    }
    return 0;
}

unsigned LookupIndexBuffer(IndexBuffer* buffer, const Vector<SharedPtr<IndexBuffer> >& buffers)
{
    for (unsigned i = 0; i < buffers.Size(); ++i)
    {
        if (buffers[i] == buffer)
            return i;
    }
    return 0;
}

Model::Model(Context* context) :
    ResourceWithMetadata(context)
{
}

Model::~Model()
{
}

void Model::RegisterObject(Context* context)
{
    context->RegisterFactory<Model>();
}

bool Model::BeginLoad(Deserializer& source)
{
    // Check ID
    String fileID = source.ReadFileID();
    if (fileID != "UMDL" && fileID != "UMD2")
    {
        if (source.GetName().Contains("gltf", false))
        {
            tinygltf::Model gltfModel;
            std::string errorMessage;
            tinygltf::TinyGLTF loader;
                       
            bool isLoaded = loader.LoadASCIIFromFile(&gltfModel, &errorMessage, std::string("C:/Users/Owner/Development/Urho3D/bin/Data/").append(source.GetName().CString()));
            if (!isLoaded)
            {
                const auto msg = "Failed to load gltf model" + errorMessage;
                throw std::exception(msg.c_str());
            }

            const auto gltfNode = gltfModel.nodes.at(0);

            if (true) //gltfNode.mesh != -1)
            {
                GltfHelper::Primitive primitive;

                const auto gltfMesh = gltfModel.meshes.at(0); // gltfNode.mesh);
                // TODO: The .bin file should already be layed out for copying into the vertex/index buffers. So we may not need to read them.
                for (const auto gltfPrimitive : gltfMesh.primitives)
                {
                    primitive = GltfHelper::ReadPrimitive(gltfModel, gltfPrimitive);
                }

                geometries_.Clear();
                geometryBoneMappings_.Clear();
                geometryCenters_.Clear();
                morphs_.Clear();
                vertexBuffers_.Clear();
                indexBuffers_.Clear();

                unsigned memoryUse = sizeof(Model);
                bool async = GetAsyncLoadState() == ASYNC_LOADING;

                // Read vertex buffers
                unsigned numVertexBuffers = 1; // source.ReadUInt();
                vertexBuffers_.Reserve(numVertexBuffers);
                morphRangeStarts_.Resize(numVertexBuffers);
                morphRangeCounts_.Resize(numVertexBuffers);
                loadVBData_.Resize(numVertexBuffers);
                for (unsigned i = 0; i < numVertexBuffers; ++i)
                {
                    VertexBufferDesc& desc = loadVBData_[i];

                    desc.vertexCount_ = primitive.Vertices.size(); // source.ReadUInt();
                 
                    {
                        // Need to pair up types and sematices for the vertices
                        desc.vertexElements_.Clear();

                        VertexElementType type1 = VertexElementType::TYPE_VECTOR3;
                        VertexElementSemantic sem1 = SEM_POSITION;
                        unsigned char index1 = 0;
                        desc.vertexElements_.Push(VertexElement(type1, sem1, index1));

                        VertexElementType type2 = VertexElementType::TYPE_VECTOR3;
                        VertexElementSemantic sem2 = SEM_NORMAL;
                        unsigned char index2 = 0;
                        desc.vertexElements_.Push(VertexElement(type2, sem2, index2));

                        VertexElementType type3 = VertexElementType::TYPE_VECTOR4;
                        VertexElementSemantic sem3 = SEM_TANGENT;
                        unsigned char index3 = 0;
                        desc.vertexElements_.Push(VertexElement(type3, sem3, index3));

                        VertexElementType type4 = VertexElementType::TYPE_VECTOR2;
                        VertexElementSemantic sem4 = SEM_TEXCOORD;
                        unsigned char index4 = 0;
                        desc.vertexElements_.Push(VertexElement(type4, sem4, index4));
                    }

                    SharedPtr<VertexBuffer> buffer(new VertexBuffer(context_));
                    unsigned vertexSize = VertexBuffer::GetVertexSize(desc.vertexElements_);
                    desc.dataSize_ = desc.vertexCount_ * vertexSize;

                    // Prepare vertex buffer data to be uploaded during EndLoad()
                    if (async)
                    {
                        desc.data_ = new unsigned char[desc.dataSize_];
                        source.Read(desc.data_.Get(), desc.dataSize_);
                    }
                    else
                    {
                        // If not async loading, use locking to avoid extra allocation & copy
                        desc.data_.Reset(); // Make sure no previous data
                        buffer->SetShadowed(true);
                        buffer->SetSize(desc.vertexCount_, desc.vertexElements_);
                        void* dest = buffer->Lock(0, desc.vertexCount_);

                        float* destfloat = static_cast<float *>(dest);

                        int current = 0;
                        // Need to read everything out of the vertex infor into the void* dest
                        for (int i = 0; i < primitive.Vertices.size(); i++)
                        {
                            auto vertex = primitive.Vertices.at(i);

                            Vector3 pos{ vertex.Position.x, vertex.Position.y, vertex.Position.z };
                            Vector3 normal{ vertex.Normal.x, vertex.Normal.y, vertex.Normal.z };
                            Vector4 tangent{ vertex.Tangent.x, vertex.Tangent.y, vertex.Tangent.z, vertex.Tangent.w };
                            Vector2 texcoord{ vertex.TexCoord0.x, vertex.TexCoord0.y };

                            memcpy(&destfloat[current], static_cast<void*>(&pos), sizeof(pos));
                            current += 3;
                            memcpy(&destfloat[current], static_cast<void*>(&normal), sizeof(normal));
                            current += 3;
                            memcpy(&destfloat[current], static_cast<void*>(&tangent), sizeof(tangent));
                            current += 4;
                            memcpy(&destfloat[current], static_cast<void*>(&texcoord), sizeof(texcoord));
                            current += 2;
                        }
                        buffer->Unlock();
                    }

                    memoryUse += sizeof(VertexBuffer) + desc.vertexCount_ * vertexSize;
                    vertexBuffers_.Push(buffer);
                }

                // Read index buffers
                unsigned numIndexBuffers = 1;
                indexBuffers_.Reserve(numIndexBuffers);
                loadIBData_.Resize(numIndexBuffers);
                for (unsigned i = 0; i < numIndexBuffers; ++i)
                {
                    unsigned indexCount = primitive.Indices.size();
                    unsigned indexSize = sizeof(primitive.Indices.at(0));

                    SharedPtr<IndexBuffer> buffer(new IndexBuffer(context_));

                    // Prepare index buffer data to be uploaded during EndLoad()
                    if (async)
                    {
                        loadIBData_[i].indexCount_ = indexCount;
                        loadIBData_[i].indexSize_ = indexSize;
                        loadIBData_[i].dataSize_ = indexCount * indexSize;
                        loadIBData_[i].data_ = new unsigned char[loadIBData_[i].dataSize_];
                        source.Read(loadIBData_[i].data_.Get(), loadIBData_[i].dataSize_);
                    }
                    else
                    {
                        // If not async loading, use locking to avoid extra allocation & copy
                        loadIBData_[i].data_.Reset(); // Make sure no previous data
                        buffer->SetShadowed(true);
                        buffer->SetSize(indexCount, indexSize > sizeof(unsigned short));
                        void* dest = buffer->Lock(0, indexCount);

                        uint32_t* destint = static_cast<uint32_t *>(dest);
                        int current = 0;

                        for (int i = 0; i < primitive.Indices.size(); i += 3)
                        {
                            memcpy(&destint[current], static_cast<void*>(&primitive.Indices.at(i)), sizeof(uint32_t));
                            memcpy(&destint[current + 1], static_cast<void*>(&primitive.Indices.at(i + 1)), sizeof(uint32_t));
                            memcpy(&destint[current + 2], static_cast<void*>(&primitive.Indices.at(i + 2)), sizeof(uint32_t));
                            current += 3;
                        }

                        //source.Read(dest, indexCount * indexSize);
                        buffer->Unlock();
                    }

                    memoryUse += sizeof(IndexBuffer) + indexCount * indexSize;
                    indexBuffers_.Push(buffer);
                }

                // Read geometries
                unsigned numGeometries = 1;
                geometries_.Reserve(numGeometries);
                geometryBoneMappings_.Reserve(numGeometries);
                geometryCenters_.Reserve(numGeometries);
                loadGeometries_.Resize(numGeometries);
                for (unsigned i = 0; i < numGeometries; ++i)
                {
                    // Read bone mappings
                    unsigned boneMappingCount = 0; // TODO: Add bones back
                    PODVector<unsigned> boneMapping(boneMappingCount);
                    for (unsigned j = 0; j < boneMappingCount; ++j)
                        boneMapping[j] = source.ReadUInt();
                    geometryBoneMappings_.Push(boneMapping);

                    unsigned numLodLevels = 1; // TODO: Use actual lod levels
                    Vector<SharedPtr<Geometry> > geometryLodLevels;
                    geometryLodLevels.Reserve(numLodLevels);
                    loadGeometries_[i].Resize(numLodLevels);

                    for (unsigned j = 0; j < numLodLevels; ++j)
                    {
                        float distance = 0; // TODO: Read the lod distance
                        PrimitiveType type = TRIANGLE_LIST; // TODO: Read the geo type from gltf (PrimitiveType)source.ReadUInt();

                        unsigned vbRef = 0; // source.ReadUInt();
                        unsigned ibRef = 0; // source.ReadUInt();
                        unsigned indexStart = 0; // source.ReadUInt();
                        unsigned indexCount = primitive.Indices.size(); // source.ReadUInt();

                        if (vbRef >= vertexBuffers_.Size())
                        {
                            URHO3D_LOGERROR("Vertex buffer index out of bounds");
                            loadVBData_.Clear();
                            loadIBData_.Clear();
                            loadGeometries_.Clear();
                            return false;
                        }
                        if (ibRef >= indexBuffers_.Size())
                        {
                            URHO3D_LOGERROR("Index buffer index out of bounds");
                            loadVBData_.Clear();
                            loadIBData_.Clear();
                            loadGeometries_.Clear();
                            return false;
                        }

                        SharedPtr<Geometry> geometry(new Geometry(context_));
                        geometry->SetLodDistance(distance);

                        // Prepare geometry to be defined during EndLoad()
                        loadGeometries_[i][j].type_ = type;
                        loadGeometries_[i][j].vbRef_ = vbRef;
                        loadGeometries_[i][j].ibRef_ = ibRef;
                        loadGeometries_[i][j].indexStart_ = indexStart;
                        loadGeometries_[i][j].indexCount_ = indexCount;

                        geometryLodLevels.Push(geometry);
                        memoryUse += sizeof(Geometry);
                    }

                    geometries_.Push(geometryLodLevels);
                }

                // Read morphs
                /*unsigned numMorphs = source.ReadUInt();
                morphs_.Reserve(numMorphs);
                for (unsigned i = 0; i < numMorphs; ++i)
                {
                    ModelMorph newMorph;

                    newMorph.name_ = source.ReadString();
                    newMorph.nameHash_ = newMorph.name_;
                    newMorph.weight_ = 0.0f;
                    unsigned numBuffers = source.ReadUInt();

                    for (unsigned j = 0; j < numBuffers; ++j)
                    {
                        VertexBufferMorph newBuffer;
                        unsigned bufferIndex = source.ReadUInt();

                        newBuffer.elementMask_ = source.ReadUInt();
                        newBuffer.vertexCount_ = source.ReadUInt();

                        // Base size: size of each vertex index
                        unsigned vertexSize = sizeof(unsigned);
                        // Add size of individual elements
                        if (newBuffer.elementMask_ & MASK_POSITION)
                            vertexSize += sizeof(Vector3);
                        if (newBuffer.elementMask_ & MASK_NORMAL)
                            vertexSize += sizeof(Vector3);
                        if (newBuffer.elementMask_ & MASK_TANGENT)
                            vertexSize += sizeof(Vector3);
                        newBuffer.dataSize_ = newBuffer.vertexCount_ * vertexSize;
                        newBuffer.morphData_ = new unsigned char[newBuffer.dataSize_];

                        source.Read(&newBuffer.morphData_[0], newBuffer.vertexCount_ * vertexSize);

                        newMorph.buffers_[bufferIndex] = newBuffer;
                        memoryUse += sizeof(VertexBufferMorph) + newBuffer.vertexCount_ * vertexSize;
                    }

                    morphs_.Push(newMorph);
                    memoryUse += sizeof(ModelMorph);
                }

                // Read skeleton
                skeleton_.Load(source);
                memoryUse += skeleton_.GetNumBones() * sizeof(Bone);*/

                // Read bounding box
                boundingBox_ = BoundingBox{ Vector3{-1, -1, -1}, Vector3{1, 1, 1} }; //source.ReadBoundingBox();

                // Read geometry centers
//                for (unsigned i = 0; i < geometries_.Size() && !source.IsEof(); ++i)
//                    geometryCenters_.Push(source.ReadVector3());
                while (geometryCenters_.Size() < geometries_.Size())
                    geometryCenters_.Push(Vector3::ZERO);
                memoryUse += sizeof(Vector3) * geometries_.Size();

                // Read metadata
                ResourceCache* cache = GetSubsystem<ResourceCache>();
                String xmlName = ReplaceExtension(GetName(), ".xml");
                SharedPtr<XMLFile> file(cache->GetTempResource<XMLFile>(xmlName, false));
                if (file)
                    LoadMetadataFromXML(file->GetRoot());

                SetMemoryUse(memoryUse);
                return true;
            }

            return false;
        }
        else
        {
            URHO3D_LOGERROR(source.GetName() + " is not a valid model file");
            return false;
        }
    }
    
    bool hasVertexDeclarations = (fileID == "UMD2");

    geometries_.Clear();
    geometryBoneMappings_.Clear();
    geometryCenters_.Clear();
    morphs_.Clear();
    vertexBuffers_.Clear();
    indexBuffers_.Clear();

    unsigned memoryUse = sizeof(Model);
    bool async = GetAsyncLoadState() == ASYNC_LOADING;

    // Read vertex buffers
    unsigned numVertexBuffers = source.ReadUInt();
    vertexBuffers_.Reserve(numVertexBuffers);
    morphRangeStarts_.Resize(numVertexBuffers);
    morphRangeCounts_.Resize(numVertexBuffers);
    loadVBData_.Resize(numVertexBuffers);
    for (unsigned i = 0; i < numVertexBuffers; ++i)
    {
        VertexBufferDesc& desc = loadVBData_[i];

        desc.vertexCount_ = source.ReadUInt();
        if (!hasVertexDeclarations)
        {
            unsigned elementMask = source.ReadUInt();
            desc.vertexElements_ = VertexBuffer::GetElements(elementMask);
        }
        else
        {
            desc.vertexElements_.Clear();
            unsigned numElements = source.ReadUInt();
            for (unsigned j = 0; j < numElements; ++j)
            {
                unsigned elementDesc = source.ReadUInt();
                VertexElementType type = (VertexElementType)(elementDesc & 0xff);
                VertexElementSemantic semantic = (VertexElementSemantic)((elementDesc >> 8) & 0xff);
                unsigned char index = (unsigned char)((elementDesc >> 16) & 0xff);
                desc.vertexElements_.Push(VertexElement(type, semantic, index));
            }
        }

        morphRangeStarts_[i] = source.ReadUInt();
        morphRangeCounts_[i] = source.ReadUInt();

        SharedPtr<VertexBuffer> buffer(new VertexBuffer(context_));
        unsigned vertexSize = VertexBuffer::GetVertexSize(desc.vertexElements_);
        desc.dataSize_ = desc.vertexCount_ * vertexSize;

        // Prepare vertex buffer data to be uploaded during EndLoad()
        if (async)
        {
            desc.data_ = new unsigned char[desc.dataSize_];
            source.Read(desc.data_.Get(), desc.dataSize_);
        }
        else
        {
            // If not async loading, use locking to avoid extra allocation & copy
            desc.data_.Reset(); // Make sure no previous data
            buffer->SetShadowed(true);
            buffer->SetSize(desc.vertexCount_, desc.vertexElements_);
            void* dest = buffer->Lock(0, desc.vertexCount_);
            source.Read(dest, desc.vertexCount_ * vertexSize);
            buffer->Unlock();
        }

        memoryUse += sizeof(VertexBuffer) + desc.vertexCount_ * vertexSize;
        vertexBuffers_.Push(buffer);
    }

    // Read index buffers
    unsigned numIndexBuffers = source.ReadUInt();
    indexBuffers_.Reserve(numIndexBuffers);
    loadIBData_.Resize(numIndexBuffers);
    for (unsigned i = 0; i < numIndexBuffers; ++i)
    {
        unsigned indexCount = source.ReadUInt();
        unsigned indexSize = source.ReadUInt();

        SharedPtr<IndexBuffer> buffer(new IndexBuffer(context_));

        // Prepare index buffer data to be uploaded during EndLoad()
        if (async)
        {
            loadIBData_[i].indexCount_ = indexCount;
            loadIBData_[i].indexSize_ = indexSize;
            loadIBData_[i].dataSize_ = indexCount * indexSize;
            loadIBData_[i].data_ = new unsigned char[loadIBData_[i].dataSize_];
            source.Read(loadIBData_[i].data_.Get(), loadIBData_[i].dataSize_);
        }
        else
        {
            // If not async loading, use locking to avoid extra allocation & copy
            loadIBData_[i].data_.Reset(); // Make sure no previous data
            buffer->SetShadowed(true);
            buffer->SetSize(indexCount, indexSize > sizeof(unsigned short));
            void* dest = buffer->Lock(0, indexCount);
            source.Read(dest, indexCount * indexSize);
            buffer->Unlock();
        }

        memoryUse += sizeof(IndexBuffer) + indexCount * indexSize;
        indexBuffers_.Push(buffer);
    }

    // Read geometries
    unsigned numGeometries = source.ReadUInt();
    geometries_.Reserve(numGeometries);
    geometryBoneMappings_.Reserve(numGeometries);
    geometryCenters_.Reserve(numGeometries);
    loadGeometries_.Resize(numGeometries);
    for (unsigned i = 0; i < numGeometries; ++i)
    {
        // Read bone mappings
        unsigned boneMappingCount = source.ReadUInt();
        PODVector<unsigned> boneMapping(boneMappingCount);
        for (unsigned j = 0; j < boneMappingCount; ++j)
            boneMapping[j] = source.ReadUInt();
        geometryBoneMappings_.Push(boneMapping);

        unsigned numLodLevels = source.ReadUInt();
        Vector<SharedPtr<Geometry> > geometryLodLevels;
        geometryLodLevels.Reserve(numLodLevels);
        loadGeometries_[i].Resize(numLodLevels);

        for (unsigned j = 0; j < numLodLevels; ++j)
        {
            float distance = source.ReadFloat();
            PrimitiveType type = (PrimitiveType)source.ReadUInt();

            unsigned vbRef = source.ReadUInt();
            unsigned ibRef = source.ReadUInt();
            unsigned indexStart = source.ReadUInt();
            unsigned indexCount = source.ReadUInt();

            if (vbRef >= vertexBuffers_.Size())
            {
                URHO3D_LOGERROR("Vertex buffer index out of bounds");
                loadVBData_.Clear();
                loadIBData_.Clear();
                loadGeometries_.Clear();
                return false;
            }
            if (ibRef >= indexBuffers_.Size())
            {
                URHO3D_LOGERROR("Index buffer index out of bounds");
                loadVBData_.Clear();
                loadIBData_.Clear();
                loadGeometries_.Clear();
                return false;
            }

            SharedPtr<Geometry> geometry(new Geometry(context_));
            geometry->SetLodDistance(distance);

            // Prepare geometry to be defined during EndLoad()
            loadGeometries_[i][j].type_ = type;
            loadGeometries_[i][j].vbRef_ = vbRef;
            loadGeometries_[i][j].ibRef_ = ibRef;
            loadGeometries_[i][j].indexStart_ = indexStart;
            loadGeometries_[i][j].indexCount_ = indexCount;

            geometryLodLevels.Push(geometry);
            memoryUse += sizeof(Geometry);
        }

        geometries_.Push(geometryLodLevels);
    }

    // Read morphs
    unsigned numMorphs = source.ReadUInt();
    morphs_.Reserve(numMorphs);
    for (unsigned i = 0; i < numMorphs; ++i)
    {
        ModelMorph newMorph;

        newMorph.name_ = source.ReadString();
        newMorph.nameHash_ = newMorph.name_;
        newMorph.weight_ = 0.0f;
        unsigned numBuffers = source.ReadUInt();

        for (unsigned j = 0; j < numBuffers; ++j)
        {
            VertexBufferMorph newBuffer;
            unsigned bufferIndex = source.ReadUInt();

            newBuffer.elementMask_ = source.ReadUInt();
            newBuffer.vertexCount_ = source.ReadUInt();

            // Base size: size of each vertex index
            unsigned vertexSize = sizeof(unsigned);
            // Add size of individual elements
            if (newBuffer.elementMask_ & MASK_POSITION)
                vertexSize += sizeof(Vector3);
            if (newBuffer.elementMask_ & MASK_NORMAL)
                vertexSize += sizeof(Vector3);
            if (newBuffer.elementMask_ & MASK_TANGENT)
                vertexSize += sizeof(Vector3);
            newBuffer.dataSize_ = newBuffer.vertexCount_ * vertexSize;
            newBuffer.morphData_ = new unsigned char[newBuffer.dataSize_];

            source.Read(&newBuffer.morphData_[0], newBuffer.vertexCount_ * vertexSize);

            newMorph.buffers_[bufferIndex] = newBuffer;
            memoryUse += sizeof(VertexBufferMorph) + newBuffer.vertexCount_ * vertexSize;
        }

        morphs_.Push(newMorph);
        memoryUse += sizeof(ModelMorph);
    }

    // Read skeleton
    skeleton_.Load(source);
    memoryUse += skeleton_.GetNumBones() * sizeof(Bone);

    // Read bounding box
    boundingBox_ = source.ReadBoundingBox();

    // Read geometry centers
    for (unsigned i = 0; i < geometries_.Size() && !source.IsEof(); ++i)
        geometryCenters_.Push(source.ReadVector3());
    while (geometryCenters_.Size() < geometries_.Size())
        geometryCenters_.Push(Vector3::ZERO);
    memoryUse += sizeof(Vector3) * geometries_.Size();

    // Read metadata
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    String xmlName = ReplaceExtension(GetName(), ".xml");
    SharedPtr<XMLFile> file(cache->GetTempResource<XMLFile>(xmlName, false));
    if (file)
        LoadMetadataFromXML(file->GetRoot());

    SetMemoryUse(memoryUse);
    return true;
}

bool Model::EndLoad()
{
    // Upload vertex buffer data
    for (unsigned i = 0; i < vertexBuffers_.Size(); ++i)
    {
        VertexBuffer* buffer = vertexBuffers_[i];
        VertexBufferDesc& desc = loadVBData_[i];
        if (desc.data_)
        {
            buffer->SetShadowed(true);
            buffer->SetSize(desc.vertexCount_, desc.vertexElements_);
            buffer->SetData(desc.data_.Get());
        }
    }

    // Upload index buffer data
    for (unsigned i = 0; i < indexBuffers_.Size(); ++i)
    {
        IndexBuffer* buffer = indexBuffers_[i];
        IndexBufferDesc& desc = loadIBData_[i];
        if (desc.data_)
        {
            buffer->SetShadowed(true);
            buffer->SetSize(desc.indexCount_, desc.indexSize_ > sizeof(unsigned short));
            buffer->SetData(desc.data_.Get());
        }
    }

    // Set up geometries
    for (unsigned i = 0; i < geometries_.Size(); ++i)
    {
        for (unsigned j = 0; j < geometries_[i].Size(); ++j)
        {
            Geometry* geometry = geometries_[i][j];
            GeometryDesc& desc = loadGeometries_[i][j];
            geometry->SetVertexBuffer(0, vertexBuffers_[desc.vbRef_]);
            geometry->SetIndexBuffer(indexBuffers_[desc.ibRef_]);
            geometry->SetDrawRange(desc.type_, desc.indexStart_, desc.indexCount_);
        }
    }

    loadVBData_.Clear();
    loadIBData_.Clear();
    loadGeometries_.Clear();
    return true;
}

bool Model::Save(Serializer& dest) const
{
    // Write ID
    if (!dest.WriteFileID("UMD2"))
        return false;

    // Write vertex buffers
    dest.WriteUInt(vertexBuffers_.Size());
    for (unsigned i = 0; i < vertexBuffers_.Size(); ++i)
    {
        VertexBuffer* buffer = vertexBuffers_[i];
        dest.WriteUInt(buffer->GetVertexCount());
        const PODVector<VertexElement>& elements = buffer->GetElements();
        dest.WriteUInt(elements.Size());
        for (unsigned j = 0; j < elements.Size(); ++j)
        {
            unsigned elementDesc = ((unsigned)elements[j].type_) |
                (((unsigned)elements[j].semantic_) << 8) |
                (((unsigned)elements[j].index_) << 16);
            dest.WriteUInt(elementDesc);
        }
        dest.WriteUInt(morphRangeStarts_[i]);
        dest.WriteUInt(morphRangeCounts_[i]);
        dest.Write(buffer->GetShadowData(), buffer->GetVertexCount() * buffer->GetVertexSize());
    }
    // Write index buffers
    dest.WriteUInt(indexBuffers_.Size());
    for (unsigned i = 0; i < indexBuffers_.Size(); ++i)
    {
        IndexBuffer* buffer = indexBuffers_[i];
        dest.WriteUInt(buffer->GetIndexCount());
        dest.WriteUInt(buffer->GetIndexSize());
        dest.Write(buffer->GetShadowData(), buffer->GetIndexCount() * buffer->GetIndexSize());
    }
    // Write geometries
    dest.WriteUInt(geometries_.Size());
    for (unsigned i = 0; i < geometries_.Size(); ++i)
    {
        // Write bone mappings
        dest.WriteUInt(geometryBoneMappings_[i].Size());
        for (unsigned j = 0; j < geometryBoneMappings_[i].Size(); ++j)
            dest.WriteUInt(geometryBoneMappings_[i][j]);

        // Write the LOD levels
        dest.WriteUInt(geometries_[i].Size());
        for (unsigned j = 0; j < geometries_[i].Size(); ++j)
        {
            Geometry* geometry = geometries_[i][j];
            dest.WriteFloat(geometry->GetLodDistance());
            dest.WriteUInt(geometry->GetPrimitiveType());
            dest.WriteUInt(LookupVertexBuffer(geometry->GetVertexBuffer(0), vertexBuffers_));
            dest.WriteUInt(LookupIndexBuffer(geometry->GetIndexBuffer(), indexBuffers_));
            dest.WriteUInt(geometry->GetIndexStart());
            dest.WriteUInt(geometry->GetIndexCount());
        }
    }

    // Write morphs
    dest.WriteUInt(morphs_.Size());
    for (unsigned i = 0; i < morphs_.Size(); ++i)
    {
        dest.WriteString(morphs_[i].name_);
        dest.WriteUInt(morphs_[i].buffers_.Size());

        // Write morph vertex buffers
        for (HashMap<unsigned, VertexBufferMorph>::ConstIterator j = morphs_[i].buffers_.Begin();
             j != morphs_[i].buffers_.End(); ++j)
        {
            dest.WriteUInt(j->first_);
            dest.WriteUInt(j->second_.elementMask_);
            dest.WriteUInt(j->second_.vertexCount_);

            // Base size: size of each vertex index
            unsigned vertexSize = sizeof(unsigned);
            // Add size of individual elements
            if (j->second_.elementMask_ & MASK_POSITION)
                vertexSize += sizeof(Vector3);
            if (j->second_.elementMask_ & MASK_NORMAL)
                vertexSize += sizeof(Vector3);
            if (j->second_.elementMask_ & MASK_TANGENT)
                vertexSize += sizeof(Vector3);

            dest.Write(j->second_.morphData_.Get(), vertexSize * j->second_.vertexCount_);
        }
    }

    // Write skeleton
    skeleton_.Save(dest);

    // Write bounding box
    dest.WriteBoundingBox(boundingBox_);

    // Write geometry centers
    for (unsigned i = 0; i < geometryCenters_.Size(); ++i)
        dest.WriteVector3(geometryCenters_[i]);

    // Write metadata
    if (HasMetadata())
    {
        File* destFile = dynamic_cast<File*>(&dest);
        if (destFile)
        {
            String xmlName = ReplaceExtension(destFile->GetName(), ".xml");

            SharedPtr<XMLFile> xml(new XMLFile(context_));
            XMLElement rootElem = xml->CreateRoot("model");
            SaveMetadataToXML(rootElem);

            File xmlFile(context_, xmlName, FILE_WRITE);
            xml->Save(xmlFile);
        }
        else
            URHO3D_LOGWARNING("Can not save model metadata when not saving into a file");
    }

    return true;
}

void Model::SetBoundingBox(const BoundingBox& box)
{
    boundingBox_ = box;
}

bool Model::SetVertexBuffers(const Vector<SharedPtr<VertexBuffer> >& buffers, const PODVector<unsigned>& morphRangeStarts,
    const PODVector<unsigned>& morphRangeCounts)
{
    for (unsigned i = 0; i < buffers.Size(); ++i)
    {
        if (!buffers[i])
        {
            URHO3D_LOGERROR("Null model vertex buffers specified");
            return false;
        }
        if (!buffers[i]->IsShadowed())
        {
            URHO3D_LOGERROR("Model vertex buffers must be shadowed");
            return false;
        }
    }

    vertexBuffers_ = buffers;
    morphRangeStarts_.Resize(buffers.Size());
    morphRangeCounts_.Resize(buffers.Size());

    // If morph ranges are not specified for buffers, assume to be zero
    for (unsigned i = 0; i < buffers.Size(); ++i)
    {
        morphRangeStarts_[i] = i < morphRangeStarts.Size() ? morphRangeStarts[i] : 0;
        morphRangeCounts_[i] = i < morphRangeCounts.Size() ? morphRangeCounts[i] : 0;
    }

    return true;
}

bool Model::SetIndexBuffers(const Vector<SharedPtr<IndexBuffer> >& buffers)
{
    for (unsigned i = 0; i < buffers.Size(); ++i)
    {
        if (!buffers[i])
        {
            URHO3D_LOGERROR("Null model index buffers specified");
            return false;
        }
        if (!buffers[i]->IsShadowed())
        {
            URHO3D_LOGERROR("Model index buffers must be shadowed");
            return false;
        }
    }

    indexBuffers_ = buffers;
    return true;
}

void Model::SetNumGeometries(unsigned num)
{
    geometries_.Resize(num);
    geometryBoneMappings_.Resize(num);
    geometryCenters_.Resize(num);

    // For easier creation of from-scratch geometry, ensure that all geometries start with at least 1 LOD level (0 makes no sense)
    for (unsigned i = 0; i < geometries_.Size(); ++i)
    {
        if (geometries_[i].Empty())
            geometries_[i].Resize(1);
    }
}

bool Model::SetNumGeometryLodLevels(unsigned index, unsigned num)
{
    if (index >= geometries_.Size())
    {
        URHO3D_LOGERROR("Geometry index out of bounds");
        return false;
    }
    if (!num)
    {
        URHO3D_LOGERROR("Zero LOD levels not allowed");
        return false;
    }

    geometries_[index].Resize(num);
    return true;
}

bool Model::SetGeometry(unsigned index, unsigned lodLevel, Geometry* geometry)
{
    if (index >= geometries_.Size())
    {
        URHO3D_LOGERROR("Geometry index out of bounds");
        return false;
    }
    if (lodLevel >= geometries_[index].Size())
    {
        URHO3D_LOGERROR("LOD level index out of bounds");
        return false;
    }

    geometries_[index][lodLevel] = geometry;
    return true;
}

bool Model::SetGeometryCenter(unsigned index, const Vector3& center)
{
    if (index >= geometryCenters_.Size())
    {
        URHO3D_LOGERROR("Geometry index out of bounds");
        return false;
    }

    geometryCenters_[index] = center;
    return true;
}

void Model::SetSkeleton(const Skeleton& skeleton)
{
    skeleton_ = skeleton;
}

void Model::SetGeometryBoneMappings(const Vector<PODVector<unsigned> >& geometryBoneMappings)
{
    geometryBoneMappings_ = geometryBoneMappings;
}

void Model::SetMorphs(const Vector<ModelMorph>& morphs)
{
    morphs_ = morphs;
}

SharedPtr<Model> Model::Clone(const String& cloneName) const
{
    SharedPtr<Model> ret(new Model(context_));

    ret->SetName(cloneName);
    ret->boundingBox_ = boundingBox_;
    ret->skeleton_ = skeleton_;
    ret->geometryBoneMappings_ = geometryBoneMappings_;
    ret->geometryCenters_ = geometryCenters_;
    ret->morphs_ = morphs_;
    ret->morphRangeStarts_ = morphRangeStarts_;
    ret->morphRangeCounts_ = morphRangeCounts_;

    // Deep copy vertex/index buffers
    HashMap<VertexBuffer*, VertexBuffer*> vbMapping;
    for (Vector<SharedPtr<VertexBuffer> >::ConstIterator i = vertexBuffers_.Begin(); i != vertexBuffers_.End(); ++i)
    {
        VertexBuffer* origBuffer = *i;
        SharedPtr<VertexBuffer> cloneBuffer;

        if (origBuffer)
        {
            cloneBuffer = new VertexBuffer(context_);
            cloneBuffer->SetSize(origBuffer->GetVertexCount(), origBuffer->GetElementMask(), origBuffer->IsDynamic());
            cloneBuffer->SetShadowed(origBuffer->IsShadowed());
            if (origBuffer->IsShadowed())
                cloneBuffer->SetData(origBuffer->GetShadowData());
            else
            {
                void* origData = origBuffer->Lock(0, origBuffer->GetVertexCount());
                if (origData)
                    cloneBuffer->SetData(origData);
                else
                    URHO3D_LOGERROR("Failed to lock original vertex buffer for copying");
            }
            vbMapping[origBuffer] = cloneBuffer;
        }

        ret->vertexBuffers_.Push(cloneBuffer);
    }

    HashMap<IndexBuffer*, IndexBuffer*> ibMapping;
    for (Vector<SharedPtr<IndexBuffer> >::ConstIterator i = indexBuffers_.Begin(); i != indexBuffers_.End(); ++i)
    {
        IndexBuffer* origBuffer = *i;
        SharedPtr<IndexBuffer> cloneBuffer;

        if (origBuffer)
        {
            cloneBuffer = new IndexBuffer(context_);
            cloneBuffer->SetSize(origBuffer->GetIndexCount(), origBuffer->GetIndexSize() == sizeof(unsigned),
                origBuffer->IsDynamic());
            cloneBuffer->SetShadowed(origBuffer->IsShadowed());
            if (origBuffer->IsShadowed())
                cloneBuffer->SetData(origBuffer->GetShadowData());
            else
            {
                void* origData = origBuffer->Lock(0, origBuffer->GetIndexCount());
                if (origData)
                    cloneBuffer->SetData(origData);
                else
                    URHO3D_LOGERROR("Failed to lock original index buffer for copying");
            }
            ibMapping[origBuffer] = cloneBuffer;
        }

        ret->indexBuffers_.Push(cloneBuffer);
    }

    // Deep copy all the geometry LOD levels and refer to the copied vertex/index buffers
    ret->geometries_.Resize(geometries_.Size());
    for (unsigned i = 0; i < geometries_.Size(); ++i)
    {
        ret->geometries_[i].Resize(geometries_[i].Size());
        for (unsigned j = 0; j < geometries_[i].Size(); ++j)
        {
            SharedPtr<Geometry> cloneGeometry;
            Geometry* origGeometry = geometries_[i][j];

            if (origGeometry)
            {
                cloneGeometry = new Geometry(context_);
                cloneGeometry->SetIndexBuffer(ibMapping[origGeometry->GetIndexBuffer()]);
                unsigned numVbs = origGeometry->GetNumVertexBuffers();
                for (unsigned k = 0; k < numVbs; ++k)
                {
                    cloneGeometry->SetVertexBuffer(k, vbMapping[origGeometry->GetVertexBuffer(k)]);
                }
                cloneGeometry->SetDrawRange(origGeometry->GetPrimitiveType(), origGeometry->GetIndexStart(),
                    origGeometry->GetIndexCount(), origGeometry->GetVertexStart(), origGeometry->GetVertexCount(), false);
                cloneGeometry->SetLodDistance(origGeometry->GetLodDistance());
            }

            ret->geometries_[i][j] = cloneGeometry;
        }
    }


    // Deep copy the morph data (if any) to allow modifying it
    for (Vector<ModelMorph>::Iterator i = ret->morphs_.Begin(); i != ret->morphs_.End(); ++i)
    {
        ModelMorph& morph = *i;
        for (HashMap<unsigned, VertexBufferMorph>::Iterator j = morph.buffers_.Begin(); j != morph.buffers_.End(); ++j)
        {
            VertexBufferMorph& vbMorph = j->second_;
            if (vbMorph.dataSize_)
            {
                SharedArrayPtr<unsigned char> cloneData(new unsigned char[vbMorph.dataSize_]);
                memcpy(cloneData.Get(), vbMorph.morphData_.Get(), vbMorph.dataSize_);
                vbMorph.morphData_ = cloneData;
            }
        }
    }

    ret->SetMemoryUse(GetMemoryUse());

    return ret;
}

unsigned Model::GetNumGeometryLodLevels(unsigned index) const
{
    return index < geometries_.Size() ? geometries_[index].Size() : 0;
}

Geometry* Model::GetGeometry(unsigned index, unsigned lodLevel) const
{
    if (index >= geometries_.Size() || geometries_[index].Empty())
        return 0;

    if (lodLevel >= geometries_[index].Size())
        lodLevel = geometries_[index].Size() - 1;

    return geometries_[index][lodLevel];
}

const ModelMorph* Model::GetMorph(unsigned index) const
{
    return index < morphs_.Size() ? &morphs_[index] : 0;
}

const ModelMorph* Model::GetMorph(const String& name) const
{
    return GetMorph(StringHash(name));
}

const ModelMorph* Model::GetMorph(StringHash nameHash) const
{
    for (Vector<ModelMorph>::ConstIterator i = morphs_.Begin(); i != morphs_.End(); ++i)
    {
        if (i->nameHash_ == nameHash)
            return &(*i);
    }

    return 0;
}

unsigned Model::GetMorphRangeStart(unsigned bufferIndex) const
{
    return bufferIndex < vertexBuffers_.Size() ? morphRangeStarts_[bufferIndex] : 0;
}

unsigned Model::GetMorphRangeCount(unsigned bufferIndex) const
{
    return bufferIndex < vertexBuffers_.Size() ? morphRangeCounts_[bufferIndex] : 0;
}

}
