// CWMFFilt.Cpp : Implementation of CWMFFilterApp and DLL registration.

#include "StdAfx.H"
#include "Resource.H"
#include "Include\WMFFilt.H"
#include "CWMFFilt.H"

#undef  DEFINE_GUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        EXTERN_C const GUID name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

DEFINE_GUID( IID_IDirectDrawSurface,		0x6C14DB81,0xA733,0x11CE,0xA5,0x21,0x00,
0x20,0xAF,0x0B,0xE5,0x60 );

RGBQUAD g_rgbHalftone[255] =
{
        { 0x00, 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x80, 0x00 },
        { 0x00, 0x80, 0x00, 0x00 },
        { 0x00, 0x80, 0x80, 0x00 },
        { 0x80, 0x00, 0x00, 0x00 },
        { 0x80, 0x00, 0x80, 0x00 },
        { 0x80, 0x80, 0x00, 0x00 },
        { 0xC0, 0xC0, 0xC0, 0x00 },
        { 0xC0, 0xDC, 0xC0, 0x00 },
        { 0xF0, 0xCA, 0xA6, 0x00 },
//        { 0x33, 0x00, 0x00, 0x00 },   Need to reserve a color for transparency
        { 0x66, 0x00, 0x00, 0x00 },
        { 0x99, 0x00, 0x00, 0x00 },
        { 0xCC, 0x00, 0x00, 0x00 },
        { 0x00, 0x33, 0x00, 0x00 },
        { 0x33, 0x33, 0x00, 0x00 },
        { 0x66, 0x33, 0x00, 0x00 },
        { 0x99, 0x33, 0x00, 0x00 },
        { 0xCC, 0x33, 0x00, 0x00 },
        { 0xFF, 0x33, 0x00, 0x00 },
        { 0x00, 0x66, 0x00, 0x00 },
        { 0x33, 0x66, 0x00, 0x00 },
        { 0x66, 0x66, 0x00, 0x00 },
        { 0x99, 0x66, 0x00, 0x00 },
        { 0xCC, 0x66, 0x00, 0x00 },
        { 0xFF, 0x66, 0x00, 0x00 },
        { 0x00, 0x99, 0x00, 0x00 },
        { 0x33, 0x99, 0x00, 0x00 },
        { 0x66, 0x99, 0x00, 0x00 },
        { 0x99, 0x99, 0x00, 0x00 },
        { 0xCC, 0x99, 0x00, 0x00 },
        { 0xFF, 0x99, 0x00, 0x00 },
        { 0x00, 0xCC, 0x00, 0x00 },
        { 0x33, 0xCC, 0x00, 0x00 },
        { 0x66, 0xCC, 0x00, 0x00 },
        { 0x99, 0xCC, 0x00, 0x00 },
        { 0xCC, 0xCC, 0x00, 0x00 },
        { 0xFF, 0xCC, 0x00, 0x00 },
        { 0x33, 0xFF, 0x00, 0x00 },
        { 0x66, 0xFF, 0x00, 0x00 },
        { 0x99, 0xFF, 0x00, 0x00 },
        { 0xCC, 0xFF, 0x00, 0x00 },
        { 0x00, 0x00, 0x33, 0x00 },
        { 0x33, 0x00, 0x33, 0x00 },
        { 0x66, 0x00, 0x33, 0x00 },
        { 0x99, 0x00, 0x33, 0x00 },
        { 0xCC, 0x00, 0x33, 0x00 },
        { 0xFF, 0x00, 0x33, 0x00 },
        { 0x00, 0x33, 0x33, 0x00 },
        { 0x33, 0x33, 0x33, 0x00 },
        { 0x66, 0x33, 0x33, 0x00 },
        { 0x99, 0x33, 0x33, 0x00 },
        { 0xCC, 0x33, 0x33, 0x00 },
        { 0xFF, 0x33, 0x33, 0x00 },
        { 0x00, 0x66, 0x33, 0x00 },
        { 0x33, 0x66, 0x33, 0x00 },
        { 0x66, 0x66, 0x33, 0x00 },
        { 0x99, 0x66, 0x33, 0x00 },
        { 0xCC, 0x66, 0x33, 0x00 },
        { 0xFF, 0x66, 0x33, 0x00 },
        { 0x00, 0x99, 0x33, 0x00 },
        { 0x33, 0x99, 0x33, 0x00 },
        { 0x66, 0x99, 0x33, 0x00 },
        { 0x99, 0x99, 0x33, 0x00 },
        { 0xCC, 0x99, 0x33, 0x00 },
        { 0xFF, 0x99, 0x33, 0x00 },
        { 0x00, 0xCC, 0x33, 0x00 },
        { 0x33, 0xCC, 0x33, 0x00 },
        { 0x66, 0xCC, 0x33, 0x00 },
        { 0x99, 0xCC, 0x33, 0x00 },
        { 0xCC, 0xCC, 0x33, 0x00 },
        { 0xFF, 0xCC, 0x33, 0x00 },
        { 0x00, 0xFF, 0x33, 0x00 },
        { 0x33, 0xFF, 0x33, 0x00 },
        { 0x66, 0xFF, 0x33, 0x00 },
        { 0x99, 0xFF, 0x33, 0x00 },
        { 0xCC, 0xFF, 0x33, 0x00 },
        { 0xFF, 0xFF, 0x33, 0x00 },
        { 0x00, 0x00, 0x66, 0x00 },
        { 0x33, 0x00, 0x66, 0x00 },
        { 0x66, 0x00, 0x66, 0x00 },
        { 0x99, 0x00, 0x66, 0x00 },
        { 0xCC, 0x00, 0x66, 0x00 },
        { 0xFF, 0x00, 0x66, 0x00 },
        { 0x00, 0x33, 0x66, 0x00 },
        { 0x33, 0x33, 0x66, 0x00 },
        { 0x66, 0x33, 0x66, 0x00 },
        { 0x99, 0x33, 0x66, 0x00 },
        { 0xCC, 0x33, 0x66, 0x00 },
        { 0xFF, 0x33, 0x66, 0x00 },
        { 0x00, 0x66, 0x66, 0x00 },
        { 0x33, 0x66, 0x66, 0x00 },
        { 0x66, 0x66, 0x66, 0x00 },
        { 0x99, 0x66, 0x66, 0x00 },
        { 0xCC, 0x66, 0x66, 0x00 },
        { 0xFF, 0x66, 0x66, 0x00 },
        { 0x00, 0x99, 0x66, 0x00 },
        { 0x33, 0x99, 0x66, 0x00 },
        { 0x66, 0x99, 0x66, 0x00 },
        { 0x99, 0x99, 0x66, 0x00 },
        { 0xCC, 0x99, 0x66, 0x00 },
        { 0xFF, 0x99, 0x66, 0x00 },
        { 0x00, 0xCC, 0x66, 0x00 },
        { 0x33, 0xCC, 0x66, 0x00 },
        { 0x66, 0xCC, 0x66, 0x00 },
        { 0x99, 0xCC, 0x66, 0x00 },
        { 0xCC, 0xCC, 0x66, 0x00 },
        { 0xFF, 0xCC, 0x66, 0x00 },
        { 0x00, 0xFF, 0x66, 0x00 },
        { 0x33, 0xFF, 0x66, 0x00 },
        { 0x66, 0xFF, 0x66, 0x00 },
        { 0x99, 0xFF, 0x66, 0x00 },
        { 0xCC, 0xFF, 0x66, 0x00 },
        { 0xFF, 0xFF, 0x66, 0x00 },
        { 0x00, 0x00, 0x99, 0x00 },
        { 0x33, 0x00, 0x99, 0x00 },
        { 0x66, 0x00, 0x99, 0x00 },
        { 0x99, 0x00, 0x99, 0x00 },
        { 0xCC, 0x00, 0x99, 0x00 },
        { 0xFF, 0x00, 0x99, 0x00 },
        { 0x00, 0x33, 0x99, 0x00 },
        { 0x33, 0x33, 0x99, 0x00 },
        { 0x66, 0x33, 0x99, 0x00 },
        { 0x99, 0x33, 0x99, 0x00 },
        { 0xCC, 0x33, 0x99, 0x00 },
        { 0xFF, 0x33, 0x99, 0x00 },
        { 0x00, 0x66, 0x99, 0x00 },
        { 0x33, 0x66, 0x99, 0x00 },
        { 0x66, 0x66, 0x99, 0x00 },
        { 0x99, 0x66, 0x99, 0x00 },
        { 0xCC, 0x66, 0x99, 0x00 },
        { 0xFF, 0x66, 0x99, 0x00 },
        { 0x00, 0x99, 0x99, 0x00 },
        { 0x33, 0x99, 0x99, 0x00 },
        { 0x66, 0x99, 0x99, 0x00 },
        { 0x99, 0x99, 0x99, 0x00 },
        { 0xCC, 0x99, 0x99, 0x00 },
        { 0xFF, 0x99, 0x99, 0x00 },
        { 0x00, 0xCC, 0x99, 0x00 },
        { 0x33, 0xCC, 0x99, 0x00 },
        { 0x66, 0xCC, 0x99, 0x00 },
        { 0x99, 0xCC, 0x99, 0x00 },
        { 0xCC, 0xCC, 0x99, 0x00 },
        { 0xFF, 0xCC, 0x99, 0x00 },
        { 0x00, 0xFF, 0x99, 0x00 },
        { 0x33, 0xFF, 0x99, 0x00 },
        { 0x66, 0xFF, 0x99, 0x00 },
        { 0x99, 0xFF, 0x99, 0x00 },
        { 0xCC, 0xFF, 0x99, 0x00 },
        { 0xFF, 0xFF, 0x99, 0x00 },
        { 0x00, 0x00, 0xCC, 0x00 },
        { 0x33, 0x00, 0xCC, 0x00 },
        { 0x66, 0x00, 0xCC, 0x00 },
        { 0x99, 0x00, 0xCC, 0x00 },
        { 0xCC, 0x00, 0xCC, 0x00 },
        { 0xFF, 0x00, 0xCC, 0x00 },
        { 0x00, 0x33, 0xCC, 0x00 },
        { 0x33, 0x33, 0xCC, 0x00 },
        { 0x66, 0x33, 0xCC, 0x00 },
        { 0x99, 0x33, 0xCC, 0x00 },
        { 0xCC, 0x33, 0xCC, 0x00 },
        { 0xFF, 0x33, 0xCC, 0x00 },
        { 0x00, 0x66, 0xCC, 0x00 },
        { 0x33, 0x66, 0xCC, 0x00 },
        { 0x66, 0x66, 0xCC, 0x00 },
        { 0x99, 0x66, 0xCC, 0x00 },
        { 0xCC, 0x66, 0xCC, 0x00 },
        { 0xFF, 0x66, 0xCC, 0x00 },
        { 0x00, 0x99, 0xCC, 0x00 },
        { 0x33, 0x99, 0xCC, 0x00 },
        { 0x66, 0x99, 0xCC, 0x00 },
        { 0x99, 0x99, 0xCC, 0x00 },
        { 0xCC, 0x99, 0xCC, 0x00 },
        { 0xFF, 0x99, 0xCC, 0x00 },
        { 0x00, 0xCC, 0xCC, 0x00 },
        { 0x33, 0xCC, 0xCC, 0x00 },
        { 0x66, 0xCC, 0xCC, 0x00 },
        { 0x99, 0xCC, 0xCC, 0x00 },
        { 0xCC, 0xCC, 0xCC, 0x00 },
        { 0xFF, 0xCC, 0xCC, 0x00 },
        { 0x00, 0xFF, 0xCC, 0x00 },
        { 0x33, 0xFF, 0xCC, 0x00 },
        { 0x66, 0xFF, 0xCC, 0x00 },
        { 0x99, 0xFF, 0xCC, 0x00 },
        { 0xCC, 0xFF, 0xCC, 0x00 },
        { 0xFF, 0xFF, 0xCC, 0x00 },
        { 0x33, 0x00, 0xFF, 0x00 },
        { 0x66, 0x00, 0xFF, 0x00 },
        { 0x99, 0x00, 0xFF, 0x00 },
        { 0xCC, 0x00, 0xFF, 0x00 },
        { 0x00, 0x33, 0xFF, 0x00 },
        { 0x33, 0x33, 0xFF, 0x00 },
        { 0x66, 0x33, 0xFF, 0x00 },
        { 0x99, 0x33, 0xFF, 0x00 },
        { 0xCC, 0x33, 0xFF, 0x00 },
        { 0xFF, 0x33, 0xFF, 0x00 },
        { 0x00, 0x66, 0xFF, 0x00 },
        { 0x33, 0x66, 0xFF, 0x00 },
        { 0x66, 0x66, 0xFF, 0x00 },
        { 0x99, 0x66, 0xFF, 0x00 },
        { 0xCC, 0x66, 0xFF, 0x00 },
        { 0xFF, 0x66, 0xFF, 0x00 },
        { 0x00, 0x99, 0xFF, 0x00 },
        { 0x33, 0x99, 0xFF, 0x00 },
        { 0x66, 0x99, 0xFF, 0x00 },
        { 0x99, 0x99, 0xFF, 0x00 },
        { 0xCC, 0x99, 0xFF, 0x00 },
        { 0xFF, 0x99, 0xFF, 0x00 },
        { 0x00, 0xCC, 0xFF, 0x00 },
        { 0x33, 0xCC, 0xFF, 0x00 },
        { 0x66, 0xCC, 0xFF, 0x00 },
        { 0x99, 0xCC, 0xFF, 0x00 },
        { 0xCC, 0xCC, 0xFF, 0x00 },
        { 0xFF, 0xCC, 0xFF, 0x00 },
        { 0x33, 0xFF, 0xFF, 0x00 },
        { 0x66, 0xFF, 0xFF, 0x00 },
        { 0x99, 0xFF, 0xFF, 0x00 },
        { 0xCC, 0xFF, 0xFF, 0x00 },
        { 0x04, 0x04, 0x04, 0x00 },
        { 0x08, 0x08, 0x08, 0x00 },
        { 0x0C, 0x0C, 0x0C, 0x00 },
        { 0x11, 0x11, 0x11, 0x00 },
        { 0x16, 0x16, 0x16, 0x00 },
        { 0x1C, 0x1C, 0x1C, 0x00 },
        { 0x21, 0x00, 0xA5, 0x00 },
        { 0x22, 0x22, 0x22, 0x00 },
        { 0x29, 0x29, 0x29, 0x00 },
        { 0x39, 0x39, 0x39, 0x00 },
        { 0x42, 0x42, 0x42, 0x00 },
        { 0x4D, 0x4D, 0x4D, 0x00 },
        { 0x50, 0x50, 0xFF, 0x00 },
        { 0x55, 0x55, 0x55, 0x00 },
        { 0x5F, 0x5F, 0x5F, 0x00 },
        { 0x77, 0x77, 0x77, 0x00 },
        { 0x86, 0x86, 0x86, 0x00 },
        { 0x96, 0x96, 0x96, 0x00 },
        { 0xB2, 0xB2, 0xB2, 0x00 },
        { 0xCB, 0xCB, 0xCB, 0x00 },
        { 0xD6, 0xE7, 0xE7, 0x00 },
        { 0xD7, 0xD7, 0xD7, 0x00 },
        { 0xDD, 0xDD, 0xDD, 0x00 },
        { 0xE3, 0xE3, 0xE3, 0x00 },
        { 0xEA, 0xEA, 0xEA, 0x00 },
        { 0xF1, 0xF1, 0xF1, 0x00 },
        { 0xF8, 0xF8, 0xF8, 0x00 },
        { 0xFF, 0xEC, 0xCC, 0x00 },
        { 0xF0, 0xFB, 0xFF, 0x00 },
        { 0xA4, 0xA0, 0xA0, 0x00 },
        { 0x80, 0x80, 0x80, 0x00 },
        { 0x00, 0x00, 0xFF, 0x00 },
        { 0x00, 0xFF, 0x00, 0x00 },
        { 0x00, 0xFF, 0xFF, 0x00 },
        { 0xFF, 0x00, 0x00, 0x00 },
        { 0xFF, 0x00, 0xFF, 0x00 },
        { 0xFF, 0xFF, 0x00, 0x00 },
        { 0xFF, 0xFF, 0xFF, 0x00 },
};

/////////////////////////////////////////////////////////////////////////////
//

CWMFFilter::CWMFFilter()
{
}

CWMFFilter::~CWMFFilter()
{
}

HRESULT CWMFFilter::GetColors()
{
	m_nColors = 255;

	memcpy(m_argbPalette, g_rgbHalftone, sizeof(g_rgbHalftone));
    
   return( S_OK );
}


void CopyPaletteEntriesFromColors(PALETTEENTRY *ppe, const RGBQUAD *prgb,
    UINT uCount)
{
    while (uCount--)
    {
        ppe->peRed   = prgb->rgbRed;
        ppe->peGreen = prgb->rgbGreen;
        ppe->peBlue  = prgb->rgbBlue;
        ppe->peFlags = 0;

        prgb++;
        ppe++;
    }
}

#pragma check_stack( off )

HRESULT CWMFFilter::ReadImage()
{
   int nOldMapMode;
   METAHEADER mh;
   LPBYTE pbBuf;
   ULONG ulSize;
   HMETAFILE hmf;
   HRESULT hResult;
   ULONG nBytesRead;
   SIZE sizeOld;
	HDC hDC;
   BYTE abBitmapInfo[sizeof( BITMAPINFOHEADER )+(256*sizeof( RGBQUAD ))];
   BITMAPINFO* pbmi;
   BYTE* pbSrcScanLine;
   BYTE* pbDestScanLine;
   LONG nSrcPitch;
   LONG nDestPitch;
   void* pSrcBits;
   void* pDestBits;
   HBITMAP hBitmap;
   ULONG iScanLine;
   HGDIOBJ hOldObject;
   RECT rect;

	// Get the metafile header so we know how big it is
   hResult = m_pStream->Read( &mh, sizeof( mh ), &nBytesRead );
   if( FAILED( hResult ) )
   {
      return( hResult );
   }

	// allocate a buffer to hold it
	ulSize = mh.mtSize * sizeof(WORD);
	pbBuf = new BYTE[ulSize];
	if( pbBuf == NULL )
   {
      return( E_OUTOFMEMORY );
   }

	// copy the header into the front of the buffer
	memcpy( pbBuf, &mh, sizeof(METAHEADER) );
	
	// read the metafile into memory after the header
   hResult = m_pStream->Read( pbBuf+sizeof( METAHEADER ), ulSize-
      sizeof( METAHEADER ), &nBytesRead );
   if( FAILED( hResult ) )
   {
      return( hResult );
   }

	// convert the buffer into a metafile handle
	hmf = SetMetaFileBitsEx( ulSize, pbBuf );
   delete pbBuf;
   if( hmf == NULL )
   {
      return( E_FAIL );
   }

	// Render the metafile into the bitmap
	
   pbmi = (BITMAPINFO*)abBitmapInfo;
   memset( &pbmi->bmiHeader, 0, sizeof( BITMAPINFOHEADER ) );
   pbmi->bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
   pbmi->bmiHeader.biWidth = m_nWidth;
   pbmi->bmiHeader.biHeight = -LONG( m_nHeight );
   pbmi->bmiHeader.biPlanes = 1;
   pbmi->bmiHeader.biBitCount = 8;
   pbmi->bmiHeader.biCompression = BI_RGB;
   pbmi->bmiHeader.biClrUsed = m_nColors;
   
   memcpy( pbmi->bmiColors, m_argbPalette, m_nColors*sizeof( RGBQUAD ) );

   hBitmap = CreateDIBSection( NULL, pbmi, DIB_RGB_COLORS, &pSrcBits, NULL, 
      0 );
   if( hBitmap == NULL )
   {
      DeleteMetaFile( hmf );
      return( E_OUTOFMEMORY );
   }

   pbSrcScanLine = LPBYTE( pSrcBits );
   nSrcPitch = (m_nWidth+3)&~0x03;

    // Prefill buffer with transparent index
    memset( pbSrcScanLine, 255, nSrcPitch * m_nHeight);
    
   hDC = CreateCompatibleDC( NULL );
   if( hDC == NULL )
   {
      DeleteObject( hBitmap );
      DeleteMetaFile( hmf );
      return( E_OUTOFMEMORY );
   }

   hOldObject = SelectObject( hDC, hBitmap );

   nOldMapMode = GetMapMode( hDC );
   SetMapMode( hDC, MM_ANISOTROPIC );
   SetViewportExtEx( hDC, m_nWidth, m_nHeight, &sizeOld );
	PlayMetaFile( hDC, hmf );
   SetViewportExtEx( hDC, sizeOld.cx, sizeOld.cy, NULL );
   SetMapMode( hDC, nOldMapMode );

   SelectObject( hDC, hOldObject );

   DeleteDC( hDC );
   DeleteMetaFile( hmf );

   rect.left = 0;
   rect.top = 0;
   rect.right = m_nWidth;
   rect.bottom = m_nHeight;

   hResult = LockBits( &rect, SURFACE_LOCK_EXCLUSIVE,
      &pDestBits, &nDestPitch );
   if( FAILED( hResult ) )
   {
      DeleteObject( hBitmap );
      return( hResult );
   }

   pbDestScanLine = LPBYTE( pDestBits );
   
    for( iScanLine = 0; iScanLine < m_nHeight; iScanLine++ )
    {
        memcpy( pbDestScanLine, pbSrcScanLine, m_nWidth );

        pbSrcScanLine += nSrcPitch;   
        pbDestScanLine += nDestPitch;
    }

   UnlockBits( &rect, pDestBits );

   DeleteObject( hBitmap );

   return( S_OK );
}

HRESULT CWMFFilter::LockBits(RECT *prcBounds, DWORD dwLockFlags, void **ppBits, long *pPitch)
{
    HRESULT hResult;
    DDSURFACEDESC   ddsd;

	// Unused parameter
	(dwLockFlags);

    ddsd.dwSize = sizeof(ddsd);
    hResult = m_pDDrawSurface->Lock(prcBounds, &ddsd, DDLOCK_WAIT | DDLOCK_SURFACEMEMORYPTR, NULL);
    if (FAILED(hResult))
        return hResult;

    *ppBits = ddsd.lpSurface;
    *pPitch = ddsd.lPitch;

    return S_OK;
}

HRESULT CWMFFilter::UnlockBits(RECT *prcBounds, void *pBits)
{
	// Unused parameter
	(prcBounds);

    if (m_pDDrawSurface)
    {
        return m_pDDrawSurface->Unlock(pBits);
    }
    else
		return E_FAIL;
}

#pragma check_stack()

HRESULT CWMFFilter::FireGetSurfaceEvent()
{
   HRESULT hResult;
   CComPtr< IUnknown > pSurface;

    hResult = m_pEventSink->GetSurface(m_nWidth, m_nHeight, BFID_INDEXED_RGB_8, 
                        0, IMGDECODE_HINT_TOPDOWN|IMGDECODE_HINT_FULLWIDTH, 
                        &pSurface);

    if (FAILED(hResult))
        return hResult;

    pSurface->QueryInterface(IID_IDirectDrawSurface, 
                                 (void **)&m_pDDrawSurface);

    if (m_pDDrawSurface)
    {
        LPDIRECTDRAWPALETTE pDDPalette;
        PALETTEENTRY        ape[256];
        DDCOLORKEY  ddKey;

		hResult = m_pDDrawSurface->GetPalette(&pDDPalette);
		if (SUCCEEDED(hResult))
		{
            CopyPaletteEntriesFromColors(ape, m_argbPalette, m_nColors);
		    pDDPalette->SetEntries(0, 0, m_nColors, ape);
		    pDDPalette->Release();
		}

        // Set the transparent index
        ddKey.dwColorSpaceLowValue = m_lTrans;
        ddKey.dwColorSpaceHighValue = m_lTrans;
        hResult = m_pDDrawSurface->SetColorKey(DDCKEY_SRCBLT, &ddKey);
    }

    if( m_dwEvents & IMGDECODE_EVENT_PALETTE )
    {
        hResult = m_pEventSink->OnPalette();
        if( FAILED( hResult ) )
        {
            return( hResult );
        }
    }

   return( S_OK );
}

HRESULT CWMFFilter::FireOnBeginDecodeEvent()
{
   HRESULT hResult;
   BOOL bFound;
   ULONG iFormat;
   GUID* pFormats;
   ULONG nFormats;

   hResult = m_pEventSink->OnBeginDecode(&m_dwEvents, &nFormats, &pFormats);
   if( FAILED( hResult ) )
   {
      return( hResult );
   }

   bFound = FALSE;
   for( iFormat = 0; (iFormat < nFormats) && !bFound; iFormat++ )
   {
      if( IsEqualGUID( pFormats[iFormat], BFID_INDEXED_RGB_8 ) )
      {
         bFound = TRUE;
      }
   }
   CoTaskMemFree( pFormats );
   if( !bFound )
   {
      return( E_FAIL );
   }

   return( S_OK );
}

HRESULT CWMFFilter::FireOnBitsCompleteEvent()
{
   HRESULT hResult;

   if( !(m_dwEvents & IMGDECODE_EVENT_BITSCOMPLETE) )
   {
      return( S_OK );
   }

   hResult = m_pEventSink->OnBitsComplete();
   if( FAILED( hResult ) )
   {
      return( hResult );
   }

   return( S_OK );
}

HRESULT CWMFFilter::FireOnProgressEvent()
{
   HRESULT hResult;
   RECT rect;

   if( !(m_dwEvents & IMGDECODE_EVENT_PROGRESS) )
   {
      return( S_OK );
   }

   rect.left = 0;
   rect.top = 0;
   rect.right = m_nWidth;
   rect.bottom = m_nHeight;

   hResult = m_pEventSink->OnProgress(&rect, TRUE);
   if( FAILED( hResult ) )
   {
      return( hResult );
   }

   return( S_OK );
}

STDMETHODIMP CWMFFilter::Initialize( IImageDecodeEventSink* pEventSink )
{
   if( pEventSink == NULL )
   {
      return( E_INVALIDARG );
   }

    m_pEventSink = pEventSink;

    m_lTrans = 255;
    
   return( S_OK );
}

STDMETHODIMP CWMFFilter::Process( IStream* pStream )
{
	ALDUSMFHEADER amfh;
   HRESULT hResult;
   ULONG nBytesRead;

   if( pStream == NULL )
   {
      return( E_INVALIDARG );
   }

   m_pStream = pStream;

   // KENSY: What do I do about the color table?
	// KENSY: scale according to DPI of screen

   hResult = FireOnBeginDecodeEvent();
   if( FAILED( hResult ) )
   {
      return( hResult );
   }

   hResult = m_pStream->Read( &amfh, sizeof( amfh ), &nBytesRead );
   if( FAILED( hResult ) )
   {
      return( hResult );
   }

	hResult = GetColors();
   if( FAILED( hResult ) )
   {
      return( hResult );
   }
	
   m_nWidth = MulDiv( amfh.bbox.right - amfh.bbox.left, 96, amfh.inch );
   m_nHeight = MulDiv( amfh.bbox.bottom - amfh.bbox.top, 96, amfh.inch );
    
   hResult = FireGetSurfaceEvent();
   if( FAILED( hResult ) )
   {
      return( hResult );
   }

   hResult = ReadImage();
   if( FAILED( hResult ) )
   {
      return( hResult );
   }

   hResult = FireOnProgressEvent();
   if( FAILED( hResult ) )
   {
      return( hResult );
   }

   hResult = FireOnBitsCompleteEvent();
   if( FAILED( hResult ) )
   {
      return( hResult );
   }

   m_pStream = NULL;

   return( S_OK );
}

STDMETHODIMP CWMFFilter::Terminate( HRESULT hrStatus )
{
   ATLTRACE( "Image decode terminated.  Status: %x\n", hrStatus );

    if (m_pDDrawSurface != NULL)
    {
        m_pDDrawSurface.Release();
    }
        
   if( m_pEventSink != NULL )
   {
      m_pEventSink->OnDecodeComplete(hrStatus);
      m_pEventSink.Release();
   }

   return( S_OK );
}
