/* br_dplaymsg.c -- net.
 *
 * Messages on the DirectPlay wire: the small fixed command packet, the
 * name-carrying message that goes through a global memory handle, the error
 * code the log prints, and the session description hosting hands out.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#include <string.h>

/* -- Ghidra-matched functions --------------------------- */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_10037720: prototype in br_funcs.h */



/* FUN_100038f0: prototype in br_funcs.h */
/* BrDPlayRawSend: prototype in br_funcs.h */

/* WHAT IT DOES: send one small fixed command message -- a three-word packet
 * with a fixed command code and two arguments -- to a player, and only if
 * that player's channel is actually up. */
/* @implements 0x100371F0 glide FUN_100371f0 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_100371f0(BrDPlayCtx *param_1,int param_2,int param_3)

{
  struct { int cmd; int a; int b; } msg;
  
  if ((param_1 != 0) && (param_1->pDP != 0)) {
    msg.a = param_2;
    msg.cmd = 0x60000008;
    msg.b = param_3;
    if (param_1->f0C != 0) {
      FUN_100038f0(param_1,&msg,0xc,param_1->idPlayer);
    }
    return BrDPlayRawSend((void *)param_1->pDP,param_1->idPlayer,0,1,&msg,0xc);
  }
  return 0;
}


#include <windows.h>
/* FUN_10036a30: prototype in br_funcs.h */
/* BrComGetAlloc: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrDPlayRawSend: prototype in br_funcs.h */

/* WHAT IT DOES: send a message to one network player, taking care of the
 * global memory handle DirectPlay wants and freeing it afterwards. Refuses
 * and reports failure when there is no target or the network layer is
 * shutting down. */
/* @implements 0x100368A0 glide FUN_100368a0 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_100368a0(HWND param_1, BrDPlayCtx *param_2, int param_3)
{
  HGLOBAL hMem;
  LPCSTR lpString;
  int result;
  int *pMem;
  LPCVOID local_4;

  pMem = 0;
  local_4 = 0;
  if (param_2 == 0 || param_2->pDP == 0 || DAT_10ac5be4 != 0) {
    return 0;
  }
  hMem = GlobalAlloc(0x42, 0xc9);
  lpString = GlobalLock(hMem);
  if (lpString == 0) {
    result = 0x8007000e;
  }
  else {
    strcpy((char *)lpString, g_szBrName4DB0);
    result = FUN_10036a30((struct BrDPlayObj *)param_2->pDP, (int)param_2->idPlayer, lpString, &local_4, param_3);
    if (result >= 0) {
      PostMessageA(param_1, 0x501, 0, (LPARAM)local_4);
      local_4 = 0;
      {
        int n = lstrlenA(lpString);
        n += 8;
        hMem = GlobalAlloc(0x42, n);
        pMem = GlobalLock(hMem);
        if (pMem == 0) {
          result = 0x8007000e;
        }
        else {
          *pMem = (param_3 != 0) + 0x60000000;
          lstrcpyA((LPSTR)(pMem + 1), lpString);
          result = BrDPlayRawSend((void *)param_2->pDP, (int)param_2->idPlayer, 0, 1, pMem, n);
        }
      }
    }
  }
  if (lpString != 0) {
    GlobalUnlock(GlobalHandle(lpString));
    GlobalFree(GlobalHandle(lpString));
  }
  if (local_4 != 0) {
    GlobalUnlock(GlobalHandle(local_4));
    GlobalFree(GlobalHandle(local_4));
  }
  if (pMem != 0) {
    GlobalUnlock(GlobalHandle(pMem));
    GlobalFree(GlobalHandle(pMem));
  }
  return result;
}


/* WHAT IT DOES: builds the "<name>: <text>" line that names a network player
 * in a log message, returning it in a freshly GlobalAlloc'd, locked buffer.
 * It asks the DirectPlay object for the player's record, reads that record's
 * name (or a fallback when it carries none), and formats it with the caller's
 * text -- "<name>: <text>\r\n" normally, or "<name><text>\r\n" when the flag
 * is set. Reports the COM error it hit, and always frees the temporary record
 * it locked. */
/* @t4-pass 0x10036A30 2 2026-09-07 probes 76 bytes 231 insns 87 regions 2 rows 1 census yes  (tools/crank.py) */
/* @t4-pass 0x10036A30 3 2026-09-07 probes 77 bytes 231 insns 87 regions 2 rows 1 census yes  (tools/crank.py) */
/* @t4-pass 0x10036A30 4 2026-09-09 probes 10 bytes 231 insns 87 regions 1 rows 1 census yes  (hand, fn.py variants: null/format/length-sum/decl; corpus MISS at +0x1a -- frame-pointer fork) */
/* @t4-pass 0x10036A30 5 2026-09-09 probes 10 bytes 231 insns 87 regions 1 rows 1 census yes  (hand, fn.py variants: decl order, format polarity, string concat; decl-order -2 B but not current) */
/* @t3 0x10036A30 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 231/227 insns 87/86 rows 0+1 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 4 zero-movement 4 5
 * residue is the frame-pointer decision: orig is frameless (ebp holds
 * lpFormat, one spill into a dead incoming-arg slot); VC5 emits an ebp
 * frame and a dedicated local.  One xor-zero remat.  Dossier in the
 * RESIDUE block below.  Do not reopen before the end-grind. */
/* @implements 0x10036A30 glide FUN_10036a30 */
/* RESIDUE (2026-09-06): +4 B / +1 insn, REGNORM 1+0. Body is complete and
 * correct -- instruction-for-instruction identical to the original except the
 * FRAME-POINTER decision. The original omits the frame (`push ecx` for the one
 * addressed local, ebp used as a general register holding lpFormat), and spills
 * that format across the length-sum into a DEAD incoming-parameter slot
 * ([esp+0x24], param_5's home); VC5 on this source instead keeps an ebp frame
 * (`push ebp; mov ebp,esp; sub esp,8`) and homes the same spill in a dedicated
 * local ([ebp-8]), which cascades every local's addressing esp->ebp. Five values
 * (hr, lpFormat, lpName, param_3, the length accumulator) are live across the
 * three lstrlenA calls against four callee-saved registers, so exactly one spill
 * is unavoidable on both sides -- only its LOCATION differs (arg-scratch vs frame
 * local). The matched sibling FUN_100368a0 in this file is frameless with 3
 * params; this one has 5. PROBED AND DEAD (all hold at +4 B): size summed into a
 * named `int n` local before GlobalAlloc; local declaration order permuted
 * (hMem-first vs hr-first). Spill LOCATION is a VC5 allocator choice not seen to
 * move from C source here.
 * @t4-pass 0x10036A30 1 2026-09-06 probes 4 bytes 227 insns 86 regions 1 rows 1 census no */
int FUN_10036a30(struct BrDPlayObj *param_1, int param_2, LPCSTR param_3, LPCVOID *param_4, int param_5)
{
    HGLOBAL hMem;
    LPSTR pBuf;
    LPCSTR lpFormat;
    LPCSTR lpName;
    int hr;
    LPCVOID local_4;

    local_4 = 0;
    if (param_1 == 0) {
        return 0;
    }
    lpFormat = "%s%s\r\n";
    if (param_5 == 0) {
        lpFormat = "%s: %s\r\n";
    }
    hr = BrComGetAlloc(param_1, param_2, &local_4);
    if (hr >= 0) {
        lpName = *(LPCSTR *)((char *)local_4 + 8);
        if (lpName == 0) {
            lpName = "unknown";
        }
        hMem = GlobalAlloc(0x42, lstrlenA(param_3) + lstrlenA(lpFormat) + 1 + lstrlenA(lpName));
        pBuf = GlobalLock(hMem);
        if (pBuf == 0) {
            hr = 0x8007000e;
        } else {
            wsprintfA(pBuf, lpFormat, lpName, param_3);
            *param_4 = pBuf;
        }
    }
    if (local_4 != 0) {
        GlobalUnlock(GlobalHandle(local_4));
        GlobalFree(GlobalHandle(local_4));
    }
    return hr;
}


/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: turn a DirectPlay error code into its printable name for the
 * log. A long comparison chain, and it returns a generic unknown string for
 * anything it does not recognise. */
/* @implements 0x100372B0 glide FUN_100372b0 */
/* auto-filed from ghidra --refine; transforms: as-is */

char *FUN_100372b0(int hr)
{
  if (hr <= (int)0x80004001) {
    if (hr != (int)0x80004001) {
      if (hr != (int)0x8000000A) goto unknown;
      return "DPERR_PENDING";
    }
    return "DPERR_UNSUPPORTED";
  }
  if (hr <= (int)0x80004005) {
    if (hr != (int)0x80004005) {
      if (hr != (int)0x80004002) goto unknown;
      return "DPERR_NOINTERFACE";
    }
    return "DPERR_GENERIC";
  }
  if (hr <= (int)0x80070057) {
    if (hr != (int)0x80070057) {
      if (hr != (int)0x8007000E) goto unknown;
      return "DPERR_OUTOFMEMORY";
    }
    return "DPERR_INVALIDPARAMS";
  }
  if (hr <= (int)0x8877000A) {
    if (hr != (int)0x8877000A) {
      if (hr != (int)0x88770005) goto unknown;
      return "DPERR_ALREADYINITIALIZED";
    }
    return "DPERR_ACCESSDENIED";
  }
  if (hr <= (int)0x8877001E) {
    if (hr != (int)0x8877001E) {
      if (hr != (int)0x88770014) goto unknown;
      return "DPERR_ACTIVEPLAYERS";
    }
    return "DPERR_BUFFERTOOSMALL";
  }
  if (hr <= (int)0x88770032) {
    if (hr != (int)0x88770032) {
      if (hr != (int)0x88770028) goto unknown;
      return "DPERR_CANTADDPLAYER";
    }
    return "DPERR_CANTCREATEGROUP";
  }
  if (hr <= (int)0x88770046) {
    if (hr != (int)0x88770046) {
      if (hr != (int)0x8877003C) goto unknown;
      return "DPERR_CANTCREATEPLAYER";
    }
    return "DPERR_CANTCREATESESSION";
  }
  if (hr <= (int)0x8877005A) {
    if (hr != (int)0x8877005A) {
      if (hr != (int)0x88770050) goto unknown;
      return "DPERR_CAPSNOTAVAILABLEYET";
    }
    return "DPERR_EXCEPTION";
  }
  if (hr <= (int)0x88770082) {
    if (hr != (int)0x88770082) {
      if (hr != (int)0x88770078) goto unknown;
      return "DPERR_INVALIDFLAGS";
    }
    return "DPERR_INVALIDOBJECT";
  }
  if (hr <= (int)0x8877009B) {
    if (hr != (int)0x8877009B) {
      if (hr != (int)0x88770096) goto unknown;
      return "DPERR_INVALIDPLAYER";
    }
    return "DPERR_INVALIDGROUP";
  }
  if (hr <= (int)0x887700AA) {
    if (hr != (int)0x887700AA) {
      if (hr != (int)0x887700A0) goto unknown;
      return "DPERR_NOCAPS";
    }
    return "DPERR_NOCONNECTION";
  }
  if (hr <= (int)0x887700C8) {
    if (hr != (int)0x887700C8) {
      if (hr != (int)0x887700BE) goto unknown;
      return "DPERR_NOMESSAGES";
    }
    return "DPERR_NONAMESERVERFOUND";
  }
  if (hr <= (int)0x887700DC) {
    if (hr != (int)0x887700DC) {
      if (hr != (int)0x887700D2) goto unknown;
      return "DPERR_NOPLAYERS";
    }
    return "DPERR_NOSESSIONS";
  }
  if (hr <= (int)0x887700F0) {
    if (hr != (int)0x887700F0) {
      if (hr != (int)0x887700E6) goto unknown;
      return "DPERR_SENDTOOBIG";
    }
    return "DPERR_TIMEOUT";
  }
  if (hr <= (int)0x8877010E) {
    if (hr != (int)0x8877010E) {
      if (hr != (int)0x887700FA) goto unknown;
      return "DPERR_UNAVAILABLE";
    }
    return "DPERR_BUSY";
  }
  if (hr <= (int)0x88770122) {
    if (hr != (int)0x88770122) {
      if (hr != (int)0x88770118) goto unknown;
      return "DPERR_USERCANCEL";
    }
    return "DPERR_CANNOTCREATESERVER";
  }
  if (hr <= (int)0x88770136) {
    if (hr != (int)0x88770136) {
      if (hr != (int)0x8877012C) goto unknown;
      return "DPERR_PLAYERLOST";
    }
    return "DPERR_SESSIONLOST";
  }
  if (hr <= (int)0x8877014A) {
    if (hr != (int)0x8877014A) {
      if (hr != (int)0x88770140) goto unknown;
      return "DPERR_UNINITIALIZED";
    }
    return "DPERR_NONEWPLAYERS";
  }
  if (hr <= (int)0x8877015E) {
    if (hr != (int)0x8877015E) {
      if (hr != (int)0x88770154) goto unknown;
      return "DPERR_INVALIDPASSWORD";
    }
    return "DPERR_CONNECTING";
  }
  if (hr <= (int)0x887703F2) {
    if (hr != (int)0x887703F2) {
      if (hr != (int)0x887703E8) goto unknown;
      return "DPERR_BUFFERTOOLARGE";
    }
    return "DPERR_CANTCREATEPROCESS";
  }
  if (hr <= (int)0x88770406) {
    if (hr != (int)0x88770406) {
      if (hr != (int)0x887703FC) goto unknown;
      return "DPERR_APPNOTSTARTED";
    }
    return "DPERR_INVALIDINTERFACE";
  }
  if (hr <= (int)0x8877041A) {
    if (hr != (int)0x8877041A) {
      if (hr != (int)0x88770410) goto unknown;
      return "DPERR_NOSERVICEPROVIDER";
    }
    return "DPERR_UNKNOWNAPPLICATION";
  }
  if (hr <= (int)0x88770438) {
    if (hr != (int)0x88770438) {
      if (hr != (int)0x8877042E) goto unknown;
      return "DPERR_NOTLOBBIED";
    }
    return "DPERR_SERVICEPROVIDERLOADED";
  }
  if (hr <= (int)0x8877044C) {
    if (hr != (int)0x8877044C) {
      if (hr != (int)0x88770442) goto unknown;
      return "DPERR_ALREADYREGISTERED";
    }
    return "DPERR_NOTREGISTERED";
  }
  if (hr <= (int)0x887707DA) {
    if (hr != (int)0x887707DA) {
      if (hr != (int)0x887707D0) goto unknown;
      return "DPERR_AUTHENTICATIONFAILED";
    }
    return "DPERR_CANTLOADSSPI";
  }
  if (hr <= (int)0x887707EE) {
    if (hr != (int)0x887707EE) {
      if (hr != (int)0x887707E4) goto unknown;
      return "DPERR_ENCRYPTIONFAILED";
    }
    return "DPERR_SIGNFAILED";
  }
  if (hr <= (int)0x88770802) {
    if (hr != (int)0x88770802) {
      if (hr != (int)0x887707F8) goto unknown;
      return "DPERR_CANTLOADSECURITYPACKAGE";
    }
    return "DPERR_ENCRYPTIONNOTSUPPORTED";
  }
  if (hr <= (int)0x88770816) {
    if (hr != (int)0x88770816) {
      if (hr != (int)0x8877080C) goto unknown;
      return "DPERR_CANTLOADCAPI";
    }
    return "DPERR_NOTLOGGEDIN";
  }
  if (hr != (int)0x88770820) {
    if (hr != 0) {
      goto unknown;
    }
    return "DP_OK";
  }
  return "DPERR_LOGONDENIED";
unknown:
  wsprintfA(DAT_10ac3070, "0x%08X", hr);
  return DAT_10ac3070;
}


/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: fill in the session description used when hosting -- copies
 * the player's chosen session name in, if they set one longer than a single
 * character, and clears the trailing field. */
/* @implements 0x100367C0 glide FUN_100367c0 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_100367c0(char *param_1)

{
  if (strlen((char *)&(DAT_10ac40a8[0])) > 1) {
    strcpy(param_1, (char *)&(DAT_10ac40a8[0]));
  }
  *(int *)(param_1 + 0xc8) = 0;
  return;
}

