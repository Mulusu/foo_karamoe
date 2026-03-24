#include "stdafx.h"
#include "foo_karamoe.h"
#include <sstream>
#include <iomanip>

namespace foo_karamoe {
	class KaraContainer : public playback_queue_callback {
		void on_changed(playback_queue_callback::t_change_origin change) override {
			playlist_manager::ptr plm = playlist_manager::get();
			unsigned int queue_lenght = plm->queue_get_count();

			pfc::list_t<t_playback_queue_item> queue;
			plm->queue_get_contents(queue);
			console::print("Queue length: ", queue_lenght);
			for (t_playback_queue_item item : queue) {
				console::print(item.m_handle->get_path());
			}
			switch (change) {
			case changed_user_added:
				console::print("Added to queue");
				playlist_manager::get()->queue_get_count();
				break;
			case changed_user_removed:
				console::print("Removed from queue");
				break;
			case changed_playback_advance:
				console::print("Queue changed");
				break;
			}
		}
	};
	FB2K_SERVICE_FACTORY(KaraContainer);

	class mem_fs : public filesystem {
		const std::string m_prefix = "MEMFILE://";

		bool get_canonical_path(const char* in, pfc::string_base& out) override { 
			console::print("get canonical path: ", in);
			out = (in + m_prefix.length());
			return true;
		}
		bool is_our_path(const char* p_path) override {
			bool ours = strncmp(p_path, m_prefix.c_str(), m_prefix.length()) == 0;
			return ours;
		}

		bool get_display_path(const char* p_path, pfc::string_base& p_out) override {
			console::print("get display path: ", p_path);
			Kara kara = getKaraFromPath(p_path);
			p_out << kara[TITLE].c_str() << " - " << kara[SINGER].c_str();
			return false;
		}

		Kara getKaraFromPath(const char* p_path) {
			std::string path = (p_path + m_prefix.length());
			uintptr_t value = std::stoull(path, nullptr, 16);
			Kara* karaPtr = reinterpret_cast<Kara*>(value);
			return *karaPtr;
		}

		void open(service_ptr_t<file>& p_out, const char* p_path, t_open_mode p_mode, abort_callback& p_abort) override {
			Kara kara = getKaraFromPath(p_path);
			std::string url = KaramoeUrl::media_dl + kara[MEDIAFILE];
			file::ptr file = http_get(url, p_abort);
			p_out = file;
			console::print("open");
		}

		void remove(const char* p_path, abort_callback& p_abort) override {
			console::print("remove");
			(void)p_path;
		}

		void move(const char* p_src, const char* p_dst, abort_callback& p_abort) override {
			console::print("move");
			(void)p_src;
			(void)p_dst;
		}

		bool is_remote(const char* p_src) override {
			console::print("is remote");
			return false;
		}

		void get_stats(const char* p_path, t_filestats& p_stats, bool& p_is_writeable, abort_callback& p_abort) override {
			console::print("get stats");
			(void)p_stats;
			p_is_writeable = false;
		}

		bool relative_path_create(const char* file_path, const char* playlist_path, pfc::string_base& out) override {
			console::print("relative path create");
			(void)file_path;
			(void)playlist_path;
			(void)out;
			return false;
		}

		bool relative_path_parse(const char* relative_path, const char* playlist_path, pfc::string_base& out) override {
			console::print("relative path parse");
			(void)relative_path;
			(void)playlist_path;
			(void)out;
			return false;
		}

		void create_directory(const char* p_path, abort_callback& p_abort) override {
			console::print("create directory");
		}

		void list_directory(const char* p_path, directory_callback& p_out, abort_callback& p_abort) override {
			console::print("list directory: ", p_path);
			Kara kara = getKaraFromPath(p_path);
			t_filestats stats;
			if (p_out.on_entry(this, p_abort, kara[MEDIAFILE].c_str(), false, stats)) {
				p_out.on_entry(this, p_abort, kara[SUBFILE].c_str(), false, stats);
			}
		}

		bool supports_content_types() override {
			console::print("Supports content types");
			return true;
		};

	};

	//FB2K_SERVICE_FACTORY(mem_fs);
}