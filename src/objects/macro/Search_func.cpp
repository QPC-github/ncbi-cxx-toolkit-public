/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  J. Chen
 *
 * File Description:
 *   Evaluate string match
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'macro.asn'.
 */

#include <ncbi_pch.hpp>
#include <objects/macro/Search_func.hpp>
#include <util/multipattern_search.hpp>

///// This file is included in macro__.cpp (sic!), so these statics are visible elsewhere
namespace
{
    static const char* digit_str = "0123456789";
    static const char* alpha_str = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
};

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


bool CSearch_func::Empty() const
{
    switch (Which()) {
        case e_String_constraint:
            return GetString_constraint().Empty();
        case  e_Prefix_and_numbers:
            return (GetPrefix_and_numbers().empty());
        default: return false;
    }
    return false;
};


bool CSearch_func::x_DoesStrContainPlural(const string& word, char last_letter, char second_to_last_letter, char next_letter) const
{
    size_t len = word.size();
    if (last_letter == 's') {
        if (len >= 5  && word.substr(len-5) == "trans") {
            return false; // not plural;
        }
        else if (len > 3) {
            if (second_to_last_letter != 's'
                && second_to_last_letter != 'i'
                && second_to_last_letter != 'u'
                && next_letter == ',') {
                return true;
            }
        }
    }

    return false;
};


bool CSearch_func::x_StringMayContainPlural(const string& str) const
{
    char last_letter, second_to_last_letter, next_letter;
    bool may_contain_plural = false;
    string word_skip = " ,";
    size_t len;

    if (str.empty()) {
        return false;
    }
    vector <string> arr;
    arr = NStr::Split(str, " ,", arr, NStr::fSplit_Tokenize);
    if (arr.size() == 1) { // doesn't have ', ', or the last char is ', '
        len = arr[0].size();
        if (len == 1) {
            return false;
        }
        last_letter = arr[0][len-1];
        second_to_last_letter = arr[0][len-2]; 
        next_letter = (len == str.size()) ? ',' : str[len];
        may_contain_plural = x_DoesStrContainPlural(arr[0], last_letter, second_to_last_letter, next_letter);
    }
    else {
        string strtmp(str);
        size_t pos;
        vector <string>::const_iterator jt;
        ITERATE (vector <string>, it, arr) { 
            pos = strtmp.find(*it);
            len = (*it).size();
            if (len == 1) {
                strtmp = strtmp.substr(pos+len);
                strtmp = NStr::TruncateSpaces(strtmp, NStr::eTrunc_Begin);
                continue;
            }
            last_letter = (*it)[len-1];
            second_to_last_letter = (*it)[len-2];
            next_letter = (len == strtmp.size()) ? ',' : strtmp[pos+len];
            may_contain_plural = x_DoesStrContainPlural(*it, last_letter, second_to_last_letter, next_letter);
            if (may_contain_plural) {
                break;
            }
            jt = it;
            if (++jt != arr.end()) { // not jt++
                strtmp = strtmp.substr(strtmp.find(*jt));
            }
        }
    }
    return may_contain_plural;
};


char CSearch_func::x_GetClose(char bp) const
{
    if (bp == '(') return ')';
    else if (bp == '[') return ']';
    else if (bp == '{') return '}';
    else return bp;
};


static const char* skip_bracket_paren[] = {
    "(NAD(P)H)",
    "(NAD(P))",
    "(I)",
    "(II)",
    "(III)",
    "(NADPH)",
    "(NAD+)",
    "(NAPPH/NADH)",
    "(NADP+)",
    "[acyl-carrier protein]",
    "[acyl-carrier-protein]",
    "(acyl carrier protein)"
};


bool CSearch_func::x_SkipBracketOrParen(size_t idx, string& start) const
{
    bool rval = false;
    size_t ep, ns;

    if (idx > 2 && start.substr(idx-3, 6) == "NAD(P)") {
        rval = true;
        start = start.substr(idx + 3);
    } 
    else {
        size_t len;
        for (size_t i = 0; i < ArraySize(skip_bracket_paren); i++) {
        len = strlen(skip_bracket_paren[i]);
            if (start.substr(idx, len) == skip_bracket_paren[i]) {
                start = start.substr(idx + len);
                rval = true;
                break;
            }
        }
        if (!rval) {
            ns = start.find(start[idx], idx+1);
            ep = start.find(x_GetClose(start[idx]), idx+1);
            if (ep != string::npos && (ns == string::npos || ns > ep)) {
                if (ep - idx < 5) {
                    rval = true;
                    start = start.substr(ep+1);
                } 
                else if (ep - idx > 3 && start.substr(ep - 3, 3) == "ing") {
                rval = true;
                    start = start.substr(ep + 1);
                }
            }
        }
    }
    return rval;
};


bool CSearch_func::x_ContainsNorMoreSetsOfBracketsOrParentheses(const string& str, const int& n) const
{
    size_t idx, end;
    int num_found = 0;
    string open_bp("(["), sch_src(str);

    if (sch_src.empty()) {
        return false;
    }

    idx = sch_src.find_first_of(open_bp);
    while (idx != string::npos && num_found < n) {
        end = sch_src.find(x_GetClose(sch_src[idx]), idx);
        if (x_SkipBracketOrParen (idx, sch_src)) { // ignore it
            idx = sch_src.find_first_of(open_bp);
        }
        else if (end == string::npos) { // skip, doesn't close the bracket
            idx = sch_src.find_first_of(open_bp, idx+1);
        }
        else {
            idx = sch_src.find_first_of(open_bp, end);
            num_found++;
        }
    }

    if (num_found >= n) {
        return true;
    }
    else {
        return false;
    }
};


static const char* ok_num_prefix[] = {
    "DUF",
    "UPF",
    "IS",
    "TIGR",
    "UCP",
    "PUF",
    "CHP"
};


bool CSearch_func::x_PrecededByOkPrefix (const string& start_str) const
{
    size_t len_str = start_str.size();
    size_t len_i;
    for (size_t i = 0; i < ArraySize(ok_num_prefix); i++) {
        len_i = string(ok_num_prefix[i]).size();
        if (len_str >= len_i && (start_str.substr(len_str-len_i) == ok_num_prefix[i])) {
            return true;
        }
    }
    return false;
};


bool CSearch_func::x_InWordBeforeCytochromeOrCoenzyme(const string& start_str) const
{
    if (start_str.empty()) {
        return false;
    }
    size_t pos = start_str.find_last_of(' ');
    string comp_str1, comp_str2, strtmp;
    if (pos != string::npos) {
        strtmp = start_str.substr(0, pos);
        pos = strtmp.find_last_not_of(' ');
        if (pos != string::npos) {
            size_t len = strtmp.size();
            comp_str1 = strtmp.substr(len-10);
            comp_str2 = strtmp.substr(len-8);
            if ( (len >= 10  && NStr::EqualNocase(comp_str1, "cytochrome")) || (len >= 8 && NStr::EqualNocase(comp_str2, "coenzyme")) ) {
                return true;
            }
        }
    }
    return false;
};


bool CSearch_func::x_FollowedByFamily(string& after_str) const
{
    size_t pos = after_str.find_first_of(' ');
    if (pos != string::npos) {
        after_str = after_str.substr(pos+1);
        if (NStr::EqualNocase(after_str, 0, 6, "family")) {
            after_str = after_str.substr(7);
            return true;
        }
    } 

    return false;
};


bool CSearch_func::x_ContainsThreeOrMoreNumbersTogether(const string& str) const
{
    size_t p=0, p2;
    unsigned num_digits = 0;
    string sch_str(str), strtmp;
  
    while (!sch_str.empty()) {
        p = sch_str.find_first_of(digit_str);
        if (p == string::npos) {
            break;
        }
        strtmp = sch_str.substr(0, p);
        if (p && ( x_PrecededByOkPrefix(strtmp) || x_InWordBeforeCytochromeOrCoenzyme (strtmp))) {
            p2 = sch_str.find_first_not_of(digit_str, p+1);
            if (p2 != string::npos) {
                sch_str = sch_str.substr(p2);
                num_digits = 0;
            }
            else break;
        }
        else {
            num_digits ++;
            if (num_digits == 3) {
                sch_str = sch_str.substr(p+1);
                if (x_FollowedByFamily(sch_str)) {
                    num_digits = 0;
                }
                else return true;
            }
            if (p < sch_str.size() - 1) {
                sch_str = sch_str.substr(p+1);
                if (!isdigit(sch_str[0])) {
                    num_digits = 0;
                }
            }
            else break;
        }
    }
    return false;
};


bool CSearch_func::x_StringContainsUnderscore(const string& str) const
{
    if (str.find('_') == string::npos) {
        return false;
    }

    string strtmp;
    vector <string> arr;
    arr = NStr::Split(str, "_", arr, 0);
    for (unsigned i=0; i< arr.size() - 1; i++) {
        strtmp = arr[i+1];
        // strtmp was changed in the FollowedByFamily
        if (x_FollowedByFamily(strtmp)) {
            continue; 
        }
        else if (arr[i].size() < 3 || str[arr[i].size()-1] == ' ') {
            return true;
        }
        else {
            strtmp = arr[i].substr(arr[i].size()-3);
            if ( (strtmp == "MFS" || strtmp == "TPR" || strtmp == "AAA") && (isdigit(arr[i+1][0]) && !isdigit(arr[i+1][1])) ) {
                continue;
            }
            else return true;
        }
    }

    return false;
};


bool CSearch_func::x_IsPrefixPlusNumbers(const string& str, const string& prefix) const
{
    if (str.empty()) return false;

    size_t pattern_len = prefix.size();
    if (pattern_len > 0 && !NStr::EqualCase(str, 0, pattern_len, prefix)) {
        return false;
    }

    size_t digit_len = str.find_first_not_of(digit_str, pattern_len);
    if (digit_len != string::npos && digit_len == str.size()) {
        return true;
    }
    else return false;
};


bool CSearch_func::x_IsPropClose(const string& str, char open_p) const
{
    if (str.empty()) return false;
    else if (str[str.size()-1] != open_p) return false;
    else return true;
};


bool CSearch_func::x_StringContainsUnbalancedParentheses(const string& str) const
{
    size_t pos = 0;
    char ch_src;
    string strtmp, sch_src(str);
    while (!sch_src.empty()) {
        pos = sch_src.find_first_of("()[]");
        if (pos == string::npos) {
            if (strtmp.empty()) {
                return false;
            }
            else return true;
        }
        else {
            ch_src = sch_src[pos];
            if (ch_src == '(' || ch_src == '[') {
                strtmp += ch_src;
            }
            else if (sch_src[pos] == ')') {
                if (!x_IsPropClose(strtmp, '(')) {
                    return true;
                }
                else {
                    strtmp = strtmp.substr(0, strtmp.size()-1);
                }
            }
            else if (sch_src[pos] == ']') {
                if (!x_IsPropClose(strtmp, '[')) {
                    return true;
                }
                else {
                    strtmp = strtmp.substr(0, strtmp.size()-1);
                }
            }
        }
        sch_src = (pos < sch_src.size()-1) ? sch_src.substr(pos+1) : kEmptyStr;
    }
  
    if (strtmp.empty()) {
        return false;
    }
    else return true;
};


bool CSearch_func::x_ProductContainsTerm(const string& str, const string& pattern) const
{
    // don't bother searching for c-term or n-term if product name contains "domain"
    if (NStr::FindNoCase(str, "domain") != string::npos) {
        return false;
    }

    size_t pos = NStr::FindNoCase(str, pattern);
    // c-term and n-term must be either first word or separated from other word 
    // by space, num, or punct 
    if (pos != string::npos && (!pos || !isalpha (str[pos-1]))) {
        return true;
    }
    else return false;
}


bool CSearch_func::Match(const CMatchString& str) const
{
    const string& orig = str;

    switch (Which()){
        case e_String_constraint:
            {
                const CString_constraint& str_cons = GetString_constraint();
                return str_cons.Match(str);
            }
        case e_Contains_plural:
            return x_StringMayContainPlural(str);
        case e_N_or_more_brackets_or_parentheses:
            return x_ContainsNorMoreSetsOfBracketsOrParentheses(str, GetN_or_more_brackets_or_parentheses());
        case e_Three_numbers:
            return x_ContainsThreeOrMoreNumbersTogether (str);
        case e_Underscore:
            return x_StringContainsUnderscore (str);
        case e_Prefix_and_numbers:
            return x_IsPrefixPlusNumbers (str, GetPrefix_and_numbers());
        case e_All_caps:
            if (orig.find_first_not_of(alpha_str) != string::npos) {
                return false;
            }
            if (orig == str.original().uppercase()) {
                return true;
            }
            else return false;
        case e_Unbalanced_paren:
            return x_StringContainsUnbalancedParentheses (str);
        case e_Too_long:
            if (NStr::FindNoCase (orig, "bifunctional") == string::npos && NStr::FindNoCase (orig, "multifunctional") == string::npos && orig.size() > (unsigned) GetToo_long()) {
                return true;
            }
            else return false;
        case e_Has_term:
            return x_ProductContainsTerm(str, GetHas_term());
        default: break;
    }
    return false;
};



string CSearch_func::GetRegex() const
{
    switch (Which()) {
        case e_String_constraint:
            {
                const CString_constraint& constr = GetString_constraint();
                if (constr.IsSetMatch_text()) {
                    if (constr.GetNot_present()) {
                        NCBI_THROW(CException, eUnknown, "GetRegex is not implemented for NOT-PRESENT: " + constr.GetMatch_text());
                    }
                    string str = CMultipatternSearch::QuoteString(constr.GetMatch_text());
                    if (constr.IsSetIgnore_words() && constr.GetIgnore_words().IsSet()) {
                        for (auto words : constr.GetIgnore_words().Get()) {
                            if (words->IsSetSynonyms()) {
                                for (auto syn : words->GetSynonyms()) {
                                    str += "|" + CMultipatternSearch::QuoteString(syn);
                                }
                            }
                        }
                    }
                    str = "/" + str + "/i";
                    return str;
                }
                else {
                    NCBI_THROW(CException, eUnknown, "GetRegex Match text is not set");
                }
            }
        case e_Contains_plural:
            return "/[A-Za-hj-rtv-z]s\\b/";
        //case e_N_or_more_brackets_or_parentheses:
        case e_Three_numbers:
            return "/\\d\\d\\d/";
        case e_Underscore:
            return "/_/";
        case e_Prefix_and_numbers:
            return "/^" + CMultipatternSearch::QuoteString(GetPrefix_and_numbers()) + "\\d+$/";
        case e_All_caps:
            return "/^[A-Z]+$/";
        case e_Unbalanced_paren:
            return "/[\\(\\)\\[\\]]/";
        case e_Too_long:
            return "/.$/";
        //case e_Has_term:
        default: break;
    }
    NCBI_THROW(CException, eUnknown, "GetRegex is not implemented for subtype: " + NStr::IntToString(Which()));
    return "";
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1726, CRC32: 9fb286c1 */
