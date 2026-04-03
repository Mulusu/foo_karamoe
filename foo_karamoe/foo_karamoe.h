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

	inline static const std::string fileDir = "\\karamoe_temp\\";
#define DEBOUNCE_WAIT 750    // Wait after typing stops before search is fired, ms

	enum KaraField {
		NAME,
		TITLE,
		LANG,
		SINGER,
		WRITER,
		LENGTH,
		TYPE,
		FRANCHISE,
		LOUDNORM,
		MEDIAFILE,
		SUBFILE,
		HS_MEDIAFILE
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
			ResultCol(200, "Title", TITLE),
			ResultCol(200, "Singer", SINGER),
			ResultCol(200, "Writer", WRITER),
			ResultCol(200, "Franchise", FRANCHISE),
			ResultCol(200, "Type", TYPE),
			ResultCol(75, "Language", LANG),
			ResultCol(75, "Length", LENGTH)
	};

	enum SearchStatus {
		Idle = 0x1F4A4,
		Waiting = 0x23f3,
		InProgress = 0x1F50D,
		Done = 0x2705,
		Download = 0x1F4E1,
		Save = 0x1F4BE,
		Error = 0x26A0
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