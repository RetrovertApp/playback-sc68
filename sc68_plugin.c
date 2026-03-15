///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SC68 Playback Plugin
//
// Implements RVPlaybackPlugin interface for Atari ST and Amiga music formats (SNDH, SC68).
// Based on SC68 by Benjamin Gerard - http://sc68.atari.org/
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// C11 nullptr compatibility
#ifndef nullptr
#define nullptr ((void*)0)
#endif

#include <retrovert/io.h>
#include <retrovert/log.h>
#include <retrovert/metadata.h>
#include <retrovert/playback.h>
#include <retrovert/service.h>

#include <sc68/file68_rsc.h>
#include <sc68/file68_vfs_mem.h>
#include <sc68/sc68.h>

#include "embedded_replays.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define OUTPUT_SAMPLE_RATE 48000

RV_PLUGIN_USE_IO_API();
RV_PLUGIN_USE_LOG_API();
RV_PLUGIN_USE_METADATA_API();

static int g_sc68_initialized = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct Sc68ReplayerData {
    sc68_t* sc68;
    int current_subsong;
    int num_subsongs;
    int duration_ms;
    int scope_enabled;
} Sc68ReplayerData;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const char* sc68_supported_extensions(void) {
    return "sc68,sndh,snd";
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void* sc68_plugin_create(const RVService* service_api) {
    Sc68ReplayerData* data = malloc(sizeof(Sc68ReplayerData));
    if (data == nullptr) {
        return nullptr;
    }
    memset(data, 0, sizeof(Sc68ReplayerData));

    // Create SC68 instance with desired sample rate
    sc68_create_t create_params;
    memset(&create_params, 0, sizeof(create_params));
    create_params.sampling_rate = OUTPUT_SAMPLE_RATE;
    create_params.name = "replay_frontend";

    data->sc68 = sc68_create(&create_params);
    if (data->sc68 == nullptr) {
        rv_error("SC68: Failed to create sc68 instance");
        free(data);
        return nullptr;
    }

    // Set PCM format to 16-bit signed (we'll convert to F32)
    sc68_cntl(data->sc68, SC68_SET_PCM, SC68_PCM_S16);

    return data;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int sc68_plugin_destroy(void* user_data) {
    Sc68ReplayerData* data = (Sc68ReplayerData*)user_data;

    if (data->sc68) {
        sc68_destroy(data->sc68);
    }

    free(data);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int sc68_plugin_open(void* user_data, const char* url, uint32_t subsong, const RVService* service_api) {
    (void)service_api;

    RVIoReadUrlResult read_res;

    if ((read_res = rv_io_read_url_to_memory(url)).data == nullptr) {
        rv_error("SC68: Failed to load %s to memory", url);
        return -1;
    }

    Sc68ReplayerData* data = (Sc68ReplayerData*)user_data;

    // Close any previously loaded disk
    sc68_close(data->sc68);

    // Load from memory
    if (sc68_load_mem(data->sc68, read_res.data, (int)read_res.data_size) != 0) {
        rv_error("SC68: Failed to parse %s: %s", url, sc68_error(data->sc68));
        rv_io_free_url_to_memory(read_res.data);
        return -1;
    }

    // Get music info to determine track count
    sc68_music_info_t info;
    memset(&info, 0, sizeof(info));
    if (sc68_music_info(data->sc68, &info, -1, nullptr) == 0) {
        data->num_subsongs = info.tracks;
        rv_debug("SC68: File has %d tracks", info.tracks);
    } else {
        data->num_subsongs = 1;
        rv_debug("SC68: Failed to get track count, defaulting to 1");
    }

    // Determine which track to play
    int track = (subsong == 0) ? SC68_DEF_TRACK : (int)subsong;
    data->current_subsong = (track == SC68_DEF_TRACK) ? 1 : track;
    rv_debug("SC68: Playing track %d (requested subsong=%u)", track, subsong);

    // Get duration for this track
    memset(&info, 0, sizeof(info));
    if (sc68_music_info(data->sc68, &info, data->current_subsong, nullptr) == 0) {
        data->duration_ms = (int)info.trk.time_ms;
        rv_debug("SC68: Track duration: %d ms", data->duration_ms);
    } else {
        data->duration_ms = 3 * 60 * 1000; // Default 3 minutes
        rv_debug("SC68: Failed to get duration, defaulting to 3 minutes");
    }

    // Start playback with 1 loop
    int play_result = sc68_play(data->sc68, track, 1);
    rv_debug("SC68: sc68_play() returned %d", play_result);
    if (play_result < 0) {
        rv_error("SC68: Failed to start playback of track %d: %s", track, sc68_error(data->sc68));
        rv_io_free_url_to_memory(read_res.data);
        return -1;
    }

    rv_io_free_url_to_memory(read_res.data);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void sc68_plugin_close(void* user_data) {
    Sc68ReplayerData* data = (Sc68ReplayerData*)user_data;
    sc68_close(data->sc68);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static RVProbeResult sc68_plugin_probe_can_play(uint8_t* data, uint64_t data_size, const char* url,
                                                uint64_t total_size) {
    (void)total_size;

    // Check for SC68 magic header "SC68 Music-file / (c)"
    if (data_size >= 16) {
        if (memcmp(data, "SC68 Music-file", 15) == 0) {
            return RVProbeResult_Supported;
        }
    }

    // Check for SNDH header (ICE! packed or raw)
    // ICE! packed files start with "ICE!"
    if (data_size >= 4) {
        if (memcmp(data, "ICE!", 4) == 0) {
            return RVProbeResult_Supported;
        }
    }

    // Raw SNDH files have "SNDH" somewhere in the header (usually at offset 12)
    if (data_size >= 16) {
        // Check common SNDH header locations
        for (int offset = 0; offset < 32 && offset + 4 <= (int)data_size; offset += 2) {
            if (memcmp(data + offset, "SNDH", 4) == 0) {
                return RVProbeResult_Supported;
            }
        }
    }

    // Fall back to extension check
    if (url != nullptr) {
        const char* ext = strrchr(url, '.');
        if (ext != nullptr) {
            ext++;
            if (strcasecmp(ext, "sc68") == 0 || strcasecmp(ext, "sndh") == 0 || strcasecmp(ext, "snd") == 0) {
                return RVProbeResult_Supported;
            }
        }
    }

    return RVProbeResult_Unsupported;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static RVReadInfo sc68_plugin_read_data(void* user_data, RVReadData dest) {
    Sc68ReplayerData* data = (Sc68ReplayerData*)user_data;
    RVAudioFormat format = { RVAudioStreamFormat_S16, 2, OUTPUT_SAMPLE_RATE };

    // Calculate how many S16 stereo frames fit in the output buffer
    uint32_t max_frames = dest.channels_output_max_bytes_size / (sizeof(int16_t) * 2);

    // SC68 produces stereo S16 samples directly to output buffer
    int samples_to_generate = (int)max_frames;
    int code = sc68_process(data->sc68, (int16_t*)dest.channels_output, &samples_to_generate);

    RVReadStatus status = RVReadStatus_Ok;
    // Check SC68_ERROR first since it has all bits set (~0) and would match SC68_END check too
    if (code == SC68_ERROR) {
        const char* err_msg = sc68_error(data->sc68);
        rv_error("SC68: sc68_process returned SC68_ERROR: %s", err_msg ? err_msg : "unknown");
        status = RVReadStatus_Error;
    } else if (code & SC68_END) {
        rv_debug("SC68: sc68_process returned SC68_END (code=%d, samples=%d)", code, samples_to_generate);
        status = RVReadStatus_Finished;
    }

    return (RVReadInfo) { format, (uint16_t)samples_to_generate, status};
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int64_t sc68_plugin_seek(void* user_data, int64_t ms) {
    Sc68ReplayerData* data = (Sc68ReplayerData*)user_data;

    // SC68 supports seeking via sc68_cntl
    if (sc68_cntl(data->sc68, SC68_SET_POS, (int)ms) == 0) {
        return ms;
    }
    return -1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int sc68_plugin_metadata(const char* url, const RVService* service_api) {
    (void)service_api;
    RVIoReadUrlResult read_res;

    if ((read_res = rv_io_read_url_to_memory(url)).data == nullptr) {
        rv_error("SC68: Failed to load %s to memory for metadata", url);
        return -1;
    }

    // Create a temporary SC68 instance for metadata extraction
    sc68_create_t create_params;
    memset(&create_params, 0, sizeof(create_params));
    create_params.sampling_rate = OUTPUT_SAMPLE_RATE;

    sc68_t* sc68 = sc68_create(&create_params);
    if (sc68 == nullptr) {
        rv_error("SC68: Failed to create sc68 instance for metadata extraction");
        rv_io_free_url_to_memory(read_res.data);
        return -1;
    }

    if (sc68_load_mem(sc68, read_res.data, (int)read_res.data_size) != 0) {
        sc68_destroy(sc68);
        rv_io_free_url_to_memory(read_res.data);
        return -1;
    }

    RVMetadataId index = rv_metadata_create_url(url);

    sc68_music_info_t info;
    memset(&info, 0, sizeof(info));

    // Get disk info first (track 0 means disk-level info)
    if (sc68_music_info(sc68, &info, -1, nullptr) == 0) {
        // Set title from track title or album
        if (info.title && info.title[0] != '\0') {
            rv_metadata_set_tag(index, RV_METADATA_TITLE_TAG, info.title);
        } else if (info.album && info.album[0] != '\0') {
            rv_metadata_set_tag(index, RV_METADATA_TITLE_TAG, info.album);
        }

        // Set artist
        if (info.artist && info.artist[0] != '\0') {
            rv_metadata_set_tag(index, RV_METADATA_ARTIST_TAG, info.artist);
        }

        // Set song type based on hardware
        const char* hw = info.trk.hw;
        if (hw && hw[0] != '\0') {
            char songtype[64];
            snprintf(songtype, sizeof(songtype), "SC68 (%s)", hw);
            rv_metadata_set_tag(index, RV_METADATA_SONGTYPE_TAG, songtype);
        } else {
            rv_metadata_set_tag(index, RV_METADATA_SONGTYPE_TAG, "SC68 (Atari ST/Amiga)");
        }

        // Set duration
        if (info.trk.time_ms > 0) {
            rv_metadata_set_tag_f64(index, RV_METADATA_LENGTH_TAG, info.trk.time_ms / 1000.0);
        }

        // Add subsongs if more than one
        int num_tracks = info.tracks;
        if (num_tracks > 1) {
            for (int i = 1; i <= num_tracks; i++) {
                sc68_music_info_t track_info;
                memset(&track_info, 0, sizeof(track_info));
                if (sc68_music_info(sc68, &track_info, i, nullptr) == 0) {
                    float length = track_info.trk.time_ms > 0 ? track_info.trk.time_ms / 1000.0f : 0.0f;
                    const char* track_title = (track_info.title && track_info.title[0]) ? track_info.title : "";
                    rv_metadata_add_subsong(index, i, track_title, length);
                }
            }
        }
    }

    sc68_close(sc68);
    sc68_destroy(sc68);
    rv_io_free_url_to_memory(read_res.data);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void sc68_plugin_event(void* user_data, uint8_t* event_data, uint64_t len) {
    (void)user_data;

    if (event_data == nullptr || len < 8) {
        return;
    }

    // SC68 doesn't provide per-channel VU meter data easily
    // Just zero out the event data for now
    memset(event_data, 0, 8);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Custom resource handler that serves replay routines from embedded data.
// This eliminates the need for external replay files.

static vfs68_t* sc68_embedded_resource_handler(rsc68_t type, const char* name, int mode, rsc68_info_t* info) {
    (void)info;

    // Only handle replay resources in read mode
    if (type != rsc68_replay || mode != 1) {
        return nullptr;
    }

    // Look up the replay in embedded data
    const Sc68EmbeddedReplay* replay = sc68_find_embedded_replay(name);
    if (replay == nullptr) {
        rv_debug("SC68: Embedded replay not found: %s", name);
        return nullptr;
    }

    // Create a memory VFS from the embedded data
    // mode=1 means read-only, which is what we want
    vfs68_t* vfs = vfs68_mem_create(replay->data, (int)replay->size, 1);
    if (vfs == nullptr) {
        rv_error("SC68: Failed to create memory VFS for replay: %s", name);
        return nullptr;
    }

    // Open the VFS (required by SC68)
    if (vfs68_open(vfs) != 0) {
        rv_error("SC68: Failed to open memory VFS for replay: %s", name);
        vfs68_destroy(vfs);
        return nullptr;
    }

    rv_debug("SC68: Loaded embedded replay: %s (%zu bytes)", name, replay->size);
    return vfs;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void sc68_plugin_static_init(const RVService* service_api) {
    rv_init_log_api(service_api);
    rv_init_io_api(service_api);
    rv_init_metadata_api(service_api);

    // Initialize SC68 library (only once)
    if (!g_sc68_initialized) {
        sc68_init_t init_params;
        memset(&init_params, 0, sizeof(init_params));
        init_params.flags.no_load_config = 1;
        init_params.flags.no_save_config = 1;

        // SC68 library expects valid argc/argv even when not using command line options.
        // Passing NULL argv causes crashes in option parsing code.
        static char* dummy_argv[] = { "sc68", NULL };
        init_params.argc = 1;
        init_params.argv = dummy_argv;

        if (sc68_init(&init_params) == 0) {
            g_sc68_initialized = 1;

            // Set custom resource handler to serve replays from embedded data
            rsc68_set_handler(sc68_embedded_resource_handler);

            rv_info("SC68: Library initialized with %d embedded replays (version %s)", SC68_EMBEDDED_REPLAY_COUNT,
                    sc68_versionstr());
        } else {
            rv_error("SC68: Failed to initialize library");
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void sc68_plugin_static_destroy(void) {
    if (g_sc68_initialized) {
        sc68_shutdown();
        g_sc68_initialized = 0;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static uint32_t sc68_plugin_get_scope_data(void* user_data, int channel, float* buffer, uint32_t num_samples) {
    Sc68ReplayerData* data = (Sc68ReplayerData*)user_data;
    if (data == nullptr || data->sc68 == nullptr || buffer == nullptr) {
        return 0;
    }

    if (!data->scope_enabled) {
        sc68_scope_enable(data->sc68, 1);
        data->scope_enabled = 1;
    }

    return sc68_scope_get_data(data->sc68, channel, buffer, num_samples);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static uint32_t sc68_plugin_get_scope_channel_names(void* user_data, const char** names, uint32_t max_channels) {
    Sc68ReplayerData* data = (Sc68ReplayerData*)user_data;
    if (data == nullptr || data->sc68 == nullptr)
        return 0;

    static const char* s_ym_names[] = { "Tone A", "Tone B", "Tone C" };
    static const char* s_paula_names[] = { "Paula 0", "Paula 1", "Paula 2", "Paula 3" };

    int channels = sc68_scope_channels(data->sc68);
    if (channels <= 0)
        return 0;

    uint32_t count = (uint32_t)channels;
    if (count > max_channels)
        count = max_channels;

    const char** src = (channels == 3) ? s_ym_names : s_paula_names;
    for (uint32_t i = 0; i < count; i++)
        names[i] = src[i];
    return count;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static RVPlaybackPlugin g_sc68_plugin = {
    RV_PLAYBACK_PLUGIN_API_VERSION,
    "sc68",
    "0.0.1",
    PACKAGE_VERSION, // Library version
    sc68_plugin_probe_can_play,
    sc68_supported_extensions,
    sc68_plugin_create,
    sc68_plugin_destroy,
    sc68_plugin_event,
    sc68_plugin_open,
    sc68_plugin_close,
    sc68_plugin_read_data,
    sc68_plugin_seek,
    sc68_plugin_metadata,
    sc68_plugin_static_init,
    nullptr, // settings_updated
    nullptr, // get_tracker_info (not a tracker format)
    nullptr, // get_pattern_cell
    nullptr, // get_pattern_num_rows
    sc68_plugin_get_scope_data,
    sc68_plugin_static_destroy,
    sc68_plugin_get_scope_channel_names,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RV_EXPORT RVPlaybackPlugin* rv_playback_plugin(void) {
    return &g_sc68_plugin;
}
