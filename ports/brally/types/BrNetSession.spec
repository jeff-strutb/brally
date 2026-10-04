# size 0xE0
# header ports/brally/include/br_coretypes.h
# headers br_coretypes.h
0x0000  uint8_t[200]                  player              the enumerated session's 200-byte player block
0x00C8  int32_t[4]                    guid                the session GUID
0x00D8  uint32_t                      cbPayload
0x00DC  void *                        pPayload            GlobalLock'd copy of the session payload
