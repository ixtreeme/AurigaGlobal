# Phase 15E-final.LPENTITY.4-architect — running status

Updated through commit `3251616` (B.1.5 state flags).

## Phase progress

| Phase | Step | Status | Commit | Notes |
| ----- | ---- | ------ | ------ | ----- |
| A     | -    | DONE   | bff448d | A.1-A.4 design docs landed |
| A.0   | -    | DONE   | local   | inventory match report; PASS |
| B.1.1 | m_pos getter rewrite | DONE | 925a2ff | atomic with MirrorLegacyMovement guard removal; previous attempt 9d27603+fefce11 reverted (a7a5b6a, 0324b0c) due to runtime regression |
| B.1.1.pre.1 | POSITION_READ_DRIFT detector | DONE | f16a96b | sweep at ~1Hz; gated by AURIGA_LPENTITY_FIXUP_AUDIT |
| B.1.2 | m_dwMoveStartTime + m_dwMoveDuration getters | DONE | 2f8fdbd | new GetCurrentMoveStartTime accessor; 4 direct field reads converted |
| B.1.3 | m_bNowWalking via IsWalking + IsNowWalking | DONE | dc22803 | 2 packet emission sites converted; SetNowWalking body retains direct field reads (write path) |
| B.1.4 | m_posDest GetCurrentDestX/Y + direct reads | DONE | 5b0f1e3 | semantic note: ECS MovementDestination is move-only; getter falls back to GetX/Y when component absent (matches legacy m_posDest settle semantic) |
| B.1.5 | m_bAddChrState reads | DONE | 3251616 | new GetAddChrStateFlag composes 4 bits from StatusFlags; 3 read sites converted |
| B.1.6 | m_pSectree migration | DEFERRED | - | ECS SectorPlacement only stores indices, not LPSECTREE pointer. Lookup cost per call is high for the 60+ call sites. Decision: defer to Phase G. The pointer is a runtime cache, not part of the desync semantic. |
| B.1.7 | m_map_view direct reads | DEFERRED | - | Per A.2 §5: m_map_view writes wait for VisibilitySystem (Phase D). Reads at the visibility-maintenance sites (ViewCleanup, UpdateSectree, ViewReencode) get rewritten as part of Phase D entirely. PacketView's m_map_view loop is part of the f76a3f1 band-aid scheduled for deletion in Phase D.8. |
| B.1.8 | m_posStart | DEFERRED | - | Field has no current ECS twin per A.2 §2. Used only in CalculateMoveDuration body. A.2 §2 m_posStart row decision: "Convert CalculateMoveDuration to take start position as a function argument (or compute it inline from current Position at the moment Goto is called). No new ECS component needed." This is a Phase C refactor, not a Phase B read-flip. |
| B.1.9 | m_iViewAge | DEFERRED | - | Per A.2 §5: "m_iViewAge migrates last, only after polling is replaced." Phase D removes UpdateSectree polling and m_iViewAge with it. |

## Phase B effective end

Phase B is read-flip migration. The 4 deferred fields each fail one of the
preconditions:

  - B.1.6 m_pSectree: pointer/cache mismatch with ECS SectorPlacement
    (which holds indices). Pure read-flip would impose a per-call
    SECTREE_MANAGER hash lookup on 60+ hot-path call sites. The
    migration belongs in Phase G alongside the legacy field deletion.

  - B.1.7 m_map_view: reads are entangled with the visibility
    machinery (UpdateSectree polling, ViewInsert/Remove). Phase D
    rewrites that machinery wholesale; touching m_map_view reads
    individually before that rewrite invents work that Phase D
    would undo.

  - B.1.8 m_posStart: no ECS twin. The architect-doc decision is to
    fold the 2 read sites into a function-argument refactor, which
    is a write-side change (Phase C).

  - B.1.9 m_iViewAge: A.2 §5 mandate.

The 5 completed commits cover the 6 movement-related fields that
caused the original two-client desync:

  Position (B.1.1), MovementDestination (B.1.4),
  MovementState.moveStartTime (B.1.2), .moveDuration (B.1.2),
  .isNowWalking (B.1.3), StatusFlags 4 bits (B.1.5).

These are exactly the fields enumerated in the desync audit
phase15e_final_lpentity_4_visibility_desync_audit.txt as the source
of the broken broadcast / packet content.

## Acceptance gate per commit (Phase B)

| Commit | Builds clean | WinTest | Drift detectors | Cleanup | Architect doc ref |
| ------ | ------------ | ------- | --------------- | ------- | ----------------- |
| 925a2ff | PASS | DEFERRED to batch | n/a (still pre-flip) | atomic with guard removal | A.2 §2 m_pos |
| f16a96b | PASS | DEFERRED | n/a | new audit, no dead body | A.2 §2 m_pos |
| 2f8fdbd | PASS | DEFERRED | n/a | n/a | A.2 §2 m_dwMoveStartTime/Duration |
| dc22803 | PASS | DEFERRED | n/a | n/a | A.2 §2 m_bNowWalking |
| 5b0f1e3 | PASS | DEFERRED | n/a | n/a | A.2 §2 m_posDest |
| 3251616 | PASS | DEFERRED | n/a | n/a | A.2 §2 m_bAddChrState |

WinTest deferred per user instruction: "haladjunk tovább és amikor meg
leszünk egy nagyobb résszel akkor tesztelünk, mert lehet épp azok a
változások fognak hiányozni ebben a részben amik szükségesek a
másikból". Test gate at the end of the 5-commit B chunk, before
proceeding to Phase C.

## Next milestone

WinTest the 5-commit Phase B chunk. Acceptance criteria:

1. Two-client position sync stable (the original desync symptom).
2. Walk / run / mount / dash all visible on both clients.
3. Map warp - new map mobs/stones visible.
4. [POSITION_READ_DRIFT] count zero across the session
   (the B.1.1.pre.1 detector verifies Phase B.1.1 dual-write is solid).
5. [MOVEMENT_DRIFT] count zero (the existing audit detector continues
   to verify all the flipped fields).

If any of (1)-(3) fails: investigate at source, do not band-aid. The
B.1.1 first-attempt regression (MirrorLegacyMovement early-return)
shows that asymmetric-source dependencies survive grep audits; the
fix was a one-line guard removal in the same atomic commit. Same
discipline applies if a new failure surfaces.

If (4) or (5) fail: one of the flipped fields has a SetXYZ caller
that bypasses the dual-write (no SyncPositionComponents / no
SyncTimingWrite / etc.). Find that site, fix at source.

## After WinTest passes

Phase C begins (write migration). Per A.2 §5 Step 4-5: each field's
writes redirect to ECS, then the legacy field becomes deletable.
B.1.6, B.1.7, B.1.8, B.1.9 deferred fields handled in their
respective phase windows (G for sectree, D for map_view + view_age,
C for pos_start refactor).
