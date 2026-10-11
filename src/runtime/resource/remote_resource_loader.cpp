#include "remote_resource_loader.h"
#include "runtime/download/http_url.h"
#include <godot_cpp/classes/audio_stream_mp3.hpp>
#include <godot_cpp/classes/audio_stream_ogg_vorbis.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

namespace meowody::detail
{
bool RemoteResourceLoader::supports(const godot::String& url)
{
    const auto extension = url_extension(url);
    for (const auto* supported : {"png", "jpg", "jpeg", "webp", "mp3", "ogg", "wav", "tres", "res", "tscn", "scn"})
        if (extension == supported) return true;
    return false;
}
RemotePreparation RemoteResourceLoader::prepare(const godot::String& local_file, const godot::String& pack_entry)
{
    if (!pack_entry.is_empty())
    {
        if (!m_mounted.contains(local_file))
        {
            if (!godot::ProjectSettings::get_singleton()->load_resource_pack(local_file, false))
                return {{}, {{}, LoadError::PackMountFailed, godot::OK, "Cannot mount downloaded resource pack"}};
            m_mounted.insert(local_file);
        }
        return {pack_entry, {}};
    }
    const auto extension = local_file.get_extension().to_lower();
    godot::Ref<godot::Resource> resource;
    if (extension == "png" || extension == "jpg" || extension == "jpeg" || extension == "webp")
    {
        const auto image = godot::Image::load_from_file(local_file);
        if (image.is_valid() && !image->is_empty()) resource = godot::ImageTexture::create_from_image(image);
    }
    else if (extension == "mp3") resource = godot::AudioStreamMP3::load_from_file(local_file);
    else if (extension == "ogg") resource = godot::AudioStreamOggVorbis::load_from_file(local_file);
    else if (extension == "wav") resource = godot::AudioStreamWAV::load_from_file(local_file);
    else if (extension == "tres" || extension == "res" || extension == "tscn" || extension == "scn")
    {
        if (!godot::ResourceLoader::get_singleton()->get_dependencies(local_file).is_empty())
            return {{}, {{}, LoadError::RemoteDependencies, godot::OK, "Use a PCK/ZIP for resources with external dependencies"}};
        return {local_file, {}};
    }
    else return {{}, {{}, LoadError::UnsupportedRemoteFormat, godot::OK, "Unsupported remote resource extension"}};
    if (resource.is_null()) return {{}, {{}, LoadError::LoadFailed, godot::OK, "Cannot decode remote image or audio"}};
    return {{}, {resource}};
}
}
