/*
 * fontconfig/src/fcfreetype.c
 *
 * Copyright © 2001 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "fcint.h"

#include "fcftint.h"

#include <fcntl.h>
#include <ft2build.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_TAGS_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include FT_TYPE1_TABLES_H
#if HAVE_FT_GET_X11_FONT_FORMAT
#  include FT_XFREE86_H
#endif
#if HAVE_FT_GET_BDF_PROPERTY
#  include FT_BDF_H
#  include FT_MODULE_H
#endif
#include FT_MULTIPLE_MASTERS_H

#include "fcfoundry.h"
#include "ftglue.h"

typedef struct {
    const FT_UShort platform_id;
    const FT_UShort encoding_id;
    const char      fromcode[12];
} FcFtEncoding;

#define TT_ENCODING_DONT_CARE 0xffff
#define FC_ENCODING_MAC_ROMAN "MACINTOSH"

static const FcFtEncoding fcFtEncoding[] = {
    { TT_PLATFORM_APPLE_UNICODE, TT_ENCODING_DONT_CARE, "UTF-16BE"   },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_ID_ROMAN,       "MACINTOSH"  },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_ID_JAPANESE,    "SJIS"       },
    { TT_PLATFORM_MICROSOFT,     TT_MS_ID_SYMBOL_CS,    "UTF-16BE"   },
    { TT_PLATFORM_MICROSOFT,     TT_MS_ID_UNICODE_CS,   "UTF-16BE"   },
    { TT_PLATFORM_MICROSOFT,     TT_MS_ID_SJIS,         "SJIS-WIN"   },
    { TT_PLATFORM_MICROSOFT,     TT_MS_ID_PRC,          "GB18030"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_ID_BIG_5,        "BIG-5"      },
    { TT_PLATFORM_MICROSOFT,     TT_MS_ID_WANSUNG,      "Wansung"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_ID_JOHAB,        "Johab"      },
    { TT_PLATFORM_MICROSOFT,     TT_MS_ID_UCS_4,        "UTF-16BE"   },
    { TT_PLATFORM_ISO,           TT_ISO_ID_7BIT_ASCII,  "ASCII"      },
    { TT_PLATFORM_ISO,           TT_ISO_ID_10646,       "UTF-16BE"   },
    { TT_PLATFORM_ISO,           TT_ISO_ID_8859_1,      "ISO-8859-1" },
};

#define NUM_FC_FT_ENCODING (int)(sizeof (fcFtEncoding) / sizeof (fcFtEncoding[0]))

typedef struct {
    const FT_UShort platform_id;
    const FT_UShort language_id;
    const char      lang[8];
} FcFtLanguage;

#define TT_LANGUAGE_DONT_CARE 0xffff

static const FcFtLanguage fcFtLanguage[] = {
    { TT_PLATFORM_APPLE_UNICODE, TT_LANGUAGE_DONT_CARE,                         ""      },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_ENGLISH,                         "en"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_FRENCH,                          "fr"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_GERMAN,                          "de"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_ITALIAN,                         "it"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_DUTCH,                           "nl"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_SWEDISH,                         "sv"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_SPANISH,                         "es"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_DANISH,                          "da"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_PORTUGUESE,                      "pt"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_NORWEGIAN,                       "no"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_HEBREW,                          "he"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_JAPANESE,                        "ja"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_ARABIC,                          "ar"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_FINNISH,                         "fi"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_GREEK,                           "el"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_ICELANDIC,                       "is"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_MALTESE,                         "mt"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_TURKISH,                         "tr"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_CROATIAN,                        "hr"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_CHINESE_TRADITIONAL,             "zh-tw" },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_URDU,                            "ur"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_HINDI,                           "hi"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_THAI,                            "th"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_KOREAN,                          "ko"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_LITHUANIAN,                      "lt"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_POLISH,                          "pl"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_HUNGARIAN,                       "hu"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_ESTONIAN,                        "et"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_LETTISH,                         "lv"    },
    /* {  TT_PLATFORM_MACINTOSH,	TT_MAC_LANGID_SAAMISK, ??? */
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_FAEROESE,                        "fo"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_FARSI,                           "fa"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_RUSSIAN,                         "ru"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_CHINESE_SIMPLIFIED,              "zh-cn" },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_FLEMISH,                         "nl"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_IRISH,                           "ga"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_ALBANIAN,                        "sq"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_ROMANIAN,                        "ro"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_CZECH,                           "cs"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_SLOVAK,                          "sk"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_SLOVENIAN,                       "sl"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_YIDDISH,                         "yi"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_SERBIAN,                         "sr"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_MACEDONIAN,                      "mk"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_BULGARIAN,                       "bg"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_UKRAINIAN,                       "uk"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_BYELORUSSIAN,                    "be"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_UZBEK,                           "uz"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_KAZAKH,                          "kk"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_AZERBAIJANI,                     "az"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_AZERBAIJANI_CYRILLIC_SCRIPT,     "az"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_AZERBAIJANI_ARABIC_SCRIPT,       "ar"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_ARMENIAN,                        "hy"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_GEORGIAN,                        "ka"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_MOLDAVIAN,                       "mo"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_KIRGHIZ,                         "ky"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_TAJIKI,                          "tg"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_TURKMEN,                         "tk"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_MONGOLIAN,                       "mn"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_MONGOLIAN_MONGOLIAN_SCRIPT,      "mn"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_MONGOLIAN_CYRILLIC_SCRIPT,       "mn"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_PASHTO,                          "ps"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_KURDISH,                         "ku"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_KASHMIRI,                        "ks"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_SINDHI,                          "sd"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_TIBETAN,                         "bo"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_NEPALI,                          "ne"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_SANSKRIT,                        "sa"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_MARATHI,                         "mr"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_BENGALI,                         "bn"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_ASSAMESE,                        "as"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_GUJARATI,                        "gu"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_PUNJABI,                         "pa"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_ORIYA,                           "or"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_MALAYALAM,                       "ml"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_KANNADA,                         "kn"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_TAMIL,                           "ta"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_TELUGU,                          "te"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_SINHALESE,                       "si"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_BURMESE,                         "my"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_KHMER,                           "km"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_LAO,                             "lo"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_VIETNAMESE,                      "vi"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_INDONESIAN,                      "id"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_TAGALOG,                         "tl"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_MALAY_ROMAN_SCRIPT,              "ms"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_MALAY_ARABIC_SCRIPT,             "ms"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_AMHARIC,                         "am"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_TIGRINYA,                        "ti"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_GALLA,                           "om"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_SOMALI,                          "so"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_SWAHILI,                         "sw"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_RUANDA,                          "rw"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_RUNDI,                           "rn"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_CHEWA,                           "ny"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_MALAGASY,                        "mg"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_ESPERANTO,                       "eo"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_WELSH,                           "cy"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_BASQUE,                          "eu"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_CATALAN,                         "ca"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_LATIN,                           "la"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_QUECHUA,                         "qu"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_GUARANI,                         "gn"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_AYMARA,                          "ay"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_TATAR,                           "tt"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_UIGHUR,                          "ug"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_DZONGKHA,                        "dz"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_JAVANESE,                        "jw"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_SUNDANESE,                       "su"    },

#if 0  /* these seem to be errors that have been dropped */

 {  TT_PLATFORM_MACINTOSH,	TT_MAC_LANGID_SCOTTISH_GAELIC },
 {  TT_PLATFORM_MACINTOSH,	TT_MAC_LANGID_IRISH_GAELIC },

#endif

    /* The following codes are new as of 2000-03-10 */
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_GALICIAN,                        "gl"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_AFRIKAANS,                       "af"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_BRETON,                          "br"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_INUKTITUT,                       "iu"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_SCOTTISH_GAELIC,                 "gd"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_MANX_GAELIC,                     "gv"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_IRISH_GAELIC,                    "ga"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_TONGAN,                          "to"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_GREEK_POLYTONIC,                 "el"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_GREELANDIC,                      "ik"    },
    { TT_PLATFORM_MACINTOSH,     TT_MAC_LANGID_AZERBAIJANI_ROMAN_SCRIPT,        "az"    },

    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_SAUDI_ARABIA,              "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_IRAQ,                      "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_EGYPT,                     "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_LIBYA,                     "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_ALGERIA,                   "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_MOROCCO,                   "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_TUNISIA,                   "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_OMAN,                      "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_YEMEN,                     "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_SYRIA,                     "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_JORDAN,                    "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_LEBANON,                   "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_KUWAIT,                    "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_UAE,                       "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_BAHRAIN,                   "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_QATAR,                     "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_BULGARIAN_BULGARIA,               "bg"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_CATALAN_SPAIN,                    "ca"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_CHINESE_TAIWAN,                   "zh-tw" },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_CHINESE_PRC,                      "zh-cn" },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_CHINESE_HONG_KONG,                "zh-hk" },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_CHINESE_SINGAPORE,                "zh-sg" },

    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_CHINESE_MACAU,                    "zh-mo" },

    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_CZECH_CZECH_REPUBLIC,             "cs"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_DANISH_DENMARK,                   "da"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_GERMAN_GERMANY,                   "de"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_GERMAN_SWITZERLAND,               "de"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_GERMAN_AUSTRIA,                   "de"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_GERMAN_LUXEMBOURG,                "de"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_GERMAN_LIECHTENSTEI,              "de"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_GREEK_GREECE,                     "el"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_UNITED_STATES,            "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_UNITED_KINGDOM,           "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_AUSTRALIA,                "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_CANADA,                   "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_NEW_ZEALAND,              "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_IRELAND,                  "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_SOUTH_AFRICA,             "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_JAMAICA,                  "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_CARIBBEAN,                "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_BELIZE,                   "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_TRINIDAD,                 "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_ZIMBABWE,                 "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_PHILIPPINES,              "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_SPAIN_TRADITIONAL_SORT,   "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_MEXICO,                   "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_SPAIN_INTERNATIONAL_SORT, "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_GUATEMALA,                "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_COSTA_RICA,               "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_PANAMA,                   "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_DOMINICAN_REPUBLIC,       "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_VENEZUELA,                "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_COLOMBIA,                 "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_PERU,                     "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_ARGENTINA,                "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_ECUADOR,                  "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_CHILE,                    "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_URUGUAY,                  "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_PARAGUAY,                 "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_BOLIVIA,                  "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_EL_SALVADOR,              "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_HONDURAS,                 "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_NICARAGUA,                "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_PUERTO_RICO,              "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FINNISH_FINLAND,                  "fi"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_FRANCE,                    "fr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_BELGIUM,                   "fr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_CANADA,                    "fr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_SWITZERLAND,               "fr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_LUXEMBOURG,                "fr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_MONACO,                    "fr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_HEBREW_ISRAEL,                    "he"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_HUNGARIAN_HUNGARY,                "hu"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ICELANDIC_ICELAND,                "is"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ITALIAN_ITALY,                    "it"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ITALIAN_SWITZERLAND,              "it"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_JAPANESE_JAPAN,                   "ja"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_KOREAN_EXTENDED_WANSUNG_KOREA,    "ko"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_KOREAN_JOHAB_KOREA,               "ko"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_DUTCH_NETHERLANDS,                "nl"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_DUTCH_BELGIUM,                    "nl"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_NORWEGIAN_NORWAY_BOKMAL,          "no"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_NORWEGIAN_NORWAY_NYNORSK,         "nn"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_POLISH_POLAND,                    "pl"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_PORTUGUESE_BRAZIL,                "pt"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_PORTUGUESE_PORTUGAL,              "pt"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_RHAETO_ROMANIC_SWITZERLAND,       "rm"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ROMANIAN_ROMANIA,                 "ro"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_MOLDAVIAN_MOLDAVIA,               "mo"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_RUSSIAN_RUSSIA,                   "ru"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_RUSSIAN_MOLDAVIA,                 "ru"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_CROATIAN_CROATIA,                 "hr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SERBIAN_SERBIA_LATIN,             "sr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SERBIAN_SERBIA_CYRILLIC,          "sr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SLOVAK_SLOVAKIA,                  "sk"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ALBANIAN_ALBANIA,                 "sq"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SWEDISH_SWEDEN,                   "sv"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SWEDISH_FINLAND,                  "sv"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_THAI_THAILAND,                    "th"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_TURKISH_TURKEY,                   "tr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_URDU_PAKISTAN,                    "ur"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_INDONESIAN_INDONESIA,             "id"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_UKRAINIAN_UKRAINE,                "uk"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_BELARUSIAN_BELARUS,               "be"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SLOVENE_SLOVENIA,                 "sl"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ESTONIAN_ESTONIA,                 "et"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_LATVIAN_LATVIA,                   "lv"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_LITHUANIAN_LITHUANIA,             "lt"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_CLASSIC_LITHUANIAN_LITHUANIA,     "lt"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_MAORI_NEW_ZEALAND,                "mi"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FARSI_IRAN,                       "fa"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_VIETNAMESE_VIET_NAM,              "vi"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARMENIAN_ARMENIA,                 "hy"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_AZERI_AZERBAIJAN_LATIN,           "az"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_AZERI_AZERBAIJAN_CYRILLIC,        "az"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_BASQUE_SPAIN,                     "eu"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SORBIAN_GERMANY,                  "wen"   },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_MACEDONIAN_MACEDONIA,             "mk"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SUTU_SOUTH_AFRICA,                "st"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_TSONGA_SOUTH_AFRICA,              "ts"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_TSWANA_SOUTH_AFRICA,              "tn"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_VENDA_SOUTH_AFRICA,               "ven"   },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_XHOSA_SOUTH_AFRICA,               "xh"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ZULU_SOUTH_AFRICA,                "zu"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_AFRIKAANS_SOUTH_AFRICA,           "af"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_GEORGIAN_GEORGIA,                 "ka"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FAEROESE_FAEROE_ISLANDS,          "fo"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_HINDI_INDIA,                      "hi"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_MALTESE_MALTA,                    "mt"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SAAMI_LAPONIA,                    "se"    },

    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SCOTTISH_GAELIC_UNITED_KINGDOM,   "gd"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_IRISH_GAELIC_IRELAND,             "ga"    },

    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_MALAY_MALAYSIA,                   "ms"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_MALAY_BRUNEI_DARUSSALAM,          "ms"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_KAZAK_KAZAKSTAN,                  "kk"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SWAHILI_KENYA,                    "sw"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_UZBEK_UZBEKISTAN_LATIN,           "uz"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_UZBEK_UZBEKISTAN_CYRILLIC,        "uz"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_TATAR_TATARSTAN,                  "tt"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_BENGALI_INDIA,                    "bn"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_PUNJABI_INDIA,                    "pa"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_GUJARATI_INDIA,                   "gu"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ORIYA_INDIA,                      "or"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_TAMIL_INDIA,                      "ta"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_TELUGU_INDIA,                     "te"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_KANNADA_INDIA,                    "kn"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_MALAYALAM_INDIA,                  "ml"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ASSAMESE_INDIA,                   "as"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_MARATHI_INDIA,                    "mr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SANSKRIT_INDIA,                   "sa"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_KONKANI_INDIA,                    "kok"   },

    /* new as of 2001-01-01 */
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ARABIC_GENERAL,                   "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_CHINESE_GENERAL,                  "zh"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_GENERAL,                  "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_WEST_INDIES,               "fr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_REUNION,                   "fr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_CONGO,                     "fr"    },

    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_SENEGAL,                   "fr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_CAMEROON,                  "fr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_COTE_D_IVOIRE,             "fr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_MALI,                      "fr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_BOSNIAN_BOSNIA_HERZEGOVINA,       "bs"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_URDU_INDIA,                       "ur"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_TAJIK_TAJIKISTAN,                 "tg"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_YIDDISH_GERMANY,                  "yi"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_KIRGHIZ_KIRGHIZSTAN,              "ky"    },

    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_TURKMEN_TURKMENISTAN,             "tk"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_MONGOLIAN_MONGOLIA,               "mn"    },

    /* the following seems to be inconsistent;
       here is the current "official" way: */
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_TIBETAN_BHUTAN,                   "bo"    },
    /* and here is what is used by Passport SDK */
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_TIBETAN_CHINA,                    "bo"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_DZONGHKA_BHUTAN,                  "dz"    },
    /* end of inconsistency */

    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_WELSH_WALES,                      "cy"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_KHMER_CAMBODIA,                   "km"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_LAO_LAOS,                         "lo"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_BURMESE_MYANMAR,                  "my"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_GALICIAN_SPAIN,                   "gl"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_MANIPURI_INDIA,                   "mni"   },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SINDHI_INDIA,                     "sd"    },
    /* the following one is only encountered in Microsoft RTF specification */
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_KASHMIRI_PAKISTAN,                "ks"    },
    /* the following one is not in the Passport list, looks like an omission */
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_KASHMIRI_INDIA,                   "ks"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_NEPALI_NEPAL,                     "ne"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_NEPALI_INDIA,                     "ne"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRISIAN_NETHERLANDS,              "fy"    },

    /* new as of 2001-03-01 (from Office Xp) */
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_HONG_KONG,                "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_INDIA,                    "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_MALAYSIA,                 "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_ENGLISH_SINGAPORE,                "en"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SYRIAC_SYRIA,                     "syr"   },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SINHALESE_SRI_LANKA,              "si"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_CHEROKEE_UNITED_STATES,           "chr"   },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_INUKTITUT_CANADA,                 "iu"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_AMHARIC_ETHIOPIA,                 "am"    },
#if 0
 {  TT_PLATFORM_MICROSOFT,	TT_MS_LANGID_TAMAZIGHT_MOROCCO },
 {  TT_PLATFORM_MICROSOFT,	TT_MS_LANGID_TAMAZIGHT_MOROCCO_LATIN },
#endif
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_PASHTO_AFGHANISTAN,               "ps"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FILIPINO_PHILIPPINES,             "phi"   },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_DHIVEHI_MALDIVES,                 "div"   },

    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_OROMO_ETHIOPIA,                   "om"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_TIGRIGNA_ETHIOPIA,                "ti"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_TIGRIGNA_ERYTHREA,                "ti"    },

/* New additions from Windows Xp/Passport SDK 2001-11-10. */

/* don't ask what this one means... It is commented out currently. */
#if 0
 {  TT_PLATFORM_MICROSOFT,	TT_MS_LANGID_GREEK_GREECE2 },
#endif

    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_UNITED_STATES,            "es"    },
    /* The following two IDs blatantly violate MS specs by using a */
    /* sublanguage >,.                                         */
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SPANISH_LATIN_AMERICA,            "es"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_NORTH_AFRICA,              "fr"    },

    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_MOROCCO,                   "fr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_FRENCH_HAITI,                     "fr"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_BENGALI_BANGLADESH,               "bn"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_PUNJABI_ARABIC_PAKISTAN,          "ar"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_MONGOLIAN_MONGOLIA_MONGOLIAN,     "mn"    },
#if 0
 {  TT_PLATFORM_MICROSOFT,	TT_MS_LANGID_EDO_NIGERIA },
 {  TT_PLATFORM_MICROSOFT,	TT_MS_LANGID_FULFULDE_NIGERIA },
 {  TT_PLATFORM_MICROSOFT,	TT_MS_LANGID_IBIBIO_NIGERIA },
#endif
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_HAUSA_NIGERIA,                    "ha"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_YORUBA_NIGERIA,                   "yo"    },
    /* language codes from, to, are (still) unknown. */
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_IGBO_NIGERIA,                     "ibo"   },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_KANURI_NIGERIA,                   "kau"   },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_GUARANI_PARAGUAY,                 "gn"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_HAWAIIAN_UNITED_STATES,           "haw"   },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_LATIN,                            "la"    },
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_SOMALI_SOMALIA,                   "so"    },
#if 0
  /* Note: Yi does not have a (proper) ISO 639-2 code, since it is mostly */
  /*       not written (but OTOH the peculiar writing system is worth     */
  /*       studying).                                                     */
 {  TT_PLATFORM_MICROSOFT,	TT_MS_LANGID_YI_CHINA },
#endif
    { TT_PLATFORM_MICROSOFT,     TT_MS_LANGID_PAPIAMENTU_NETHERLANDS_ANTILLES,  "pap"   },
};

#define NUM_FC_FT_LANGUAGE (int)(sizeof (fcFtLanguage) / sizeof (fcFtLanguage[0]))

typedef struct {
    FT_UShort language_id;
    char      fromcode[12];
} FcMacRomanFake;

static const FcMacRomanFake fcMacRomanFake[] = {
    { TT_MS_LANGID_JAPANESE_JAPAN,        "SJIS-WIN" },
    { TT_MS_LANGID_ENGLISH_UNITED_STATES, "ASCII"    },
};

static const char fcSilfCapability[] = "ttable:Silf";

static FcChar8 *
FcFontCapabilities (FT_Face face);

static FcBool
FcFontHasHint (FT_Face face);

static int
FcFreeTypeSpacing (FT_Face face);

#define NUM_FC_MAC_ROMAN_FAKE (int)(sizeof (fcMacRomanFake) / sizeof (fcMacRomanFake[0]))

/* From http://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/ROMAN.TXT */
static const FcChar16 fcMacRomanNonASCIIToUnicode[128] = {
    /*0x80*/ 0x00C4, /* LATIN CAPITAL LETTER A WITH DIAERESIS */
    /*0x81*/ 0x00C5, /* LATIN CAPITAL LETTER A WITH RING ABOVE */
    /*0x82*/ 0x00C7, /* LATIN CAPITAL LETTER C WITH CEDILLA */
    /*0x83*/ 0x00C9, /* LATIN CAPITAL LETTER E WITH ACUTE */
    /*0x84*/ 0x00D1, /* LATIN CAPITAL LETTER N WITH TILDE */
    /*0x85*/ 0x00D6, /* LATIN CAPITAL LETTER O WITH DIAERESIS */
    /*0x86*/ 0x00DC, /* LATIN CAPITAL LETTER U WITH DIAERESIS */
    /*0x87*/ 0x00E1, /* LATIN SMALL LETTER A WITH ACUTE */
    /*0x88*/ 0x00E0, /* LATIN SMALL LETTER A WITH GRAVE */
    /*0x89*/ 0x00E2, /* LATIN SMALL LETTER A WITH CIRCUMFLEX */
    /*0x8A*/ 0x00E4, /* LATIN SMALL LETTER A WITH DIAERESIS */
    /*0x8B*/ 0x00E3, /* LATIN SMALL LETTER A WITH TILDE */
    /*0x8C*/ 0x00E5, /* LATIN SMALL LETTER A WITH RING ABOVE */
    /*0x8D*/ 0x00E7, /* LATIN SMALL LETTER C WITH CEDILLA */
    /*0x8E*/ 0x00E9, /* LATIN SMALL LETTER E WITH ACUTE */
    /*0x8F*/ 0x00E8, /* LATIN SMALL LETTER E WITH GRAVE */
    /*0x90*/ 0x00EA, /* LATIN SMALL LETTER E WITH CIRCUMFLEX */
    /*0x91*/ 0x00EB, /* LATIN SMALL LETTER E WITH DIAERESIS */
    /*0x92*/ 0x00ED, /* LATIN SMALL LETTER I WITH ACUTE */
    /*0x93*/ 0x00EC, /* LATIN SMALL LETTER I WITH GRAVE */
    /*0x94*/ 0x00EE, /* LATIN SMALL LETTER I WITH CIRCUMFLEX */
    /*0x95*/ 0x00EF, /* LATIN SMALL LETTER I WITH DIAERESIS */
    /*0x96*/ 0x00F1, /* LATIN SMALL LETTER N WITH TILDE */
    /*0x97*/ 0x00F3, /* LATIN SMALL LETTER O WITH ACUTE */
    /*0x98*/ 0x00F2, /* LATIN SMALL LETTER O WITH GRAVE */
    /*0x99*/ 0x00F4, /* LATIN SMALL LETTER O WITH CIRCUMFLEX */
    /*0x9A*/ 0x00F6, /* LATIN SMALL LETTER O WITH DIAERESIS */
    /*0x9B*/ 0x00F5, /* LATIN SMALL LETTER O WITH TILDE */
    /*0x9C*/ 0x00FA, /* LATIN SMALL LETTER U WITH ACUTE */
    /*0x9D*/ 0x00F9, /* LATIN SMALL LETTER U WITH GRAVE */
    /*0x9E*/ 0x00FB, /* LATIN SMALL LETTER U WITH CIRCUMFLEX */
    /*0x9F*/ 0x00FC, /* LATIN SMALL LETTER U WITH DIAERESIS */
    /*0xA0*/ 0x2020, /* DAGGER */
    /*0xA1*/ 0x00B0, /* DEGREE SIGN */
    /*0xA2*/ 0x00A2, /* CENT SIGN */
    /*0xA3*/ 0x00A3, /* POUND SIGN */
    /*0xA4*/ 0x00A7, /* SECTION SIGN */
    /*0xA5*/ 0x2022, /* BULLET */
    /*0xA6*/ 0x00B6, /* PILCROW SIGN */
    /*0xA7*/ 0x00DF, /* LATIN SMALL LETTER SHARP S */
    /*0xA8*/ 0x00AE, /* REGISTERED SIGN */
    /*0xA9*/ 0x00A9, /* COPYRIGHT SIGN */
    /*0xAA*/ 0x2122, /* TRADE MARK SIGN */
    /*0xAB*/ 0x00B4, /* ACUTE ACCENT */
    /*0xAC*/ 0x00A8, /* DIAERESIS */
    /*0xAD*/ 0x2260, /* NOT EQUAL TO */
    /*0xAE*/ 0x00C6, /* LATIN CAPITAL LETTER AE */
    /*0xAF*/ 0x00D8, /* LATIN CAPITAL LETTER O WITH STROKE */
    /*0xB0*/ 0x221E, /* INFINITY */
    /*0xB1*/ 0x00B1, /* PLUS-MINUS SIGN */
    /*0xB2*/ 0x2264, /* LESS-THAN OR EQUAL TO */
    /*0xB3*/ 0x2265, /* GREATER-THAN OR EQUAL TO */
    /*0xB4*/ 0x00A5, /* YEN SIGN */
    /*0xB5*/ 0x00B5, /* MICRO SIGN */
    /*0xB6*/ 0x2202, /* PARTIAL DIFFERENTIAL */
    /*0xB7*/ 0x2211, /* N-ARY SUMMATION */
    /*0xB8*/ 0x220F, /* N-ARY PRODUCT */
    /*0xB9*/ 0x03C0, /* GREEK SMALL LETTER PI */
    /*0xBA*/ 0x222B, /* INTEGRAL */
    /*0xBB*/ 0x00AA, /* FEMININE ORDINAL INDICATOR */
    /*0xBC*/ 0x00BA, /* MASCULINE ORDINAL INDICATOR */
    /*0xBD*/ 0x03A9, /* GREEK CAPITAL LETTER OMEGA */
    /*0xBE*/ 0x00E6, /* LATIN SMALL LETTER AE */
    /*0xBF*/ 0x00F8, /* LATIN SMALL LETTER O WITH STROKE */
    /*0xC0*/ 0x00BF, /* INVERTED QUESTION MARK */
    /*0xC1*/ 0x00A1, /* INVERTED EXCLAMATION MARK */
    /*0xC2*/ 0x00AC, /* NOT SIGN */
    /*0xC3*/ 0x221A, /* SQUARE ROOT */
    /*0xC4*/ 0x0192, /* LATIN SMALL LETTER F WITH HOOK */
    /*0xC5*/ 0x2248, /* ALMOST EQUAL TO */
    /*0xC6*/ 0x2206, /* INCREMENT */
    /*0xC7*/ 0x00AB, /* LEFT-POINTING DOUBLE ANGLE QUOTATION MARK */
    /*0xC8*/ 0x00BB, /* RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK */
    /*0xC9*/ 0x2026, /* HORIZONTAL ELLIPSIS */
    /*0xCA*/ 0x00A0, /* NO-BREAK SPACE */
    /*0xCB*/ 0x00C0, /* LATIN CAPITAL LETTER A WITH GRAVE */
    /*0xCC*/ 0x00C3, /* LATIN CAPITAL LETTER A WITH TILDE */
    /*0xCD*/ 0x00D5, /* LATIN CAPITAL LETTER O WITH TILDE */
    /*0xCE*/ 0x0152, /* LATIN CAPITAL LIGATURE OE */
    /*0xCF*/ 0x0153, /* LATIN SMALL LIGATURE OE */
    /*0xD0*/ 0x2013, /* EN DASH */
    /*0xD1*/ 0x2014, /* EM DASH */
    /*0xD2*/ 0x201C, /* LEFT DOUBLE QUOTATION MARK */
    /*0xD3*/ 0x201D, /* RIGHT DOUBLE QUOTATION MARK */
    /*0xD4*/ 0x2018, /* LEFT SINGLE QUOTATION MARK */
    /*0xD5*/ 0x2019, /* RIGHT SINGLE QUOTATION MARK */
    /*0xD6*/ 0x00F7, /* DIVISION SIGN */
    /*0xD7*/ 0x25CA, /* LOZENGE */
    /*0xD8*/ 0x00FF, /* LATIN SMALL LETTER Y WITH DIAERESIS */
    /*0xD9*/ 0x0178, /* LATIN CAPITAL LETTER Y WITH DIAERESIS */
    /*0xDA*/ 0x2044, /* FRACTION SLASH */
    /*0xDB*/ 0x20AC, /* EURO SIGN */
    /*0xDC*/ 0x2039, /* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
    /*0xDD*/ 0x203A, /* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
    /*0xDE*/ 0xFB01, /* LATIN SMALL LIGATURE FI */
    /*0xDF*/ 0xFB02, /* LATIN SMALL LIGATURE FL */
    /*0xE0*/ 0x2021, /* DOUBLE DAGGER */
    /*0xE1*/ 0x00B7, /* MIDDLE DOT */
    /*0xE2*/ 0x201A, /* SINGLE LOW-9 QUOTATION MARK */
    /*0xE3*/ 0x201E, /* DOUBLE LOW-9 QUOTATION MARK */
    /*0xE4*/ 0x2030, /* PER MILLE SIGN */
    /*0xE5*/ 0x00C2, /* LATIN CAPITAL LETTER A WITH CIRCUMFLEX */
    /*0xE6*/ 0x00CA, /* LATIN CAPITAL LETTER E WITH CIRCUMFLEX */
    /*0xE7*/ 0x00C1, /* LATIN CAPITAL LETTER A WITH ACUTE */
    /*0xE8*/ 0x00CB, /* LATIN CAPITAL LETTER E WITH DIAERESIS */
    /*0xE9*/ 0x00C8, /* LATIN CAPITAL LETTER E WITH GRAVE */
    /*0xEA*/ 0x00CD, /* LATIN CAPITAL LETTER I WITH ACUTE */
    /*0xEB*/ 0x00CE, /* LATIN CAPITAL LETTER I WITH CIRCUMFLEX */
    /*0xEC*/ 0x00CF, /* LATIN CAPITAL LETTER I WITH DIAERESIS */
    /*0xED*/ 0x00CC, /* LATIN CAPITAL LETTER I WITH GRAVE */
    /*0xEE*/ 0x00D3, /* LATIN CAPITAL LETTER O WITH ACUTE */
    /*0xEF*/ 0x00D4, /* LATIN CAPITAL LETTER O WITH CIRCUMFLEX */
    /*0xF0*/ 0xF8FF, /* Apple logo */
    /*0xF1*/ 0x00D2, /* LATIN CAPITAL LETTER O WITH GRAVE */
    /*0xF2*/ 0x00DA, /* LATIN CAPITAL LETTER U WITH ACUTE */
    /*0xF3*/ 0x00DB, /* LATIN CAPITAL LETTER U WITH CIRCUMFLEX */
    /*0xF4*/ 0x00D9, /* LATIN CAPITAL LETTER U WITH GRAVE */
    /*0xF5*/ 0x0131, /* LATIN SMALL LETTER DOTLESS I */
    /*0xF6*/ 0x02C6, /* MODIFIER LETTER CIRCUMFLEX ACCENT */
    /*0xF7*/ 0x02DC, /* SMALL TILDE */
    /*0xF8*/ 0x00AF, /* MACRON */
    /*0xF9*/ 0x02D8, /* BREVE */
    /*0xFA*/ 0x02D9, /* DOT ABOVE */
    /*0xFB*/ 0x02DA, /* RING ABOVE */
    /*0xFC*/ 0x00B8, /* CEDILLA */
    /*0xFD*/ 0x02DD, /* DOUBLE ACUTE ACCENT */
    /*0xFE*/ 0x02DB, /* OGONEK */
    /*0xFF*/ 0x02C7, /* CARON */
};

#if USE_ICONV
#  include <iconv.h>

#  ifdef _WIN32
#    ifdef WINICONV_CONST
#      define FC_ICONV_CONST WINICONV_CONST
#    endif
#  endif
#  ifndef FC_ICONV_CONST
#    define FC_ICONV_CONST
#  endif

#endif

/*
 * A shift-JIS will have many high bits turned on
 */
static FcBool
FcLooksLikeSJIS (FcChar8 *string, int len)
{
    int nhigh = 0, nlow = 0;

    while (len-- > 0) {
	if (*string++ & 0x80)
	    nhigh++;
	else
	    nlow++;
    }
    /*
     * Heuristic -- if more than 1/3 of the bytes have the high-bit set,
     * this is likely to be SJIS and not ROMAN
     */
    if (nhigh * 2 > nlow)
	return FcTrue;
    return FcFalse;
}

static FcChar8 *
FcSfntNameTranscode (FT_SfntName *sname)
{
    int         i;
    const char *fromcode;
#if USE_ICONV
    iconv_t cd;
#endif
    FcChar8 *utf8;
    FcBool   redecoded = FcFalse;

    for (i = 0; i < NUM_FC_FT_ENCODING; i++)
	if (fcFtEncoding[i].platform_id == sname->platform_id &&
	    (fcFtEncoding[i].encoding_id == TT_ENCODING_DONT_CARE ||
	     fcFtEncoding[i].encoding_id == sname->encoding_id))
	    break;
    if (i == NUM_FC_FT_ENCODING)
	return 0;
    fromcode = fcFtEncoding[i].fromcode;

#if USE_ICONV
retry:
#endif
    /*
     * Many names encoded for TT_PLATFORM_MACINTOSH are broken
     * in various ways. Kludge around them.
     */
    if (!strcmp (fromcode, FC_ENCODING_MAC_ROMAN)) {
	if (sname->language_id == TT_MAC_LANGID_ENGLISH &&
	    FcLooksLikeSJIS (sname->string, sname->string_len)) {
	    fromcode = "SJIS";
	} else if (sname->language_id == TT_MAC_LANGID_JAPANESE) {
	    fromcode = "SJIS";
	} else if (sname->language_id >= 0x100) {
	    /*
	     * "real" Mac language IDs are all less than 150.
	     * Names using one of the MS language IDs are assumed
	     * to use an associated encoding (Yes, this is a kludge)
	     */
	    int f;

	    fromcode = NULL;
	    for (f = 0; f < NUM_FC_MAC_ROMAN_FAKE; f++)
		if (fcMacRomanFake[f].language_id == sname->language_id) {
		    fromcode = fcMacRomanFake[f].fromcode;
		    break;
		}
	    if (!fromcode)
		return 0;
	}
    }
    if (!strcmp (fromcode, "UCS-2BE") || !strcmp (fromcode, "UTF-16BE")) {
	FcChar8 *src = sname->string;
	int      src_len = sname->string_len;
	int      len;
	int      wchar;
	int      ilen, olen;
	FcChar8 *u8;
	FcChar32 ucs4;

	/*
	 * Convert Utf16 to Utf8
	 */

	if (!FcUtf16Len (src, FcEndianBig, src_len, &len, &wchar))
	    return 0;

	/*
	 * Allocate plenty of space.  Freed below
	 */
	utf8 = malloc (len * FC_UTF8_MAX_LEN + 1);
	if (!utf8)
	    return 0;

	u8 = utf8;

	while ((ilen = FcUtf16ToUcs4 (src, FcEndianBig, &ucs4, src_len)) > 0) {
	    src_len -= ilen;
	    src += ilen;
	    olen = FcUcs4ToUtf8 (ucs4, u8);
	    u8 += olen;
	}
	*u8 = '\0';
	goto done;
    }
    if (!strcmp (fromcode, "ASCII") || !strcmp (fromcode, "ISO-8859-1")) {
	FcChar8 *src = sname->string;
	int      src_len = sname->string_len;
	int      olen;
	FcChar8 *u8;
	FcChar32 ucs4;

	/*
	 * Convert Latin1 to Utf8. Freed below
	 */
	utf8 = malloc (src_len * 2 + 1);
	if (!utf8)
	    return 0;

	u8 = utf8;
	while (src_len > 0) {
	    ucs4 = *src++;
	    src_len--;
	    olen = FcUcs4ToUtf8 (ucs4, u8);
	    u8 += olen;
	}
	*u8 = '\0';
	goto done;
    }
    if (!strcmp (fromcode, FC_ENCODING_MAC_ROMAN)) {
	FcChar8 *src = sname->string;
	int      src_len = sname->string_len;
	int      olen;
	FcChar8 *u8;
	FcChar32 ucs4;

	/*
	 * Convert Latin1 to Utf8. Freed below
	 */
	utf8 = malloc (src_len * 3 + 1);
	if (!utf8)
	    return 0;

	u8 = utf8;
	while (src_len > 0) {
	    ucs4 = *src++;
	    if (ucs4 >= 128)
		ucs4 = fcMacRomanNonASCIIToUnicode[ucs4 - 128];
	    src_len--;
	    olen = FcUcs4ToUtf8 (ucs4, u8);
	    u8 += olen;
	}
	*u8 = '\0';
	goto done;
    }

#if USE_ICONV
    cd = iconv_open ("UTF-8", fromcode);
    if (cd && cd != (iconv_t)(-1)) {
	size_t in_bytes_left = sname->string_len;
	size_t out_bytes_left = sname->string_len * FC_UTF8_MAX_LEN;
	char  *inbuf, *outbuf;

	utf8 = malloc (out_bytes_left + 1);
	if (!utf8) {
	    iconv_close (cd);
	    return 0;
	}

	outbuf = (char *)utf8;
	inbuf = (char *)sname->string;

	while (in_bytes_left) {
	    size_t did = iconv (cd,
	                        (FC_ICONV_CONST char **)&inbuf, &in_bytes_left,
	                        &outbuf, &out_bytes_left);
	    if (did == (size_t)(-1)) {
		iconv_close (cd);
		free (utf8);
		if (!redecoded) {
		    /* Regard the encoding as UTF-16BE and try again. */
		    redecoded = FcTrue;
		    fromcode = "UTF-16BE";
		    goto retry;
		}
		return 0;
	    }
	}
	iconv_close (cd);
	*outbuf = '\0';
	goto done;
    }
#endif
    return 0;
done:
    if (FcStrCmpIgnoreBlanksAndCase (utf8, (FcChar8 *)"") == 0) {
	free (utf8);
	return 0;
    }
    return utf8;
}

static const FcChar8 *
FcSfntNameLanguage (FT_SfntName *sname)
{
    int       i;
    FT_UShort platform_id = sname->platform_id;
    FT_UShort language_id = sname->language_id;

    /*
     * Many names encoded for TT_PLATFORM_MACINTOSH are broken
     * in various ways. Kludge around them.
     */
    if (platform_id == TT_PLATFORM_MACINTOSH &&
        sname->encoding_id == TT_MAC_ID_ROMAN &&
        FcLooksLikeSJIS (sname->string, sname->string_len)) {
	language_id = TT_MAC_LANGID_JAPANESE;
    }

    for (i = 0; i < NUM_FC_FT_LANGUAGE; i++)
	if (fcFtLanguage[i].platform_id == platform_id &&
	    (fcFtLanguage[i].language_id == TT_LANGUAGE_DONT_CARE ||
	     fcFtLanguage[i].language_id == language_id)) {
	    if (fcFtLanguage[i].lang[0] == '\0')
		return NULL;
	    else
		return (FcChar8 *)fcFtLanguage[i].lang;
	}
    return 0;
}

static const FcChar8 *
FcNoticeFoundry (const FT_String *notice)
{
    int i;

    if (notice)
	for (i = 0; i < NUM_NOTICE_FOUNDRIES; i++) {
	    const char *n = FcNoticeFoundries[i][0];
	    const char *f = FcNoticeFoundries[i][1];

	    if (strstr ((const char *)notice, n))
		return (const FcChar8 *)f;
	}
    return 0;
}

typedef struct _FcStringConst {
    const FcChar8 *name;
    int            value;
} FcStringConst;

static int
FcStringIsConst (const FcChar8       *string,
                 const FcStringConst *c,
                 int                  nc)
{
    int i;

    for (i = 0; i < nc; i++)
	if (FcStrCmpIgnoreBlanksAndCase (string, c[i].name) == 0)
	    return c[i].value;
    return -1;
}

static int
FcStringContainsConst (const FcChar8       *string,
                       const FcStringConst *c,
                       int                  nc)
{
    int i;

    for (i = 0; i < nc; i++) {
	if (c[i].name[0] == '<') {
	    if (FcStrContainsWord (string, c[i].name + 1))
		return c[i].value;
	} else {
	    if (FcStrContainsIgnoreBlanksAndCase (string, c[i].name))
		return c[i].value;
	}
    }
    return -1;
}

typedef FcChar8 *FC8;

static const FcStringConst weightConsts[] = {
    { (FC8) "thin",       FC_WEIGHT_THIN       },
    { (FC8) "extralight", FC_WEIGHT_EXTRALIGHT },
    { (FC8) "ultralight", FC_WEIGHT_ULTRALIGHT },
    { (FC8) "demilight",  FC_WEIGHT_DEMILIGHT  },
    { (FC8) "semilight",  FC_WEIGHT_SEMILIGHT  },
    { (FC8) "light",      FC_WEIGHT_LIGHT      },
    { (FC8) "book",       FC_WEIGHT_BOOK       },
    { (FC8) "regular",    FC_WEIGHT_REGULAR    },
    { (FC8) "normal",     FC_WEIGHT_NORMAL     },
    { (FC8) "medium",     FC_WEIGHT_MEDIUM     },
    { (FC8) "demibold",   FC_WEIGHT_DEMIBOLD   },
    { (FC8) "demi",       FC_WEIGHT_DEMIBOLD   },
    { (FC8) "semibold",   FC_WEIGHT_SEMIBOLD   },
    { (FC8) "extrabold",  FC_WEIGHT_EXTRABOLD  },
    { (FC8) "superbold",  FC_WEIGHT_EXTRABOLD  },
    { (FC8) "ultrabold",  FC_WEIGHT_ULTRABOLD  },
    { (FC8) "bold",       FC_WEIGHT_BOLD       },
    { (FC8) "ultrablack", FC_WEIGHT_ULTRABLACK },
    { (FC8) "superblack", FC_WEIGHT_EXTRABLACK },
    { (FC8) "extrablack", FC_WEIGHT_EXTRABLACK },
    { (FC8) "<ultra",     FC_WEIGHT_ULTRABOLD  }, /* only if a word */
    { (FC8) "black",      FC_WEIGHT_BLACK      },
    { (FC8) "heavy",      FC_WEIGHT_HEAVY      },
};

#define NUM_WEIGHT_CONSTS   (int)(sizeof (weightConsts) / sizeof (weightConsts[0]))

#define FcIsWeight(s)       FcStringIsConst (s, weightConsts, NUM_WEIGHT_CONSTS)
#define FcContainsWeight(s) FcStringContainsConst (s, weightConsts, NUM_WEIGHT_CONSTS)

static const FcStringConst widthConsts[] = {
    { (FC8) "ultracondensed", FC_WIDTH_ULTRACONDENSED },
    { (FC8) "extracondensed", FC_WIDTH_EXTRACONDENSED },
    { (FC8) "semicondensed",  FC_WIDTH_SEMICONDENSED  },
    { (FC8) "condensed",      FC_WIDTH_CONDENSED      }, /* must be after *condensed */
    { (FC8) "normal",         FC_WIDTH_NORMAL         },
    { (FC8) "semiexpanded",   FC_WIDTH_SEMIEXPANDED   },
    { (FC8) "extraexpanded",  FC_WIDTH_EXTRAEXPANDED  },
    { (FC8) "ultraexpanded",  FC_WIDTH_ULTRAEXPANDED  },
    { (FC8) "expanded",       FC_WIDTH_EXPANDED       }, /* must be after *expanded */
    { (FC8) "extended",       FC_WIDTH_EXPANDED       },
};

#define NUM_WIDTH_CONSTS   (int)(sizeof (widthConsts) / sizeof (widthConsts[0]))

#define FcIsWidth(s)       FcStringIsConst (s, widthConsts, NUM_WIDTH_CONSTS)
#define FcContainsWidth(s) FcStringContainsConst (s, widthConsts, NUM_WIDTH_CONSTS)

static const FcStringConst slantConsts[] = {
    { (FC8) "italic",  FC_SLANT_ITALIC  },
    { (FC8) "kursiv",  FC_SLANT_ITALIC  },
    { (FC8) "oblique", FC_SLANT_OBLIQUE },
};

#define NUM_SLANT_CONSTS   (int)(sizeof (slantConsts) / sizeof (slantConsts[0]))

#define FcContainsSlant(s) FcStringContainsConst (s, slantConsts, NUM_SLANT_CONSTS)

static const FcStringConst decorativeConsts[] = {
    { (FC8) "shadow",  FcTrue },
    { (FC8) "caps",    FcTrue },
    { (FC8) "antiqua", FcTrue },
    { (FC8) "romansc", FcTrue },
    { (FC8) "embosed", FcTrue },
    { (FC8) "dunhill", FcTrue },
};

#define NUM_DECORATIVE_CONSTS   (int)(sizeof (decorativeConsts) / sizeof (decorativeConsts[0]))

#define FcContainsDecorative(s) FcStringContainsConst (s, decorativeConsts, NUM_DECORATIVE_CONSTS)

static double
FcGetPixelSize (FT_Face face, int i)
{
#if HAVE_FT_GET_BDF_PROPERTY
    if (face->num_fixed_sizes == 1) {
	BDF_PropertyRec prop;
	int             rc;

	rc = FT_Get_BDF_Property (face, "PIXEL_SIZE", &prop);
	if (rc == 0 && prop.type == BDF_PROPERTY_TYPE_INTEGER)
	    return (double)prop.u.integer;
    }
#endif
    return (double)face->available_sizes[i].y_ppem / 64.0;
}

static FcBool
FcStringInPatternElement (FcPattern *pat, FcObject obj, const FcChar8 *string)
{
    FcPatternIter  iter;
    FcValueListPtr l;

    FcPatternIterStart (pat, &iter);
    if (!FcPatternFindObjectIter (pat, &iter, obj))
	return FcFalse;
    for (l = FcPatternIterGetValues (pat, &iter); l; l = FcValueListNext (l)) {
	FcValue v = FcValueCanonicalize (&l->value);
	if (v.type != FcTypeString)
	    break;
	if (!FcStrCmpIgnoreBlanksAndCase (v.u.s, string))
	    return FcTrue;
    }
    return FcFalse;
}

static const FT_UShort platform_order[] = {
    TT_PLATFORM_MICROSOFT,
    TT_PLATFORM_APPLE_UNICODE,
    TT_PLATFORM_MACINTOSH,
    TT_PLATFORM_ISO,
};
#define NUM_PLATFORM_ORDER (sizeof (platform_order) / sizeof (platform_order[0]))

static const FT_UShort nameid_order[] = {
    TT_NAME_ID_WWS_FAMILY,
    TT_NAME_ID_TYPOGRAPHIC_FAMILY,
    TT_NAME_ID_FONT_FAMILY,
    TT_NAME_ID_MAC_FULL_NAME,
    TT_NAME_ID_FULL_NAME,
    TT_NAME_ID_WWS_SUBFAMILY,
    TT_NAME_ID_TYPOGRAPHIC_SUBFAMILY,
    TT_NAME_ID_FONT_SUBFAMILY,
    TT_NAME_ID_TRADEMARK,
    TT_NAME_ID_MANUFACTURER,
};

#define NUM_NAMEID_ORDER (sizeof (nameid_order) / sizeof (nameid_order[0]))

typedef struct
{
    unsigned int platform_id;
    unsigned int name_id;
    unsigned int encoding_id;
    unsigned int language_id;
    unsigned int idx;
} FcNameMapping;

static FcBool
_is_english (int platform, int language)
{
    FcBool ret = FcFalse;

    switch (platform) {
    case TT_PLATFORM_MACINTOSH:
	ret = language == TT_MAC_LANGID_ENGLISH;
	break;
    case TT_PLATFORM_MICROSOFT:
	ret = language == TT_MS_LANGID_ENGLISH_UNITED_STATES;
	break;
    }
    return ret;
}

static int
name_mapping_cmp (const void *pa, const void *pb)
{
    const FcNameMapping *a = (const FcNameMapping *)pa;
    const FcNameMapping *b = (const FcNameMapping *)pb;

    if (a->platform_id != b->platform_id)
	return (int)a->platform_id - (int)b->platform_id;
    if (a->name_id != b->name_id)
	return (int)a->name_id - (int)b->name_id;
    if (a->encoding_id != b->encoding_id)
	return (int)a->encoding_id - (int)b->encoding_id;
    if (a->language_id != b->language_id)
	return _is_english (a->platform_id, a->language_id) ? -1 : _is_english (b->platform_id, b->language_id) ? 1
	                                                                                                        : (int)a->language_id - (int)b->language_id;
    if (a->idx != b->idx)
	return (int)a->idx - (int)b->idx;

    return 0;
}

static int
FcFreeTypeGetFirstName (const FT_Face  face,
                        unsigned int   platform,
                        unsigned int   nameid,
                        FcNameMapping *mapping,
                        unsigned int   count,
                        FT_SfntName   *sname)
{
    int min = 0, max = (int)count - 1;

    while (min <= max) {
	int mid = (min + max) / 2;

	if (FT_Get_Sfnt_Name (face, mapping[mid].idx, sname) != 0)
	    return FcFalse;

	if (platform < sname->platform_id ||
	    (platform == sname->platform_id &&
	     (nameid < sname->name_id ||
	      (nameid == sname->name_id &&
	       (mid &&
	        platform == mapping[mid - 1].platform_id &&
	        nameid == mapping[mid - 1].name_id)))))
	    max = mid - 1;
	else if (platform > sname->platform_id ||
	         (platform == sname->platform_id &&
	          nameid > sname->name_id))
	    min = mid + 1;
	else
	    return mid;
    }

    return -1;
}

static FcPattern *
FcFreeTypeQueryFaceInternal (const FT_Face   face,
                             const FcChar8  *file,
                             unsigned int    id,
                             FcCharSet     **cs_share,
                             FcLangSet     **ls_share,
                             FcNameMapping **nm_share)
{
    FcPattern     *pat;
    int            slant = -1;
    double         weight = -1;
    double         width = -1;
    FcBool         decorative = FcFalse;
    FcBool         variable = FcFalse;
    FcBool         variable_weight = FcFalse;
    FcBool         variable_width = FcFalse;
    FcBool         variable_size = FcFalse;
    FcCharSet     *cs;
    FcLangSet     *ls;
    FcNameMapping *name_mapping = NULL;
#if 0
    FcChar8	    *family = 0;
#endif
    FcChar8       *complex_, *foundry_ = NULL;
    const FcChar8 *foundry = 0;
    int            spacing;

    /* Support for glyph-variation named-instances. */
    FT_MM_Var          *mmvar = NULL;
    FT_Var_Named_Style *instance = NULL;
    double              weight_mult = 1.0;
    double              width_mult = 1.0;

    TT_OS2 *os2;
#if HAVE_FT_GET_PS_FONT_INFO
    PS_FontInfoRec psfontinfo;
#endif
#if HAVE_FT_GET_BDF_PROPERTY
    BDF_PropertyRec prop;
#endif
    TT_Header     *head;
    const FcChar8 *exclusiveLang = 0;

    int          name_count = 0;
    int          nfamily = 0;
    int          nfamily_lang = 0;
    int          nstyle = 0;
    int          nstyle_lang = 0;
    int          nfullname = 0;
    int          nfullname_lang = 0;
    unsigned int p, n;

    FcChar8 *style = 0;
    int      st;

    FcBool   symbol = FcFalse;
    FT_Error ftresult;
    FcChar8 *canon_file = NULL;

    FcInitDebug(); /* We might be called with no initizalization whatsoever. */

    pat = FcPatternCreate();
    if (!pat)
	goto bail0;

    {
	int has_outline = !!(face->face_flags & FT_FACE_FLAG_SCALABLE);
	int has_color = 0;

	if (!FcPatternObjectAddBool (pat, FC_OUTLINE_OBJECT, has_outline))
	    goto bail1;

	has_color = !!FT_HAS_COLOR (face);
	if (!FcPatternObjectAddBool (pat, FC_COLOR_OBJECT, has_color))
	    goto bail1;

	/* All color fonts are designed to be scaled, even if they only have
	 * bitmap strikes.  Client is responsible to scale the bitmaps.  This
	 * is in contrast to non-color strikes... */
	if (!FcPatternObjectAddBool (pat, FC_SCALABLE_OBJECT, has_outline || has_color))
	    goto bail1;
    }

    ftresult = FT_Get_MM_Var (face, &mmvar);

    if (id >> 16) {
	if (ftresult)
	    goto bail1;

	if (id >> 16 == 0x8000) {
	    /* Query variable font itself. */
	    unsigned int i;
	    for (i = 0; i < mmvar->num_axis; i++) {
		double   min_value = mmvar->axis[i].minimum / (double)(1U << 16);
		double   def_value = mmvar->axis[i].def / (double)(1U << 16);
		double   max_value = mmvar->axis[i].maximum / (double)(1U << 16);
		FcObject obj = FC_INVALID_OBJECT;

		if (min_value > def_value || def_value > max_value || min_value == max_value)
		    continue;

		switch (mmvar->axis[i].tag) {
		case FT_MAKE_TAG ('w', 'g', 'h', 't'):
		    obj = FC_WEIGHT_OBJECT;
		    min_value = FcWeightFromOpenTypeDouble (min_value);
		    max_value = FcWeightFromOpenTypeDouble (max_value);
		    variable_weight = FcTrue;
		    weight = 0; /* To stop looking for weight. */
		    break;

		case FT_MAKE_TAG ('w', 'd', 't', 'h'):
		    obj = FC_WIDTH_OBJECT;
		    /* Values in 'wdth' match Fontconfig FC_WIDTH_* scheme directly. */
		    variable_width = FcTrue;
		    width = 0; /* To stop looking for width. */
		    break;

		case FT_MAKE_TAG ('o', 'p', 's', 'z'):
		    obj = FC_SIZE_OBJECT;
		    /* Values in 'opsz' match Fontconfig FC_SIZE, both are in points. */
		    variable_size = FcTrue;
		    break;
		}

		if (obj != FC_INVALID_OBJECT) {
		    FcRange *r = FcRangeCreateDouble (min_value, max_value);
		    if (!FcPatternObjectAddRange (pat, obj, r)) {
			FcRangeDestroy (r);
			goto bail1;
		    }
		    FcRangeDestroy (r);
		    variable = FcTrue;
		}
	    }

	    if (!variable)
		goto bail1;

	    id &= 0xFFFF;
	} else if ((id >> 16) - 1 < mmvar->num_namedstyles) {
	    /* Pull out weight and width from named-instance. */
	    unsigned int i;

	    instance = &mmvar->namedstyle[(id >> 16) - 1];

	    for (i = 0; i < mmvar->num_axis; i++) {
		double value = instance->coords[i] / (double)(1U << 16);
		double default_value = mmvar->axis[i].def / (double)(1U << 16);
		double mult = default_value ? value / default_value : 1;
		// printf ("named-instance, axis %d tag %lx value %g\n", i, mmvar->axis[i].tag, value);
		switch (mmvar->axis[i].tag) {
		case FT_MAKE_TAG ('w', 'g', 'h', 't'):
		    weight_mult = mult;
		    break;

		case FT_MAKE_TAG ('w', 'd', 't', 'h'):
		    width_mult = mult;
		    break;

		case FT_MAKE_TAG ('o', 'p', 's', 'z'):
		    if (!FcPatternObjectAddDouble (pat, FC_SIZE_OBJECT, value))
			goto bail1;
		    break;
		}
	    }
	} else
	    goto bail1;
    } else {
	if (!ftresult) {
	    unsigned int i;
	    for (i = 0; i < mmvar->num_axis; i++) {
		switch (mmvar->axis[i].tag) {
		case FT_MAKE_TAG ('o', 'p', 's', 'z'):
		    if (!FcPatternObjectAddDouble (pat, FC_SIZE_OBJECT, mmvar->axis[i].def / (double)(1U << 16)))
			goto bail1;
		    variable_size = FcTrue;
		    break;
		}
	    }
	} else {
	    /* ignore an error of FT_Get_MM_Var() */
	}
    }
    if (!FcPatternObjectAddBool (pat, FC_VARIABLE_OBJECT, variable))
	goto bail1;

    /*
     * Get the OS/2 table
     */
    os2 = (TT_OS2 *)FT_Get_Sfnt_Table (face, FT_SFNT_OS2);

    /*
     * Look first in the OS/2 table for the foundry, if
     * not found here, the various notices will be searched for
     * that information, either from the sfnt name tables or
     * the Postscript FontInfo dictionary.  Finally, the
     * BDF properties will queried.
     */

    if (os2 && os2->version != 0xffff) {
	if (os2->achVendID[0] != 0) {
	    foundry_ = (FcChar8 *)malloc (sizeof (os2->achVendID) + 1);
	    memcpy ((void *)foundry_, os2->achVendID, sizeof (os2->achVendID));
	    foundry_[sizeof (os2->achVendID)] = 0;
	    foundry = foundry_;
	}
    }

    if (FcDebug() & FC_DBG_SCANV)
	printf ("\n");
    /*
     * Grub through the name table looking for family
     * and style names.  FreeType makes quite a hash
     * of them
     */
    name_count = FT_Get_Sfnt_Name_Count (face);
    if (nm_share)
	name_mapping = *nm_share;
    if (!name_mapping) {
	int i = 0;
	name_mapping = malloc (name_count * sizeof (FcNameMapping));
	if (!name_mapping)
	    name_count = 0;
	for (i = 0; i < name_count; i++) {
	    FcNameMapping *p = &name_mapping[i];
	    FT_SfntName    sname;
	    if (FT_Get_Sfnt_Name (face, i, &sname) == 0) {
		p->platform_id = sname.platform_id;
		p->name_id = sname.name_id;
		p->encoding_id = sname.encoding_id;
		p->language_id = sname.language_id;
		p->idx = i;
	    } else {
		p->platform_id =
		    p->name_id =
			p->encoding_id =
			    p->language_id =
				p->idx = (unsigned int)-1;
	    }
	}
	qsort (name_mapping, name_count, sizeof (FcNameMapping), name_mapping_cmp);

	if (nm_share)
	    *nm_share = name_mapping;
    }
    for (p = 0; p < NUM_PLATFORM_ORDER; p++) {
	int platform = platform_order[p];

	/*
	 * Order nameids so preferred names appear first
	 * in the resulting list
	 */
	for (n = 0; n < NUM_NAMEID_ORDER; n++) {
	    FT_SfntName    sname;
	    int            nameidx;
	    const FcChar8 *lang;
	    int           *np = 0, *nlangp = 0;
	    size_t         len;
	    int            nameid, lookupid;
	    FcObject       obj = FC_INVALID_OBJECT, objlang = FC_INVALID_OBJECT;

	    nameid = lookupid = nameid_order[n];

	    if (instance) {
		/* For named-instances, we skip regular style nameIDs,
		 * and treat the instance's nameid as FONT_SUBFAMILY.
		 * Postscript name is automatically handled by FreeType. */
		if (nameid == TT_NAME_ID_WWS_SUBFAMILY ||
		    nameid == TT_NAME_ID_TYPOGRAPHIC_SUBFAMILY ||
		    nameid == TT_NAME_ID_FULL_NAME)
		    continue;

		if (nameid == TT_NAME_ID_FONT_SUBFAMILY)
		    lookupid = instance->strid;
	    }

	    nameidx = FcFreeTypeGetFirstName (face, platform, lookupid,
	                                      name_mapping, name_count,
	                                      &sname);
	    if (nameidx == -1)
		continue;
	    do {
		switch (nameid) {
		case TT_NAME_ID_WWS_FAMILY:
		case TT_NAME_ID_TYPOGRAPHIC_FAMILY:
		case TT_NAME_ID_FONT_FAMILY:
#if 0
		case TT_NAME_ID_UNIQUE_ID:
#endif
		    if (FcDebug() & FC_DBG_SCANV)
			printf ("found family (n %2d p %d e %d l 0x%04x)",
			        sname.name_id, sname.platform_id,
			        sname.encoding_id, sname.language_id);

		    obj = FC_FAMILY_OBJECT;
		    objlang = FC_FAMILYLANG_OBJECT;
		    np = &nfamily;
		    nlangp = &nfamily_lang;
		    break;
		case TT_NAME_ID_MAC_FULL_NAME:
		case TT_NAME_ID_FULL_NAME:
		    if (variable)
			break;
		    if (FcDebug() & FC_DBG_SCANV)
			printf ("found full   (n %2d p %d e %d l 0x%04x)",
			        sname.name_id, sname.platform_id,
			        sname.encoding_id, sname.language_id);

		    obj = FC_FULLNAME_OBJECT;
		    objlang = FC_FULLNAMELANG_OBJECT;
		    np = &nfullname;
		    nlangp = &nfullname_lang;
		    break;
		case TT_NAME_ID_WWS_SUBFAMILY:
		case TT_NAME_ID_TYPOGRAPHIC_SUBFAMILY:
		case TT_NAME_ID_FONT_SUBFAMILY:
		    if (variable)
			break;
		    if (FcDebug() & FC_DBG_SCANV)
			printf ("found style  (n %2d p %d e %d l 0x%04x) ",
			        sname.name_id, sname.platform_id,
			        sname.encoding_id, sname.language_id);

		    obj = FC_STYLE_OBJECT;
		    objlang = FC_STYLELANG_OBJECT;
		    np = &nstyle;
		    nlangp = &nstyle_lang;
		    break;
		case TT_NAME_ID_TRADEMARK:
		case TT_NAME_ID_MANUFACTURER:
		    /* If the foundry wasn't found in the OS/2 table, look here */
		    if (!foundry) {
			FcChar8 *utf8;
			utf8 = FcSfntNameTranscode (&sname);
			foundry = FcNoticeFoundry ((FT_String *)utf8);
			free (utf8);
		    }
		    break;
		}
		if (obj != FC_INVALID_OBJECT) {
		    FcChar8 *utf8, *pp;

		    utf8 = FcSfntNameTranscode (&sname);
		    lang = FcSfntNameLanguage (&sname);

		    if (FcDebug() & FC_DBG_SCANV)
			printf ("%s\n", utf8 ? (char *)utf8 : "(null)");

		    if (!utf8)
			continue;

		    /* Trim surrounding whitespace. */
		    pp = utf8;
		    while (*pp == ' ')
			pp++;
		    len = strlen ((const char *)pp);
		    memmove (utf8, pp, len + 1);
		    pp = utf8 + len;
		    while (pp > utf8 &&
			   (*(pp - 1) == ' ' ||
			    *(pp - 1) == '\r' ||
			    *(pp - 1) == '\n'))
			pp--;
		    *pp = 0;

		    if (FcStringInPatternElement (pat, obj, utf8)) {
			free (utf8);
			continue;
		    }

		    /* add new element */
		    if (!FcPatternObjectAddString (pat, obj, utf8)) {
			free (utf8);
			goto bail1;
		    }
		    free (utf8);
		    if (lang) {
			if (!FcPatternObjectAddString (pat, objlang, lang))
			    goto bail1;
			++*nlangp;
		    } else {
			/* Add und as a fallback */
			if (!FcPatternObjectAddString (pat, objlang, (FcChar8 *)"und"))
			    goto bail1;
			++*nlangp;
		    }
		    ++*np;
		}
	    } while (++nameidx < name_count &&
	             FT_Get_Sfnt_Name (face, name_mapping[nameidx].idx, &sname) == 0 &&
	             platform == sname.platform_id && lookupid == sname.name_id);
	}
    }
    if (!nm_share) {
	free (name_mapping);
	name_mapping = NULL;
    }

    if (!nfamily && face->family_name &&
        FcStrCmpIgnoreBlanksAndCase ((FcChar8 *)face->family_name, (FcChar8 *)"") != 0) {
	if (FcDebug() & FC_DBG_SCANV)
	    printf ("using FreeType family \"%s\"\n", face->family_name);
	if (!FcPatternObjectAddString (pat, FC_FAMILY_OBJECT, (FcChar8 *)face->family_name))
	    goto bail1;
	if (!FcPatternObjectAddString (pat, FC_FAMILYLANG_OBJECT, (FcChar8 *)"en"))
	    goto bail1;
	++nfamily;
    }

    if (!variable && !nstyle) {
	const FcChar8 *style_regular = (const FcChar8 *)"Regular";
	const FcChar8 *ss;

	if (face->style_name &&
	    FcStrCmpIgnoreBlanksAndCase ((FcChar8 *)face->style_name, (FcChar8 *)"") != 0) {
	    if (FcDebug() & FC_DBG_SCANV)
		printf ("using FreeType style \"%s\"\n", face->style_name);

	    ss = (const FcChar8 *)face->style_name;
	} else {
	    if (FcDebug() & FC_DBG_SCANV)
		printf ("applying default style Regular\n");
	    ss = style_regular;
	}
	if (!FcPatternObjectAddString (pat, FC_STYLE_OBJECT, ss))
	    goto bail1;
	if (!FcPatternObjectAddString (pat, FC_STYLELANG_OBJECT, (FcChar8 *)"en"))
	    goto bail1;
	++nstyle;
    }

    if (!nfamily && file && *file) {
	FcChar8 *start, *end;
	FcChar8 *family;

	start = (FcChar8 *)strrchr ((char *)file, '/');
	if (start)
	    start++;
	else
	    start = (FcChar8 *)file;
	end = (FcChar8 *)strrchr ((char *)start, '.');
	if (!end)
	    end = start + strlen ((char *)start);
	/* freed below */
	family = malloc (end - start + 1);
	strncpy ((char *)family, (char *)start, end - start);
	family[end - start] = '\0';
	if (FcDebug() & FC_DBG_SCANV)
	    printf ("using filename for family %s\n", family);
	if (!FcPatternObjectAddString (pat, FC_FAMILY_OBJECT, family)) {
	    free (family);
	    goto bail1;
	}
	if (!FcPatternObjectAddString (pat, FC_FAMILYLANG_OBJECT, (FcChar8 *)"en")) {
	    free (family);
	    goto bail1;
	}
	free (family);
	++nfamily;
    }

    /* Add the fullname into the cache */
    if (!variable && !nfullname) {
	FcChar8 *family, *style, *lang = NULL;
	int      n = 0;
	size_t   len, i;
	FcStrBuf sbuf;

	while (FcPatternObjectGetString (pat, FC_FAMILYLANG_OBJECT, n, &lang) == FcResultMatch) {
	    if (FcStrCmp (lang, (const FcChar8 *)"en") == 0)
		break;
	    n++;
	    lang = NULL;
	}
	if (!lang)
	    n = 0;
	if (FcPatternObjectGetString (pat, FC_FAMILY_OBJECT, n, &family) != FcResultMatch)
	    goto bail1;
	len = strlen ((const char *)family);
	for (i = len; i > 0; i--) {
	    if (!isspace (family[i - 1]))
		break;
	}
	family[i] = 0;
	n = 0;
	while (FcPatternObjectGetString (pat, FC_STYLELANG_OBJECT, n, &lang) == FcResultMatch) {
	    if (FcStrCmp (lang, (const FcChar8 *)"en") == 0)
		break;
	    n++;
	    lang = NULL;
	}
	if (!lang)
	    n = 0;
	if (FcPatternObjectGetString (pat, FC_STYLE_OBJECT, n, &style) != FcResultMatch)
	    goto bail1;
	len = strlen ((const char *)style);
	for (i = 0; style[i] != 0 && isspace (style[i]); i++)
	    ;
	memmove (style, &style[i], len - i);
	FcStrBufInit (&sbuf, NULL, 0);
	FcStrBufString (&sbuf, family);
	FcStrBufChar (&sbuf, ' ');
	FcStrBufString (&sbuf, style);
	if (!FcPatternObjectAddString (pat, FC_FULLNAME_OBJECT, FcStrBufDoneStatic (&sbuf))) {
	    FcStrBufDestroy (&sbuf);
	    goto bail1;
	}
	FcStrBufDestroy (&sbuf);
	if (!FcPatternObjectAddString (pat, FC_FULLNAMELANG_OBJECT, (const FcChar8 *)"en"))
	    goto bail1;
	++nfullname;
    }
    /* Add the PostScript name into the cache */
    if (!variable) {
	char        psname[256];
	const char *tmp;

	if (instance)
	    FT_Set_Named_Instance (face, id >> 16);
	tmp = FT_Get_Postscript_Name (face);
	if (!tmp) {
	    unsigned int i;
	    FcChar8     *family, *familylang = NULL;
	    size_t       len;
	    int          n = 0;

	    /* Workaround when FT_Get_Postscript_Name didn't give any name.
	     * try to find out the English family name and convert.
	     */
	    while (FcPatternObjectGetString (pat, FC_FAMILYLANG_OBJECT, n, &familylang) == FcResultMatch) {
		if (FcStrCmp (familylang, (const FcChar8 *)"en") == 0)
		    break;
		n++;
		familylang = NULL;
	    }
	    if (!familylang)
		n = 0;

	    if (FcPatternObjectGetString (pat, FC_FAMILY_OBJECT, n, &family) != FcResultMatch)
		goto bail1;
	    len = strlen ((const char *)family);
	    /* the literal name in PostScript Language is limited to 127 characters though,
	     * It is the architectural limit. so assuming 255 characters may works enough.
	     */
	    for (i = 0; i < len && i < 255; i++) {
		/* those characters are not allowed to be the literal name in PostScript */
		static const char exclusive_chars[] = "\x04()/<>[]{}\t\f\r\n ";

		if (strchr (exclusive_chars, family[i]) != NULL)
		    psname[i] = '-';
		else
		    psname[i] = family[i];
	    }
	    psname[i] = 0;
	} else {
	    strncpy (psname, tmp, 255);
	    psname[255] = 0;
	}
	if (!FcPatternObjectAddString (pat, FC_POSTSCRIPT_NAME_OBJECT, (const FcChar8 *)psname))
	    goto bail1;
    }

    canon_file = FcStrCanonFilename(file);
    if (canon_file && *canon_file && !FcPatternObjectAddString (pat, FC_FILE_OBJECT, canon_file))
	goto bail1;

    if (!FcPatternObjectAddInteger (pat, FC_INDEX_OBJECT, id))
	goto bail1;

#if 0
    /*
     * don't even try this -- CJK 'monospace' fonts are really
     * dual width, and most other fonts don't bother to set
     * the attribute.  Sigh.
     */
    if ((face->face_flags & FT_FACE_FLAG_FIXED_WIDTH) != 0)
	if (!FcPatternObjectAddInteger (pat, FC_SPACING_OBJECT, FC_MONO))
	    goto bail1;
#endif

    /*
     * Find the font revision (if available)
     */
    head = (TT_Header *)FT_Get_Sfnt_Table (face, ft_sfnt_head);
    if (head) {
	if (!FcPatternObjectAddInteger (pat, FC_FONTVERSION_OBJECT, head->Font_Revision))
	    goto bail1;
    } else {
	if (!FcPatternObjectAddInteger (pat, FC_FONTVERSION_OBJECT, 0))
	    goto bail1;
    }
    if (!FcPatternObjectAddInteger (pat, FC_ORDER_OBJECT, 0))
	goto bail1;

    if (os2 && os2->version >= 0x0001 && os2->version != 0xffff) {
	exclusiveLang = FcLangIsExclusiveFromOs2 (os2->ulCodePageRange1, os2->ulCodePageRange2);
    }

    if (os2 && os2->version != 0xffff) {
	weight = os2->usWeightClass;
	weight = FcWeightFromOpenTypeDouble (weight * weight_mult);
	if ((FcDebug() & FC_DBG_SCANV) && weight != -1)
	    printf ("\tos2 weight class %d multiplier %g maps to weight %g\n",
	            os2->usWeightClass, weight_mult, weight);

	switch (os2->usWidthClass) {
	case 1: width = FC_WIDTH_ULTRACONDENSED; break;
	case 2: width = FC_WIDTH_EXTRACONDENSED; break;
	case 3: width = FC_WIDTH_CONDENSED; break;
	case 4: width = FC_WIDTH_SEMICONDENSED; break;
	case 5: width = FC_WIDTH_NORMAL; break;
	case 6: width = FC_WIDTH_SEMIEXPANDED; break;
	case 7: width = FC_WIDTH_EXPANDED; break;
	case 8: width = FC_WIDTH_EXTRAEXPANDED; break;
	case 9: width = FC_WIDTH_ULTRAEXPANDED; break;
	}
	width *= width_mult;
	if ((FcDebug() & FC_DBG_SCANV) && width != -1)
	    printf ("\tos2 width class %d multiplier %g maps to width %g\n",
	            os2->usWidthClass, width_mult, width);
    }
    if (os2 && (complex_ = FcFontCapabilities (face))) {
	if (!FcPatternObjectAddString (pat, FC_CAPABILITY_OBJECT, complex_)) {
	    free (complex_);
	    goto bail1;
	}
	free (complex_);
    }

    if (!FcPatternObjectAddBool (pat, FC_FONT_HAS_HINT_OBJECT, FcFontHasHint (face)))
	goto bail1;

    if (!variable_size && os2 && os2->version >= 0x0005 && os2->version != 0xffff) {
	double   lower_size, upper_size;
	FcRange *r;

	/* usLowerPointSize and usUpperPointSize is actually twips */
	lower_size = os2->usLowerOpticalPointSize / 20.0L;
	upper_size = os2->usUpperOpticalPointSize / 20.0L;

	if (lower_size == upper_size) {
	    if (!FcPatternObjectAddDouble (pat, FC_SIZE_OBJECT, lower_size))
		goto bail1;
	} else {
	    r = FcRangeCreateDouble (lower_size, upper_size);
	    if (!FcPatternObjectAddRange (pat, FC_SIZE_OBJECT, r)) {
		FcRangeDestroy (r);
		goto bail1;
	    }
	    FcRangeDestroy (r);
	}
    }

    /*
     * Type 1: Check for FontInfo dictionary information
     * Code from g2@magestudios.net (Gerard Escalante)
     */

#if HAVE_FT_GET_PS_FONT_INFO
    if (FT_Get_PS_Font_Info (face, &psfontinfo) == 0) {
	if (weight == -1 && psfontinfo.weight) {
	    weight = FcIsWeight ((FcChar8 *)psfontinfo.weight);
	    if (FcDebug() & FC_DBG_SCANV)
		printf ("\tType1 weight %s maps to %g\n",
		        psfontinfo.weight, weight);
	}

#  if 0
	/*
	 * Don't bother with italic_angle; FreeType already extracts that
	 * information for us and sticks it into style_flags
	 */
        if (psfontinfo.italic_angle)
            slant = FC_SLANT_ITALIC;
        else
            slant = FC_SLANT_ROMAN;
#  endif

	if (!foundry)
	    foundry = FcNoticeFoundry (psfontinfo.notice);
    }
#endif /* HAVE_FT_GET_PS_FONT_INFO */

#if HAVE_FT_GET_BDF_PROPERTY
    /*
     * Finally, look for a FOUNDRY BDF property if no other
     * mechanism has managed to locate a foundry
     */

    if (!foundry) {
	int rc;
	rc = FT_Get_BDF_Property (face, "FOUNDRY", &prop);
	if (rc == 0 && prop.type == BDF_PROPERTY_TYPE_ATOM)
	    foundry = (FcChar8 *)prop.u.atom;
    }

    if (width == -1) {
	if (FT_Get_BDF_Property (face, "RELATIVE_SETWIDTH", &prop) == 0 &&
	    (prop.type == BDF_PROPERTY_TYPE_INTEGER ||
	     prop.type == BDF_PROPERTY_TYPE_CARDINAL)) {
	    FT_Int32 value;

	    if (prop.type == BDF_PROPERTY_TYPE_INTEGER)
		value = prop.u.integer;
	    else
		value = (FT_Int32)prop.u.cardinal;
	    switch ((value + 5) / 10) {
	    case 1: width = FC_WIDTH_ULTRACONDENSED; break;
	    case 2: width = FC_WIDTH_EXTRACONDENSED; break;
	    case 3: width = FC_WIDTH_CONDENSED; break;
	    case 4: width = FC_WIDTH_SEMICONDENSED; break;
	    case 5: width = FC_WIDTH_NORMAL; break;
	    case 6: width = FC_WIDTH_SEMIEXPANDED; break;
	    case 7: width = FC_WIDTH_EXPANDED; break;
	    case 8: width = FC_WIDTH_EXTRAEXPANDED; break;
	    case 9: width = FC_WIDTH_ULTRAEXPANDED; break;
	    }
	}
	if (width == -1 &&
	    FT_Get_BDF_Property (face, "SETWIDTH_NAME", &prop) == 0 &&
	    prop.type == BDF_PROPERTY_TYPE_ATOM && prop.u.atom != NULL) {
	    width = FcIsWidth ((FcChar8 *)prop.u.atom);
	    if (FcDebug() & FC_DBG_SCANV)
		printf ("\tsetwidth %s maps to %g\n", prop.u.atom, width);
	}
    }
#endif

    /*
     * Look for weight, width and slant names in the style value
     */
    for (st = 0; FcPatternGetString (pat, FC_STYLE, st, &style) == FcResultMatch; st++) {
	if (weight == -1) {
	    weight = FcContainsWeight (style);
	    if (FcDebug() & FC_DBG_SCANV)
		printf ("\tStyle %s maps to weight %g\n", style, weight);
	}
	if (width == -1) {
	    width = FcContainsWidth (style);
	    if (FcDebug() & FC_DBG_SCANV)
		printf ("\tStyle %s maps to width %g\n", style, width);
	}
	if (slant == -1) {
	    slant = FcContainsSlant (style);
	    if (FcDebug() & FC_DBG_SCANV)
		printf ("\tStyle %s maps to slant %d\n", style, slant);
	}
	if (decorative == FcFalse) {
	    decorative = FcContainsDecorative (style) > 0;
	    if (FcDebug() & FC_DBG_SCANV)
		printf ("\tStyle %s maps to decorative %d\n", style, decorative);
	}
    }
    /*
     * Pull default values from the FreeType flags if more
     * specific values not found above
     */
    if (slant == -1) {
	slant = FC_SLANT_ROMAN;
	if (face->style_flags & FT_STYLE_FLAG_ITALIC)
	    slant = FC_SLANT_ITALIC;
    }

    if (weight == -1) {
	weight = FC_WEIGHT_MEDIUM;
	if (face->style_flags & FT_STYLE_FLAG_BOLD)
	    weight = FC_WEIGHT_BOLD;
    }

    if (width == -1)
	width = FC_WIDTH_NORMAL;

    if (foundry == 0)
	foundry = (FcChar8 *)"unknown";

    if (!FcPatternObjectAddInteger (pat, FC_SLANT_OBJECT, slant))
	goto bail1;

    if (!variable_weight && !FcPatternObjectAddDouble (pat, FC_WEIGHT_OBJECT, weight))
	goto bail1;

    if (!variable_width && !FcPatternObjectAddDouble (pat, FC_WIDTH_OBJECT, width))
	goto bail1;

    if (!FcPatternObjectAddString (pat, FC_FOUNDRY_OBJECT, foundry))
	goto bail1;

    if (!FcPatternObjectAddBool (pat, FC_DECORATIVE_OBJECT, decorative))
	goto bail1;

    /*
     * Compute the unicode coverage for the font
     */
    if (cs_share && *cs_share)
	cs = FcCharSetCopy (*cs_share);
    else {
	cs = FcFreeTypeCharSet (face, NULL);
	if (cs_share)
	    *cs_share = FcCharSetCopy (cs);
    }
    if (!cs)
	goto bail1;

    /* The FcFreeTypeCharSet() chose the encoding; test it for symbol. */
    symbol = face->charmap && face->charmap->encoding == FT_ENCODING_MS_SYMBOL;
    if (!FcPatternObjectAddBool (pat, FC_SYMBOL_OBJECT, symbol))
	goto bail1;

    spacing = FcFreeTypeSpacing (face);
#if HAVE_FT_GET_BDF_PROPERTY
    /* For PCF fonts, override the computed spacing with the one from
       the property */
    if (FT_Get_BDF_Property (face, "SPACING", &prop) == 0 &&
        prop.type == BDF_PROPERTY_TYPE_ATOM && prop.u.atom != NULL) {
	if (!strcmp (prop.u.atom, "c") || !strcmp (prop.u.atom, "C"))
	    spacing = FC_CHARCELL;
	else if (!strcmp (prop.u.atom, "m") || !strcmp (prop.u.atom, "M"))
	    spacing = FC_MONO;
	else if (!strcmp (prop.u.atom, "p") || !strcmp (prop.u.atom, "P"))
	    spacing = FC_PROPORTIONAL;
    }
#endif

    /*
     * Skip over PCF fonts that have no encoded characters; they're
     * usually just Unicode fonts transcoded to some legacy encoding
     * FT forces us to approximate whether a font is a PCF font
     * or not by whether it has any BDF properties.  Try PIXEL_SIZE;
     * I don't know how to get a list of BDF properties on the font. -PL
     */
    if (FcCharSetCount (cs) == 0) {
#if HAVE_FT_GET_BDF_PROPERTY
	if (FT_Get_BDF_Property (face, "PIXEL_SIZE", &prop) == 0)
	    goto bail2;
#endif
    }

    if (!FcPatternObjectAddCharSet (pat, FC_CHARSET_OBJECT, cs))
	goto bail2;

    if (!symbol) {
	if (ls_share && *ls_share)
	    ls = FcLangSetCopy (*ls_share);
	else {
	    ls = FcLangSetFromCharSet (cs, exclusiveLang);
	    if (ls_share)
		*ls_share = FcLangSetCopy (ls);
	}
	if (!ls)
	    goto bail2;
    } else {
	/* Symbol fonts don't cover any language, even though they
	 * claim to support Latin1 range. */
	ls = FcLangSetCreate();
    }

    if (!FcPatternObjectAddLangSet (pat, FC_LANG_OBJECT, ls)) {
	FcLangSetDestroy (ls);
	goto bail2;
    }

    FcLangSetDestroy (ls);

    if (spacing != FC_PROPORTIONAL)
	if (!FcPatternObjectAddInteger (pat, FC_SPACING_OBJECT, spacing))
	    goto bail2;

    if (!(face->face_flags & FT_FACE_FLAG_SCALABLE)) {
	int i;
	for (i = 0; i < face->num_fixed_sizes; i++)
	    if (!FcPatternObjectAddDouble (pat, FC_PIXEL_SIZE_OBJECT,
	                                   FcGetPixelSize (face, i)))
		goto bail2;
	if (!FcPatternObjectAddBool (pat, FC_ANTIALIAS_OBJECT, FcFalse))
	    goto bail2;
    }

    FcChar8 *wrapper = NULL;

#if HAVE_FT_GET_X11_FONT_FORMAT
    /*
     * Use the (not well documented or supported) X-specific function
     * from FreeType to figure out the font format
     */
    {
	const char *font_format = FT_Get_X11_Font_Format (face);
	if (font_format) {
	    if (!FcPatternObjectAddString (pat, FC_FONTFORMAT_OBJECT, (FcChar8 *)font_format))
		goto bail2;

	    /* If this is not a SFNT font and format is CFF, then it is a standlone CFF font */
	    if (!FT_IS_SFNT (face) && !strcmp (font_format, "CFF"))
		wrapper = (FcChar8 *)"CFF";
	}
    }
#endif

    if (!FcPatternObjectAddBool (pat, FC_NAMED_INSTANCE_OBJECT, !!(id > 0xffff)))
	goto bail2;

    if (FT_IS_SFNT (face)) {
	/* If this is an SFNT wrapper, try to sniff the SFNT tag which is the
	 * first 4 bytes, to see if it is a WOFF or WOFF2 wrapper. */
	wrapper = (FcChar8 *)"SFNT";

	char buf[4];
	int  fd = FcOpen ((char *)file, O_RDONLY);
	if (fd != -1 && read (fd, buf, 4)) {
	    if (buf[0] == 'w' && buf[1] == 'O' && buf[2] == 'F') {
		if (buf[3] == 'F')
		    wrapper = (FcChar8 *)"WOFF";
		else if (buf[3] == '2')
		    wrapper = (FcChar8 *)"WOFF2";
	    }
	}
	close (fd);
    }

    if (wrapper)
	if (!FcPatternObjectAddString (pat, FC_FONT_WRAPPER_OBJECT, wrapper))
	    goto bail2;

    /*
     * Drop our reference to the charset
     */
    FcCharSetDestroy (cs);
    if (foundry_)
	free (foundry_);
    if (canon_file)
	free (canon_file);

    if (mmvar) {
#ifdef HAVE_FT_DONE_MM_VAR
	if (face->glyph)
	    FT_Done_MM_Var (face->glyph->library, mmvar);
#else
	free (mmvar);
#endif
    }

    return pat;

bail2:
    FcCharSetDestroy (cs);
bail1:
    FcPatternDestroy (pat);
    if (mmvar) {
#ifdef HAVE_FT_DONE_MM_VAR
	if (face->glyph)
	    FT_Done_MM_Var (face->glyph->library, mmvar);
#else
	free (mmvar);
#endif
    }
    if (!nm_share && name_mapping)
	free (name_mapping);
    if (foundry_)
	free (foundry_);
    if (canon_file)
	free (canon_file);
bail0:
    return NULL;
}

FcPattern *
FcFreeTypeQueryFace (const FT_Face  face,
                     const FcChar8 *file,
                     unsigned int   id,
                     FcBlanks      *blanks FC_UNUSED)
{
    return FcFreeTypeQueryFaceInternal (face, file, id, NULL, NULL, NULL);
}

FcPattern *
FcFreeTypeQuery (const FcChar8 *file,
                 unsigned int   id,
                 FcBlanks      *blanks FC_UNUSED,
                 int           *count)
{
    FT_Face    face;
    FT_Library ftLibrary;
    FcPattern *pat = NULL;

    if (FT_Init_FreeType (&ftLibrary))
	return NULL;

    if (FT_New_Face (ftLibrary, (char *)file, id & 0x7FFFFFFF, &face))
	goto bail;

    if (count)
	*count = face->num_faces;

    pat = FcFreeTypeQueryFaceInternal (face, file, id, NULL, NULL, NULL);

    FT_Done_Face (face);
bail:
    FT_Done_FreeType (ftLibrary);
    return pat;
}

unsigned int
FcFreeTypeQueryAll (const FcChar8 *file,
                    unsigned int   id,
                    FcBlanks      *blanks,
                    int           *count,
                    FcFontSet     *set)
{
    FT_Face        face = NULL;
    FT_Library     ftLibrary = NULL;
    FcCharSet     *cs = NULL;
    FcLangSet     *ls = NULL;
    FcNameMapping *nm = NULL;
    FT_MM_Var     *mm_var = NULL;
    FcBool         index_set = id != (unsigned int)-1;
    unsigned int   set_face_num = index_set ? id & 0xFFFF : 0;
    unsigned int   set_instance_num = index_set ? id >> 16 : 0;
    unsigned int   face_num = set_face_num;
    unsigned int   instance_num = set_instance_num;
    unsigned int   num_faces = 0;
    unsigned int   num_instances = 0;
    unsigned int   ret = 0;
    int            err = 0;

    if (count)
	*count = 0;

    if (FT_Init_FreeType (&ftLibrary))
	return 0;

    if (FT_New_Face (ftLibrary, (const char *)file, face_num, &face))
	goto bail;

    num_faces = face->num_faces;
    num_instances = face->style_flags >> 16;
    if (num_instances && (!index_set || instance_num)) {
	FT_Get_MM_Var (face, &mm_var);
	if (!mm_var)
	    num_instances = 0;
    }

    if (count)
	*count = num_faces;

    do {
	FcPattern *pat = NULL;

	if (instance_num == 0x8000 || instance_num > num_instances)
	    FT_Set_Var_Design_Coordinates (face, 0, NULL); /* Reset variations. */
	else if (instance_num) {
	    FT_Var_Named_Style *instance = &mm_var->namedstyle[instance_num - 1];
	    FT_Fixed           *coords = instance->coords;
	    FcBool              nonzero;
	    unsigned int        i;

	    /* Skip named-instance that coincides with base instance. */
	    nonzero = FcFalse;
	    for (i = 0; i < mm_var->num_axis; i++)
		if (coords[i] != mm_var->axis[i].def) {
		    nonzero = FcTrue;
		    break;
		}
	    if (!nonzero)
		goto skip;

	    FT_Set_Var_Design_Coordinates (face, mm_var->num_axis, coords);
	}

	id = ((instance_num << 16) + face_num);
	pat = FcFreeTypeQueryFaceInternal (face, (const FcChar8 *)file, id, &cs, &ls, &nm);

	if (pat) {
	    ret++;
	    if (!set || !FcFontSetAdd (set, pat))
		FcPatternDestroy (pat);
	} else if (instance_num != 0x8000)
	    err = 1;

    skip:
	if (!index_set && instance_num < num_instances)
	    instance_num++;
	else if (!index_set && instance_num == num_instances)
	    instance_num = 0x8000; /* variable font */
	else {
	    free (nm);
	    nm = NULL;
	    FcLangSetDestroy (ls);
	    ls = NULL;
	    FcCharSetDestroy (cs);
	    cs = NULL;
	    FT_Done_Face (face);
	    face = NULL;
#ifdef HAVE_FT_DONE_MM_VAR
	    FT_Done_MM_Var (ftLibrary, mm_var);
#else
	    free (mm_var);
#endif
	    mm_var = NULL;

	    face_num++;
	    instance_num = set_instance_num;

	    if (FT_New_Face (ftLibrary, (const char *)file, face_num, &face))
		break;

	    num_instances = face->style_flags >> 16;
	    if (num_instances && (!index_set || instance_num)) {
		FT_Get_MM_Var (face, &mm_var);
		if (!mm_var)
		    num_instances = 0;
	    }
	}
    } while (!err && (!index_set || face_num == set_face_num) && face_num < num_faces);

bail:
#ifdef HAVE_FT_DONE_MM_VAR
    FT_Done_MM_Var (ftLibrary, mm_var);
#else
    free (mm_var);
#endif
    FcLangSetDestroy (ls);
    FcCharSetDestroy (cs);
    if (face)
	FT_Done_Face (face);
    FT_Done_FreeType (ftLibrary);
    if (nm)
	free (nm);

    return ret;
}

static const FT_Encoding fcFontEncodings[] = {
    FT_ENCODING_UNICODE,
    FT_ENCODING_MS_SYMBOL
};

#define NUM_DECODE (int)(sizeof (fcFontEncodings) / sizeof (fcFontEncodings[0]))

/*
 * Map a UCS4 glyph to a glyph index.  Use all available encoding
 * tables to try and find one that works.  This information is expected
 * to be cached by higher levels, so performance isn't critical
 */

FT_UInt
FcFreeTypeCharIndex (FT_Face face, FcChar32 ucs4)
{
    int     initial, offset, decode;
    FT_UInt glyphindex;

    initial = 0;

    if (!face)
	return 0;

    /*
     * Find the current encoding
     */
    if (face->charmap) {
	for (; initial < NUM_DECODE; initial++)
	    if (fcFontEncodings[initial] == face->charmap->encoding)
		break;
	if (initial == NUM_DECODE)
	    initial = 0;
    }
    /*
     * Check each encoding for the glyph, starting with the current one
     */
    for (offset = 0; offset < NUM_DECODE; offset++) {
	decode = (initial + offset) % NUM_DECODE;
	if (!face->charmap || face->charmap->encoding != fcFontEncodings[decode])
	    if (FT_Select_Charmap (face, fcFontEncodings[decode]) != 0)
		continue;
	glyphindex = FT_Get_Char_Index (face, (FT_ULong)ucs4);
	if (glyphindex)
	    return glyphindex;
	if (ucs4 < 0x100 && face->charmap &&
	    face->charmap->encoding == FT_ENCODING_MS_SYMBOL) {
	    /* For symbol-encoded OpenType fonts, we duplicate the
	     * U+F000..F0FF range at U+0000..U+00FF.  That's what
	     * Windows seems to do, and that's hinted about at:
	     * http://www.microsoft.com/typography/otspec/recom.htm
	     * under "Non-Standard (Symbol) Fonts".
	     *
	     * See thread with subject "Webdings and other MS symbol
	     * fonts don't display" on mailing list from May 2015.
	     */
	    glyphindex = FT_Get_Char_Index (face, (FT_ULong)ucs4 + 0xF000);
	    if (glyphindex)
		return glyphindex;
	}
    }
    return 0;
}

static inline int    fc_min (int a, int b) { return a <= b ? a : b; }
static inline int    fc_max (int a, int b) { return a >= b ? a : b; }
static inline FcBool fc_approximately_equal (int x, int y)
{
    return abs (x - y) * 33 <= fc_max (abs (x), abs (y));
}

static int
FcFreeTypeSpacing (FT_Face face)
{
    FT_Int       load_flags = FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH | FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING;
    FT_Pos       advances[3] = { 0 };
    unsigned int num_advances = 0;
    int          o;

    /* When using scalable fonts, only report those glyphs
     * which can be scaled; otherwise those fonts will
     * only be available at some sizes, and never when
     * transformed.  Avoid this by simply reporting bitmap-only
     * glyphs as missing
     */
    if (face->face_flags & FT_FACE_FLAG_SCALABLE)
	load_flags |= FT_LOAD_NO_BITMAP;

    if (!(face->face_flags & FT_FACE_FLAG_SCALABLE) &&
        face->num_fixed_sizes > 0 &&
        FT_Get_Sfnt_Table (face, ft_sfnt_head)) {
	FT_Int strike_index = 0, i;
	/* Select the face closest to 16 pixels tall */
	for (i = 1; i < face->num_fixed_sizes; i++) {
	    if (abs (face->available_sizes[i].height - 16) <
	        abs (face->available_sizes[strike_index].height - 16))
		strike_index = i;
	}

	FT_Select_Size (face, strike_index);
    }

    for (o = 0; o < NUM_DECODE; o++) {
	FcChar32 ucs4;
	FT_UInt  glyph;

	if (FT_Select_Charmap (face, fcFontEncodings[o]) != 0)
	    continue;

	ucs4 = FT_Get_First_Char (face, &glyph);
	while (glyph != 0 && num_advances < 3) {
	    FT_Pos advance = 0;
	    if (!FT_Get_Advance (face, glyph, load_flags, &advance) && advance) {
		unsigned int j;
		for (j = 0; j < num_advances; j++)
		    if (fc_approximately_equal (advance, advances[j]))
			break;
		if (j == num_advances)
		    advances[num_advances++] = advance;
	    }

	    ucs4 = FT_Get_Next_Char (face, ucs4, &glyph);
	}
	break;
    }

    if (num_advances <= 1)
	return FC_MONO;
    else if (num_advances == 2 &&
             fc_approximately_equal (fc_min (advances[0], advances[1]) * 2,
                                     fc_max (advances[0], advances[1])))
	return FC_DUAL;
    else
	return FC_PROPORTIONAL;
}

FcCharSet *
FcFreeTypeCharSet (FT_Face face, FcBlanks *blanks FC_UNUSED)
{
    const FT_Int load_flags = FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH | FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING;
    FcCharSet   *fcs;
    int          o;

    fcs = FcCharSetCreate();
    if (!fcs)
	goto bail;

#ifdef CHECK
    printf ("Family %s style %s\n", face->family_name, face->style_name);
#endif
    for (o = 0; o < NUM_DECODE; o++) {
	FcChar32 ucs4;
	FT_UInt  glyph;

	if (FT_Select_Charmap (face, fcFontEncodings[o]) != 0)
	    continue;

	ucs4 = FT_Get_First_Char (face, &glyph);
	while (glyph != 0) {
	    FcBool good = FcTrue;

	    /* CID fonts built by Adobe used to make ASCII control chars to cid1
	     * (space glyph). As such, always check contour for those characters. */
	    if (ucs4 <= 0x001F) {
		if (FT_Load_Glyph (face, glyph, load_flags) ||
		    (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE &&
		     face->glyph->outline.n_contours == 0))
		    good = FcFalse;
	    }

	    if (good)
		FcCharSetAddChar (fcs, ucs4);

	    ucs4 = FT_Get_Next_Char (face, ucs4, &glyph);
	}
	if (fcFontEncodings[o] == FT_ENCODING_MS_SYMBOL) {
	    /* For symbol-encoded OpenType fonts, we duplicate the
	     * U+F000..F0FF range at U+0000..U+00FF.  That's what
	     * Windows seems to do, and that's hinted about at:
	     * http://www.microsoft.com/typography/otspec/recom.htm
	     * under "Non-Standard (Symbol) Fonts".
	     *
	     * See thread with subject "Webdings and other MS symbol
	     * fonts don't display" on mailing list from May 2015.
	     */
	    for (ucs4 = 0xF000; ucs4 < 0xF100; ucs4++) {
		if (FcCharSetHasChar (fcs, ucs4))
		    FcCharSetAddChar (fcs, ucs4 - 0xF000);
	    }
	}
#ifdef CHECK
	for (ucs4 = 0x0020; ucs4 < 0x10000; ucs4++) {
	    FcBool FT_Has, FC_Has;

	    FT_Has = FT_Get_Char_Index (face, ucs4) != 0;
	    FC_Has = FcCharSetHasChar (fcs, ucs4);
	    if (FT_Has != FC_Has) {
		printf ("0x%08x FT says %d FC says %d\n", ucs4, FT_Has, FC_Has);
	    }
	}
#endif
	break;
    }

    return fcs;
bail:
    FcCharSetDestroy (fcs);
    return 0;
}

FcCharSet *
FcFreeTypeCharSetAndSpacing (FT_Face face, FcBlanks *blanks FC_UNUSED, int *spacing)
{
    if (spacing)
	*spacing = FcFreeTypeSpacing (face);

    return FcFreeTypeCharSet (face, blanks);
}

/* Graphite Rules Table */
#define TTAG_SILF          FT_MAKE_TAG ('S', 'i', 'l', 'f')

#define OTLAYOUT_HEAD      "otlayout:"
#define OTLAYOUT_HEAD_LEN  9
#define OTLAYOUT_ID_LEN    4
/* space + head + id */
#define OTLAYOUT_LEN       (1 + OTLAYOUT_HEAD_LEN + OTLAYOUT_ID_LEN)

/*
 * This is a bit generous; the registry has only lower case and space
 * except for 'DFLT'.
 */
#define FcIsSpace(x)       (040 == (x))
#define FcIsDigit(c)       (('0' <= (c) && (c) <= '9'))
#define FcIsValidScript(x) (FcIsLower (x) || FcIsUpper (x) || FcIsDigit (x) || FcIsSpace (x))

static void
addtag (FcChar8 *complex_, FT_ULong tag)
{
    FcChar8 tagstring[OTLAYOUT_ID_LEN + 1];

    tagstring[0] = (FcChar8)(tag >> 24),
    tagstring[1] = (FcChar8)(tag >> 16),
    tagstring[2] = (FcChar8)(tag >> 8),
    tagstring[3] = (FcChar8)(tag);
    tagstring[4] = '\0';

    /* skip tags which aren't alphanumeric, under the assumption that
     * they're probably broken
     */
    if (!FcIsValidScript (tagstring[0]) ||
        !FcIsValidScript (tagstring[1]) ||
        !FcIsValidScript (tagstring[2]) ||
        !FcIsValidScript (tagstring[3]))
	return;

    if (*complex_ != '\0')
	strcat ((char *)complex_, " ");
    strcat ((char *)complex_, OTLAYOUT_HEAD);
    strcat ((char *)complex_, (char *)tagstring);
}

static int
compareulong (const void *a, const void *b)
{
    const FT_ULong *ua = (const FT_ULong *)a;
    const FT_ULong *ub = (const FT_ULong *)b;
    return *ua - *ub;
}

static FcBool
FindTable (FT_Face face, FT_ULong tabletag, FT_ULong *tablesize)
{
    FT_Stream stream = face->stream;
    FT_Error  error;

    if (!stream)
	return FcFalse;

    if ((error = ftglue_face_goto_table (face, tabletag, stream, tablesize)))
	return FcFalse;

    return FcTrue;
}

static int
GetScriptTags (FT_Face face, FT_ULong tabletag, FT_ULong **stags)
{
    FT_ULong  cur_offset, new_offset, base_offset;
    FT_Stream stream = face->stream;
    FT_Error  error;
    FT_UShort n, p;
    int       script_count;

    if (!stream)
	return 0;

    if ((error = ftglue_face_goto_table (face, tabletag, stream, NULL)))
	return 0;

    base_offset = ftglue_stream_pos (stream);

    /* skip version */

    if (ftglue_stream_seek (stream, base_offset + 4L) || ftglue_stream_frame_enter (stream, 2L))
	return 0;

    new_offset = GET_UShort() + base_offset;

    ftglue_stream_frame_exit (stream);

    cur_offset = ftglue_stream_pos (stream);

    if (ftglue_stream_seek (stream, new_offset) != FT_Err_Ok)
	return 0;

    base_offset = ftglue_stream_pos (stream);

    if (ftglue_stream_frame_enter (stream, 2L))
	return 0;

    script_count = GET_UShort();

    ftglue_stream_frame_exit (stream);

    *stags = malloc (script_count * sizeof (FT_ULong));
    if (!*stags)
	return 0;

    p = 0;
    for (n = 0; n < script_count; n++) {
	if (ftglue_stream_frame_enter (stream, 6L))
	    goto Fail;

	(*stags)[p] = GET_ULong();
	new_offset = GET_UShort() + base_offset;

	ftglue_stream_frame_exit (stream);

	cur_offset = ftglue_stream_pos (stream);

	error = ftglue_stream_seek (stream, new_offset);

	if (error == FT_Err_Ok)
	    p++;

	(void)ftglue_stream_seek (stream, cur_offset);
    }

    if (!p)
	goto Fail;

    /* sort the tag list before returning it */
    qsort (*stags, p, sizeof (FT_ULong), compareulong);

    return p;

Fail:
    free (*stags);
    *stags = NULL;
    return 0;
}

static FcChar8 *
FcFontCapabilities (FT_Face face)
{
    FcBool    issilgraphitefont = 0;
    FT_Error  err;
    FT_ULong  len = 0;
    FT_ULong *gsubtags = NULL, *gpostags = NULL;
    FT_UShort gsub_count = 0, gpos_count = 0;
    FT_ULong  maxsize;
    FcChar8  *complex_ = NULL;
    int       indx1 = 0, indx2 = 0;

    err = FT_Load_Sfnt_Table (face, TTAG_SILF, 0, 0, &len);
    issilgraphitefont = (err == FT_Err_Ok);

    gpos_count = GetScriptTags (face, TTAG_GPOS, &gpostags);
    gsub_count = GetScriptTags (face, TTAG_GSUB, &gsubtags);

    if (!issilgraphitefont && !gsub_count && !gpos_count)
	goto bail;

    maxsize = (((FT_ULong)gpos_count + (FT_ULong)gsub_count) * OTLAYOUT_LEN +
               (issilgraphitefont ? strlen(fcSilfCapability) + 1: 0));
    complex_ = malloc (sizeof (FcChar8) * maxsize);
    if (!complex_)
	goto bail;

    complex_[0] = '\0';
    if (issilgraphitefont)
	strcpy ((char *)complex_, fcSilfCapability);

    while ((indx1 < gsub_count) || (indx2 < gpos_count)) {
	if (indx1 == gsub_count) {
	    addtag (complex_, gpostags[indx2]);
	    indx2++;
	} else if ((indx2 == gpos_count) || (gsubtags[indx1] < gpostags[indx2])) {
	    addtag (complex_, gsubtags[indx1]);
	    indx1++;
	} else if (gsubtags[indx1] == gpostags[indx2]) {
	    addtag (complex_, gsubtags[indx1]);
	    indx1++;
	    indx2++;
	} else {
	    addtag (complex_, gpostags[indx2]);
	    indx2++;
	}
    }
    if (FcDebug() & FC_DBG_SCANV)
	printf ("complex_ features in this font: %s\n", complex_);
bail:
    free (gsubtags);
    free (gpostags);
    return complex_;
}

static FcBool
FcFontHasHint (FT_Face face)
{
    FT_ULong size;

    /* For a workaround of gttools fix-nonhinting.
     * See https://gitlab.freedesktop.org/fontconfig/fontconfig/-/issues/426
     */
    if (FcDebug() & FC_DBG_SCANV) {
	FT_ULong ret;

	fprintf (stderr, "*** Has hint:\n");
	fprintf (stderr, "    fpgm table: %s\n",
	         FindTable (face, TTAG_fpgm, NULL) ? "True" : "False");
	fprintf (stderr, "    cvt table: %s\n",
	         FindTable (face, TTAG_cvt, NULL) ? "True" : "False");
	fprintf (stderr, "    prep table: %s\n",
	         FindTable (face, TTAG_prep, &ret) ? "True" : "False");
	fprintf (stderr, "    prep size: %lu\n", ret);
    }
    return FindTable (face, TTAG_fpgm, NULL) ||
           FindTable (face, TTAG_cvt, NULL) ||
           (FindTable (face, TTAG_prep, &size) && size > 7);
}

#define __fcfreetype__
#include "fcaliastail.h"
#include "fcftaliastail.h"
#undef __fcfreetype__
