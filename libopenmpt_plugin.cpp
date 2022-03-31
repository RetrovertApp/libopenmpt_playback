#include <libopenmpt/libopenmpt.h>
#include <libopenmpt/libopenmpt.hpp>

#include "retrovert_api/c/retrovert/rv_io.h"
#include "retrovert_api/c/retrovert/rv_log.h"
#include "retrovert_api/c/retrovert/rv_metadata.h"
#include "retrovert_api/c/retrovert/rv_playback_plugin.h"
#include "retrovert_api/c/retrovert/rv_service.h"
#include "retrovert_api/c/retrovert/rv_settings.h"

#include <assert.h>
#include <string.h>
#include <sstream>
#include <string>
#include <vector>

const int MAX_EXT_COUNT = 16 * 1024;
static char s_supported_extensions[MAX_EXT_COUNT];
const char* PLUGIN_NAME = "openmpt";

const RVIoAPI* g_io_api = nullptr;
RVLogAPI* g_rv_log = nullptr;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ID_SAMPLE_RATE "SampleRate"
#define ID_CHANNELS "Channels"
#define ID_MASTER_GAIN "MasterGain"
#define ID_STEREO_SEPARATION "StereoSeparation"
#define ID_VOLUME_RAMPING "VolumeRamping"
#define ID_INTERPOLATION_RANGE "InterploationRange"
#define ID_TEMPO_FACTOR "TempoFactor"
#define ID_PITCH_FACTOR "PitchFactor"
#define ID_USE_AMIGA_RESAMPLER_AMIGA_MODS "AmigaModResampling"
#define ID_AMIGA_RESAMPLER_FILTER "AmigaModResamplerFilter"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// clang-format off

static const RVSIntegerRangeValue s_interpolation_filter_ranges[] = {
    {"Default", 0},
    {"No Interpolation (zero order hold)", 1},
    {"Linear Interpolation", 2},
    {"Cubic Interpolation", 4},
    {"Windowed sinc with 8 taps", 8},
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const RVSIntegerRangeValue s_volume_ramping_range[] = {
    {"Default", -1},
    {"Off", 0},
    {"1 ms", 1},
    {"2 ms", 2},
    {"3 ms", 3},
    {"5 ms", 5},
    {"10 ms", 10},
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const RVSIntegerRangeValue s_sample_rate[] = {
    {"Default", 0},
    {"6000", 6000},
    {"8000", 6000},
    {"11025", 11025},
    {"22050", 22050},
    {"32000", 32000},
    {"44100", 44100},
    {"48000", 48000},
    {"82000", 82000},
    {"96000", 96000},
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const RVSIntegerRangeValue s_channels[] = {
    {"Default", 0},
    {"Mono", 1},
    {"Stereo", 2},
    {"Quad", 3},
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const RVSStringRangeValue s_amiga_filter_values[] = {
    {"Default filter", "auto"},
    {"Amiga A500 filter", "a500"},
    {"Amiga A1200 filter", "a1200"},
    {"Unfilterned", "unfiltered"},
};

// clang-format on
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

static RVSSetting s_settings[] = {
    RVSBoolValue(ID_USE_AMIGA_RESAMPLER_AMIGA_MODS, "Use Amiga Resampler for Amiga modules",
                 "Set to enable the Amiga resampler for Amiga modules. This emulates the sound characteristics of "
                 "the Paula chip and overrides the selected interpolation filter. Non-Amiga module formats are not "
                 "affected by this setting.",
                 false),
    RVSStringValue_DescRange(ID_AMIGA_RESAMPLER_FILTER, "Filter type for Amiga Resampler",
                             "Filter type for Amiga filter if enabled", "auto", s_amiga_filter_values),
    RVSIntValue_DescRange(ID_SAMPLE_RATE, "Sample rate",
                          "Default (recommended) uses the sample rate by the output device", 0, s_sample_rate),
    RVSIntValue_DescRange(ID_CHANNELS, "Channels",
                          "Default (recommended) uses the number of channels the current song has.", 0, s_channels),
    RVSFloatValue_Range(ID_MASTER_GAIN, "Master Gain",
                        "The related value represents a relative gain in decibel. The default value is 0", 0, -12, 12),
    RVSIntValue_Range(ID_STEREO_SEPARATION, "Stereo Separation",
                      "The related value represents the stereo separation generated by the libopenmpt mixer in "
                      "percent. The default value is 100 and supported range is 0 - 200",
                      100, 0, 200),
    RVSIntValue_DescRange(ID_VOLUME_RAMPING, "Volume Ramping Strength",
                          "Off completely disables volume ramping. This might cause clicks in sound output. Higher "
                          "values imply slower/softer volume ramps.",
                          0, s_volume_ramping_range),
    RVSIntValue_DescRange(ID_INTERPOLATION_RANGE, "Interpolation Filter",
                          "The related value represents the interpolation filter length used by the libopenmpt mixer.",
                          0, s_interpolation_filter_ranges),
    RVSFloatValue_Range(ID_TEMPO_FACTOR, "Tempo Factor", "Set the tempo factor. Default value is 1.0", 1.0, 0.01f,
                        2.0f),
    RVSFloatValue_Range(ID_PITCH_FACTOR, "Pitch Factor", "Set the pitch factor. Default value is 1.0", 1.0, 0.01f,
                        2.0f),
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum class Channels {
    Default,
    Mono,
    Stereo,
    Quad,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct OpenMptData {
    openmpt::module* mod = 0;
    const RVMessageAPI* message_api;
    std::string ext;
    // number of channels to render
    Channels channels;
    int sample_rate;
    float length = 0.0f;
    void* song_data = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const char* openmpt_supported_extensions() {
    // If we have already populated this we can just return

    if (s_supported_extensions[0] != 0) {
        return s_supported_extensions;
    }

    std::vector<std::string> ext_list = openmpt::get_supported_extensions();
    size_t count = ext_list.size();

    for (size_t i = 0; i < count; ++i) {
        strcat(s_supported_extensions, ext_list[i].c_str());
        if (i != count - 1) {
            strcat(s_supported_extensions, ",");
        }
    }

    return s_supported_extensions;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void* openmpt_create(const RVServiceAPI* service_api) {
    OpenMptData* user_data = new OpenMptData;

    g_io_api = RVServiceAPI_get_io_api(service_api, 1);
    user_data->message_api = RVServiceAPI_get_message_api(service_api, 1);

    return (void*)user_data;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int openmpt_destroy(void* user_data) {
    OpenMptData* data = (OpenMptData*)user_data;
    delete data;
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static RVProbeResult openmpt_probe_can_play(const uint8_t* data, uint32_t data_size, const char* filename,
                                            uint64_t total_size) {
    int res = openmpt::probe_file_header(openmpt::probe_file_header_flags_default2, data, data_size, total_size);

    switch (res) {
        case openmpt::probe_file_header_result_success: {
            rv_info("Supported: %s", filename);
            return RVProbeResult_Supported;
        }
        case openmpt::probe_file_header_result_failure: {
            rv_debug("Unsupported: %s", filename);
            return RVProbeResult_Unsupported;
        }

        case openmpt::probe_file_header_result_wantmoredata: {
            rv_warn("openmpt: Unable to probe because not enough data\n");
            break;
        }
    }

    rv_warn("openmpt: case %d not handled in switch. Assuming unsupported file\n", res);

    return RVProbeResult_Unsupported;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void settings_apply(OpenMptData* data, const RVSettingsAPI* api) {
    int int_value = 0;
    float float_value = 0;
    const char* str = nullptr;
    bool bool_value = false;
    const char* ext = data->ext.c_str();

    if (RVSettings_get_int(api, ext, ID_SAMPLE_RATE, &int_value) == RVSettingsError_Ok) {
        data->sample_rate = int_value;
    }

    if (RVSettings_get_int(api, ext, ID_CHANNELS, &int_value) == RVSettingsError_Ok) {
        data->channels = (Channels)int_value;
    }

    if (RVSettings_get_float(api, ext, ID_MASTER_GAIN, &float_value) == RVSettingsError_Ok) {
        data->mod->set_render_param(openmpt::module::RENDER_MASTERGAIN_MILLIBEL, int(float_value * 1000));
    }

    if (RVSettings_get_int(api, ext, ID_STEREO_SEPARATION, &int_value) == RVSettingsError_Ok) {
        data->mod->set_render_param(openmpt::module::RENDER_STEREOSEPARATION_PERCENT, int_value);
    }

    if (RVSettings_get_int(api, ext, ID_VOLUME_RAMPING, &int_value) == RVSettingsError_Ok) {
        data->mod->set_render_param(openmpt::module::RENDER_VOLUMERAMPING_STRENGTH, int_value);
    }

    if (RVSettings_get_int(api, ext, ID_INTERPOLATION_RANGE, &int_value) == RVSettingsError_Ok) {
        data->mod->set_render_param(openmpt::module::RENDER_INTERPOLATIONFILTER_LENGTH, int_value);
    }

    if (RVSettings_get_string(api, ext, ID_AMIGA_RESAMPLER_FILTER, &str) == RVSettingsError_Ok) {
        data->mod->ctl_set_text("render.resampler.emulate_amiga_type", str);
    }

    if (RVSettings_get_bool(api, ext, ID_USE_AMIGA_RESAMPLER_AMIGA_MODS, &bool_value) == RVSettingsError_Ok) {
        data->mod->ctl_set_boolean("render.resampler.emulate_amiga", bool_value);
    }

    if (RVSettings_get_float(api, ext, ID_TEMPO_FACTOR, &float_value) == RVSettingsError_Ok) {
        data->mod->ctl_set_floatingpoint("play.tempo_factor", float_value);
    }

    if (RVSettings_get_float(api, ext, ID_PITCH_FACTOR, &float_value) == RVSettingsError_Ok) {
        data->mod->ctl_set_floatingpoint("play.pitch_factor", float_value);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int openmpt_open(void* user_data, const char* filename, int subsong, const RVSettingsAPI* settings) {
    uint64_t size = 0;
    struct OpenMptData* replayer_data = (struct OpenMptData*)user_data;

    RVIoErrorCode res = RVIo_read_file_to_memory(g_io_api, filename, &replayer_data->song_data, &size);

    if (res < 0) {
        rv_error("Failed to load %s to memory", filename);
        return -1;
    }

    try {
        replayer_data->mod = new openmpt::module(replayer_data->song_data, size);
    } catch (...) {
        rv_error("Failed to open %s even if is as supported format", filename);
        return -1;
    }

    rv_info("Started to play %s (subsong %d)", filename, subsong);

    replayer_data->length = (float)replayer_data->mod->get_duration_seconds();
    replayer_data->mod->select_subsong(subsong);

    settings_apply(replayer_data, settings);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int openmpt_close(void* user_data) {
    struct OpenMptData* replayer_data = (struct OpenMptData*)user_data;

    if (g_io_api) {
        RVIo_free_file_to_memory(g_io_api, replayer_data->song_data);
    }

    delete replayer_data->mod;
    delete replayer_data;

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static RVReadInfo openmpt_read_data(void* user_data, void* dest, uint32_t max_output_bytes, uint32_t sample_rate) {
    struct OpenMptData* replayer_data = (struct OpenMptData*)user_data;

    const int samples_to_generate = std::min(uint32_t(512), max_output_bytes / 8);

    // support overringing the default sample rate
    if (replayer_data->sample_rate != 0) {
        sample_rate = replayer_data->sample_rate;
    }

    uint8_t channel_count = 2;
    uint16_t gen_count = 0;

    switch (replayer_data->channels) {
        default:
        case Channels::Stereo:
        case Channels::Default: {
            gen_count =
                (uint16_t)replayer_data->mod->read_interleaved_stereo(sample_rate, samples_to_generate, (float*)dest);
            break;
        }

        case Channels::Mono: {
            gen_count = (uint16_t)replayer_data->mod->read(sample_rate, samples_to_generate, (float*)dest);
            channel_count = 1;
            break;
        }

        case Channels::Quad: {
            gen_count =
                (uint16_t)replayer_data->mod->read_interleaved_quad(sample_rate, samples_to_generate, (float*)dest);
            channel_count = 4;
            break;
        }
    }

    // Send current positions back to frontend if we have some more data
    /*
    if (gen_count > 0) {
        flatbuffers::FlatBufferBuilder builder(1024);
        builder.Finish(CreateRVMessageDirect(
            builder, MessageType_current_position,
            CreateRVCurrentPosition(builder, replayer_data->mod->get_position_seconds(),
                                       replayer_data->mod->get_current_pattern(), replayer_data->mod->get_current_row(),
                                       replayer_data->mod->get_current_speed(), replayer_data->length)
                .Union()));
        RVMessageAPI_send(replayer_data->message_api, builder.GetBufferPointer(), builder.GetSize());
    }
    */

    return RVReadInfo{sample_rate, gen_count, channel_count, RVOutputType_f32};
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int openmpt_seek(void* user_data, int ms) {
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* filename_from_path(const char* path) {
    for (size_t i = strlen(path) - 1; i > 0; i--) {
        if (path[i] == '/') {
            return &path[i + 1];
        }
    }

    return path;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int openmpt_metadata(const char* filename, const RVServiceAPI* service_api) {
    void* data = 0;
    uint64_t size = 0;

    const RVIoAPI* io_api = RVServiceAPI_get_io_api(service_api, RV_FILE_API_VERSION);
    const RVMetadataAPI* metadata_api = RVServiceAPI_get_metadata_api(service_api, RV_METADATA_API_VERSION);

    RVIoErrorCode res = RVIo_read_file_to_memory(io_api, filename, &data, &size);

    if (res < 0) {
        return res;
    }

    openmpt::module* mod = nullptr;

    try {
        mod = new openmpt::module(data, size);
    } catch (...) {
        rv_error("Failed to open %s even if is as supported format", filename);
        return -1;
    }

    auto index = RVMetadata_create_url(metadata_api, filename);
    char title[512] = {0};

    const auto& mod_title = mod->get_metadata("title");

    if (mod_title != "") {
        strcpy(title, mod_title.c_str());
    } else {
        const char* file_title = filename_from_path(filename);
        strcpy(title, file_title);
    }

    rv_info("Updating meta data for %s", filename);

    RVMetadata_set_tag(metadata_api, index, RVMetadata_TitleTag, title);
    RVMetadata_set_tag(metadata_api, index, RVMetadata_SongTypeTag, mod->get_metadata("type_long").c_str());
    RVMetadata_set_tag(metadata_api, index, RVMetadata_AuthoringToolTag, mod->get_metadata("tracker").c_str());
    RVMetadata_set_tag(metadata_api, index, RVMetadata_ArtistTag, mod->get_metadata("artist").c_str());
    RVMetadata_set_tag(metadata_api, index, RVMetadata_DateTag, mod->get_metadata("date").c_str());
    RVMetadata_set_tag(metadata_api, index, RVMetadata_MessageTag, mod->get_metadata("message").c_str());
    RVMetadata_set_tag_f64(metadata_api, index, RVMetadata_LengthTag, mod->get_duration_seconds());

    for (const auto& sample : mod->get_sample_names()) {
        RVMetadata_add_sample(metadata_api, index, sample.c_str());
    }

    for (const auto& instrument : mod->get_instrument_names()) {
        RVMetadata_add_instrument(metadata_api, index, instrument.c_str());
    }

    const int subsong_count = mod->get_num_subsongs();

    if (subsong_count > 1) {
        int i = 0;
        for (const auto& name : mod->get_subsong_names()) {
            char subsong_name[1024] = {0};

            if (name != "") {
                sprintf(subsong_name, "%s - %s (%d/%d)", title, name.c_str(), i + 1, subsong_count);
            } else {
                sprintf(subsong_name, "%s (%d/%d)", title, i + 1, subsong_count);
            }

            mod->select_subsong(i);
            RVMetadata_add_subsong(metadata_api, index, i, subsong_name, (float)mod->get_duration_seconds());

            ++i;
        }
    }

    // Make sure to free the buffer before we leave
    RVIo_free_file_to_memory(io_api, data);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void openmpt_event(void* user_data, const unsigned char* data, int len) {
    (void)len;
    (void)user_data;
    (void)data;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static RVSettingsUpdate openmpt_settings_updated(void* user_data, const RVSettingsAPI* settings) {
    settings_apply((OpenMptData*)user_data, settings);

    return RVSettingsUpdate_Default;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void openmpt_static_init(struct RVLogAPI* log, const RVServiceAPI* service_api) {
    g_rv_log = log;

    auto settings_api = RVServiceAPI_get_settings_api(service_api, RV_SETTINGS_API_VERSION);

    if (RVSettings_register(settings_api, PLUGIN_NAME, s_settings) != RVSettingsError_Ok) {
        // rv_error("Unable to register settings, error: %s", RVSettings_get_last_error(api));
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static RVPlaybackPlugin s_openmpt_plugin = {
    RV_PLAYBACK_PLUGIN_API_VERSION,
    PLUGIN_NAME,
    "0.0.2",
    "libopenmpt SVN 2021-05-01",
    openmpt_probe_can_play,
    openmpt_supported_extensions,
    openmpt_create,
    openmpt_destroy,
    openmpt_event,
    openmpt_open,
    openmpt_close,
    openmpt_read_data,
    openmpt_seek,
    openmpt_metadata,
    openmpt_static_init,
    openmpt_settings_updated,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" RV_EXPORT RVPlaybackPlugin* hippo_playback_plugin() {
    return &s_openmpt_plugin;
}
