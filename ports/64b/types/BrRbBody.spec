# size 0x1DC
# header ports/64b/include/slice3_44.h
# headers slice3_44.h
0x0000  float                         f00                 
0x0004  struct BrRbBody *[4]          child                 the four sibling bodies of one car (body and wheels)
0x0014  float                         f14                 
0x0018  struct BrRbForce *            pForces               head of the force list
0x001C  int                           mode                  2 == no torque
0x0020  float[3]                      dim                   box extents
0x002C  float                         mass                
0x0030  BrMat3                        inertia             
0x0054  BrMat3                        invInertia          
0x0078  BrRbState                     st                    the current state
0x00BC  BrMat4                        m                     built from st; row 3 is the origin
0x00FC  BrVec3                        accel               
0x0108  BrVec3                        angAccel            
0x0114  BrRbState                     st1                   saved state
0x0158  BrRbState                     st2                   saved state
0x019C  struct BrCollPlane *           pPlane              the wheel's hit plane (br_phys.h)
0x01A0  unsigned char                 f01A0               
0x01A4  float                         f01A4               
0x01A8  float                         f01A8               
0x01AC  float                         f01AC               
0x01B0  float                         f01B0               
0x01B4  float                         f1B4                  0 disables the torque leg
0x01B8  float                         f1B8                  suspension spring rate
0x01BC  float                         f1BC                  shock absorber rate
0x01C0  float                         f1C0                
0x01C4  float                         f1C4                
0x01C8  float                         f1C8                
0x01CC  float                         f1CC                
0x01D0  float                         f1D0                
0x01D4  float                         f1D4                
0x01D8  float                         f1D8                
