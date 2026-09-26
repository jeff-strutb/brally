/* ctlconfig.c -- the controller configuration screen
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextAlignLeft(void);
void BrTextAlignRight(void);
void BrTextSetFont(int param_1);
void BrTextPrint(int param_1,int param_2,int param_3);
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
    BrTextPrint("%wwACCELERATE",0xc9,0x7f);
    BrTextPrint("%wwPLUS BRAKE = E-BRAKE",0xc1,0x87);
    BrTextPrint("%wwCHANGE VIEW",0xd7,0x68);
    BrTextPrint("%wwGEAR UP",199,0x56);
    BrTextAlignRight();
    BrTextPrint("%wwSTEERING / REVERSE",0x6b,99);
    BrTextPrint("%wwGEAR DOWN",0x75,0x83);
    BrTextAlignCentre();
    BrTextPrint("%wwBRAKE",0x9c,0x56);
    break;
  case 1:
    func_8023DF9C(&D_802725A4,0x6f,0x55,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725A4,0x6f,0x55,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725A4,0x6f,0x55,0x68,0x30,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,1);
    BrTextAlignLeft();
    BrTextPrint("%wwACCELERATE",0xc9,0x80);
    BrTextPrint("%wwCHANGE VIEW",0xd7,0x68);
    BrTextPrint("%wwGEAR UP",199,0x56);
    BrTextPrint("%wwE-BRAKE",0xd3,0x76);
    BrTextAlignRight();
    BrTextPrint("%wwSTEERING / REVERSE",0x6b,99);
    BrTextPrint("%wwGEAR DOWN",0x75,0x83);
    BrTextAlignCentre();
    BrTextPrint("%wwBRAKE",0x9c,0x56);
    break;
  case 2:
    func_8023DF9C(&D_802725B8,0x6f,0x55,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725B8,0x6f,0x55,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725B8,0x6f,0x55,0x68,0x30,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,1);
    BrTextAlignLeft();
    BrTextPrint("%wwACCELERATE",0xc9,0x7f);
    BrTextPrint("%wwPLUS BRAKE = E-BRAKE",0xc1,0x87);
    BrTextPrint("%wwCHANGE VIEW",0xd7,0x68);
    BrTextPrint("%wwGEAR UP",199,0x56);
    BrTextAlignRight();
    BrTextPrint("%wwSTEERING, REVERSE",0x6b,0x7d);
    BrTextPrint("%wwGEAR DOWN",0x74,0x58);
    BrTextAlignCentre();
    BrTextPrint("%wwBRAKE",0x9c,0x56);
    break;
  case 3:
    func_8023DF9C(&D_802725CC,0x6f,0x55,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725CC,0x6f,0x55,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725CC,0x6f,0x55,0x68,0x30,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,1);
    BrTextAlignLeft();
    BrTextPrint("%wwGEAR DOWN",0xc9,0x80);
    BrTextPrint("%wwCHANGE VIEW",0xd8,0x67);
    BrTextAlignRight();
    BrTextPrint("%wwACCELERATE / REVERSE,",0x7c,0x59);
    BrTextPrint("%wwSTEERING ",0x7c,0x61);
    BrTextPrint("%wwGEAR UP/DOWN",0x6e,0x72);
    BrTextPrint("%wwBRAKE",0x77,0x81);
    BrTextPrint("%wwPLUS ACCELERATE = E-BRAKE ",0x77,0x89);
    BrTextAlignCentre();
    BrTextPrint("%wwGEAR UP",0x9c,0x56);
    break;
  case 4:
    func_8023DF9C(&D_802725E0,0x6e,0x51,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725E0,0x6e,0x51,0x68,0x30,0,0,0,0xff,0,0,0,0xff,1);
    func_8023DF9C(&D_802725E0,0x6e,0x51,0x68,0x30,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,1);
    BrTextAlignLeft();
    BrTextPrint("%wwBRAKE",199,0x59);
    BrTextPrint("%wwACCELERATE",0xd0,0x6e);
    BrTextPrint("%wwPLUS BRAKE = E-BRAKE",200,0x76);
    BrTextAlignCentre();
    BrTextPrint("%wwCHANGE VIEW",0xa7,0x55);
    BrTextAlignRight();
    BrTextPrint("%wwSTEERING",0x72,0x7c);
    BrTextPrint("%wwREVERSE",0x7f,0x57);
    BrTextPrint("%wwGEAR UP/DOWN",0x74,0x65);
    BrTextAlignCentre();
  }
}

/* WHAT IT DOES: Does nothing. An empty function the retail build kept after
 * the controller-configuration code. */
/* @implements 0x80217C8C tgr BrStub80217C8C */
void BrStub80217C8C(void)
{
}
