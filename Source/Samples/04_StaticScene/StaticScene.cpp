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

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/Graphics/Technique.h>
#include <Urho3D/Graphics/Geometry.h>
#include <Urho3D/Graphics/VertexBuffer.h>
#include <Urho3D/Graphics/IndexBuffer.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/UI.h>

#include "../Gltf/GltfHelper.h"

#include "StaticScene.h"
#include "VideoPlayer.h"

#include <Urho3D/DebugNew.h>

#include <tiny_gltf.h>

URHO3D_DEFINE_APPLICATION_MAIN(StaticScene)

StaticScene::StaticScene(Context* context) :
    Sample(context)
{
}

void StaticScene::Start()
{
    // Execute base class startup
    Sample::Start();

    // Create the scene content
    CreateScene();

    // Create the UI content
    CreateInstructions();

    // Setup the viewport for displaying the scene
    SetupViewport();

    // Hook up to the frame update events
    SubscribeToEvents();

    // Set the mouse mode to use in the sample
    Sample::InitMouseMode(MM_RELATIVE);
}

void StaticScene::LoadNode(Node& parent, tinygltf::Model& gltfModel, const int parentId)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();

    auto gltfNode = gltfModel.nodes.at(parentId);

    if (gltfNode.mesh != -1) {
        auto mesh = gltfModel.meshes.at(gltfNode.mesh);
        auto primitive = mesh.primitives.at(0); //TODO: handle case with more than one primitive

        if (mesh.name == "technic_driver_head_p_SOLIDS_1") {
            auto thing = mesh.primitives.at(1);
        }
    }    

    auto node = parent.CreateChild();

    // Add the current gltf node to the urho node.
    if (gltfNode.matrix.size() == 16)
    {
        Matrix3x4 matrix;
        matrix = Matrix3x4::IDENTITY;
        matrix.m00_ = gltfNode.matrix.at(0);
        matrix.m01_ = gltfNode.matrix.at(4);
        matrix.m02_ = gltfNode.matrix.at(8);
        matrix.m03_ = gltfNode.matrix.at(12);
        matrix.m10_ = gltfNode.matrix.at(1);
        matrix.m11_ = gltfNode.matrix.at(5);
        matrix.m12_ = gltfNode.matrix.at(9);
        matrix.m13_ = gltfNode.matrix.at(13);
        matrix.m20_ = gltfNode.matrix.at(2);
        matrix.m21_ = gltfNode.matrix.at(6);
        matrix.m22_ = gltfNode.matrix.at(10);
        matrix.m23_ = gltfNode.matrix.at(14);

        Vector3 position;
        Vector3 scale;
        Quaternion rotation;

        matrix.Decompose(position, rotation, scale);
        
        position.z_ = -position.z_;

        rotation.z_ = -rotation.z_;
        rotation.w_ = -rotation.w_;

        matrix.SetRotation(rotation.RotationMatrix());
        matrix.SetTranslation(position);

        node->SetTransform(matrix);
    }
    else
    {
        Vector3 position = Vector3::ZERO;
        Vector3 scale = Vector3::ONE;
        Quaternion rotation;

        if (gltfNode.translation.size() == 3) // Need to check, some nodes may not have any transform defined at all
        {
            // Need to flip the z-axis to convert from the right-handed system.
            //node->SetPosition(Vector3(gltfNode.translation.at(0), gltfNode.translation.at(1), gltfNode.translation.at(2)));
            position = (Vector3(gltfNode.translation.at(0), gltfNode.translation.at(1), -gltfNode.translation.at(2)));
        }
        if (gltfNode.scale.size() == 3) // Need to check, some nodes may not have any transform defined at all
        {
            //node->SetScale(Vector3(gltfNode.scale.at(0), gltfNode.scale.at(1), gltfNode.scale.at(2)));
            scale = (Vector3(gltfNode.scale.at(0), gltfNode.scale.at(1), gltfNode.scale.at(2)));
        }
        if (gltfNode.rotation.size() == 4) // Need to check, some nodes may not have any transform defined at all
        {
            // TODO: Figure out the correct rotation change for right to left handed.
            //node->SetRotation(Quaternion(gltfNode.rotation.at(3), gltfNode.rotation.at(0), gltfNode.rotation.at(1), gltfNode.rotation.at(2)));
            rotation = (Quaternion(-gltfNode.rotation.at(3), gltfNode.rotation.at(0), gltfNode.rotation.at(1), -gltfNode.rotation.at(2)));
        }

        Matrix3x4 matrix;
        matrix = Matrix3x4::IDENTITY;
        matrix.SetRotation(rotation.RotationMatrix().Scaled(scale));
        matrix.SetTranslation(position);

        node->SetTransform(matrix);
    }

    // Add the mesh if there is one.
    if (gltfNode.mesh != -1)
    {
        auto staticModel = node->CreateComponent<StaticModel>();

        SharedPtr<Model> model(new Model(context_));
       
        // Now load the vertex buffer for this model
        auto mesh = gltfModel.meshes.at(gltfNode.mesh);
        
        model->SetNumGeometries(mesh.primitives.size());
        int geometryIndex = 0;

        std::vector<SharedPtr<Material>> materials;

        for (auto gltfPrimitive : mesh.primitives)
        {
            SharedPtr<VertexBuffer> vb(new VertexBuffer(context_));
            SharedPtr<IndexBuffer> ib(new IndexBuffer(context_));
            SharedPtr<Geometry> geom(new Geometry(context_));

            auto helperPrimitive = GltfHelper::ReadPrimitive(gltfModel, gltfPrimitive);

            const auto numVertices = helperPrimitive.Vertices.size();
            const auto numIndices = helperPrimitive.Indices.size();
            std::vector<float> vertices;
            vertices.resize(12 * numVertices);
            auto count = 0;

            for (auto vertex : helperPrimitive.Vertices)
            {
                vertices[count++] = vertex.Position.x;
                vertices[count++] = vertex.Position.y;
                vertices[count++] = -vertex.Position.z;
                vertices[count++] = vertex.Normal.x;
                vertices[count++] = vertex.Normal.y;
                vertices[count++] = -vertex.Normal.z;
                vertices[count++] = vertex.Tangent.x;
                vertices[count++] = vertex.Tangent.y;
                vertices[count++] = -vertex.Tangent.z;
                vertices[count++] = vertex.Tangent.w;
                vertices[count++] = -vertex.TexCoord0.x;
                vertices[count++] = vertex.TexCoord0.y;
            }

            float* vertexData = (float*)&vertices[0];

            // Need to change winding order on the indices
            auto indexData = &helperPrimitive.Indices[0];

            for (int i = 0; i < numIndices; i += 3)
            {
                std::swap(indexData[i], indexData[i + 1]);
            }

            vb->SetShadowed(true);
            PODVector<VertexElement> elements;
            elements.Push(VertexElement(TYPE_VECTOR3, SEM_POSITION));
            elements.Push(VertexElement(TYPE_VECTOR3, SEM_NORMAL));
            elements.Push(VertexElement(TYPE_VECTOR2, SEM_TEXCOORD));
            elements.Push(VertexElement(TYPE_VECTOR4, SEM_TANGENT));

            vb->SetSize(numVertices, elements);
            vb->SetData((void*)vertexData);

            // Then load the index buffer for this
            ib->SetShadowed(true);
            ib->SetSize(numIndices, true);
            ib->SetData(indexData);

            geom->SetVertexBuffer(0, vb);
            geom->SetIndexBuffer(ib);
            geom->SetDrawRange(TRIANGLE_LIST, 0, numIndices);

            model->SetGeometry(geometryIndex, 0, geom);

            // The bounding box should be set by the min max property on the position accessor
            // TODO: This may not be right yet.
            auto accessorIndex = gltfPrimitive.attributes.at("POSITION");
            auto posAccessor = gltfModel.accessors.at(accessorIndex);
            Vector3 maximum(posAccessor.maxValues.at(0), posAccessor.maxValues.at(1), -posAccessor.maxValues.at(2));
            Vector3 minimum(posAccessor.minValues.at(0), posAccessor.minValues.at(1), -posAccessor.minValues.at(2));

            model->SetBoundingBox(BoundingBox(minimum, maximum));

            SharedPtr<Material> material(new Material(context_));

            material->SetTechnique(0, cache->GetResource<Technique>("Techniques/NoTexture.xml"));
            auto matIndex = gltfPrimitive.material;
            auto mat = gltfModel.materials.at(matIndex);
            
            if (mat.values["baseColorFactor"].number_array.size() > 1) {
                auto baseColor = mat.values["baseColorFactor"].number_array;
                // Write the material to a list, apply them all after the loop
                material->SetShaderParameter("MatDiffColor", Vector4(baseColor.at(0), baseColor.at(1), baseColor.at(2), 1));
            }
            
            const auto &gltfTextureIt = mat.values.find("baseColorTexture");
            if (gltfTextureIt != std::end(mat.values))
            {
                const int textureIndex = (int)gltfTextureIt->second.json_double_value.at("index");
                
                const tinygltf::Texture& gltfTexture = gltfModel.textures.at(textureIndex);
                if (gltfTexture.source != -1)
                {
                    auto image = gltfModel.images.at(gltfTexture.source);
                    std::string path = std::string("Models/Avocado/").append(image.uri);

                    auto texture = cache->GetResource<Texture2D>(String(path.c_str()));
                    material->SetTechnique(0, cache->GetResource<Technique>("Techniques/Diff.xml"));
                    material->SetTexture(TextureUnit::TU_DIFFUSE, texture);
                }

                
            }
            
            
            materials.push_back(material);

            geometryIndex++;
        }
        
        staticModel->SetModel(model);
        for (int i = 0; i < materials.size(); i++) 
        {
            staticModel->SetMaterial(i, materials[i]);
        }
    }
    
    // Then recurse on the child nodes.
    for (auto nodeId : gltfNode.children)
    {
        LoadNode(*node, gltfModel, nodeId);
    }
}

void StaticScene::CreateScene()
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();

    scene_ = new Scene(context_);

    // Create the Octree component to the scene. This is required before adding any drawable components, or else nothing will
    // show up. The default octree volume will be from (-1000, -1000, -1000) to (1000, 1000, 1000) in world coordinates; it
    // is also legal to place objects outside the volume but their visibility can then not be checked in a hierarchically
    // optimizing manner
    scene_->CreateComponent<Octree>();

    tinygltf::Model gltfModel;
    std::string errorMessage;
    tinygltf::TinyGLTF loader;

    // Screen
    {
        Node* screenNode = scene_->CreateChild("Screen");
        screenNode->SetPosition(Vector3(0.0f, 10.0f, -0.27f));
        screenNode->SetRotation(Quaternion(-90.0f, 0.0f, 0.0f));
        screenNode->SetScale(Vector3(26.67f, 0.0f, 15.0f));
        StaticModel* screenObject = screenNode->CreateComponent<StaticModel>();
        screenObject->SetModel(cache->GetResource<Model>("Models/Plane.mdl"));

        VideoPlayer* videoPlayer = new VideoPlayer(context_);
        screenNode->AddComponent(videoPlayer, 1234, CreateMode::LOCAL);
        videoPlayer->SetSource("C:\\Users\\Owner\\Development\\Urho3D\\bin\\Data\\Videos\\Bunny.mp4");
        videoPlayer->SetGenerateMipmaps(true);
        videoPlayer->Play();

        SharedPtr<Material> screenMaterial(new Material(context_));
        screenMaterial->SetTechnique(0, cache->GetResource<Technique>("Techniques/DiffUnlit.xml"));
        screenMaterial->SetTexture(TU_DIFFUSE, videoPlayer->GetTexture());
        screenMaterial->SetDepthBias(BiasParameters(-0.00001f, 0.0f));
        screenObject->SetMaterial(screenMaterial); //cache->GetResource<Material>("Materials/Mushroom.xml"));
    }

    bool isLoaded = loader.LoadASCIIFromFile(&gltfModel, &errorMessage, std::string("C:/Users/Owner/Development/Urho3D/bin/Data/Models/Avocado/Avocado.gltf"));
    if (!isLoaded)
    {
        const auto msg = "Failed to load gltf model" + errorMessage;
        throw std::exception(msg.c_str());
    }

    // Do loading of the tank, most other models will have recursive structure, the tank doesn't.
    Node* modelRoot = scene_->CreateChild("tankroot");
    modelRoot->SetPosition(Vector3(0.0f, 0.0f, 3.0f));
    modelRoot->SetScale(1);

    const int defaultSceneId = (gltfModel.defaultScene == -1) ? 0 : gltfModel.defaultScene;
    const tinygltf::Scene& defaultScene = gltfModel.scenes.at(defaultSceneId);

    // Process the root scene nodes. The children will be processed recursively.
    for (const int rootNodeId : defaultScene.nodes)
    {
        LoadNode(*modelRoot, gltfModel, rootNodeId);
    }

    /**
    for (auto gltfNode : gltfModel.nodes)
    {
        if (gltfNode.mesh == -1)
        {
            continue;
        }

        Node* node = modelRoot->CreateChild(String(gltfNode.name.c_str()));

        if (gltfNode.matrix.size() == 16)
        {
            Matrix3x4 matrix;
            matrix.m00_ = gltfNode.matrix.at(0);
            matrix.m01_ = gltfNode.matrix.at(1);
            matrix.m02_ = gltfNode.matrix.at(2);
            matrix.m03_ = gltfNode.matrix.at(3);
            matrix.m10_ = gltfNode.matrix.at(4);
            matrix.m11_ = gltfNode.matrix.at(5);
            matrix.m12_ = gltfNode.matrix.at(6);
            matrix.m13_ = gltfNode.matrix.at(7);
            matrix.m20_ = gltfNode.matrix.at(8);
            matrix.m21_ = gltfNode.matrix.at(9);
            matrix.m22_ = gltfNode.matrix.at(10);
            matrix.m23_ = gltfNode.matrix.at(11);
            
            node->SetTransform(matrix);
        }
        else if (gltfNode.translation.size() == 3) // Need to check, some nodes may not have any transform defined at all
        {
            // Need to flip the z-axis to convert from the right-handed system.
            node->SetPosition(Vector3(gltfNode.translation.at(0), gltfNode.translation.at(1), -gltfNode.translation.at(2)));
            node->SetScale(Vector3(gltfNode.scale.at(0), gltfNode.scale.at(1), gltfNode.scale.at(2)));
            // TODO: Figure out the correct rotation change for right to left handed.
            node->SetRotation(Quaternion(gltfNode.rotation.at(3), gltfNode.rotation.at(0), gltfNode.rotation.at(1), gltfNode.rotation.at(2)));
        } 

        auto staticModel = node->CreateComponent<StaticModel>();
        
        SharedPtr<Model> model(new Model(context_));
        SharedPtr<VertexBuffer> vb(new VertexBuffer(context_));
        SharedPtr<IndexBuffer> ib(new IndexBuffer(context_));
        SharedPtr<Geometry> geom(new Geometry(context_));

        // Now load the vertex buffer for this model
        auto mesh = gltfModel.meshes.at(gltfNode.mesh);
        auto primitive = mesh.primitives.at(0); //TODO: handle case with more than one primitive

        if (mesh.primitives.size() > 1) {
            auto thing = mesh.primitives.at(1);
        }

        auto helperPrimitive = GltfHelper::ReadPrimitive(gltfModel, primitive);

        const auto numVertices = helperPrimitive.Vertices.size();
        const auto numIndices = helperPrimitive.Indices.size();
        std::vector<float> vertices;
        vertices.resize(12 * numVertices);
        auto count = 0;

        // TODO: Instead of loading the vertices from the helper primitive, load them from the buffer directly
        // primitives contain the index of the accessor that reads the data.
        
        for (auto vertex : helperPrimitive.Vertices)
        {
            vertices[count++] = vertex.Position.x;
            vertices[count++] = vertex.Position.y;
            vertices[count++] = vertex.Position.z;
            vertices[count++] = vertex.Normal.x;
            vertices[count++] = vertex.Normal.y;
            vertices[count++] = vertex.Normal.z;
            vertices[count++] = vertex.Tangent.x;
            vertices[count++] = vertex.Tangent.y;
            vertices[count++] = vertex.Tangent.z;
            vertices[count++] = vertex.Tangent.w;
            vertices[count++] = vertex.TexCoord0.x;
            vertices[count++] = vertex.TexCoord0.y;
        }

        float* vertexData = (float*)&vertices[0];

        auto indexData = &helperPrimitive.Indices[0];

        vb->SetShadowed(true);
        PODVector<VertexElement> elements;
        elements.Push(VertexElement(TYPE_VECTOR3, SEM_POSITION));
        elements.Push(VertexElement(TYPE_VECTOR3, SEM_NORMAL));
        elements.Push(VertexElement(TYPE_VECTOR2, SEM_TEXCOORD));
        elements.Push(VertexElement(TYPE_VECTOR4, SEM_TANGENT));
        
        vb->SetSize(numVertices, elements);
        vb->SetData((void*)vertexData);

        // Then load the index buffer for this
        ib->SetShadowed(true);
        ib->SetSize(numIndices, true);
        ib->SetData(indexData);

        geom->SetVertexBuffer(0, vb);
        geom->SetIndexBuffer(ib);
        geom->SetDrawRange(TRIANGLE_LIST, 0, numIndices);

        model->SetNumGeometries(1);
        model->SetGeometry(0, 0, geom);

        // The bounding box should be set by the min max property on the position accessor


        model->SetBoundingBox(BoundingBox(Vector3(-2.0f, -2.0f, -2.0f), Vector3(2.5f, 2.5f, 2.5f)));

        staticModel->SetModel(model);
        
        SharedPtr<Material> material(new Material(context_));

        material->SetTechnique(0, cache->GetResource<Technique>("Techniques/NoTexture.xml"));
        auto matIndex = mesh.primitives.at(0).material;
        auto mat = gltfModel.materials.at(matIndex);
        auto baseColor = mat.values["baseColorFactor"].number_array;
        
        material->SetShaderParameter("MatDiffColor", Vector4(baseColor.at(0), baseColor.at(1), baseColor.at(2), 1));

        staticModel->SetMaterial(material);

    }
    */

    // Create a child scene node (at world origin) and a StaticModel component into it. Set the StaticModel to show a simple
    // plane mesh with a "stone" material. Note that naming the scene nodes is optional. Scale the scene node larger
    // (100 x 100 world units)
    Node* planeNode = scene_->CreateChild("Plane");
    planeNode->SetScale(Vector3(100.0f, 1.0f, 100.0f));
    StaticModel* planeObject = planeNode->CreateComponent<StaticModel>();
    //planeObject->SetModel(cache->GetResource<Model>("Models/Plane.mdl"));
    //planeObject->SetMaterial(cache->GetResource<Material>("Materials/StoneTiled.xml"));

    // Create a directional light to the world so that we can see something. The light scene node's orientation controls the
    // light direction; we will use the SetDirection() function which calculates the orientation from a forward direction vector.
    // The light will use default settings (white light, no shadows)
    Node* lightNode = scene_->CreateChild("DirectionalLight");
    lightNode->SetDirection(Vector3(0.6f, -1.0f, 0.8f)); // The direction vector does not need to be normalized
    Light* light = lightNode->CreateComponent<Light>();
    light->SetLightType(LIGHT_DIRECTIONAL);
    light->SetBrightness(2);

    lightNode = scene_->CreateChild("DirectionalLight");
    lightNode->SetDirection(Vector3(-0.6f, 1.0f, -0.8f)); // The direction vector does not need to be normalized
    light = lightNode->CreateComponent<Light>();
    light->SetLightType(LIGHT_DIRECTIONAL);

    // Create more StaticModel objects to the scene, randomly positioned, rotated and scaled. For rotation, we construct a
    // quaternion from Euler angles where the Y angle (rotation about the Y axis) is randomized. The mushroom model contains
    // LOD levels, so the StaticModel component will automatically select the LOD level according to the view distance (you'll
    // see the model get simpler as it moves further away). Finally, rendering a large number of the same object with the
    // same material allows instancing to be used, if the GPU supports it. This reduces the amount of CPU work in rendering the
    // scene.
    const unsigned NUM_OBJECTS = 0;
    for (unsigned i = 0; i < NUM_OBJECTS; ++i)
    {
        Node* mushroomNode = scene_->CreateChild("Mushroom");
        mushroomNode->SetPosition(Vector3(0.0f, 0.0f, 3.0f));
        mushroomNode->SetRotation(Quaternion(0.0f, Random(360.0f), 0.0f));
        mushroomNode->SetScale(0.02f); // + Random(2.0f));
        StaticModel* mushroomObject = mushroomNode->CreateComponent<StaticModel>();
        mushroomObject->SetModel(cache->GetResource<Model>("Models/TAPV/TAPV.gltf"));
        mushroomObject->ApplyGltfMaterials("Models/TAPV/TAPV.gltf");
        //mushroomObject->SetMaterial(cache->GetResource<Material>("Models/Avocado/Avocado.gltf"));
    }

    // Create a scene node for the camera, which we will move around
    // The camera will use default settings (1000 far clip distance, 45 degrees FOV, set aspect ratio automatically)
    cameraNode_ = scene_->CreateChild("Camera");
    cameraNode_->CreateComponent<Camera>();

    // Set an initial position for the camera scene node above the plane
    cameraNode_->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
}

void StaticScene::CreateInstructions()
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    UI* ui = GetSubsystem<UI>();

    // Construct new Text object, set string to display and font to use
    Text* instructionText = ui->GetRoot()->CreateChild<Text>();
    instructionText->SetText("Use WASD keys and mouse/touch to move");
    instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);

    // Position the text relative to the screen center
    instructionText->SetHorizontalAlignment(HA_CENTER);
    instructionText->SetVerticalAlignment(VA_CENTER);
    instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 4);
}

void StaticScene::SetupViewport()
{
    Renderer* renderer = GetSubsystem<Renderer>();

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen. We need to define the scene and the camera
    // at minimum. Additionally we could configure the viewport screen size and the rendering path (eg. forward / deferred) to
    // use, but now we just use full screen and default render path configured in the engine command line options
    SharedPtr<Viewport> viewport(new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetViewport(0, viewport);
}

void StaticScene::MoveCamera(float timeStep)
{
    // Do not move if the UI has a focused element (the console)
    if (GetSubsystem<UI>()->GetFocusElement())
        return;

    Input* input = GetSubsystem<Input>();

    // Movement speed as world units per second
    const float MOVE_SPEED = 20.0f;
    // Mouse sensitivity as degrees per pixel
    const float MOUSE_SENSITIVITY = 0.1f;

    // Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
    IntVector2 mouseMove = input->GetMouseMove();
    yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
    pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
    pitch_ = Clamp(pitch_, -90.0f, 90.0f);

    // Construct new orientation for the camera scene node from yaw and pitch. Roll is fixed to zero
    cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));

    // Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
    // Use the Translate() function (default local space) to move relative to the node's orientation.
    if (input->GetKeyDown(KEY_W))
        cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_S))
        cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_A))
        cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_D))
        cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);
}

void StaticScene::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for processing update events
    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(StaticScene, HandleUpdate));
}

void StaticScene::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace Update;

    // Take the frame time step, which is stored as a float
    float timeStep = eventData[P_TIMESTEP].GetFloat();

    // Move the camera, scale movement with time step
    MoveCamera(timeStep);
}
