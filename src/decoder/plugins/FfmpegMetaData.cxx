/*
 * Copyright 2003-2016 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* necessary because libavutil/common.h uses UINT64_C */
#define __STDC_CONSTANT_MACROS

#include "config.h"
#include "FfmpegMetaData.hxx"
#include "tag/TagTable.hxx"
#include "tag/TagHandler.hxx"
#include "../DecoderError.hxx"
#include "Log.hxx"

#include <string.h>
#include <regex.h>

extern "C" {
#include <libavutil/dict.h>
}

static constexpr struct tag_table ffmpeg_tags[] = {
	{ "year", TAG_DATE },
	{ "author-sort", TAG_ARTIST_SORT },
	{ "album_artist", TAG_ALBUM_ARTIST },
	{ "album_artist-sort", TAG_ALBUM_ARTIST_SORT },

	/* sentinel */
	{ nullptr, TAG_NUM_OF_ITEM_TYPES }
};

static void
FfmpegScanTag(TagType type,
	      AVDictionary *m, const char *name,
	      const TagHandler &handler, void *handler_ctx)
{
	AVDictionaryEntry *mt = nullptr;

	while ((mt = av_dict_get(m, name, mt, 0)) != nullptr)
		tag_handler_invoke_tag(handler, handler_ctx,
				       type, mt->value);
}

#define STR_SIZE 256
#define REGEX_MATCHES_MAX 2

static void
mildred_songid_from_ffmpeg_comment(const char *comment,
				   const TagHandler &handler,
				   void *handler_ctx)
{
	const char *pat = "Mildred Songid: \\([a-fA-F0-9-]*\\)";
	regex_t re;
	regmatch_t pmatch[REGEX_MATCHES_MAX];
	char str[STR_SIZE];
	int ret;

	if (0 != (ret = regcomp(&re, pat, 0))) {
		LogError(decoder_domain, "Error compiling regex");
		return;
	}

	if (0 != (ret = regexec(&re, comment, REGEX_MATCHES_MAX, pmatch, 0))) {
		regerror(ret, &re, str, STR_SIZE);
		FormatErrno(decoder_domain,
			    "Error executing regex match: \"%s\"", str);
		return;
	}

	strncpy(str,
		comment + pmatch[1].rm_so,
		(1 + pmatch[1].rm_eo - pmatch[1].rm_so));
	tag_handler_invoke_tag(handler, handler_ctx,
			       TAG_MILDRED_SONGID, str);
	regfree(&re);
}

static void
FfmpegScanPairs(AVDictionary *dict,
		const TagHandler &handler, void *handler_ctx)
{
	AVDictionaryEntry *i = nullptr;

	while ((i = av_dict_get(dict, "", i, AV_DICT_IGNORE_SUFFIX)) != nullptr) {

		tag_handler_invoke_pair(handler, handler_ctx,
					i->key, i->value);
		if (strcmp("comment", i->key) == 0) {
			mildred_songid_from_ffmpeg_comment(i->value,
							   handler,
							   handler_ctx);
		}
        }
}

void
FfmpegScanDictionary(AVDictionary *dict,
		     const TagHandler &handler, void *handler_ctx)
{
	if (handler.tag != nullptr) {
		for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
			FfmpegScanTag(TagType(i), dict, tag_item_names[i],
				      handler, handler_ctx);

		for (const struct tag_table *i = ffmpeg_tags;
		     i->name != nullptr; ++i)
			FfmpegScanTag(i->type, dict, i->name,
				      handler, handler_ctx);
	}

	if (handler.pair != nullptr)
		FfmpegScanPairs(dict, handler, handler_ctx);
}
