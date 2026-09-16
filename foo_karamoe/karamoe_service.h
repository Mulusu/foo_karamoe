#pragma once
#include "stdafx.h"
#include "json.hpp"

namespace foo_karamoe {

	const float REPLAY_GAIN_LUFT_TARGET = -18;
	inline static const std::string fileDir = "\\karamoe_temp\\";

	struct KaramoeUrl {
		inline static std::string api = "https://kara.moe/api/";
		inline static std::string media_dl = "https://kara.moe/downloads/medias/";
		inline static std::string lyric_dl = "https://kara.moe/downloads/lyrics/";
		inline static std::string hardsub_dl = "https://kara.moe/hardsubs/";
	};

	struct Collection {
		inline static std::string asia = "dbcf2c22-524d-4708-99bb-601703633927";
		inline static std::string geek = "c7db86a0-ff64-4044-9be4-66dd1ef1d1c1";
		inline static std::string shitpost = "f2462778-f986-4844-a4b8-e1d3ccdb861b";
		inline static std::string non_latin = "2fa2fe3f-bb56-45ee-aa38-eae60e76f224";
		inline static std::string west = "efe171c0-e8a1-4d03-98c0-60ecf741ad52";
	};

	enum KaraField {
		NAME,
		TITLE,
		LANG,
		ARTIST,
		WRITER,
		LENGTH,
		TYPE,
		FRANCHISE,
		LOUDNORM,
		MEDIAFILE,
		SUBFILE,
		HS_MEDIAFILE,
		COLLECTIONS,
		WARNINGS
	};

	typedef std::unordered_map<KaraField, std::string> Kara;

	class KaramoeService {
	public:
		KaramoeService();

		nlohmann::json search(const std::string& query);
		std::vector<Kara*> parse_karas(nlohmann::json& search_results);

		std::pair<std::string, std::string> prepare_files(const Kara &kara);
		file_info_impl queue_file(const Kara &kara, std::string& file_path);
		std::pair<file::ptr, file::ptr> download_files(const Kara &kara);
		bool write_to_disk(file::ptr &sourceFile, std::string &path);
		void write_tags(file_info_impl &info, std::string &filepath);

	private:
		file::ptr http_get(std::string& url, abort_callback& p_abort);
		std::string url_encode(const std::string& raw);
		std::string parseNames(nlohmann::json &data, const char* field);
	};
}
