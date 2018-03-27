#pragma once

#include "../Container/Ptr.h"
#include "../Scene/Component.h"
#include "../Core/Context.h"
#include "../Graphics/Texture2D.h"
#include <atlbase.h>

struct IMFMediaEngine;

namespace Urho3D
{

    class VideoPlayer : public Urho3D::Component
    {
        URHO3D_OBJECT(VideoPlayer, Urho3D::Object);
    public:
        VideoPlayer(Urho3D::Context* context);

        Urho3D::String GetSource() const;
        void SetSource(const Urho3D::String& value);
        void Play();
        void Pause();
        void Stop();

        double GetVolume() const;
        void SetVolume(double value);
        bool GetLoop() const;
        void SetLoop(bool value);
        bool GetGenerateMipmaps() const;
        void SetGenerateMipmaps(bool value);

        Urho3D::Texture2D* GetTexture() const;

        ~VideoPlayer();

    private:
        void HandleRenderSurfaceUpdate(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData);
        void ResizeTexture();
        Urho3D::String source_;
        bool generateMipmaps_;
        Urho3D::SharedPtr<Urho3D::Texture2D> texture_;
        CComPtr<IMFMediaEngine> mediaEngine_;
    };

}