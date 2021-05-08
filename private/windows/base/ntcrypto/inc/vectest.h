// CJ vector test constants
//

// GenKey flag to get vectors into key.
#define	VECTTEST	0x4000

BYTE VTRC2[/*RC2_KEYSIZE*/] = { 0x59, 0x45, 0x9a, 0xf9, 0x27 };
// 0x84, 0x74, 0xca };

BYTE VTRC4[/*RC4_KEYSIZE*/] = { 0x61, 0x8a, 0x63, 0xd2, 0xfb };

#define DES_TEST        0x2000

BYTE DESTEST[/*DES_KEYSIZE*/] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};

#define DES3_TEST       0x1000

BYTE DES3TEST[/*DES3_KEYSIZE*/] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                                  0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01,
                                  0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23};
