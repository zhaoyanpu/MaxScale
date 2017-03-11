/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2019-07-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include "maxscale/trxboundarymatcher.hh"
#include <maxscale/platform.h>
#include <maxscale/modutil.h>

namespace
{

struct REGEX_DATA
{
    const char* zMatch;
    uint32_t    type_mask;
    pcre2_code* pCode;
};

REGEX_DATA this_unit_regexes[] =
{
    {
        "^\\s*BEGIN(\\s+WORK)?\\s*;?\\s*$",
        QUERY_TYPE_BEGIN_TRX
    },
    {
        "^\\s*COMMIT(\\s+WORK)?\\s*;?\\s*$",
        QUERY_TYPE_COMMIT,
    },
    {
        "^\\s*ROLLBACK(\\s+WORK)?\\s*;?\\s*$",
        QUERY_TYPE_ROLLBACK
    },
    {
        "^\\s*START\\s+TRANSACTION\\s+READ\\s+ONLY\\s*;?\\s*$",
        QUERY_TYPE_BEGIN_TRX | QUERY_TYPE_READ
    },
    {
        "^\\s*START\\s+TRANSACTION\\s+READ\\s+WRITE\\s*;?\\s*$",
        QUERY_TYPE_BEGIN_TRX | QUERY_TYPE_WRITE
    },
    {
        "^\\s*START\\s+TRANSACTION(\\s*;?\\s*|(\\s+.*))$",
        QUERY_TYPE_BEGIN_TRX
    },
    {
        "^\\s*SET\\s+AUTOCOMMIT\\s*\\=\\s*(1|true)\\s*;?\\s*$",
        QUERY_TYPE_COMMIT|QUERY_TYPE_ENABLE_AUTOCOMMIT
    },
    {
        "^\\s*SET\\s+AUTOCOMMIT\\s*\\=\\s*(0|false)\\s*;?\\s*$",
        QUERY_TYPE_BEGIN_TRX|QUERY_TYPE_DISABLE_AUTOCOMMIT
    }
};

const size_t N_REGEXES = sizeof(this_unit_regexes)/sizeof(this_unit_regexes[0]);

struct this_unit
{
    REGEX_DATA* pRegexes;
} this_unit =
{
    .pRegexes = this_unit_regexes
};

thread_local struct this_thread
{
    pcre2_match_data* match_datas[N_REGEXES];
} this_thread;


bool compile_regexes();
void free_regexes();

bool create_thread_data();
void free_thread_data();

bool compile_regexes()
{
    REGEX_DATA* i = this_unit.pRegexes;
    REGEX_DATA* end = i + N_REGEXES;

    bool success = true;

    while (success && (i < end))
    {
        int errcode;
        PCRE2_SIZE erroffset;
        i->pCode = pcre2_compile((PCRE2_SPTR)i->zMatch, PCRE2_ZERO_TERMINATED, PCRE2_CASELESS,
                                 &errcode, &erroffset, NULL);

        if (!i->pCode)
        {
            success = false;
            PCRE2_UCHAR errbuf[512];
            pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));

            MXS_ERROR("Regex compilation failed at %lu for regex '%s': %s.",
                      erroffset, i->zMatch, errbuf);
        }

        ++i;
    }

    if (!success)
    {
        free_regexes();
    }

    return success;
}

void free_regexes()
{
    REGEX_DATA* begin = this_unit.pRegexes;
    REGEX_DATA* i = begin + N_REGEXES;

    while (i > begin)
    {
        --i;

        if (i->pCode)
        {
            pcre2_code_free(i->pCode);
            i->pCode = NULL;
        }
    }
}

bool create_thread_data()
{
    bool success = true;

    REGEX_DATA* i = this_unit.pRegexes;
    REGEX_DATA* end = i + N_REGEXES;

    pcre2_match_data** ppData = this_thread.match_datas;

    while (success && (i < end))
    {
        *ppData = pcre2_match_data_create_from_pattern(i->pCode, NULL);

        if (!*ppData)
        {
            success = false;
            MXS_ERROR("PCRE2 match data creation failed.");
        }

        ++i;
        ++ppData;
    }

    if (!success)
    {
        free_thread_data();
    }

    return success;
}

void free_thread_data()
{
    pcre2_match_data** begin = this_thread.match_datas;
    pcre2_match_data** i = begin + N_REGEXES;

    while (i > begin)
    {
        --i;

        if (*i)
        {
            pcre2_match_data_free(*i);
            *i = NULL;
        }
    }
}

}

namespace maxscale
{

//static
bool TrxBoundaryMatcher::process_init()
{
    bool rc = compile_regexes();

    if (rc)
    {
        rc = thread_init();

        if (!rc)
        {
            free_regexes();
        }
    }

    return rc;
}

//static
void TrxBoundaryMatcher::process_end()
{
    thread_end();
    free_regexes();
}

//static
bool TrxBoundaryMatcher::thread_init()
{
    return create_thread_data();
}

//static
void TrxBoundaryMatcher::thread_end()
{
    free_thread_data();
}

//static
uint32_t TrxBoundaryMatcher::type_mask_of(const char* pSql, size_t len)
{
    uint32_t type_mask = 0;

    REGEX_DATA* i = this_unit.pRegexes;
    REGEX_DATA* end = i + N_REGEXES;
    pcre2_match_data** ppData = this_thread.match_datas;

    while ((type_mask == 0) && (i < end))
    {
        if (pcre2_match(i->pCode, (PCRE2_SPTR)pSql, len, 0, 0, *ppData, NULL) >= 0)
        {
            type_mask = i->type_mask;
        }

        ++i;
        ++ppData;
    }

    return type_mask;
}

//static
uint32_t TrxBoundaryMatcher::type_mask_of(GWBUF* pBuf)
{
    uint32_t type_mask = 0;

    char* pSql;
    int len;

    // This will exclude prepared statement but we are fine with that.
    if (modutil_extract_SQL(pBuf, &pSql, &len))
    {
        type_mask = type_mask_of(pSql, len);
    }

    return type_mask;
}

}
