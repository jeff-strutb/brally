# size 0x2B68
# header ports/brally/include/slice3_41.h
# headers slice3_41.h
0x0000  BrVec3                        fwd                   world frame row 0 (the body matrix is rows 0..3)
0x000C  float                         f0C                 
0x0010  BrVec3                        right                 frame row 1
0x001C  float                         f1C                 
0x0020  BrVec3                        up                    frame row 2
0x002C  float                         f2C                 
0x0030  BrVec3                        pos                   frame row 3: the car's position
0x003C  float                         f3C                 
0x0040  BrMat4[4]                     aWheel                one matrix per wheel
0x0140  int32_t                       f140                  player/car index
0x0144  int32_t                       iNetPlayer            the DirectPlay player this car belongs to
0x0148  char[26]                      szName                the driver name shown on the HUD (%s)
0x0162  short                         f0162               
0x0164  BrCarBody[5]                  aBody                 physics: [0] the body, [1..4] the wheels LF, LR, RF, RR
0x0BA0  BrRbForce[16]                 aForce              the bodies' force-list nodes (linked through pNext)
0x0E20  float                         f0E20               
0x0E24  float                         f0E24               
0x0E28  float                         f0E28               
0x0E2C  float                         f0E2C               
0x0E30  int32_t                       f0E30               seen in the trace
0x0E34  int32_t                       f0E34               seen in the trace
0x0E38  int32_t                       f0E38               seen in the trace
0x0E3C  int32_t                       f0E3C               seen in the trace
0x0E40  int32_t                       f0E40               seen in the trace
0x0E44  float                         f0E44               
0x0E48  float                         f0E48               
0x0E4C  float                         f0E4C               
0x0E50  float                         f0E50               
0x0E54  float                         f0E54               
0x0E58  int                           f0E58               
0x0E5C  int32_t                       f0E5C               seen in the trace
0x0E60  int                           f0E60               
0x0E64  int32_t                       f0E64               seen in the trace
0x0E68  float                         fE68                
0x0E6C  float                         f0E6C               
0x0E70  int32_t                       fE70                
0x0E74  float                         f0E74               
0x0E78  unsigned char                 f0E78               
0x0E7C  float                         f0E7C               
0x0E80  unsigned char                 f0E80               
0x0E81  char                          f0E81               
0x0E84  int                           f0E84               
0x0E88  int32_t                       fE88                
0x0E8C  uint8_t *                     pEquip                the equipment record
0x0E90  int32_t                       fE90                
0x0E94  int32_t                       fE94                
0x0E98  int32_t                       fE98                
0x0E9C  int32_t                       fE9C                
0x0EA0  int32_t                       cHoldFwd            
0x0EA4  int32_t                       cHoldRev            
0x0EA8  int32_t                       cRevRun             
0x0EAC  int32_t                       cFwdRun             
0x0EB0  struct BrDriver *[16]         apRank              the field in race order (the lap-save restore writes it)
0x0F00  struct BrDriver *            pProfile              the entrant/profile record
0x0F04  int32_t                       fF04                
0x0F08  fnptr:void(struct BrDriverCar *)  pfnControl          
0x0F0C  BrVec3                        aim                   the AI's smoothed aim point
0x0F18  BrVec3                        pathPos             the path frame's blended position
0x0F24  BrVec3                        tangent             
0x0F30  BrVec3                        lateral             
0x0F3C  BrVec3                        pathUp              
0x0F48  float                         fF48                
0x0F4C  BrVec3                        d                   
0x0F58  float                         fF58                
0x0F5C  int                           f0F5C               
0x0F60  int32_t                       f0F60               seen in the trace
0x0F64  int32_t                       f0F64               seen in the trace
0x0F68  int                           f0F68               
0x0F6C  int                           f0F6C               
0x0F70  int                           f0F70               
0x0F74  float                         f0F74               
0x0F78  int32_t                       fF78                
0x0F7C  int32_t                       fF7C                
0x0F80  BrVec3                        posPrev             
0x0F8C  BrAiNodeArg                   pNode                 the AI's waypoint cursor: node
0x0F90  BrAiIdxArg                    iPt                   ... and point
0x0F94  float                         f0F94               
0x0F98  float                         f0F98               
0x0F9C  float                         f0F9C               
0x0FA0  int32_t                       gateHi              
0x0FA4  int32_t                       gate                
0x0FA8  int32_t                       lap                 
0x0FAC  int32_t                       lapB                
0x0FB0  float                         tRun                
0x0FB4  float[12]                     aLapTime            
0x0FE4  float                         tBest               
0x0FE8  int32_t                       lapBest             
0x0FEC  float                         tFinal              
0x0FF0  float                         fFF0                
0x0FF4  float                         fFF4                  distance along the track, across laps
0x0FF8  int32_t                       fFF8                  ranking key
0x0FFC  char *                        pszBanner             a string-table result or one of the two banner buffers
0x1000  float                         f1000               
0x1004  char *                        psz1004               a second banner string ...
0x1004  int32_t                       n1004                 ... and, during race setup, the last replay row's size
0x1008  float                         f1008               
0x100C  char[24]                      sz100C                the HUD banner buffer
0x1024  BrVec3                        f1024                 velocity
0x1030  float                         f1030                 speed
0x1034  float                         f1034               
0x1038  uint32_t[2]                   f1038               seen in the trace
0x1040  uint32_t[2]                   f1040               seen in the trace
0x1048  uint32_t[2]                   f1048               seen in the trace
0x1050  uint32_t[2]                   f1050               seen in the trace
0x1058  int32_t                       f1058               seen in the trace
0x105C  float                         f105C               
0x1060  uint32_t[2]                   f1060               seen in the trace
0x1068  int32_t                       f1068               seen in the trace
0x106C  float[4]                      f106C               the four floats the table init writes 0x44 back
0x107C  int32_t                       f107C               seen in the trace
0x1080  int32_t                       f1080               seen in the trace
0x1084  int32_t                       f1084               seen in the trace
0x1088  int32_t                       f1088               seen in the trace
0x108C  int32_t                       f108C               seen in the trace
0x1090  int32_t                       f1090               seen in the trace
0x1094  int32_t                       f1094               seen in the trace
0x1098  int32_t                       f1098               seen in the trace
0x109C  int32_t                       f109C               seen in the trace
0x10A0  int32_t                       f10A0               seen in the trace
0x10A4  int32_t                       f10A4               seen in the trace
0x10A8  int32_t                       f10A8               seen in the trace
0x10AC  int32_t[4]                    a10AC               (float on the first pass, then cleared)
0x10BC  int32_t[4]                    a10BC
0x10CC  int32_t[4]                    a10CC
0x10DC  int32_t[4]                    a10DC
0x10EC  BrVec3[4]                     aWheelPrev          each wheel's previous position
0x1120  int32_t[0x90][8]              aHist               144 history records of 0x20 bytes
0x2320  int16_t[0x90][3]              aWHist              144 three-short entries
0x2680  uint16_t[0x24]                a2680               pairs, set to 2,2 each
0x26C8  float                         f26C8               
0x26CC  float                         f26CC               
0x26D0  float                         f26D0               
0x26D4  uint32_t[2]                   f26D4               seen in the trace
0x26DC  uint32_t[2]                   f26DC               seen in the trace
0x26E4  uint32_t[2]                   f26E4               seen in the trace
0x26EC  uint32_t[2]                   f26EC               seen in the trace
0x26F4  uint32_t[2]                   f26F4               seen in the trace
0x26FC  uint32_t[2]                   f26FC               seen in the trace
0x2704  uint32_t[2]                   f2704               seen in the trace
0x270C  uint32_t[2]                   f270C               seen in the trace
0x2714  int32_t                       i2714               
0x2718  float                         f2718               
0x2720  float                         f2720               
0x2724  float                         f2724               
0x2728  float                         aimFwd              
0x272C  float                         f272C               
0x2730  float                         f2730               
0x2734  BrSnapMtx *                   pMatA               
0x2738  BrSnapMtx *                   pMatB               
0x273C  BrSnapMtx[6]                  aSnap               
0x28D4  int32_t                       f28D4               seen in the trace
0x28D8  int32_t                       f28D8               seen in the trace
0x28DC  float                         f28DC               
0x28E0  float                         f28E0               
0x28E4  float                         f28E4               
0x28E8  float                         f28E8               
0x28EC  uint32_t[2]                   f28EC               seen in the trace
0x28F4  int32_t                       f28F4               seen in the trace
0x28F8  int32_t                       f28F8               seen in the trace
0x2900  int32_t                       f2900               seen in the trace
0x2904  int32_t                       f2904               seen in the trace
0x2908  int32_t                       f2908               seen in the trace
0x290C  uint16_t[32]                  aNearIds              ray-cast: near face ids (the first indexes the 84-byte records)
0x294C  int32_t                       gotHit                ray-cast: a near hit was found
0x2950  uint16_t[32]                  aFarIds               ray-cast: far face ids
0x2990  int32_t                       farCount            
0x2994  float                         fHitDist            
0x2998  int32_t                       iHitFace            
0x299C  short                         f299C               
0x299E  short                         f299E               
0x29A0  short                         f29A0               
0x29A2  short                         f29A2               
0x29A4  int32_t                       f29A4               
0x29A8  int32_t                       f29A8               
0x29AC  unsigned char                 f29AC               
0x29AD  unsigned char                 f29AD               
0x29AE  unsigned char                 f29AE               
0x29AF  uint8_t                       b29AF                 draw class; 2 is the translucent pass
0x29B0  float                         f29B0                 alpha
0x29B4  int32_t                       i29B4               
0x29B8  int                           f29B8               
0x29BC  unsigned char                 f29BC               
0x29BD  unsigned char                 f29BD               
0x29C0  struct BrRaceCtl *            pCtl                  the control block (br_racebegin.h)
0x29C4  void *                        pModel                the car's model record
0x29C8  uint32_t[2]                   f29C8               seen in the trace
0x29D0  uint32_t[2]                   f29D0               seen in the trace
0x29D8  uint16_t                      f29D8               seen in the trace
0x2A70  int32_t                       f2A70               seen in the trace
0x2A74  int32_t                       f2A74               seen in the trace
0x2A78  int32_t                       f2A78               seen in the trace
0x2A7C  int32_t                       f2A7C               seen in the trace
0x2A80  int32_t                       f2A80               seen in the trace
0x2A84  int32_t                       f2A84               seen in the trace
0x2A88  int32_t                       f2A88               seen in the trace
0x2A8C  int32_t                       f2A8C               seen in the trace
0x2A90  uint32_t[2]                   f2A90               seen in the trace
0x2A98  uint32_t[2]                   f2A98               seen in the trace
0x2AA0  uint32_t[2]                   f2AA0               seen in the trace
0x2AA8  uint32_t[2]                   f2AA8               seen in the trace
0x2AB0  float                         f2AB0               
0x2AB4  float                         f2AB4               
0x2AB8  float                         f2AB8               
0x2ABC  char[172]                     sz2ABC                the banner's second buffer
