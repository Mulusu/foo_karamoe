#include "stdafx.h"

namespace {
    /**
    * A component that monitors song changes and swaps the topmost fullscreen window depending on if video or audio-only is playing
    * Not strictly related to Karamoe in any way, may be split to its own plugin later
    **/
    class FSSwapper : public initquit, private play_callback {
    private:
        HWND m_audioWindow;
        HWND m_videoWindow;
        metadb_handle_ptr m_last;  // In case a window hasn't changed its name on song change, it might be found by the last
    public:
        void on_init() {
            play_callback_manager::get()->register_callback(this, play_callback::flag_on_playback_new_track, true);
        }

        void on_quit() {
            play_callback_manager::get()->unregister_callback(this);
        }

        // Dummy implementations of member methods that do nothing in our case
        void on_playback_starting(play_control::t_track_command p_command, bool p_paused) override {}
        void on_playback_stop(play_control::t_stop_reason p_reason) override {}
        void on_playback_seek(double p_time) override {}
        void on_playback_pause(bool p_state) override {}
        void on_playback_edited(metadb_handle_ptr p_track) override {}
        void on_playback_dynamic_info(const file_info& p_info) override {}
        void on_playback_dynamic_info_track(const file_info& p_info) override {}
        void on_playback_time(double p_time) override {}
        void on_volume_change(float p_new_val) override {}

        void on_playback_new_track(metadb_handle_ptr p_track) override {
            try {
                std::string path = p_track->get_path();
                auto fs = filesystem::get(path.c_str());
                std::string ext = fs->get_extension(path.c_str()).toString();

                bool is_video = ext == "mp4";

                if (!IsWindow(is_video ? m_videoWindow : m_audioWindow)) {
                    // No window yet
                    std::wstring wanted;
                    if (is_video) {
                        m_videoWindow = findAndFocusWindow(getVideoWindowName(p_track));
                        if (m_videoWindow == NULL && m_last != NULL) {
                            m_videoWindow = findAndFocusWindow(getVideoWindowName(m_last));
                        }
                    }
                    else {
                        m_audioWindow = findAndFocusWindow(L"ESLyric");
                    }
                }
                else {
                    focusWindow(is_video ? m_videoWindow : m_audioWindow);
                }
                m_last = p_track;
            }
            catch (std::exception e) {
                // Just print error to console, don't interrupt normal operation
                console::print(e.what());
            }
        }

        std::wstring getVideoWindowName(metadb_handle_ptr& p_track) {
            try {
                const file_info& info = p_track->get_full_info_ref(fb2k::noAbort)->info();
                std::string title = info.meta_exists("title") ? info.meta_get("title", 0) : fb2k::filename(p_track->get_path()).toString();
                std::string artist = info.meta_exists("artist") ? info.meta_get("artist", 0) : "?";
                std::string windowName = title + " - " + artist;
                std::wstring wide_name(windowName.begin(), windowName.end());
                return wide_name;
            }
            catch (std::exception& e) {
                console::print(e.what());
                std::string path = p_track->get_path();
                std::wstring wide_name(path.begin(), path.end());
                return wide_name;
            }
        }

        HWND findAndFocusWindow(std::wstring name) {
            HWND window = FindWindow(NULL, name.c_str());
            if (window) {
                focusWindow(window);
                return window;
            }
            return NULL;
        }

        void focusWindow(HWND window) {
            SetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    };
    static service_factory_single_t<FSSwapper> g_fs_swapper_factory;
}