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
 * Author:  Chris Lanczycki
 *
 * File Description:
 *
 *       Various utility functions for CDTree
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/PDB_mol_id.hpp>
#include <objects/cdd/Cdd.hpp>
#include <objects/cdd/Cdd_id.hpp>
#include <objects/cdd/Cdd_id_set.hpp>
#include <objects/cdd/Global_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <algorithm>
#include <stdio.h>
#include <algo/structure/cd_utils/cuUtils.hpp>

//  comma is now allowed (4/15/04)
//  single-quote is now allowed (4/28/04)

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

const int CDTreeColorCycle[] = {
    0x806000,  // dark brown
    0xff0000,  // red
    0x149600,  // dark green
    0x0000ff,  // dark blue
    0xb428ff,  // purple
    0xff8c1a,  // orange
    0x00ffff,  // light blue
    0xff499b,  // pink
    0xc89600,  // brown
    0x00ff00,  // light green
    0xaaaaaa,  // gray
    0xcee000,  // olive
    0x4a85ff,  // blue
    0xffc800,  // light orange
    0xc800bc,  // maroon
    0xeeee00,  // yellow
};
const int kNumColorsInCDTreeColorCycle = sizeof(CDTreeColorCycle)/sizeof(CDTreeColorCycle[0]);

void Make_GI_or_PDB_String_CN3D(const CRef< CSeq_id > SeqID, std::string& Str) {
//-------------------------------------------------------------------
// make a string for a seq-id (underscore delimiter for PDBs)
//-------------------------------------------------------------------
  TGi   GI;

  if (SeqID.Empty()) {
      Str += "<Empty Sequence>";
      return;
  } else if (!SeqID->IsGi() && !SeqID->IsPdb()) {
      Str += "<Non-gi/pdb Sequence Types Unsupported>";
      return;
  }

  if (SeqID->IsGi()) {
    GI = SeqID->GetGi();
    Str += NStr::NumericToString<TGi>(GI);
  } else if (SeqID->IsPdb()) {

      const CPDB_seq_id&  pPDB_ID = SeqID->GetPdb();
      
#ifndef _STRUCTURE_USE_LONG_PDB_CHAINS_

      char buf[1024];
      char chain=pPDB_ID.GetChain();
      sprintf(buf,"pdb %s_%c",pPDB_ID.GetMol().Get().c_str(),chain);
      int len=strlen(buf);
      if(chain==' ')buf[len-2]=0; // if there is no chain info (i.e., uses the default chain value)

      Str += string(buf);
#else
      Str += pPDB_ID.GetMol().Get() + "_" + pPDB_ID.GetEffectiveChain_id();
#endif
      
  }

}

string Make_SeqID_String(const CRef< CSeq_id > SeqID, bool Pad, int Len) {
//-------------------------------------------------------------------
// make a string for a seq-id (space delimiter for PDBs)
//-------------------------------------------------------------------
  TGi   GI;
  string Str = kEmptyStr;

  if (SeqID.Empty()) {
      return "<Empty Sequence>";
  } else if (!SeqID->IsGi() && !SeqID->IsPdb() && !SeqID->IsOther() && !SeqID->IsLocal()) {
      return SeqID->GetSeqIdString();  
  }

  //  Custom string construction for Gi, PDB, Other, or Local types
  if (SeqID->IsGi()) {
      GI = SeqID->GetGi();
      Str = NStr::NumericToString<TGi>(GI);
  } else if (SeqID->IsPdb()) {
      const CPDB_seq_id&  pPDB_ID = SeqID->GetPdb();

#ifndef _STRUCTURE_USE_LONG_PDB_CHAINS_
      Str = pPDB_ID.GetMol().Get() + " " + string(1, (char) pPDB_ID.GetChain());;
#else
      Str = pPDB_ID.GetMol().Get() + " " + pPDB_ID.GetEffectiveChain_id();
#endif
      
  } else if (SeqID->IsOther()) {
      Str = SeqID->GetOther().GetAccession();
  } else if (SeqID->IsLocal()) {
	  if (SeqID->GetLocal().IsId()) {
          Str = SeqID->GetLocal().GetId();
	  } else if (SeqID->GetLocal().IsStr()) {
		  Str = SeqID->GetLocal().GetStr();
	  }
  }

  // pad with spaces to length of Len
  if (Pad) {
      if ( (int)Str.size() < Len) {
          Str.append(Len - Str.size(), ' ');
      }
  }
  return Str;
}

string GetSeqIDStr(const CRef< CSeq_id >& SeqID) 
{
    string Str = Make_SeqID_String(SeqID, false, 0);
    return Str;
}

//  Return the first CCdd_id object in a CD.
string CddIdString(const CCdd& cdd) {
    string s;
    list< CRef< CCdd_id > >::const_iterator i, ibegin, iend;

    ibegin = cdd.GetId().Get().begin();
    iend   = cdd.GetId().Get().end();
    for (i = ibegin; i != iend; ++i) {
        if (i != ibegin) {
            s.append(", ");
        }
        s.append(CddIdString(**i));
    }
	return s;
}


string CddIdString(const CCdd_id& id) {

    CCdd_id::E_Choice e = id.Which();
    if (e == CCdd_id::e_Uid) {
        return "UID " + NStr::IntToString(id.GetUid());
    } else if (e == CCdd_id::e_Gid) {
        string s = "Accession " + id.GetGid().GetAccession();
        if (id.GetGid().IsSetDatabase()) {
            s.append(" Database " + id.GetGid().GetDatabase());
        }
        if (id.GetGid().IsSetRelease()) {
            s.append(" Release " + id.GetGid().GetRelease());
        }
        if (id.GetGid().IsSetVersion()) {
            s.append(" Version " + NStr::IntToString(id.GetGid().GetVersion()));
        }
        return s;
    } else {
        return "Unset/Unknown Cdd_id";
    }

}

bool SameCDAccession(const CCdd_id& id1, const CCdd_id& id2) {

    bool result = false;
    CCdd_id::E_Choice e1 = id1.Which(), e2 = id2.Which();
    if (e1 == CCdd_id::e_Gid && e1 == e2) {
        if (id1.GetGid().GetAccession() == id2.GetGid().GetAccession()) {
            result = true;
        }
    }
    return result;
}

bool Prosite2Regex(const std::string& prosite, std::string* regex, std::string* errString) {
//-------------------------------------------------------------------
// copied from Paul.  see sequence_set.cpp
//-------------------------------------------------------------------
  errString->erase();
  try {
    // check allowed characters
    static const std::string allowed = "-ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[],(){}<>.";
    unsigned int i;
    for (i=0; i<prosite.size(); i++) {
      if (allowed.find(toupper((unsigned char) prosite[i])) == std::string::npos) break;
    }
    if (i != prosite.size()) throw "invalid ProSite character";
    if (prosite[prosite.size() - 1] != '.') throw "ProSite pattern must end with '.'";
    
    // translate into real regex syntax;
    regex->erase();
    
    bool inGroup = false;
    for (i=0; i<prosite.size(); i++) {
      
      // handle grouping and termini
      bool characterHandled = true;
      switch (prosite[i]) {
      case '-': case '.': case '>':
        if (inGroup) {
          *regex += ')';
          inGroup = false;
        }
        if (prosite[i] == '>') *regex += '$';
        break;
      case '<':
        *regex += '^';
        break;
      default:
        characterHandled = false;
        break;
      }
      if (characterHandled) continue;
      if (!inGroup && (
        (isalpha((unsigned char) prosite[i]) && toupper((unsigned char) prosite[i]) != 'X') ||
        prosite[i] == '[' || prosite[i] == '{')) {
        *regex += '(';
        inGroup = true;
      }
      
      // translate syntax
      switch (prosite[i]) {
      case '(':
        *regex += '{';
        break;
      case ')':
        *regex += '}';
        break;
      case '{':
        *regex += "[^";
        break;
      case '}':
        *regex += ']';
        break;
      case 'X': case 'x':
        *regex += '.';
        break;
      default:
        *regex += toupper((unsigned char) prosite[i]);
        break;
      }
    }
  }
 
  catch (const char *err) {
    *errString = string(err);
      //    AFrame::ShowError(err);
  }
  
  return true;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
