/* markdown.cpp - Markdown subsets
 *
 * LIMITATIONS:
 *
 *  1. fixed four columns tab stops.
 *
 * License: The BSD 3-Clause
 *
 * Copyright (c) 2015, MIZUTANI Tociyuki
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <iostream>
#include <deque>
#include <map>
#include <string>
#include <algorithm>
#include <functional>
#include <locale>

static const std::wstring blocktag (
    L" blockquote del div dl fieldset figure form h1 h2 h3 h4 h5 h6"
    L" hr iframe ins noscript math ol p pre script table ul !COMMENT ");

/* token kinds */
enum {
    BLANK,
    LINE,
    HTML,
    CODE,
    TEXT,
    INLINE,
    LINKID, URI,
    /* inline HTML markup */
    SABEGIN, TITLE, SAEND,
    IMGBEGIN, ALT, IMGEND,
    BREAK, SCODE, ECODE, EA,
    SEM, EEM, SSTRONG, ESTRONG,
    /* block HTML markup */
    HRULE,
    SPRE, EPRE,
    SHEADING1, EHEADING1,
    SHEADING2, EHEADING2,
    SHEADING3, EHEADING3,
    SHEADING4, EHEADING4,
    SHEADING5, EHEADING5,
    SHEADING6, EHEADING6,
    SBLOCKQUOTE, EBLOCKQUOTE,
    SULIST, EULIST,
    SOLIST, EOLIST,
    SLITEM, ELITEM,
    SPARAGRAPH, EPARAGRAPH,
};

static wchar_t const *kindname[]{
    L"BLANK",
    L"LINE",
    L"HTML",
    L"CODE",
    L"TEXT",
    L"INLINE",
    L"LINKID", L"URI",
    /* inline HTML markup */
    L"<a href=\"", L"\" title=\"", L"\">",
    L"<img src=\"", L"\" alt=\"", L"\" />",
    L"<br />\n", L"<code>", L"</code>", L"</a>",
    L"<em>", L"</em>", L"<strong>", L"</strong>",
    /* block HTML markup */
    L"<hr />\n",
    L"<pre><code>", L"</code></pre>\n",
    L"<h1>", L"</h1>\n",
    L"<h2>", L"</h2>\n",
    L"<h3>", L"</h3>\n",
    L"<h4>", L"</h4>\n",
    L"<h5>", L"</h5>\n",
    L"<h6>", L"</h6>\n",
    L"<blockquote>\n", L"</blockquote>\n",
    L"<ul>\n", L"</ul>\n",
    L"<ol>\n", L"</ol>\n",
    L"<li>", L"</li>\n",
    L"<p>", L"</p>\n",
};

typedef std::wstring::const_iterator char_iterator;
typedef std::function<bool(int)> char_predicate;

struct token_type {
    int kind;
    char_iterator cbegin;
    char_iterator cend;
};

struct reflink_type {
    std::wstring id;
    std::wstring uri;
    std::wstring title;
};

struct nest_type {
    std::size_t pos;
    int n;
};

typedef std::deque<token_type>::const_iterator token_iterator;
typedef std::deque<token_type>::const_iterator line_iterator;
typedef std::map<std::wstring, reflink_type> refdict_type;

static void
split_lines (std::wstring const &input, std::deque<token_type>& output,
    refdict_type& dict);
static void parse_block (std::deque<token_type> const& input,
    std::deque<token_type>& output);
static void
print_block (std::deque<token_type> const& input, std::wostream& output,
    refdict_type const& dict);

void markdown (std::wstring const& input, std::wostream& output)
{
    std::deque<token_type> pass1;
    std::deque<token_type> pass2;
    refdict_type dict;
    split_lines (input, pass1, dict);
    parse_block (pass1, pass2);
    print_block (pass2, output, dict);
}

static bool
ismdescapable (int c)
{
    static std::wstring esc (L"\\`*_{}[]()<>#+-.!");
    return esc.find (c) != std::wstring::npos;
}

static bool
ismdwhite (int c)
{
    return '\n' == c || '\t' == c || ' ' == c;
}

static bool
ismdspace (int c)
{
    return '\t' == c || ' ' == c;
}

static bool
ismdgraph (int c)
{
    return ' ' < c && 0x7f != c;
}

static bool
ismdprint (int c)
{
    return '\t' == c || (' ' <= c && 0x7f != c);
}

static bool
ismdany (int c)
{
    return '\n' == c || '\t' == c || (' ' <= c && 0x7f != c);
}

static bool
ismddigit (int c)
{
    return '0' <= c && c <= '9';
}

static bool
ismdxdigit (int c)
{
    return ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f') || ismddigit (c);
}

static bool
ismdalnum (int c)
{
    return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || ismddigit (c);
}

static bool
ishtname (int c)
{
    return ismdalnum (c) || '-' == c || '_' == c || ':' == c;
}

static bool
ishtattr (int c)
{
    return ' ' < c && '<' != c && '>' != c && '"' != c && '\'' != c && '`' != c;
}

/* scan /$c{n1,n2}/ from pos to eos */
static char_iterator
scan_of (char_iterator const pos, char_iterator const eos,
         int const n1, int const n2, int const c)
{
    char_iterator p = pos;
    for (int i = 0; n2 < 0 || i < n2; ++i, ++p) {
        if (p < eos && c == *p)
            continue;
        else if (i < n1)
            return pos;
        else
            break;
    }
    return p;
}

static char_iterator
scan_of (char_iterator const pos, char_iterator const eos,
         int const n1, int const n2, char_predicate predicate)
{
    char_iterator p = pos;
    for (int i = 0; n2 < 0 || i < n2; ++i, ++p) {
        if (p < eos && predicate (*p))
            continue;
        else if (i < n1)
            return pos;
        else
            break;
    }
    return p;
}

/* reverse scan */
static char_iterator
rscan_of (char_iterator const bos, char_iterator const pos, int c)
{
    char_iterator p = pos;
    for (; bos <= p - 1 && c == p[-1]; --p)
        ;
    return p;
}

static char_iterator
rscan_of (char_iterator const bos, char_iterator const pos,
          std::function<bool(int)> predicate)
{
    char_iterator p = pos;
    for (; bos <= p - 1 && predicate (p[-1]); --p)
        ;
    return p;
}

/* scan quoted string "abc", or [abc], or (abc). may be nested and escaped */
static char_iterator
scan_quoted (char_iterator const pos, char_iterator const eos,
             int lquote, int rquote, int escape, char_predicate predicate)
{
    if (! (pos < eos && lquote == *pos))
        return pos;
    char_iterator p = pos + 1;
    int level = 1;
    while (level > 0) {
        if (! (p < eos && predicate (*p)))
            return pos;
        if (escape == *p && p + 1 < eos
                && (escape == p[1] || rquote == p[1] || lquote == p[1]))
            ++p;
        else if (lquote == '(' && '<' == *p)
            p = scan_quoted (p, eos, '<', '>', escape, predicate) - 1;
        else if (rquote == *p)
            --level;
        else if (lquote == *p)
            ++level;
        ++p;
    }
    return p;
}

/* decode reference style link id */
static std::wstring
decode_linkid (char_iterator s, char_iterator const eos)
{
    std::wstring id;
    for (; s < eos; ++s)
        if ('A' <= *s && *s <= 'Z')
            id.push_back (*s + ('a' - 'A'));
        else if ('\\' == *s && s + 1 < eos && ismdescapable (s[1]))
            id.push_back (*++s);
        else if (ismdwhite (*s)) {
            while (s < eos && ismdwhite (*s))
                ++s;
            --s;
            id.push_back (' ');
        }
        else
            id.push_back (*s);
    return id;
}

/* unescape backslash */
static std::wstring
unescape_backslash (char_iterator s, char_iterator const eos)
{
    std::wstring str;
    for (; s < eos; ++s)
        if ('\\' == *s && s + 1 < eos && ismdescapable (s[1]))
            str.push_back (*++s);
        else
            str.push_back (*s);
    return str;
}

/* parse_block - BLOCK parser */

/* four columns tab */
static char_iterator
scan_tab (char_iterator const pos, char_iterator const eos)
{
    char_iterator p1 = scan_of (pos, eos, 0, 3, ' ');
    char_iterator p2 = scan_of (p1, eos, 1, 1, ' ');
    char_iterator p3 = scan_of (p1, eos, 1, 1, '\t');
    return p2 - pos == 4 ? p2 : p1 < p3 ? p3 : pos;
}

/* not tab */
static char_iterator
scan_tab_not (char_iterator const pos, char_iterator const eos)
{
    return scan_of (pos, eos, 0, 3, ' ');
}

static char_iterator
scan_hrule (char_iterator const pos, char_iterator const eos)
{
    char_iterator p1 = scan_tab_not (pos, eos);
    if (! (p1 < eos && ('*' == *p1 || '_' == *p1 || '-' == *p1)))
        return pos;
    int dash = *p1;
    int n = 0;
    while (p1 < eos && (ismdspace (*p1) || dash == *p1)) {
        if (dash == *p1)
            ++n;
        ++p1;
    }
    if (n < 3 || ! (p1 >= eos || '\n' == *p1))
        return pos;
    return p1;
}

static char_iterator
scan_listmark (char_iterator const pos, char_iterator const eos)
{
    char_iterator p1 = scan_tab_not (pos, eos);
    if (p1 >= eos)
        return pos;
    else if ('*' == *p1 || '+' == *p1 || '-' == *p1) {
        char_iterator p2 = p1 + 1;
        char_iterator p3 = scan_of (p2, eos, 1, 1, ismdspace);
        if (p2 < p3)
            return p2;
    }
    else if (ismddigit (*p1)) {
        char_iterator p2 = scan_of (p1, eos, 1, -1, ismddigit);
        char_iterator p3 = scan_of (p2, eos, 1, 1, '.');
        char_iterator p4 = scan_of (p3, eos, 1, 1, ismdspace);
        if (p2 < p3 && p3 < p4)
            return p3;
    }
    return pos;
}

static line_iterator
parse_blank (line_iterator const dot, line_iterator const dol,
    std::deque<token_type>& output)
{
    line_iterator line1 = dot;
    for (; line1 != dol && BLANK == line1->kind; ++line1)
        ;
    return line1;
}

static line_iterator
parse_hrule (line_iterator const dot, line_iterator const dol,
    std::deque<token_type>& output)
{
    char_iterator p1 = scan_hrule (dot->cbegin, dot->cend);
    if (p1 == dot->cbegin)
        return dot;
    output.push_back ({HRULE, dot->cbegin, dot->cend});
    return dot + 1;
}

static line_iterator
parse_seheading (line_iterator const dot, line_iterator const dol,
    std::deque<token_type>& output)
{
    line_iterator line2 = dot + 1;
    if (line2 == dol)
        return dot;
    char_iterator p1 = scan_tab_not (dot->cbegin, dot->cend);
    if (! (p1 < dot->cend && ismdgraph (*p1)))
        return dot;
    char_iterator p2 = scan_tab_not (line2->cbegin, line2->cend);
    if (! (p2 < line2->cend && ('=' == *p2 || '-' == *p2)))
        return dot;
    int dash = *p2;
    char_iterator p3 = scan_of (p2, line2->cend, 0, -1, dash);
    char_iterator p4 = scan_of (p3, line2->cend, 0, -1, ismdspace);
    if (! (p4 >= line2->cend || '\n' == *p4))
        return dot;
    int stag = '=' == dash ? SHEADING1 : SHEADING2;
    int etag = '=' == dash ? EHEADING1 : EHEADING2;
    output.push_back ({stag, p1, p1});
    output.push_back ({INLINE, p1, dot->cend});
    output.push_back ({etag, dot->cend, dot->cend});
    return line2 + 1;
}

static line_iterator
parse_atxheading (line_iterator const dot, line_iterator const dol,
    std::deque<token_type>& output)
{
    static const int stag[6] = {
        SHEADING1, SHEADING2, SHEADING3, SHEADING4, SHEADING5, SHEADING6};
    static const int etag[6] = {
        EHEADING1, EHEADING2, EHEADING3, EHEADING4, EHEADING5, EHEADING6};
    char_iterator p1 = scan_tab_not (dot->cbegin, dot->cend);
    char_iterator p2 = scan_of (p1, dot->cend, 1, -1, '#');
    if (p2 == p1)
        return dot;
    int n = p2 - p1 > 6 ? 6 : p2 - p1;
    char_iterator p3 = scan_of (p2, dot->cend, 0, -1, ismdspace);
    char_iterator p4 = dot->cend;
    p4 = rscan_of (p3, p4, ismdwhite);
    p4 = rscan_of (p3, p4, '#');
    p4 = rscan_of (p3, p4, ismdspace);
    if (p3 == p4)
        return dot;
    output.push_back ({stag[n - 1], p3, p3});
    output.push_back ({INLINE, p3, p4});
    output.push_back ({etag[n - 1], p4, p4});
    return dot + 1;
}

static line_iterator
parse_listitem (line_iterator const dot, line_iterator const dol,
    std::deque<token_type>& output)
{
    char_iterator p1 = scan_tab_not (dot->cbegin, dot->cend);
    if (! (p1 < dot->cend && ismdgraph (*p1)))
        return dot;
    output.push_back ({INLINE, p1, dot->cend});
    line_iterator line1 = dot + 1;
    for (; line1 != dol && LINE == line1->kind; ++line1) {
        char_iterator p2 = scan_listmark (line1->cbegin, line1->cend);
        if (p2 != line1->cbegin)
            break;
        output.push_back ({INLINE, line1->cbegin, line1->cend});
    }
    return line1;
}

static line_iterator
parse_paragraph (line_iterator const dot, line_iterator const dol,
    std::deque<token_type>& output)
{
    char_iterator p1 = scan_tab_not (dot->cbegin, dot->cend);
    if (! (p1 < dot->cend && ismdgraph (*p1)))
        return dot;
    output.push_back ({SPARAGRAPH, p1, p1});
    output.push_back ({INLINE, p1, dot->cend});
    line_iterator line1 = dot + 1;
    for (; line1 != dol && LINE == line1->kind; ++line1)
        output.push_back ({INLINE, line1->cbegin, line1->cend});
    output.push_back ({EPARAGRAPH, line1->cbegin, line1->cbegin});
    return line1;
}

static line_iterator
parse_tabcode_line (line_iterator const dot, line_iterator const dol,
    std::deque<token_type>& output)
{
    char_iterator p = scan_tab (dot->cbegin, dot->cend);
    if (p == dot->cbegin)
        return dot;
    output.push_back ({CODE, p, dot->cend});
    return dot + 1;
}

static line_iterator
parse_tabcode_blank (line_iterator const dot, line_iterator const dol,
    std::deque<token_type>& output)
{
    line_iterator line2 = parse_blank (dot, dol, output);
    if (! (line2 != dol && LINE == line2->kind))
        return dot;
    char_iterator p2 = scan_tab (line2->cbegin, line2->cend);
    if (p2 == line2->cbegin)
        return dot;
    for (line_iterator line1 = dot; line1 != line2; ++line1)
        output.push_back ({CODE, line1->cbegin, line1->cend});
    return line2;
}

static line_iterator
parse_tabcode (line_iterator const dot, line_iterator const dol,
    std::deque<token_type>& output)
{
    char_iterator p1 = scan_tab (dot->cbegin, dot->cend);
    if (p1 == dot->cbegin)
        return dot;
    output.push_back ({SPRE, p1, p1});
    output.push_back ({CODE, p1, dot->cend});
    line_iterator line1 = dot + 1;
    while (line1 != dol) {
        line_iterator line2 = line1;
        if (LINE == line1->kind)
            line2 = parse_tabcode_line (line1, dol, output);
        else if (BLANK == line1->kind)
            line2 = parse_tabcode_blank (line1, dol, output);
        if (line2 == line1)
            break;
        line1 = line2;
    }
    output.push_back ({EPRE, line1->cend, line1->cend});
    return line1;
}

static line_iterator
parse_blockquote_line (line_iterator const dot, line_iterator const dol,
    std::deque<token_type>& block, bool& lazyline)
{
    char_iterator p1 = scan_tab_not (dot->cbegin, dot->cend);
    char_iterator p2 = scan_of (p1, dot->cend, 0, 1, '>');
    char_iterator p3 = scan_of (p2, dot->cend, 0, 1, ' ');
    char_iterator p4 = scan_of (p3, dot->cend, 0, -1, ismdspace);
    if (p4 >= dot->cend || '\n' == *p4)
        block.push_back ({BLANK, p4, dot->cend});
    else {
        if (lazyline && p1 != p2)
            block.push_back ({BLANK, p3, p3});
        block.push_back ({LINE, p3, dot->cend});
    }
    lazyline = p1 == p2;
    return dot + 1;
}

static line_iterator
parse_blockquote_blank (line_iterator const dot, line_iterator const dol,
    std::deque<token_type>& block, bool& lazyline)
{
    line_iterator line2 = parse_blank (dot, dol, block);
    if (! (line2 != dol && LINE == line2->kind))
        return dot;
    char_iterator p1 = scan_tab_not (line2->cbegin, line2->cend);
    char_iterator p2 = scan_of (p1, line2->cend, 1, 1, '>');
    if (p1 == p2)
        return dot;
    for (line_iterator line1 = dot; line1 != line2; ++line1)
        block.push_back (*line1);
    lazyline = false;
    return line2;
}

static line_iterator
parse_blockquote (line_iterator const dot, line_iterator const dol,
    std::deque<token_type>& output)
{
    std::deque<token_type> block;
    char_iterator p1 = scan_tab_not (dot->cbegin, dot->cend);
    char_iterator p2 = scan_of (p1, dot->cend, 1, 1, '>');
    if (p1 == p2)
        return dot;
    block.push_back ({SBLOCKQUOTE, p2, p2});
    line_iterator line1 = dot;
    bool lazyline = false;
    while (line1 != dol) {
        line_iterator line2 = line1;
        if (LINE == line1->kind)
            line2 = parse_blockquote_line (line1, dol, block, lazyline);
        else if (BLANK == line1->kind)
            line2 = parse_blockquote_blank (line1, dol, block, lazyline);
        if (line2 == line1)
            break;
        line1 = line2;
    }
    block.push_back ({EBLOCKQUOTE, line1->cend, line1->cend});
    parse_block (block, output);
    return line1;
}

static line_iterator
parse_list_line (line_iterator const dot, line_iterator const dol,
    std::deque<token_type>& block, int const indicator)
{
    char_iterator p1 = scan_listmark (dot->cbegin, dot->cend);
    if (p1 == dot->cbegin) {
        char_iterator p2 = scan_tab (dot->cbegin, dot->cend);
        block.push_back ({LINE, p2, dot->cend});
    }
    else {
        char_iterator p2 = scan_of (p1, dot->cend, 1, -1, ismdspace);
        block.push_back ({ELITEM, p2, p2});
        block.push_back ({SLITEM, p2, p2});
        block.push_back ({LINE, p2, dot->cend});
    }
    return dot + 1;
}

static line_iterator
parse_list_blank (line_iterator const dot, line_iterator const dol,
    std::deque<token_type>& block, int const indicator)
{
    line_iterator line2 = parse_blank (dot, dol, block);
    if (! (line2 != dol && LINE == line2->kind))
        return dot;
    char_iterator p1 = scan_hrule (line2->cbegin, line2->cend);
    if (p1 != line2->cbegin)
        return dot;
    char_iterator p2 = scan_listmark (line2->cbegin, line2->cend);
    char_iterator p3 = scan_tab (line2->cbegin, line2->cend);
    if (p3 != line2->cbegin)
        for (line_iterator line1 = dot; line1 != line2; ++line1)
            block.push_back (*line1);
    else if (p2 == line2->cbegin)
        return dot;
    return line2;
}

static line_iterator
parse_list (line_iterator const dot, line_iterator const dol,
    std::deque<token_type>& output)
{
    std::deque<token_type> block;
    char_iterator p1 = scan_listmark (dot->cbegin, dot->cend);
    if (p1 == dot->cbegin)
        return dot;
    int indicator = p1[-1];
    int stag = '.' == indicator ? SOLIST : SULIST;
    int etag = '.' == indicator ? EOLIST : EULIST;
    char_iterator p2 = scan_of (p1, dot->cend, 1, -1, ismdspace);
    block.push_back ({stag, p2, p2});
    block.push_back ({SLITEM, p2, p2});
    block.push_back ({LINE, p2, dot->cend});
    line_iterator line1 = dot + 1;
    while (line1 != dol) {
        line_iterator line2 = line1;
        if (LINE == line1->kind)
            line2 = parse_list_line (line1, dol, block, indicator);
        else if (BLANK == line1->kind)
            line2 = parse_list_blank (line1, dol, block, indicator);
        if (line2 == line1)
            break;
        line1 = line2;
    }
    block.push_back ({ELITEM, line1->cbegin, line1->cbegin});
    block.push_back ({etag, line1->cbegin, line1->cbegin});
    parse_block (block, output);
    return line1;
}

static void
parse_block (std::deque<token_type> const& input, std::deque<token_type>& output)
{
    line_iterator dot = input.cbegin ();
    line_iterator dol = input.cend ();
    bool listitem = false;
    while (dot != dol) {
        line_iterator line = dot;
        if (SLITEM == line->kind)
            listitem = true;
        if (LINE == line->kind) {
            if ((dot = parse_hrule (line, dol, output)) != line)
                continue;
            if ((dot = parse_tabcode (line, dol, output)) != line)
                continue;
            if ((dot = parse_blockquote (line, dol, output)) != line)
                continue;
            if ((dot = parse_atxheading (line, dol, output)) != line)
                continue;
            if ((dot = parse_list (line, dol, output)) != line)
                continue;
            if ((dot = parse_seheading (line, dol, output)) != line)
                continue;
            if (listitem)
                dot = parse_listitem (line, dol, output);
            else
                dot = parse_paragraph (line, dol, output);
            listitem = false;
        }
        if (line == dot) {
            output.push_back (*dot);
            ++dot;
        }
    }
}

/* split_lines - BLOCK tokenizer */

static char_iterator
check_blockend (char_iterator const pos, char_iterator const eos)
{
    char_iterator p1 = scan_of (pos, eos, 0, -1, ismdspace);
    char_iterator p2 = scan_of (p1, eos, 1, 1, '\n');
    char_iterator p3 = scan_of (p2, eos, 0, -1, ismdspace);
    char_iterator p4 = scan_of (p3, eos, 1, 1, '\n');
    return eos <= p4 || (p1 < p2 && p3 < p4) ? p2 : pos;
}

static char_iterator
parse_blockcode (char_iterator const bos, char_iterator const pos,
    char_iterator const eos, std::deque<token_type>& output)
{
    static const std::wstring pat (L"\n```");
    if (pos - 2 >= bos && '\n' != pos[-2])
        return pos;
    if (pos - 1 >= bos && '\n' != pos[-1])
        return pos;
    char_iterator p1 = scan_of (pos, eos, 3, 3, '`');
    if (p1 == pos)
        return pos;
    char_iterator p2 = scan_of (p1, eos, 0, -1, ismdprint);
    char_iterator p3 = scan_of (p2, eos, 1, 1, '\n');
    if (p3 == p2)
        return pos;
    char_iterator cbegin = p3 + 1;
    char_iterator cend = p3 + 1;
    while (p3 < eos) {
        char_iterator p4 = std::search (p3, eos, pat.cbegin (), pat.cend ());
        if (p4 == eos)
            return pos;
        cend = p4;
        p3 = p4 + pat.size ();
        char_iterator p5 = check_blockend (p3, eos);
        if (p5 >= eos || p3 < p5) {
            output.push_back ({SPRE, p1, p2});
            output.push_back ({CODE, cbegin, cend});
            output.push_back ({EPRE, cend, cend});
            return p5;
        }
    }
    return pos;
}

static char_iterator
scan_htmlcomment (char_iterator const pos, char_iterator const eos,
    std::wstring& tagname)
{
    char_iterator p1 = scan_of (pos, eos, 1, 1, '<');
    char_iterator p2 = scan_of (p1, eos, 1, 1, '!');
    char_iterator p3 = scan_of (p2, eos, 2, 2, '-');
    if (! (pos < p1 && p1 < p2 && p2 < p3))
        return pos;
    tagname.assign (L"!COMMENT");
    std::wstring pat (L"-->");
    char_iterator p4 = std::search (p3, eos, pat.cbegin (), pat.cend ());
    if (p4 == eos)
        return pos;
    return p4 + pat.size ();
}

static char_iterator
scan_htmlattr (char_iterator const pos, char_iterator const eos)
{
    char_iterator p1 = scan_of (pos, eos, 1, -1, ismdwhite);
    char_iterator p2 = scan_of (p1, eos, 1, -1, ishtname);
    if (! (pos < p1 && p1 < p2))
        return pos;
    char_iterator p3 = scan_of (p2, eos, 0, -1, ismdwhite);
    char_iterator p4 = scan_of (p3, eos, 1, 1, '=');
    if (p4 == p3)
        return p2;
    char_iterator p5 = scan_of (p4, eos, 0, -1, ismdwhite);
    char_iterator p6 = p5;
    if (p5 < eos && ('"' == *p5 || '\'' == *p5 || '`' == *p5))
        p6 = scan_quoted (p5, eos, *p5, *p5, '\\', ismdany);
    else
        p6 = scan_of (p5, eos, 1, -1, ishtattr);
    if (p6 == p5)
        return pos;
    return p6;
}

static char_iterator
scan_htmltag (char_iterator const pos, char_iterator const eos,
    std::wstring& tagname)
{
    char_iterator pcom = scan_htmlcomment (pos, eos, tagname);
    if (pos < pcom)
        return pcom;
    char_iterator p1 = scan_of (pos, eos, 1, 1, '<');
    char_iterator p2 = scan_of (p1, eos, 0, 1, '/');
    char_iterator p3 = scan_of (p2, eos, 1, -1, ishtname);
    if (! (pos < p1 && p2 < p3))
        return pos;
    tagname.assign (p1, p3);
    char_iterator p4 = p3;
    while (p4 < eos) {
        char_iterator p5 = scan_htmlattr (p4, eos);
        if (p5 == p4)
            break;
        p4 = p5;
    }
    char_iterator p6 = scan_of (p4, eos, 0, -1, ismdwhite);
    char_iterator p7 = scan_of (p6, eos, 0, 1, '/');
    char_iterator p8 = scan_of (p7, eos, 1, 1, '>');
    if (p8 == p7)
        return pos;
    return p8;
}

static char_iterator
parse_blockhtml (char_iterator const bos, char_iterator const pos,
    char_iterator const eos, std::deque<token_type>& output)
{
    if (pos - 2 >= bos && '\n' != pos[-2])
        return pos;
    if (pos - 1 >= bos && '\n' != pos[-1])
        return pos;
    std::wstring tagname;
    char_iterator p1 = scan_htmltag (pos, eos, tagname);
    if (p1 == pos)
        return pos;
    std::wstring pat1 = std::wstring (L" ") + tagname + L" ";
    if (blocktag.find (pat1) == std::wstring::npos)
        return pos;
    if (tagname == L"hr" || tagname == L"!COMMENT" || '/' == p1[-2]) {
        char_iterator p3 = check_blockend (p1, eos);
        if (p3 >= eos || p1 < p3) {
            output.push_back ({HTML, pos, p3});
            return p3;
        }
    }
    else {
        std::wstring pat2 = std::wstring (L"</") + tagname;
        while (p1 < eos) {
            char_iterator p2 = std::search (p1, eos, pat2.cbegin (), pat2.cend ());
            if (p2 == eos)
                return pos;
            char_iterator p3 = scan_of (p2 + pat2.size (), eos, 0, -1, ismdwhite);
            p1 = scan_of (p3, eos, 1, 1, '>');
            if (p1 == p3)
                return pos;
            char_iterator p5 = check_blockend (p1, eos);
            if (p5 >= eos || p1 < p5) {
                output.push_back ({HTML, pos, p5});
                return p5;
            }
        }
    }
    return pos;
}

static char_iterator
scan_refdef_id (char_iterator const pos, char_iterator const eos,
    std::wstring& id)
{
    char_iterator p1 = scan_tab_not (pos, eos);
    char_iterator p2 = scan_quoted (p1, eos, '[', ']', '\\', ismdprint);
    if (p1 < p2 && ']' == p1[1])
        return pos;
    char_iterator p3 = scan_of (p2, eos, 1, 1, ':');
    char_iterator p4 = scan_of (p3, eos, 1, -1, ismdspace);
    if (! (p1 < p2 && p2 < p3 && p3 < p4))
        return pos;
    id = decode_linkid (p1 + 1, p2 - 1);
    return p4;
}

static char_iterator
scan_refdef_uri (char_iterator const pos, char_iterator const eos,
    std::wstring& uri)
{
    char_iterator p1 = pos;
    char_iterator p2 = pos;
    char_iterator p3 = scan_quoted (pos, eos, '<', '>', '\\', ismdprint);
    if (pos < p3) {
        p1 = pos + 1;
        p2 = p3 - 1;
    }
    else {
        p3 = scan_of (pos, eos, 1, -1, ismdgraph);
        p1 = pos;
        p2 = p3;
    }
    if (p1 >= p2)
        return pos;
    uri.assign (p1, p2);
    return p3;
}

static char_iterator
scan_refdef_title (char_iterator const pos, char_iterator const eos,
    std::wstring& title)
{
    char_iterator p1 = scan_of (pos, eos, 0, -1, ismdspace);
    char_iterator p2 = scan_of (p1, eos, 1, 1, '\n');
    if (p1 < p2)
        p2 = scan_of (p2, eos, 0, -1, ismdspace);
    if (pos < p2 && p2 < eos) {
        if ('"' == *p2 || '\'' == *p2 || '`' == *p2 || '(' == *p2) {
            int qq = '(' == *p2 ? ')' : *p2;
            char_iterator p4 = scan_of (p2, eos, 0, -1, ismdprint);
            char_iterator p3 = rscan_of (p2, p4, ismdspace);
            if (p3 - p2 > 2 && qq == p3[-1]) {
                title.assign (p2 + 1, p3 - 1);
                return p4;
            }
        }
    }
    return pos;
}

static char_iterator
parse_refdef (char_iterator const pos, char_iterator const eos,
    refdict_type& dict)
{
    reflink_type entry;
    char_iterator p1 = scan_refdef_id (pos, eos, entry.id);
    if (p1 == pos || '^' == entry.id[0])
        return pos;
    char_iterator p2 = scan_refdef_uri (p1, eos, entry.uri);
    if (p2 == pos)
        return pos;
    char_iterator p3 = scan_refdef_title (p2, eos, entry.title);
    char_iterator p4 = scan_of (p3, eos, 0, -1, ismdspace);
    char_iterator p5 = scan_of (p4, eos, 1, 1, '\n');
    if (p5 < eos && p4 == p5)
        return pos;
    dict[entry.id] = entry;
    return p5;
}

static void
split_lines (std::wstring const &input, std::deque<token_type>& output,
    refdict_type& dict)
{
    char_iterator p4 = input.cbegin ();
    while (p4 < input.cend ()) {
        char_iterator p1 = p4;
        if ((p4 = parse_blockcode (input.cbegin (), p1, input.cend (), output)) > p1)
            continue;
        if ((p4 = parse_blockhtml (input.cbegin (), p1, input.cend (), output)) > p1)
            continue;
        if ((p4 = parse_refdef (p1, input.cend (), dict)) > p1)
            continue;
        char_iterator p2 = scan_of (p1, input.cend (), 0, -1, ismdspace);
        char_iterator p3 = scan_of (p2, input.cend (), 0, -1, ismdprint);
                      p4 = scan_of (p3, input.cend (), 1, 1, '\n');
        if (p2 == p3)
            output.push_back ({BLANK, p3, p4});
        else
            output.push_back ({LINE, p1, p4});
    }
}

/* parse_inline - INLINE tokenizer and parser */

static char_iterator
parse_inline_loop (char_iterator const bos, char_iterator const pos,
    char_iterator const eos,
    std::deque<token_type>& output, refdict_type const& dict,
    std::deque<nest_type>& nest);

static char_iterator
parse_text (char_iterator const tbegin, char_iterator const tend,
    std::deque<token_type>& output)
{
    if (tbegin >= tend)
        return tend;
    if (! output.empty () && TEXT == output.back ().kind) {
        if (output.back ().cend == tbegin) {
            output.back ().cend = tend;
            return tend;
        }
    }
    output.push_back ({TEXT, tbegin, tend});
    return tend;
}

bool nest_exists (std::deque<nest_type>& nestlist, int n)
{
    for (auto i = nestlist.cbegin (); i < nestlist.cend (); ++i)
        if (n == 0 && i->n == 0)
            return true;
        else if (n == 1 && (i->n == 1 || i->n == 3))
            return true;
        else if (n == 2 && (i->n == 2 || i->n == 3))
            return true;
        else if (n == 3 && (i->n == 1 || i->n == 2 || i->n == 3))
            return true;
    return false;
}

static void
patch_emphasis (char_iterator embegin, char_iterator emend,
    bool leftwhite, bool rightwhite,
    std::deque<token_type>& output, std::deque<nest_type>& nest)
{
    int n1 = emend - embegin;
    int n2 = 3 - n1;
    int sem1 = 1 == n1 ? SEM : SSTRONG;
    int eem1 = 1 == n1 ? EEM : ESTRONG;
    int sem2 = 1 == n2 ? SEM : SSTRONG;
    bool already = nest_exists (nest, n1);
    if (! already) {
        if (! rightwhite) {
            nest.push_back ({output.size (), n1});
            output.push_back ({sem1, embegin, emend});
            return;
        }
    }
    else if (nest.back ().n == n1 || nest.back ().n == 3) {
        int smark = output[nest.back ().pos].cbegin[0];
        if (! leftwhite && smark == embegin[0]) {
            nest.pop_back ();
            output.push_back ({eem1, embegin, emend});
            if (! nest.empty () && nest.back ().n == 3) {
                int pos = nest.back ().pos;
                output[pos].kind = sem2;
                output[pos].cend = output[pos].cbegin + n2;
                output[pos + 1].kind = sem1;
                nest.back ().n = n2;
            }
            return;
        }
    }
    output.push_back ({TEXT, embegin, emend});
}

static void
patch_emphasis_three (char_iterator embegin, char_iterator emend,
    bool leftwhite, bool rightwhite,
    std::deque<token_type>& output, std::deque<nest_type>& nest)
{
    std::size_t nnest = nest.size ();
    bool already = nest_exists (nest, 3);
    if (! already) {
        if (! rightwhite) {
            nest.push_back ({output.size (), 3});
            nest.push_back ({output.size (), 3});
            output.push_back ({SSTRONG, embegin, emend});
            output.push_back ({SEM, embegin, embegin});
            return;
        }
    }
    else if (nnest >= 2 && nest[nnest - 1].n > 0 && nest[nnest - 2].n > 0) {
        int smark = output[nest.back ().pos].cbegin[0];
        if (leftwhite || smark != embegin[0])
            ;
        else if (nest.back ().n != 2) {
            output.push_back ({EEM, embegin, emend});
            output.push_back ({ESTRONG, embegin, emend});
            nest.pop_back ();
            nest.pop_back ();
            return;
        }
        else {
            output.push_back ({ESTRONG, embegin, emend});
            output.push_back ({EEM, embegin, emend});
            nest.pop_back ();
            nest.pop_back ();
            return;
        }
    }
    parse_text (embegin, emend, output);
}

static char_iterator
parse_space (char_iterator const pos, char_iterator const eos,
    std::deque<token_type>& output)
{
    char_iterator p1 = scan_of (pos, eos, 1, -1, ' ');
    char_iterator p2 = scan_of (p1, eos, 1, 1, '\n');
    if (p1 - pos >= 2 && p1 < p2) {
        output.push_back ({BREAK, pos, p2});
        return p2;
    }
    return parse_text (pos, p2, output);
}

static char_iterator
parse_escape (char_iterator const pos, char_iterator const eos,
    std::deque<token_type>& output)
{
    char_iterator p1 = scan_of (pos, eos, 1, 1, '\\');
    char_iterator p2 = scan_of (p1, eos, 1, 1, ismdescapable);
    if (p1 == p2)
        return parse_text (pos, p1, output);
    else
        return parse_text (pos, p2, output);    // deferred unescape
}

static char_iterator
parse_inlinecode (char_iterator const pos, char_iterator const eos,
    std::deque<token_type>& output)
{
    char_iterator p1 = scan_of (pos, eos, 1, -1, '`');
    char_iterator p2 = scan_of (p1, eos, 0, -1, ismdwhite);
    char_iterator p3 = std::search (p2, eos, pos, p1);
    if (p3 == eos)
        return parse_text (pos, p2, output);
    char_iterator p4 = scan_of (p3 + (p1 - pos), eos, 0, -1, '`');
    p3 = p4 - (p1 - pos);
    p3 = rscan_of (p2, p3, ismdwhite);
    output.push_back ({SCODE, p2, p2});
    output.push_back ({CODE, p2, p3});
    output.push_back ({ECODE, p3, p3});
    return p4;
}

static char_iterator
parse_emphasis (char_iterator const bos, char_iterator const pos,
    char_iterator const eos,
    std::deque<token_type>& output, std::deque<nest_type>& nest)
{
    char_iterator p1 = scan_of (pos, eos, 1, -1, *pos);
    int n = p1 - pos;
    bool leftwhite = pos == bos || ismdwhite (pos[-1]);
    bool rightwhite = p1 == eos || ismdwhite (p1[0])
        || (('.' == p1[0] || ',' == p1[0] || ';' == p1[0] || ':' == p1[0])
            && (p1 + 1 == eos || ismdwhite (p1[1])));
    if (n > 3 || (leftwhite && rightwhite))
        return parse_text (pos, p1, output);
    else if (n == 1)
        patch_emphasis (pos, p1, leftwhite, rightwhite, output, nest);
    else if (n == 2)
        patch_emphasis (pos, p1, leftwhite, rightwhite, output, nest);
    else if (n == 3)
        patch_emphasis_three (pos, p1, leftwhite, rightwhite, output, nest);
    return p1;
}

bool
match_uri (char_iterator const p, char_iterator const e)
{
    static const wchar_t *scheme[] = {
        L"https://", L"http://", L"ftp://", L"ftps://", L"mailto:"};
    int n = sizeof (scheme) / sizeof (scheme[0]);
    for (int i = 0; i < n; ++i) {
        std::wstring s (scheme[i]);
        if (e - p >= s.size () && std::equal (s.cbegin (), s.cend (), p))
            return true;
    }
    return false;
}

static char_iterator
parse_angle (char_iterator const pos, char_iterator const eos,
    std::deque<token_type>& output)
{
    std::wstring tagname;
    char_iterator p1 = scan_htmltag (pos, eos, tagname);
    if (pos < p1) {
        output.push_back ({HTML, pos, p1});
        return p1;
    }
    char_iterator p2 = scan_quoted (pos, eos, '<', '>', '\\', ismdprint);
    if (p2 - pos > 2) {
        if (match_uri (pos + 1, p2 - 1)) {
            output.push_back ({SABEGIN, pos, pos});
            output.push_back ({URI, pos + 1, p2 - 1});
            output.push_back ({SAEND, p2, p2});
            output.push_back ({TEXT, pos + 1, p2 - 1});
            output.push_back ({EA, p2, p2});
            return p2;
        }
        else
            parse_text (pos, p2, output);
    }
    char_iterator p3 = scan_of (pos, eos, 1, -1, '<');
    return parse_text (pos, p3, output);
}

static char_iterator
parse_link_bracket (
    char_iterator const pos,
    char_iterator const eos,
    char_iterator const altbegin,
    char_iterator const altend,
    std::deque<token_type>& attribute)
{
    char_iterator p1 = scan_of (pos, eos, 0, -1, ismdwhite);
    char_iterator p2 = scan_quoted (p1, eos, '[', ']', '\\', ismdany);
    if (p2 - p1 > 2)
        attribute.push_back ({LINKID, p1 + 1, p2 - 1});
    else
        attribute.push_back ({LINKID, altbegin, altend});
    return p2;
}

static char_iterator
parse_link_paren (
    char_iterator const pos,
    char_iterator const eos,
    std::deque<token_type>& attribute)
{
    char_iterator p6 = scan_quoted (pos, eos, '(', ')', '\\', ismdany);
    if (pos == p6)
        return pos;
    char_iterator p1 = pos + 1;
    char_iterator p5 = rscan_of (p1, p6 - 1, ismdwhite);
    char_iterator p2 = p5;
    if ('<' == *p1) {
        p2 = scan_quoted (p1, p5, '<', '>', '\\', ismdany);
        p2 = p1 == p2 ? p1 + 1 : p2 - 1;
    }
    else
        p2 = p2 == p5 ? p1 + 1 : p2 + 1;
    char_iterator p3 = p5;
    char_iterator p4 = p5;
    if ('"' == p5[-1] || '\'' == p5[-1]) {
        int qq = p5[-1];
        p4 = std::find (p2, p5, qq);
        while (p4 < p5 && ! ismdwhite (p4[-1]))
            p4 = std::find (p4 + 1, p5, qq);
        p3 = rscan_of (p2, p4, ismdwhite);
    }
    if (p3 - p1 > 1 && '<' == p1[0] && '>' == p3[-1])
        attribute.push_back ({URI, p1 + 1, p3 - 1});
    else
        attribute.push_back ({URI, p1, p3});
    if (p5 - p4 > 1 && p4[0] == p5[-1] && ('"' == p5[-1] || '\'' == p5[-1]))
        attribute.push_back ({TITLE, p4 + 1, p5 - 1});
    return p6;
}

static char_iterator
parse_make_link (
    char_iterator const cbegin,
    char_iterator const cend,
    std::deque<token_type>& inner,
    std::deque<token_type>& attribute,
    std::deque<token_type>& output)
{
    output.push_back ({SABEGIN, cbegin, cbegin});
    output.insert (output.end (), attribute.begin (), attribute.end ());
    output.push_back ({SAEND, cbegin, cbegin});
    output.insert (output.end (), inner.begin (), inner.end ());
    output.push_back ({EA, cend, cend});
    return cend;
}

static bool
parse_fetch_reference_link (
    refdict_type const& dict, std::deque<token_type>& attribute)
{
    std::wstring linkid = decode_linkid (attribute[0].cbegin, attribute[0].cend);
    auto i = dict.find (linkid);
    if (i == dict.end ())
        return false;
    reflink_type const& rf = i->second;
    attribute.clear ();
    attribute.push_back ({URI, rf.uri.cbegin (), rf.uri.cend ()});
    if (! rf.title.empty ())
        attribute.push_back ({TITLE, rf.title.cbegin (), rf.title.cend ()});
    return true;
}

static char_iterator
parse_link (
    char_iterator const bos,
    char_iterator const pos,
    char_iterator const eos,
    std::deque<token_type>& output, refdict_type const& dict,
    std::deque<nest_type>& nest)
{
    std::deque<token_type> inner;
    std::deque<token_type> attribute;
    nest.push_back ({output.size (), 0});
    char_iterator p1 = scan_of (pos, eos, 1, 1, '[');
    if (pos == p1)
        return pos;
    char_iterator p2
        = parse_inline_loop (bos, p1, eos, inner, dict, nest);
    while (nest.back ().n != 0) {
        inner[nest.back ().pos].kind = TEXT;
        nest.pop_back ();
    }
    char_iterator p3 = scan_of (p2, eos, 1, 1, ']');
    nest.pop_back ();
    bool already = nest_exists (nest, 0);
    if (p1 == p2 || p2 == p3)
        return parse_text (pos, p1, output);
    char_iterator p4 = parse_link_paren (p3, eos, attribute);
    if (! already && p3 < p4)
        return parse_make_link (pos, p4, inner, attribute, output);
    char_iterator p5 = parse_link_bracket (p3, eos, p1, p2, attribute);
    if (! already && parse_fetch_reference_link (dict, attribute))
        return parse_make_link (pos, p5, inner, attribute, output);
    parse_text (pos, p1, output);           // '['
    parse_inline_loop (bos, p1, p2, output, dict, nest);
    return parse_text (p2, p5, output);    // ']'
}

static char_iterator
parse_make_image (
    char_iterator const pos,
    std::deque<token_type>& inner,
    std::deque<token_type>& attribute,
    std::deque<token_type>& output)
{
    output.push_back ({IMGBEGIN, pos, pos});
    output.insert (output.end (), attribute.begin (), attribute.end ());
    output.insert (output.end (), inner.begin (), inner.end ());
    output.push_back ({IMGEND, pos, pos});
    return pos;
}

static char_iterator
parse_image (
    char_iterator const pos,
    char_iterator const eos,
    std::deque<token_type>& output, refdict_type const& dict)
{
    std::deque<token_type> inner;
    std::deque<token_type> attribute;
    char_iterator p1 = scan_of (pos, eos, 1, 1, '!');
    char_iterator p2 = scan_of (p1, eos, 1, 1, '[');
    if (pos == p1 || p1 == p2)
        return pos;
    char_iterator p3 = scan_quoted (p1, eos, '[', ']', '\\', ismdany);
    if (p1 == p3)
        return parse_text (pos, p2, output);
    inner.push_back ({ALT, p2, p3 - 1});
    char_iterator p4 = parse_link_paren (p3, eos, attribute);
    if (p3 < p4)
        return parse_make_image (p4, inner, attribute, output);
    char_iterator p5 = parse_link_bracket (p3, eos, p2, p3 - 1, attribute);
    if (parse_fetch_reference_link (dict, attribute))
        return parse_make_image (p5, inner, attribute, output);
    return parse_text (pos, p5, output);
}

static char_iterator
parse_inline_loop (
    char_iterator const bos,
    char_iterator const pos,
    char_iterator const eos,
    std::deque<token_type>& output,
    refdict_type const& dict,
    std::deque<nest_type>& nest)
{
    char_iterator p1 = pos;
    while (p1 < eos && ']' != *p1) {
        if (' ' == *p1)
            p1 = parse_space (p1, eos, output);
        else if ('\\' == *p1)
            p1 = parse_escape (p1, eos, output);
        else if ('`' == *p1)
            p1 = parse_inlinecode (p1, eos, output);
        else if ('*' == *p1 || '_' == *p1)
            p1 = parse_emphasis (bos, p1, eos, output, nest);
        else if ('<' == *p1)
            p1 = parse_angle (p1, eos, output);
        else if ('[' == *p1)
            p1 = parse_link (bos, p1, eos, output, dict, nest);
        else if ('!' == *p1)
            p1 = parse_image (p1, eos, output, dict);
        else {
            static std::wstring ccls (L" \\`*_<![]");
            char_iterator p2
                = std::find_first_of (p1, eos, ccls.cbegin (), ccls.cend ());
            p1 = parse_text (p1, p2, output);
        }
    }
    return p1;
}

static void
parse_inline (std::wstring const& input, std::deque<token_type>& output,
    refdict_type const& dict)
{
    std::deque<nest_type> nest;
    char_iterator const bos = input.cbegin ();
    char_iterator const eos = input.cend ();
    char_iterator pos = bos;
    while (pos < eos) {
        char_iterator pos0 = pos;
        pos = parse_inline_loop (bos, pos0, eos, output, dict, nest);
        if (pos0 == pos && ']' == *pos)
            pos = parse_text (pos, pos + 1, output);
    }
    while (! nest.empty ()) {
        output[nest.back ().pos].kind = TEXT;
        nest.pop_back ();
    }
}

/* print_inline - INLINE output builder */

static bool
check_html5entity (char_iterator& s0, char_iterator const e)
{
    static int tbl[7][6] = {
    //      d   x   a   #   ;
        {0, 0,  0,  0,  0,  0},
        {0, 0,  2,  2,  3,  0}, // S1: [A-Za-z] S2 | '#' S3
        {0, 2,  2,  2,  0,  9}, // S2: [A-Za-z0-9] S2 | ';' ACCEPT
        {0, 4,  0,  0,  0,  0}, // S3: [0-9] S4 | 'x' S5
        {0, 4,  0,  0,  0,  9}, // S4: [0-9] S4 | ';' ACCEPT
        {0, 6,  6,  0,  0,  0}, // S5: [0-9] S6 | [A-Fa-f] S6
        {0, 6,  6,  0,  0,  9}, // S6: [0-9] S6 | [A-Fa-f] S6 | ';' ACCEPT
    };
    if (! (s0 < e && '&' == *s0))
        return false;
    char_iterator s = s0 + 1;
    int state = 1;
    for (; s < e; ++s) {
        int ccls = ('0' <= *s && *s <= '9') ? 1
                 : ('A' <= *s && *s <= 'F') ? 2 : ('a' <= *s && *s <= 'f') ? 2
                 : ('G' <= *s && *s <= 'Z') ? 3 : ('g' <= *s && *s <= 'z') ? 3
                 : '#' == *s ? 4 : ';' == *s ? 5 : 0;
        state = (3 == state && 'x' == *s) ? 5 : tbl[state][ccls];
        if (0 == state)
            break;
        if (9 == state) {
            s0 = s + 1;
            return true;
        }
    }
    return false;
}

static std::wstring
decode_utf8 (std::string octets)
{
    std::locale loc (std::locale::classic (), "C.UTF-8", std::locale::ctype);
    auto& cvt = std::use_facet<std::codecvt<wchar_t, char, std::mbstate_t>> (loc);
    auto mb = std::mbstate_t ();
    std::wstring str (octets.size (), L'\0');
    char const* octetsnext;
    wchar_t* strnext;
    cvt.in (mb, &octets[0], &octets[octets.size ()], octetsnext,
                &str[0], &str[str.size ()], strnext);
    str.resize (strnext - &str[0]);
    return str;
}

static std::string
encode_utf8 (std::wstring str)
{
    std::locale loc (std::locale::classic (), "C.UTF-8", std::locale::ctype);
    auto& cvt = std::use_facet<std::codecvt<wchar_t, char, std::mbstate_t>> (loc);
    auto mb = std::mbstate_t ();
    std::string octets(str.size () * cvt.max_length (), '\0');
    wchar_t const* strnext;
    char* octetsnext;
    cvt.out (mb, &str[0], &str[str.size ()], strnext,
                 &octets[0], &octets[octets.size ()], octetsnext);
    octets.resize (octetsnext - &octets[0]);
    return octets;
}

static void
print_with_escape_html (char_iterator s, char_iterator const e,
    std::wostream& output)
{
    for (; s < e; ++s)
        if ('&' == *s) {
            char_iterator s1 = s;
            if (check_html5entity (s1, e)) {
                for (; s < s1; ++s)
                    output.put (*s);
                --s;
            }
            else
                output << L"&amp;";
        }
        else switch (*s) {
        default: output << *s; break;
        case '<': output << L"&lt;"; break;
        case '>': output << L"&gt;"; break;
        case '"': output << L"&quot;"; break;
        case '\'': output << L"&#39;"; break;
        }
}

static void
print_with_escape_htmlall (char_iterator s, char_iterator const e,
    std::wostream& output)
{
    for (; s < e; ++s)
        switch (*s) {
        default: output << *s; break;
        case '&': output << L"&amp;"; break;
        case '<': output << L"&lt;"; break;
        case '>': output << L"&gt;"; break;
        case '"': output << L"&quot;"; break;
        case '\'': output << L"&#39;"; break;
        }
}

static void
print_with_escape_uri (char_iterator s, char_iterator const e,
    std::wostream& output)
{
    static const std::string safe ("-_.,:;*+=()/~?#");
    static const std::string amp ("&amp;");
    std::wstring w (s, e);
    std::string o = encode_utf8 (w);
    std::string t;
    for (std::string::const_iterator s = o.begin (); s != o.end (); ++s) {
        int c = static_cast<unsigned char> (*s);
        if (ismdalnum (c) || safe.find (c) != std::string::npos)
            t.append (1, c);        // [0-9A-Za-z\-_.,:;*+=()/~?\#]
        else if (s + 2 < o.end ()
                && c == '%' && ismdxdigit (s[1]) && ismdxdigit (s[2]))
            t.append (1, c);        // %[0-9A-Fa-f]{2}
        else if (s + 4 < o.end () && std::equal (amp.cbegin (), amp.cend (), s))
            t.append (1, c);        // &amp;
        else if (c == '&')
            t += "&amp;";           // &
        else {
            int hi ((c >> 4) & 15);
            int lo (c & 15);
            t.append (1, '%');
            t.append (1, hi < 10 ? hi + '0' : hi + 'A' - 10);
            t.append (1, lo < 10 ? lo + '0' : lo + 'A' - 10);
        }
    }
    output << decode_utf8 (t);
}

static token_iterator
print_innerlink (token_iterator p, std::wostream& output, refdict_type const& dict)
{
    static const std::wstring stremtpy (L"");
    int skind = p->kind;    // SABEGIN || IMGBEGIN
    output << kindname[skind];
    ++p;
    char_iterator titleb = stremtpy.cbegin ();
    char_iterator titlee = stremtpy.cend ();
    if (URI == p->kind) {
        std::wstring uri = unescape_backslash (p->cbegin, p->cend);
        print_with_escape_uri (uri.cbegin(), uri.cend (), output);
        ++p;
        if (TITLE == p->kind) {
            titleb = p->cbegin;
            titlee = p->cend;
            ++p;
        }
    }
    if (IMGBEGIN == skind) {
        output << kindname[p->kind];    // ALT
        std::wstring alt = unescape_backslash (p->cbegin, p->cend);
        print_with_escape_html (alt.cbegin(), alt.cend (), output);
        ++p;
    }
    if (titleb < titlee) {
        output << kindname[TITLE];
        std::wstring title = unescape_backslash (titleb, titlee);
        print_with_escape_html (title.cbegin(), title.cend (), output);
    }
    output << kindname[p->kind];  // EAEND || IMGEND
    return p;
}

static void
print_inline (std::deque<token_type>const & input, std::wostream& output,
    refdict_type const& dict)
{
    for (token_iterator p = input.cbegin (); p < input.cend (); ++p) {
        if (BREAK <= p->kind)
            output << kindname[p->kind];
        else if (CODE == p->kind)
            print_with_escape_htmlall (p->cbegin, p->cend, output);
        else if (HTML == p->kind)
            for (char_iterator i = p->cbegin; i < p->cend; ++i)
                output << *i;
        else if (SABEGIN == p->kind || IMGBEGIN == p->kind)
            p = print_innerlink (p, output, dict);
        else if (TEXT == p->kind) {
            std::wstring src;
            for (; p < input.cend () && TEXT == p->kind; ++p)
                if (p->cbegin < p->cend)
                    src.append (p->cbegin, p->cend);
            std::wstring text = unescape_backslash (src.cbegin(), src.cend ());
            print_with_escape_html (text.cbegin (), text.cend (), output);
            --p;
        }
    }
}

/* print_block - BLOCK output builder */

static void
print_block (std::deque<token_type> const& input,
    std::wostream& output,
    refdict_type const& dict)
{
    std::deque<token_type> doc;
    line_iterator dot = input.cbegin ();
    line_iterator dol = input.cend ();
    for (; dot < dol && BLANK == dot->kind; ++dot)
        ;
    while (dot < dol) {
        line_iterator olddot = dot;
        if (BLANK == dot->kind) {
            for (; dot < dol && BLANK == dot->kind; ++dot)
                ;
            if (dot < dol)
                output << L"\n";
        }
        else if (HRULE <= dot->kind) {
            if (SOLIST == dot->kind || SULIST == dot->kind) {
                if (dot - 1 > input.cbegin () && dot[-1].kind == INLINE)
                    output << L"\n";
            }
            output << kindname[dot->kind];
            ++dot;
        }
        else if (HTML == dot->kind) {
            for (char_iterator p = dot->cbegin; p < dot->cend; ++p)
                output << *p;
            ++dot;
        }
        else if (CODE == dot->kind) {
            for (; dot < dol && CODE == dot->kind; ++dot) {
                if (dot + 1 < dol && CODE != dot[1].kind
                        && dot->cbegin < dot->cend - 1 && '\n' == dot->cend[-1])
                    print_with_escape_htmlall (dot->cbegin, dot->cend - 1, output);
                else
                    print_with_escape_htmlall (dot->cbegin, dot->cend, output);
            }
        }
        else if (INLINE == dot->kind) {
            std::wstring src;
            for (; dot < dol && INLINE == dot->kind; ++dot)
                src.append (dot->cbegin, dot->cend);
            if (src.size () > 0 && '\n' == src.back ())
                src.pop_back ();
            std::deque<token_type> inline_input;
            parse_inline (src, inline_input, dict);
            print_inline (inline_input, output, dict);
        }
        if (olddot == dot)
            ++dot;
    }
}
