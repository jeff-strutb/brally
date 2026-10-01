# size 0x50
# header ports/64b/include/br_cartypes.h
# headers br_cartypes.h
0x0000  uint32_t                      ctl                   the car's control word: brake/finish bits (BR_DRIVERCAR_CTL_*)
0x0020  float                         steer                 the controller's steering command, -1..1
0x0024  uint8_t                       b24
0x0025  uint8_t                       b25
0x002C  void *[BR_RACEBEGIN_MAXREC]   apRec                 per-entrant replay record
0x0034  int32_t[BR_RACEBEGIN_MAXREC]  aLen
0x003C  int32_t[BR_RACEBEGIN_MAXREC]  aCap
0x0044  uint8_t *                     pHdr                  the 8-byte replay header
0x0048  int32_t                       f48
0x004C  int32_t                       f4C
