//
// Technically, all these registers are 64 bits, not 32 bits
// but I don't know yet if WINDBG can handle that.
// MBH TODO bugbug
#define RSIZE 64

{ szR0, rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntV0 },
{ szR1, rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntT0 },
{ szR2, rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntT1 },
{ szR3, rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntT2 },
{ szR4, rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntT3 },
{ szR5, rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntT4 },
{ szR6, rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntT5 },
{ szR7, rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntT6 },
{ szR8, rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntT7 },
{ szR9, rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntS0 },
{ szR10,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntS1 },
{ szR11,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntS2 },
{ szR12,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntS3 },
{ szR13,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntS4 },
{ szR14,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntS5 },
{ szR15,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntFP },
{ szR16,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntA0 },
{ szR17,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntA1 },
{ szR18,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntA2 },
{ szR19,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntA3 },
{ szR20,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntA4 },
{ szR21,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntA5 },
{ szR22,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntT8 },
{ szR23,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntT9 },
{ szR24,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntT10 },
{ szR25,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntT11 },
{ szR26,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntRA },
{ szR27,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntT12 },
{ szR28,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntAT },
{ szR29,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntGP },
{ szR30,rtCPU | rtRegular | rtInteger | rtExtended | rtFrame, RSIZE, 0, CV_ALPHA_IntSP },
{ szR31,rtCPU | rtRegular | rtInteger | rtExtended, RSIZE, 0, CV_ALPHA_IntZERO },

{ szFpcr,rtFPU | rtRegular | rtExtended | rtInteger, RSIZE, 0, CV_ALPHA_Fpcr}, 
{ szSoftFpcr,
         rtFPU | rtRegular | rtExtended | rtInteger, RSIZE, 0, CV_ALPHA_SoftFpcr}, 
{ szFir,rtCPU | rtExtended | rtInteger | rtExtended | rtPC, 64, 0, CV_ALPHA_Fir },
{ szPsr,rtCPU | rtExtended | rtInteger | rtExtended, 32, 0, CV_ALPHA_Psr },

{ szF0, rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF0 },
{ szF1, rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF1 },
{ szF2, rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF2 },
{ szF3, rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF3 },
{ szF4, rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF4 },
{ szF5, rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF5 },
{ szF6, rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF6 },
{ szF7, rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF7 },
{ szF8, rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF8 },
{ szF9, rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF9 },
{ szF10,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF10 },
{ szF11,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF11 },
{ szF12,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF12 },
{ szF13,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF13 },
{ szF14,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF14 },
{ szF15,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF15 },
{ szF16,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF16 },
{ szF17,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF17 },
{ szF18,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF18 },
{ szF19,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF19 },
{ szF20,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF20 },
{ szF21,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF21 },
{ szF22,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF22 },
{ szF23,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF23 },
{ szF24,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF24 },
{ szF25,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF25 },
{ szF26,rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF26 },
{ szF27, rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF27 },
{ szF28, rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF28 },
{ szF29, rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF29 },
{ szF30, rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF30 },
{ szF31, rtFPU | rtRegular | rtFloat | rtExtended, RSIZE, 0, CV_ALPHA_FltF31 }
