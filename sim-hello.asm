  ld hl, dst
  ld de, src
  ld c, len
: ld a, [de]
  ld [hli], a
  inc de
  dec c
  jr nz, :-
