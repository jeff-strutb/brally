/* ctlconfig.c -- the controller configuration screen
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextAlignLeft(void);
void BrTextAlignRight(void);
void BrTextSetFont(int param_1);
void func_8022F5DC(int param_1,int param_2,int param_3);
int func_8023DF9C();
extern int D_80272590;
extern int D_802725A4;
extern int D_802725B8;
extern int D_802725CC;
extern int D_802725E0;
/* -- end declarations -- */

/* WHAT IT DOES: Draw the controller diagram for the chosen control type,
 * labelling each button with what it does (accelerate, brake, gear up,
 * change view and so on). Each of the layouts puts the labels in different
 * places. */
/* @implements 0x802167E0 tgr BrCtlConfigDrawLayout */
void BrCtlConfigDrawLayout(int param_1)
{
  BrTextSetFont(8);
  BrTextHighlightOff();
  switch(param_1) {
  case 0:
    func_8023DF9C(&D_80272590,0x6f,0x55,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_80272590,0x6f,0x55,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_80272590,0x6f,0x55,0x68,0x30,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,1);
    BrTextAlignLeft();
    func_8022F5DC("%wwACCELERATE",0xc9,0x7f);
    func_8022F5DC("%wwPLUS BRAKE = E-BRAKE",0xc1,0x87);
    func_8022F5DC("%wwCHANGE VIEW",0xd7,0x68);
    func_8022F5DC("%wwGEAR UP",199,0x56);
    BrTextAlignRight();
    func_8022F5DC("%wwSTEERING / REVERSE",0x6b,99);
    func_8022F5DC("%wwGEAR DOWN",0x75,0x83);
    BrTextAlignCentre();
    func_8022F5DC("%wwBRAKE",0x9c,0x56);
    break;
  case 1:
    func_8023DF9C(&D_802725A4,0x6f,0x55,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725A4,0x6f,0x55,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725A4,0x6f,0x55,0x68,0x30,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,1);
    BrTextAlignLeft();
    func_8022F5DC("%wwACCELERATE",0xc9,0x80);
    func_8022F5DC("%wwCHANGE VIEW",0xd7,0x68);
    func_8022F5DC("%wwGEAR UP",199,0x56);
    func_8022F5DC("%wwE-BRAKE",0xd3,0x76);
    BrTextAlignRight();
    func_8022F5DC("%wwSTEERING / REVERSE",0x6b,99);
    func_8022F5DC("%wwGEAR DOWN",0x75,0x83);
    BrTextAlignCentre();
    func_8022F5DC("%wwBRAKE",0x9c,0x56);
    break;
  case 2:
    func_8023DF9C(&D_802725B8,0x6f,0x55,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725B8,0x6f,0x55,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725B8,0x6f,0x55,0x68,0x30,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,1);
    BrTextAlignLeft();
    func_8022F5DC("%wwACCELERATE",0xc9,0x7f);
    func_8022F5DC("%wwPLUS BRAKE = E-BRAKE",0xc1,0x87);
    func_8022F5DC("%wwCHANGE VIEW",0xd7,0x68);
    func_8022F5DC("%wwGEAR UP",199,0x56);
    BrTextAlignRight();
    func_8022F5DC("%wwSTEERING, REVERSE",0x6b,0x7d);
    func_8022F5DC("%wwGEAR DOWN",0x74,0x58);
    BrTextAlignCentre();
    func_8022F5DC("%wwBRAKE",0x9c,0x56);
    break;
  case 3:
    func_8023DF9C(&D_802725CC,0x6f,0x55,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725CC,0x6f,0x55,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725CC,0x6f,0x55,0x68,0x30,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,1);
    BrTextAlignLeft();
    func_8022F5DC("%wwGEAR DOWN",0xc9,0x80);
    func_8022F5DC("%wwCHANGE VIEW",0xd8,0x67);
    BrTextAlignRight();
    func_8022F5DC("%wwACCELERATE / REVERSE,",0x7c,0x59);
    func_8022F5DC("%wwSTEERING ",0x7c,0x61);
    func_8022F5DC("%wwGEAR UP/DOWN",0x6e,0x72);
    func_8022F5DC("%wwBRAKE",0x77,0x81);
    func_8022F5DC("%wwPLUS ACCELERATE = E-BRAKE ",0x77,0x89);
    BrTextAlignCentre();
    func_8022F5DC("%wwGEAR UP",0x9c,0x56);
    break;
  case 4:
    func_8023DF9C(&D_802725E0,0x6e,0x51,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725E0,0x6e,0x51,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725E0,0x6e,0x51,0x68,0x30,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,1);
    BrTextAlignLeft();
    func_8022F5DC("%wwBRAKE",199,0x59);
    func_8022F5DC("%wwACCELERATE",0xd0,0x6e);
    func_8022F5DC("%wwPLUS BRAKE = E-BRAKE",200,0x76);
    BrTextAlignCentre();
    func_8022F5DC("%wwCHANGE VIEW",0xa7,0x55);
    BrTextAlignRight();
    func_8022F5DC("%wwSTEERING",0x72,0x7c);
    func_8022F5DC("%wwREVERSE",0x7f,0x57);
    func_8022F5DC("%wwGEAR UP/DOWN",0x74,0x65);
    BrTextAlignCentre();
  }
}

/* WHAT IT DOES: Does nothing. An empty function the retail build kept after
 * the controller-configuration code. */
/* @implements 0x80217C8C tgr BrStub80217C8C */
void BrStub80217C8C(void)
{
}
