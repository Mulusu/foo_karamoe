#pragma once
#include "stdafx.h"
#include <iostream>
#include <foobar2000/helpers/foobar2000+atl.h>
#include <foobar2000/helpers/atl-misc.h>
#include <libPPUI/win32_op.h>
#include <foobar2000/helpers/CListControlFb2kColors.h>
#include <libPPUI/CEditWithButtons.h>
#include <libPPUI/CListControlSimple.h>

namespace foo_karamoe {
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

	inline static const std::string fileDir = "\\karamoe_temp\\";
#define DEBOUNCE_WAIT 750    // Wait after typing stops before search is fired, ms

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

	const int SEARCH_BAR_HEIGHT = 30;
	const int SEARCH_STATUS_WIDTH = SEARCH_BAR_HEIGHT;
	const float REPLAY_GAIN_LUFT_TARGET = -18;

	class ResultCol {
	public:
		std::string m_label;	// Label for the column in the header
		KaraField m_field;		// Which kara field value to write in this column
		unsigned int m_width;	// Width of the column

		ResultCol(unsigned int width, std::string label, KaraField field) : m_width(width), m_label(label), m_field(field) {};
	};

	const static std::vector<ResultCol> rows{
		// The columns are put in the order defined here
			ResultCol(200, "Title", TITLE),
			ResultCol(200, "Band / Singer", ARTIST),
			ResultCol(200, "Writer", WRITER),
			ResultCol(200, "Franchise", FRANCHISE),
			ResultCol(200, "Type", TYPE),
			ResultCol(75, "Language", LANG),
			ResultCol(75, "Length", LENGTH),
			ResultCol(200, "Collections", COLLECTIONS),
			ResultCol(200, "Warnings", WARNINGS)
	};

	enum SearchStatus {
		// Numbers are the unicode number of the emoji used to indicate that status
		Idle = 0x1F4A4,
		Waiting = 0x23f3,
		InProgress = 0x1F50D,
		Done = 0x2705,
		Download = 0x1F4E1,
		Save = 0x1F4BE,
		Error = 0x26A0
	};

	const static std::vector<SearchStatus> statusBarIcons = {
		Idle, Waiting, InProgress, Download, Save, Done, Error
	};

	struct Colors {
		CBrush Brush;
		COLORREF Bg{};
		COLORREF Text{};
		COLORREF SelBg{};
		COLORREF SelText{};
	};

	typedef std::unordered_map<KaraField, std::string> Kara;
}