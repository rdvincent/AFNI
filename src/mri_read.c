#include <stdio.h>
#include <ctype.h>
#include <string.h>

/*** for non ANSI compilers ***/

#ifndef SEEK_END
#define SEEK_END 2
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#include "mrilib.h"

/*** 7D SAFE (but most routines only return 2D images!) ***/

MRI_IMAGE *mri_try_mri( FILE * , int * ) ;
MRI_IMAGE *mri_try_7D ( FILE * , int * ) ;
MRI_IMAGE *mri_try_pgm( FILE * , int * ) ;

/*-----------------------------------------------------------------
  database of preset sizes to get formatting
  information for reading with 3D prefixes:
    size   = file size in bytes if head < 0
               image size in bytes if head >= 0
    head   = global header size if >=0
    prefix = character string to prefix to filename

  if( head < 0 ) then file length must match size
  else file length must == n*size + head for some
       integer n, which will be the number of slices
       to read from the file.
-------------------------------------------------------------------*/

typedef struct {
   int size , head ;
   char * prefix ;
} MCW_imsize ;

#define MAX_MCW_IMSIZE 99

static MCW_imsize imsize[MAX_MCW_IMSIZE] ;
static int MCW_imsize_good = -1 ;           /* < 0 ==> must initialize */

/*-----------------------------------------------------------------*/

/************************************************************************/

MRI_IMAGE *mri_read( char *fname )
{
   FILE      *imfile ;
   MRI_IMAGE *im ;
   int       length , skip=0 ;
   void      *data ;

WHOAMI ;

   imfile = fopen( fname , "r" ) ;
   if( imfile == NULL ){
      fprintf( stderr , "couldn't open file %s\n" , fname ) ;
      return NULL ;
   }

   fseek( imfile , 0L , SEEK_END ) ;  /* get the length of the file */
   length = ftell( imfile ) ;

   switch( length ){

      case 4096:   /* raw 64x64 byte -- RWC 3/21/95 */
         im = mri_new( 64 , 64 , MRI_byte ) ;
         break ;

      case 8192:   /* raw 64x64 short */
      case 16096:  /* with Signa 5.x header */
         im = mri_new( 64 , 64 , MRI_short ) ;
         skip = length - 8192 ;
         break ;

#if 0
      case 18432:  /* raw 96x96 short */
         im = mri_new( 96 , 96 , MRI_short ) ;
         break ;
#endif

      case 16384:  /* raw 128x128 byte -- RWC 3/21/95 */
         im = mri_new( 128 , 128 , MRI_byte ) ;
         break ;

      case 32768:  /* raw 128x128 short */
      case 40672:  /* with Signa 5.x header */
         im = mri_new( 128 , 128 , MRI_short ) ;
         skip = length - 32768 ;
         break ;

      case 65536:  /* raw 256x256 8-bit -- Matthew Belmonte March 1995 */
         im = mri_new( 256 , 256 , MRI_byte ) ;
         break ;

      case 131072:  /* raw 256x256 short */
      case 138976:  /* Signa 5.x */
      case 145408:  /* Signa 4.x */

         im   = mri_new( 256 , 256 , MRI_short ) ;
         skip = length - 131072 ;
         break ;

#if 0
      case 262144:  /* raw 256x256 float */
         im = mri_new( 256 , 256 , MRI_float ) ;
         break ;
#else
      case 262144:  /* raw 512x512 byte -- RWC 3/21/95 */
         im = mri_new( 512 , 512 , MRI_byte ) ;
         break ;

      case 524288:  /* raw 512x512 short -- RWC 3/21/95 */
         im = mri_new( 512 , 512 , MRI_short ) ;
         break ;

      case 1048576: /* raw 1024x1024 byte -- RWC 3/21/95 */
         im = mri_new( 1024 , 1024 , MRI_byte ) ;
         break ;

      case 2097152: /* raw 1024x1024 short -- RWC 3/21/95 */
         im = mri_new( 1024 , 1024 , MRI_short ) ;
         break ;
#endif

      /** not a canonical length: try something else **/

      default:
                          im = mri_try_mri( imfile , &skip ) ;  /* Cox format */
         if( im == NULL ) im = mri_try_7D ( imfile , &skip ) ;  /* 7D format  */
         if( im == NULL ) im = mri_try_pgm( imfile , &skip ) ;  /* PGM format */
         if( im != NULL ) break ;

         fclose( imfile ) ; /* close it, since we failed (so far) */

         im = mri_read_ascii( fname ) ;    /* list of ASCII numbers */
         if( im != NULL ) return im ;

         im = mri_read_ppm( fname ) ;      /* 15 Apr 1999 */
         if( im != NULL ) return im ;

         fprintf( stderr , "do not recognize file %s\n" , fname );
         fprintf( stderr , "length seen as %d\n" , length ) ;
         return NULL ;
   }

   data = mri_data_pointer( im ) ;

   length = fseek( imfile , skip , SEEK_SET ) ;
   if( length != 0 ){
      fprintf( stderr , "mri_read error in skipping in file %s\n" , fname ) ;
      mri_free( im ) ;
      return NULL ;
   }

   length = fread( data , im->pixel_size , im->nvox , imfile ) ;
   fclose( imfile ) ;

   if( length != im->nvox ){
      mri_free( im ) ;
      fprintf( stderr , "couldn't read data from file %s\n" , fname ) ;
      return NULL ;
   }

   mri_add_name( fname , im ) ;

   return im ;
}

/*********************************************************************/

#define NUMSCAN(var)                                                       \
   { while( isspace(ch) ) {ch = getc(imfile) ;}                            \
     if(ch == '#') do{ch = getc(imfile) ;}while(ch != '\n' && ch != EOF) ; \
     if(ch =='\n') ch = getc(imfile) ;                                     \
     for( nch=0 ; isdigit(ch) ; nch++,ch=getc(imfile) ) {buf[nch] = ch ;}  \
     buf[nch]='\0';                                                        \
     var = strtol( buf , NULL , 10 ) ; }

MRI_IMAGE *mri_try_mri( FILE *imfile , int *skip )
{
   int ch , nch , nx,ny,imcode ;
   char buf[64] ;
   MRI_IMAGE *im ;

   fseek( imfile , 0 , SEEK_SET ) ;  /* rewind file */

   ch = getc( imfile ) ; if( ch != 'M' ) return NULL ;  /* check for MRI */
   ch = getc( imfile ) ; if( ch != 'R' ) return NULL ;
   ch = getc( imfile ) ; if( ch != 'I' ) return NULL ;

   /* magic MRI found, so read numbers */

   ch = getc(imfile) ;

   NUMSCAN(imcode) ;
   NUMSCAN(nx) ;  if( nx <= 0 ) return NULL ;
   NUMSCAN(ny) ;  if( ny <= 0 ) return NULL ;

   *skip = ftell(imfile) ;
   im    = mri_new( nx , ny , imcode ) ;
   return im ;
}

/**************************************************************************
   7D format: MRn kind n-dimensions data, where 'n' = 1-7.
***************************************************************************/

MRI_IMAGE *mri_try_7D( FILE *imfile , int *skip )
{
   int ch , nch , nx,ny,nz,nt,nu,nv,nw , imcode , ndim ;
   char buf[64] ;
   MRI_IMAGE *im ;

   fseek( imfile , 0 , SEEK_SET ) ;  /* rewind file */

   ch = getc( imfile ) ; if( ch != 'M' ) return NULL ;  /* check for MR[1-7] */
   ch = getc( imfile ) ; if( ch != 'R' ) return NULL ;
   ch = getc( imfile ) ;
   switch( ch ){
      default:  return NULL ;   /* not what I expected */

      case '1': ndim = 1 ; break ;
      case '2': ndim = 2 ; break ;
      case '3': ndim = 3 ; break ;
      case '4': ndim = 4 ; break ;
      case '5': ndim = 5 ; break ;
      case '6': ndim = 6 ; break ;
      case '7': ndim = 7 ; break ;
   }
   /* magic MR? found, so read numbers */

   ch = getc(imfile) ;
   NUMSCAN(imcode) ;

   nx = ny = nz = nt = nu = nv = nw = 1 ;

                   NUMSCAN(nx) ;  if( nx <= 0 ) return NULL ;
   if( ndim > 1 ){ NUMSCAN(ny) ;  if( ny <= 0 ) return NULL ; }
   if( ndim > 2 ){ NUMSCAN(nz) ;  if( nz <= 0 ) return NULL ; }
   if( ndim > 3 ){ NUMSCAN(nt) ;  if( nt <= 0 ) return NULL ; }
   if( ndim > 4 ){ NUMSCAN(nu) ;  if( nu <= 0 ) return NULL ; }
   if( ndim > 5 ){ NUMSCAN(nv) ;  if( nv <= 0 ) return NULL ; }
   if( ndim > 6 ){ NUMSCAN(nw) ;  if( nw <= 0 ) return NULL ; }

   *skip = ftell(imfile) ;
   im    = mri_new_7D_generic( nx,ny,nz,nt,nu,nv,nw , imcode , TRUE ) ;
   return im ;
}


/*********************************************************************/

MRI_IMAGE *mri_try_pgm( FILE *imfile , int *skip )
{
   int ch , nch , nx,ny,maxval ;
   char buf[64] ;
   MRI_IMAGE *im ;

   fseek( imfile , 0 , SEEK_SET ) ;  /* rewind file */

   ch = getc( imfile ) ; if( ch != 'P' ) return NULL ;  /* check for magic */
   ch = getc( imfile ) ; if( ch != '5' ) return NULL ;

   /* magic P5 found, so read numbers */

   ch = getc(imfile) ;

   NUMSCAN(nx)     ; if( nx     <= 0 ) return NULL ;
   NUMSCAN(ny)     ; if( ny     <= 0 ) return NULL ;
   NUMSCAN(maxval) ; if( maxval <= 0 || maxval >  255 ) return NULL ;

   *skip = ftell(imfile) ;
   im    = mri_new( nx , ny , MRI_byte ) ;
   return im ;
}

/*--------------------------------------------------------------
   Read a pile of images from one file.
   Modified 4/4/95 to read short or byte data.
   Modified 10/02/95 to allow byte swapping with 3Ds:
   Modified 11/06/95 to allow float images with 3Df:
                 and to allow int images with 3Di:
                 and to allow complex images with 3Dc:

   [N.B.: if this routine is altered, don't forget mri_imcount!]
----------------------------------------------------------------*/

MRI_IMARR * mri_read_3D( char * tname )
{
   int hglobal , himage , nx , ny , nz ;
   char fname[256] , buf[512] ;
   int ngood , length , kim , koff , datum_type , datum_len , swap ;
   MRI_IMARR * newar ;
   MRI_IMAGE * newim ;
   void      * imar ;
   FILE      * imfile ;

   /*** get info from 3D tname ***/

   if( tname == NULL || strlen(tname) < 10 ) return NULL ;

   switch( tname[2] ){  /* allow for 3D: or 3Ds: or 3Db: */

      default:
      case ':':
         ngood = sscanf( tname , "3D:%d:%d:%d:%d:%d:%s" ,
                         &hglobal , &himage , &nx , &ny , &nz , fname ) ;

         swap       = 0 ;
         datum_type = MRI_short ;
         datum_len  = sizeof(short) ;  /* better be 2 */
         break ;

      case 's':
         ngood = sscanf( tname , "3Ds:%d:%d:%d:%d:%d:%s" ,
                         &hglobal , &himage , &nx , &ny , &nz , fname ) ;

         swap       = 1 ;
         datum_type = MRI_short ;
         datum_len  = sizeof(short) ;  /* better be 2 */
         break ;

      case 'b':
         ngood = sscanf( tname , "3Db:%d:%d:%d:%d:%d:%s" ,
                         &hglobal , &himage , &nx , &ny , &nz , fname ) ;

         swap       = 0 ;
         datum_type = MRI_byte ;
         datum_len  = sizeof(byte) ;  /* better be 1 */
         break ;

      case 'f':
         ngood = sscanf( tname , "3Df:%d:%d:%d:%d:%d:%s" ,
                         &hglobal , &himage , &nx , &ny , &nz , fname ) ;

         swap       = 0 ;
         datum_type = MRI_float ;
         datum_len  = sizeof(float) ;  /* better be 4 */
         break ;

      case 'i':
         ngood = sscanf( tname , "3Di:%d:%d:%d:%d:%d:%s" ,
                         &hglobal , &himage , &nx , &ny , &nz , fname ) ;

         swap       = 0 ;
         datum_type = MRI_int ;
         datum_len  = sizeof(int) ;  /* better be 4 */
         break ;

      case 'c':
         ngood = sscanf( tname , "3Dc:%d:%d:%d:%d:%d:%s" ,
                         &hglobal , &himage , &nx , &ny , &nz , fname ) ;

         swap       = 0 ;
         datum_type = MRI_complex ;
         datum_len  = sizeof(complex) ;  /* better be 8 */
         break ;
   }

   if( ngood < 6 || himage < 0 ||
       nx <= 0   || ny <= 0    || nz <= 0 ||
       strlen(fname) <= 0                   ) return NULL ;   /* bad info */

   /*** open the input file and position it ***/

   imfile = fopen( fname , "r" ) ;
   if( imfile == NULL ){
      fprintf( stderr , "couldn't open file %s\n" , fname ) ;
      return NULL ;
   }

   fseek( imfile , 0L , SEEK_END ) ;  /* get the length of the file */
   length = ftell( imfile ) ;

   /** 13 Apr 1999: modified to allow actual hglobal < -1
                    as long as hglobal+himage >= 0       **/

#if 0                 /* the old code */
   if( hglobal < 0 ){
      hglobal = length - nz*(datum_len*nx*ny+himage) ;
      if( hglobal < 0 ) hglobal = 0 ;
   }
#else                 /* 13 Apr 1999 */
   if( hglobal == -1 || hglobal+himage < 0 ){
      hglobal = length - nz*(datum_len*nx*ny+himage) ;
      if( hglobal < 0 ) hglobal = 0 ;
   }
#endif

   ngood = hglobal + nz*(datum_len*nx*ny+himage) ;
   if( length < ngood ){
      fprintf( stderr,
        "file %s is %d bytes long but must be at least %d bytes long\n"
        "for hglobal=%d himage=%d nx=%d ny=%d nz=%d and voxel=%d bytes\n",
        fname,length,ngood,hglobal,himage,nx,ny,nz,datum_len ) ;
      fclose( imfile ) ;
      return NULL ;
   }

   /*** read images from the file ***/

   INIT_IMARR(newar) ;

   for( kim=0 ; kim < nz ; kim++ ){
      koff = hglobal + (kim+1)*himage + datum_len*nx*ny*kim ;
      fseek( imfile , koff , SEEK_SET ) ;

      newim  = mri_new( nx , ny , datum_type ) ;
      imar   = mri_data_pointer( newim ) ;
      length = fread( imar , datum_len , nx * ny , imfile ) ;
      if( swap ) mri_swapbytes( newim ) ;

      if( nz == 1 ) mri_add_name( fname , newim ) ;
      else {
         sprintf( buf , "%s#%d" , fname,kim ) ;
         mri_add_name( buf , newim ) ;
      }

      ADDTO_IMARR(newar,newim) ;
   }

   fclose(imfile) ;
   return newar ;
}

/*--------------------------------------------------------------*/

MRI_IMARR * mri_read_file( char * fname )
{
   MRI_IMARR * newar ;
   MRI_IMAGE * newim ;
   char * new_fname ;

   new_fname = imsized_fname( fname ) ;
   if( new_fname == NULL ) return NULL ;

   if( strlen(new_fname) > 9 && new_fname[0] == '3' && new_fname[1] == 'D' ){

      newar = mri_read_3D( new_fname ) ;   /* read from a 3D file */

   } else if( strlen(new_fname) > 9 &&
              new_fname[0] == '3' && new_fname[1] == 'A' && new_fname[3] == ':' ){

      newar = mri_read_3A( new_fname ) ;

   } else {
      newim = mri_read( new_fname ) ;      /* read from a 2D file */
      if( newim == NULL ){ free(new_fname) ; return NULL ; }
      INIT_IMARR(newar) ;
      ADDTO_IMARR(newar,newim) ;
   }
   free(new_fname) ;
   return newar ;
}

MRI_IMAGE * mri_read_just_one( char * fname )
{
   MRI_IMARR * imar ;
   MRI_IMAGE * im ;
   char * new_fname ;

   new_fname = imsized_fname( fname ) ;
   if( new_fname == NULL ) return NULL ;

   imar = mri_read_file( new_fname ) ; free(new_fname) ;
   if( imar == NULL ) return NULL ;
   if( imar->num != 1 ){ DESTROY_IMARR(imar) ; return NULL ; }
   im = IMAGE_IN_IMARR(imar,0) ;
   FREE_IMARR(imar) ;
   return im ;
}

/*-----------------------------------------------------------------
  return a count of how many 2D images will be read from this file
-------------------------------------------------------------------*/

int mri_imcount( char * tname )
{
   int hglobal , himage , nx , ny , nz , ngood ;
   char fname[256]="\0" ;
   char * new_fname ;

   if( tname == NULL ) return 0 ;
   new_fname = imsized_fname( tname ) ;
   if( new_fname == NULL ) return 0 ;

   /*** a 3D filename ***/

   if( strlen(new_fname) > 9 && new_fname[0] == '3' && new_fname[1] == 'D' ){

      switch( new_fname[2] ){

         default:
         case ':':
            ngood = sscanf( new_fname , "3D:%d:%d:%d:%d:%d:%s" ,
                            &hglobal , &himage , &nx , &ny , &nz , fname ) ;
            break ;

         case 's':
            ngood = sscanf( new_fname , "3Ds:%d:%d:%d:%d:%d:%s" ,
                            &hglobal , &himage , &nx , &ny , &nz , fname ) ;
            break ;

         case 'b':
            ngood = sscanf( new_fname , "3Db:%d:%d:%d:%d:%d:%s" ,
                            &hglobal , &himage , &nx , &ny , &nz , fname ) ;
            break ;

         case 'f':
            ngood = sscanf( new_fname , "3Df:%d:%d:%d:%d:%d:%s" ,
                            &hglobal , &himage , &nx , &ny , &nz , fname ) ;
            break ;

         case 'i':
            ngood = sscanf( new_fname , "3Di:%d:%d:%d:%d:%d:%s" ,
                            &hglobal , &himage , &nx , &ny , &nz , fname ) ;
            break ;

         case 'c':
            ngood = sscanf( new_fname , "3Dc:%d:%d:%d:%d:%d:%s" ,
                            &hglobal , &himage , &nx , &ny , &nz , fname ) ;
            break ;
      }

      free( new_fname ) ;
      if( ngood < 6 || himage < 0 ||
          nx <= 0   || ny <= 0    || nz <= 0 ||
          strlen(fname) <= 0                       ) return 0 ;
      else                                           return nz ;
   }

   /*** a 3A filename ***/

   if( strlen(new_fname) > 9 &&
       new_fname[0] == '3' && new_fname[1] == 'A' && new_fname[3] == ':' ){

      switch( new_fname[2] ){

         default: ngood = 0 ; break ;

         case 's':
            ngood = sscanf( new_fname, "3As:%d:%d:%d:%s", &nx, &ny, &nz, fname ) ;
            break ;

         case 'b':
            ngood = sscanf( new_fname, "3Ab:%d:%d:%d:%s", &nx, &ny, &nz, fname ) ;
            break ;

         case 'f':
            ngood = sscanf( new_fname, "3Af:%d:%d:%d:%s", &nx, &ny, &nz, fname ) ;
            break ;
      }

      free( new_fname ) ;
      if( ngood < 4 || nx <= 0 || ny <= 0 || nz <= 0 || strlen(fname) <= 0 ) return 0 ;
      else                                                                   return nz ;
   }

   /*** not a 3D filename ***/

   free( new_fname ) ;
   return 1 ;
}

/*--------------------------------------------------------------*/
/* added 07 Mar 1995 */

MRI_IMARR * mri_read_many_files( int nf , char * fn[] )
{
   MRI_IMARR * newar , * outar ;
   int kf , ii ;

   if( nf <= 0 ) return NULL ;  /* no inputs! */
   INIT_IMARR(outar) ;          /* initialize output array */

   for( kf=0 ; kf < nf ; kf++ ){
      newar = mri_read_file( fn[kf] ) ;  /* read all images in this file */

      if( newar == NULL ){  /* none?  flush the output array! */
         fprintf(stderr,"cannot read images from file %s\n",fn[kf]) ;
         for( ii=0 ; ii < outar->num ; ii++ ) mri_free(outar->imarr[ii]) ;
         FREE_IMARR(outar) ;
         return NULL ;
      }

      for( ii=0 ; ii < newar->num ; ii++ )  /* move images to output array */
         ADDTO_IMARR( outar , newar->imarr[ii] ) ;

      FREE_IMARR(newar) ;  /* don't need this no more */
   }
   return outar ;
}

/*---------------------------------------------------------------
  May 16, 1995: reads a raw PPM file into 3 images in RGB order
-----------------------------------------------------------------*/

MRI_IMARR * mri_read_ppm3( char * fname )
{
   int ch , nch , nx,ny,maxval , length , npix,ii ;
   char buf[512] ;
   MRI_IMAGE *rim , *gim , *bim ;
   MRI_IMARR * outar ;
   FILE * imfile ;
   byte * rby , * gby , * bby , * rgby ;

WHOAMI ;

   /*** open input file ***/

   imfile = fopen( fname , "r" ) ;
   if( imfile == NULL ){
      fprintf(stderr,"couldn't open file %s in mri_read_ppm3\n",fname); return NULL ;
   }

   /*** check if a raw PPM file ***/

   ch = getc( imfile ) ; if( ch != 'P' ) { fclose(imfile) ; return NULL ; }
   ch = getc( imfile ) ; if( ch != '6' ) { fclose(imfile) ; return NULL ; }

   /* magic P6 found, so read numbers in header */

   ch = getc(imfile) ;

   NUMSCAN(nx)     ; if( nx     <= 0 )   { fclose(imfile) ; return NULL ; }
   NUMSCAN(ny)     ; if( ny     <= 0 )   { fclose(imfile) ; return NULL ; }
   NUMSCAN(maxval) ; if( maxval <= 0 ||
                         maxval >  255 ) { fclose(imfile) ; return NULL ; }

   /*** create output images and workspace array ***/

   rim = mri_new( nx , ny , MRI_byte ) ; rby = mri_data_pointer( rim ) ;
   gim = mri_new( nx , ny , MRI_byte ) ; gby = mri_data_pointer( gim ) ;
   bim = mri_new( nx , ny , MRI_byte ) ; bby = mri_data_pointer( bim ) ;

   sprintf(buf,"%s#R",fname) ; mri_add_name( buf , rim ) ;
   sprintf(buf,"%s#G",fname) ; mri_add_name( buf , gim ) ;
   sprintf(buf,"%s#B",fname) ; mri_add_name( buf , bim ) ;

   rgby = (byte *) malloc( sizeof(byte) * 3*nx*ny ) ;
   if( rgby == NULL ){
      fprintf(stderr,"couldn't malloc workspace in mri_read_ppm3!\n") ; exit(-1) ;
   }

   /*** read all data into workspace array ***/

   length = fread( rgby , sizeof(byte) , 3*nx*ny , imfile ) ;
   fclose( imfile ) ;

   if( length != 3*nx*ny ){
      free(rgby) ; mri_free(rim) ; mri_free(gim) ; mri_free(bim) ;
      fprintf(stderr,"couldn't read data from file %s in mri_read_ppm3\n",fname) ;
      return NULL ;
   }

   /*** put data from workspace array into output images ***/

   npix = nx*ny ;
   for( ii=0 ; ii < npix ; ii++ ){
      rby[ii] = rgby[3*ii  ] ;
      gby[ii] = rgby[3*ii+1] ;
      bby[ii] = rgby[3*ii+2] ;
   }
   free( rgby ) ;

   /*** create output image array ***/

   INIT_IMARR(outar) ;
   ADDTO_IMARR( outar , rim ) ;
   ADDTO_IMARR( outar , gim ) ;
   ADDTO_IMARR( outar , bim ) ;
   return outar ;
}

/*-----------------------------------------------------------------
   routines added 1 Oct 1995
-------------------------------------------------------------------*/

MRI_IMAGE *mri_read_nsize( char * fname )
{
   MRI_IMARR *imar ;
   MRI_IMAGE *imout ;

   imar = mri_read_file( fname ) ;
   if( imar == NULL ) return NULL ;
   if( imar->num != 1 ){ DESTROY_IMARR(imar) ; return NULL ; }

   imout = mri_nsize( IMAGE_IN_IMARR(imar,0) ) ;
   mri_add_name( IMAGE_IN_IMARR(imar,0)->name , imout ) ;

   DESTROY_IMARR(imar) ;
   return imout ;
}

MRI_IMARR *mri_read_many_nsize( int nf , char * fn[] )
{
   MRI_IMARR * newar , * outar ;
   MRI_IMAGE * im ;
   int ii ;

   newar = mri_read_many_files( nf , fn ) ;
   if( newar == NULL ) return NULL ;

   INIT_IMARR(outar) ;
   for( ii=0 ; ii < newar->num ; ii++ ){
      im = mri_nsize( IMAGE_IN_IMARR(newar,ii) ) ;
      mri_add_name( IMAGE_IN_IMARR(newar,ii)->name , im ) ;
      ADDTO_IMARR(outar,im) ;
      mri_free( IMAGE_IN_IMARR(newar,ii) ) ;
   }
   FREE_IMARR(newar) ;
   return outar ;
}

/*------------------------------------------------------------------------
  Set up MCW_SIZE_# database for input (added 07-Nov-95)
--------------------------------------------------------------------------*/

void init_MCW_sizes(void)
{
   int num , count ;
   char ename[32] ;
   char * str ;

   if( MCW_imsize_good >= 0 ) return ;

   MCW_imsize_good = 0 ;

   for( num=0 ; num < MAX_MCW_IMSIZE ; num++ ){ /* look for environment string */

      imsize[num].size = -1 ;

      /* try to find environment variable with the num-th name */

      sprintf( ename , "AFNI_IMSIZE_%d" , num+1 ) ;
      str = my_getenv( ename ) ;

      if( str == NULL ){
         sprintf( ename , "MCW_IMSIZE_%d" , num+1 ) ;
         str = my_getenv( ename ) ;
         if( str == NULL ) continue ;
      }

      imsize[num].prefix = (char *) malloc( sizeof(char) * strlen(str) ) ;
      if( imsize[num].prefix == NULL ){
         fprintf(stderr,"\n*** Can't malloc in init_MCW_sizes! ***\a\n");
         exit(1) ;
      }

      if( str[0] != '%' ){  /* e.g., 16096=3D:-1:0:64:64:1: */

         imsize[num].head = -1 ;
         count = sscanf( str , "%d=%s" , &(imsize[num].size) , imsize[num].prefix ) ;
         if( count != 2 || imsize[num].size < 2 || strlen(imsize[num].prefix) < 2 ){
            free( imsize[num].prefix ) ;
            fprintf(stderr,"bad environment %s = %s\n" ,
                    ename , str ) ;
         }

      } else {              /* e.g., %16096+0=3D:0:7904:64:64: */

         count = sscanf( str+1 , "%d+%d=%s" ,
                         &(imsize[num].size) , &(imsize[num].head) , imsize[num].prefix ) ;

         if( count != 3 || imsize[num].size < 2 ||
             imsize[num].head < 0 || strlen(imsize[num].prefix) < 2 ){

            free( imsize[num].prefix ) ;
            fprintf(stderr,"bad environment %s = %s\n" ,
                    ename , str ) ;
         }
      }

      MCW_imsize_good ++ ;
   }

   return ;
}

/*------------------------------------------------------------------------------
  Check if a filesize fits an MCW_IMSIZE setup.
  Return new "filename" with 3D header attached if so.
  If not, return a copy of the filename.  In any case the
  returned string should be free-d when it is no longer needed.
--------------------------------------------------------------------------------*/

char * my_strdup( char * str )
{
   char * new_str ;
   if( str == NULL ) return NULL ;
   new_str = (char *) malloc( sizeof(char) * (strlen(str)+1) ) ;
   if( new_str != NULL ) strcpy( new_str , str ) ;
   return new_str ;
}

char * imsized_fname( char * fname )
{
   int num , lll ;
   long len ;
   char * new_name ;

   init_MCW_sizes() ;
   if( MCW_imsize_good == 0 ){
      new_name = my_strdup(fname) ;  /* nothing to fit */
      return new_name ;              /* --> return copy of old name */
   }

   len = mri_filesize( fname ) ;
   if( len <= 0 ){
      new_name = my_strdup(fname) ;  /* not an existing filename */
      return new_name ;              /* --> return copy of old name */
   }

   for( num=0 ; num < MAX_MCW_IMSIZE ; num++ ){     /* check each possibility */

      if( imsize[num].size <= 0 ) continue ;        /* skip to next one */

      if( imsize[num].head < 0 && len == imsize[num].size ){  /* fixed size fit */

         lll = strlen(fname) + strlen(imsize[num].prefix) + 4 ;
         new_name = (char *) malloc( sizeof(char) * lll ) ;
         if( new_name == NULL ){
            fprintf(stderr,"\n*** Can't malloc in imsized_fname! ***\a\n");
            exit(1) ;
         }
         sprintf( new_name , "%s%s" , imsize[num].prefix , fname ) ;
         return new_name ;

      } else if( (len-imsize[num].head) % imsize[num].size == 0 ){
         int count = (len-imsize[num].head) / imsize[num].size ;

         if( count < 1 ) continue ;  /* skip to next one */

         lll = strlen(fname) + strlen(imsize[num].prefix) + 32 ;
         new_name = (char *) malloc( sizeof(char) * lll ) ;
         if( new_name == NULL ){
            fprintf(stderr,"\n*** Can't malloc in imsized_fname! ***\a\n");
            exit(1) ;
         }
         sprintf( new_name , "%s%d:%s" , imsize[num].prefix , count , fname ) ;
         return new_name ;
      }

   }

   new_name = my_strdup(fname) ;  /* no fit --> return copy of old name */
   return new_name ;
}

#include <sys/stat.h>

long mri_filesize( char * pathname )
{
   static struct stat buf ;
   int ii ;

   if( pathname == NULL ) return -1 ;
   ii = stat( pathname , &buf ) ; if( ii != 0 ) return -1 ;
   return buf.st_size ;
}

/*---------------------------------------------------------------
  May 13, 1996: reads a raw PPM file into 1 image
-----------------------------------------------------------------*/

MRI_IMAGE * mri_read_ppm( char * fname )
{
   int ch , nch , nx,ny,maxval , length ;
   MRI_IMAGE * rgbim ;
   FILE      * imfile ;
   byte      * rgby ;
   char        buf[256] ;

WHOAMI ;

   /*** open input file ***/

   imfile = fopen( fname , "r" ) ;
   if( imfile == NULL ) return NULL ;

   /*** check if a raw PPM file ***/

   ch = getc( imfile ) ; if( ch != 'P' ) { fclose(imfile) ; return NULL ; }
   ch = getc( imfile ) ; if( ch != '6' ) { fclose(imfile) ; return NULL ; }

   /* magic P6 found, so read numbers in header */

   ch = getc(imfile) ;

   NUMSCAN(nx)     ; if( nx     <= 0 )   { fclose(imfile) ; return NULL ; }
   NUMSCAN(ny)     ; if( ny     <= 0 )   { fclose(imfile) ; return NULL ; }
   NUMSCAN(maxval) ; if( maxval <= 0 ||
                         maxval >  255 ) { fclose(imfile) ; return NULL ; }

   /*** create output image ***/

   rgbim = mri_new( nx , ny , MRI_rgb ) ; mri_add_name( fname , rgbim ) ;
   rgby  = MRI_RGB_PTR(rgbim) ;

   /*** read all data into image array */

   length = fread( rgby , sizeof(byte) , 3*nx*ny , imfile ) ;
   fclose( imfile ) ;

   if( length != 3*nx*ny ){ mri_free(rgbim) ; return NULL ; }

   return rgbim ;
}

/*---------------------------------------------------------------
  June 1996: read a 1D or 2D file formatted in ASCII;
             will always return in MRI_float format.
-----------------------------------------------------------------*/

#define INC_TSARSIZE 128
#define LBUF         32768

MRI_IMAGE * mri_read_ascii( char * fname )
{
   MRI_IMAGE * outim ;
   int ii,jj,val , used_tsar , alloc_tsar ;
   float * tsar ;
   float ftemp ;
   FILE * fts ;
   char buf[LBUF] ;
   char * ptr ;
   int  ncol , bpos , blen , nrow ;

   if( fname == NULL || fname[0] == '\0' ) return NULL ;

   fts = fopen( fname , "r" ) ; if( fts == NULL ) return NULL ;

   /** step 1: read in the first line and see how many numbers are in it **/

   ptr = fgets( buf , LBUF , fts ) ;
   if( ptr == NULL ){ fclose( fts ) ; return NULL ; }  /* bad read? */

   blen = strlen(buf) ;
   bpos = 0 ;
   ncol = 0 ;
   do{
       for( ; bpos < blen && (isspace(buf[bpos])||buf[bpos]==',') ; bpos++ ) ; /* nada */
       if( bpos == blen ) break ;
       ii = sscanf( buf+bpos , "%f%n" , &ftemp , &jj ) ;
       if( ii < 1 ){ ncol = 0 ; break ; }           /* bad scan? */
       ncol++ ; bpos += jj ;
   } while( bpos < blen ) ;
   if( ncol == 0 ){ fclose( fts ) ; return NULL ; } /* couldn't read? */

   /** At this point, ncol is the number of floats to be read from each line **/

   rewind( fts ) ;  /* will start over */

   used_tsar  = 0 ;
   alloc_tsar = INC_TSARSIZE ;
   tsar       = (float *) malloc( sizeof(float) * alloc_tsar ) ;
   if( tsar == NULL ){
      fprintf(stderr,"\n*** malloc error in mri_read_ascii ***\n") ; exit(1) ;
   }

   /** read lines, convert to floats, store **/

   nrow = 0 ;
   while( 1 ){
      ptr = fgets( buf , LBUF , fts ) ;  /* read */
      if( ptr == NULL ) break ;          /* failure --> end of data */
      blen = strlen(buf) ;
      if( blen <= 0 ) break ;            /* nothing --> end of data */

      /* convert commas to blanks */

      for( ii=0 ; ii < blen ; ii++ ) if( buf[ii] == ',' ) buf[ii] = ' ' ;

      for( ii=0,bpos=0 ; ii < ncol && bpos < blen ; ii++ ){
         val = sscanf( buf+bpos , "%f%n" , &ftemp , &jj ) ;  /* read from string */
         if( val < 1 ) break ;                               /* bad read? */
         bpos += jj ;                                        /* start of next read */

         if( used_tsar == alloc_tsar ){
            alloc_tsar += INC_TSARSIZE ;
            tsar        = (float *)realloc( tsar,sizeof(float)*alloc_tsar );
            if( tsar == NULL ){
               fprintf(stderr,"\n*** realloc error in mri_read_ascii ***\n") ; exit(1) ;
            }
         }

         tsar[used_tsar++] = ftemp ;  /* store input */
      }

      if( ii != ncol ) break ;  /* didn't get all of them? */

      nrow++ ;                  /* got one more complete row! */
   }
   fclose( fts ) ; /* finished with this file! */

   if( used_tsar <= 1 ){ free(tsar) ; return NULL ; }

   tsar = (float *) realloc( tsar , sizeof(float) * used_tsar ) ;
   if( tsar == NULL ){
      fprintf(stderr,"\n*** final realloc error in mri_read_ascii ***\n") ; exit(1) ;
   }

   outim = mri_new_vol_empty( ncol , nrow , 1 , MRI_float ) ;
   mri_fix_data_pointer( tsar , outim ) ;
   mri_add_name( fname , outim ) ;

   return outim ;
}

/*---------------------------------------------------------------------------
  16 Nov 1999: read an ASCII file as columns, transpose to rows,
               and allow column selectors.
-----------------------------------------------------------------------------*/

MRI_IMAGE * mri_read_1D( char * fname )
{
   MRI_IMAGE * inim , * outim , * flim ;
   char dname[256] , subv[256] , *cpt ;
   int ii,nx,ny,nts , *ivlist , *ivl ;
   float * far , * oar ;

   if( fname == NULL || fname[0] == '\0' || strlen(fname) > 255 ) return NULL ;

   /*-- split filename and subvector list --*/

   cpt = strstr(fname,"[") ;

   if( cpt == NULL ){                   /* no subvector list */
      strcpy( dname , fname ) ;
      subv[0] = '\0' ;
   } else if( cpt == fname ){           /* can't be at start of filename! */
      fprintf(stderr,"*** Illegal filename in mri_read_1D: %s\n",fname) ;
      return NULL ;
   } else {                             /* got a subvector list */
      ii = cpt - fname ;
      memcpy(dname,fname,ii) ; dname[ii] = '\0' ;
      strcpy(subv,cpt) ;
   }

   /*-- read file in --*/

   inim = mri_read_ascii(dname) ;
   if( inim == NULL ) return NULL ;
   flim = mri_transpose(inim) ; mri_free(inim) ;
   if( subv[0] == '\0' ) return flim ;             /* no subvector => am done */

   /*-- get the subvector list --*/

   nx = flim->nx ;
   ny = flim->ny ;

   ivlist = MCW_get_intlist( ny , subv ) ;         /* in thd_intlist.c */
   if( ivlist == NULL || ivlist[0] < 1 ){
      fprintf(stderr,"*** Illegal subvector list in mri_read_1D: %s\n",fname) ;
      if( ivlist != NULL ) free(ivlist) ;
      mri_free(flim) ; return NULL ;
   }

   nts = ivlist[0] ;                          /* number of subvectors */
   ivl = ivlist + 1 ;                         /* start of array of subvectors */

   for( ii=0 ; ii < nts ; ii++ ){            /* check them out */
      if( ivl[ii] < 0 || ivl[ii] >= ny ){
         fprintf(stderr,"*** Out-of-range subvector list in mri_read_1D: %s\n",fname) ;
         mri_free(flim) ; free(ivlist) ; return NULL ;
      }
   }

   outim = mri_new( nx , nts , MRI_float ) ;   /* make output image */
   far   = MRI_FLOAT_PTR( flim ) ;
   oar   = MRI_FLOAT_PTR( outim ) ;

   for( ii=0 ; ii < nts ; ii++ )               /* copy desired rows */
      memcpy( oar + ii*nx , far + ivl[ii]*nx , sizeof(float)*nx ) ;

   mri_free(flim) ; free(ivlist) ; return outim ;
}

/*---------------------------------------------------------------------------
  Read in an ASCII file to a float array.
-----------------------------------------------------------------------------*/

static void read_ascii_floats( char * fname, int * nff , float ** ff )
{
   int ii,jj,val , used_tsar , alloc_tsar ;
   float * tsar ;
   float ftemp ;
   FILE * fts ;
   char buf[LBUF] ;
   char * ptr ;
   int  bpos , blen , nrow ;

   /* check inputs */

   if( nff == NULL || ff == NULL ) return ;
   if( fname == NULL || fname[0] == '\0' ){ *nff=0 ; *ff=NULL ; return ; }

   fts = fopen( fname , "r" ) ;
   if( fts == NULL ){ *nff=0 ; *ff=NULL ; return ; }

   /* make some space */

   used_tsar  = 0 ;
   alloc_tsar = INC_TSARSIZE ;
   tsar       = (float *) malloc( sizeof(float) * alloc_tsar ) ;
   if( tsar == NULL ){
      fprintf(stderr,"\n*** malloc fails: read_ascii_floats ***\n"); exit(1);
   }

   /** read lines, convert to floats, store **/

   nrow = 0 ;
   while( 1 ){
      ptr = fgets( buf , LBUF , fts ) ;  /* read */
      if( ptr == NULL ) break ;          /* failure --> end of data */
      blen = strlen(buf) ;
      if( blen <= 0 ) break ;            /* nothing --> end of data */

      for( ii=0 ; ii < blen && isspace(buf[ii]) ; ii++ ) ; /* skip blanks */

      if( ii      == blen ) continue ;    /* skip all blank line */
      if( buf[ii] == '#'  ) continue ;    /* skip a comment line */
      if( buf[ii] == '!'  ) continue ;

      /* convert commas to blanks */

      for( jj=ii ; jj < blen ; jj++ ) if( buf[jj] == ',' ) buf[jj] = ' ' ;

      for( bpos=ii ; bpos < blen ; ){
         val = sscanf( buf+bpos , "%f%n" , &ftemp , &jj ) ;  /* read from string */
         if( val < 1 ) break ;                               /* bad read? */
         bpos += jj ;                                        /* start of next read */

         if( used_tsar == alloc_tsar ){
            alloc_tsar += INC_TSARSIZE ;
            tsar        = (float *)realloc( tsar,sizeof(float)*alloc_tsar );
            if( tsar == NULL ){
               fprintf(stderr,"\n*** realloc fails: read_ascii_floats ***\n"); exit(1);
            }
         }

         tsar[used_tsar++] = ftemp ;  /* store input */
      }

      nrow++ ;                  /* got one more complete row! */
   }
   fclose( fts ) ; /* finished with this file! */

   if( used_tsar <= 1 ){ free(tsar) ; *nff=0 ; *ff=NULL ; return ; }

   tsar = (float *) realloc( tsar , sizeof(float) * used_tsar ) ;
   if( tsar == NULL ){
      fprintf(stderr,"\n*** final realloc fails: read_ascii_floats ***\n"); exit(1);
   }

   *nff = used_tsar ; *ff  = tsar ; return ;
}

/*--------------------------------------------------------------
   Read a pile of images from one ASCII file.
   Adapted from mri_read_3D - Feb 2000 - RWCox.
   [N.B.: if this routine is altered, don't forget mri_imcount!]
----------------------------------------------------------------*/

MRI_IMARR * mri_read_3A( char * tname )
{
   int nx , ny , nz , ii , nxyz,nxy , nff ;
   int ngood , length , kim , datum_type ;
   char fname[256]="\0" , buf[512] ;
   MRI_IMARR * newar ;
   MRI_IMAGE * newim , * flim ;
   float * ff ;

   /*** get info from 3A tname ***/

   if( tname == NULL || strlen(tname) < 10 ) return NULL ;

   switch( tname[2] ){  /* allow for 3As:, 3Ab:, 3Af: */

      default: ngood = 0 ; break ;

      case 's':
         ngood = sscanf( tname, "3As:%d:%d:%d:%s", &nx, &ny, &nz, fname ) ;
         datum_type = MRI_short ;
         break ;

      case 'b':
         ngood = sscanf( tname, "3Ab:%d:%d:%d:%s", &nx, &ny, &nz, fname ) ;
         datum_type = MRI_byte ;
         break ;

      case 'f':
         ngood = sscanf( tname, "3Af:%d:%d:%d:%s", &nx, &ny, &nz, fname ) ;
         datum_type = MRI_float ;
         break ;
   }

   if( ngood < 4 || nx <= 0 || ny <= 0 || nz <= 0 || strlen(fname) <= 0 ) return NULL ;

   /* read the input file */

   read_ascii_floats( fname , &nff , &ff ) ;

   if( nff <= 0 || ff == NULL ) return NULL ;

   nxy = nx*ny ; nxyz = nxy*nz ;

   if( nff < nxyz ){
      fprintf(stderr,
                "\n** WARNING: %s is too short - padding with %d zeros\n",
                tname,nxyz-nff) ;
      ff = (float *) realloc( ff , sizeof(float) * nxyz ) ;
      for( ii=nff ; ii < nxyz ; ii++ ) ff[ii] = 0.0 ;
      nff = nxyz ;
   } else if( nff > nxyz ){
      fprintf(stderr,
                "\n** WARNING: %s is too long - truncating off last %d values\n",
                tname,nff-nxyz) ;
   }

   /* put the input data into MRI_IMAGEs */

   INIT_IMARR(newar) ;

   for( kim=0 ; kim < nz ; kim++ ){
      flim = mri_new( nx,ny , MRI_float ) ;
      memcpy( MRI_FLOAT_PTR(flim) , ff+nxy*kim , sizeof(float)*nxy ) ;
      switch( datum_type ){
         case MRI_float: newim = flim                                           ; break ;
         case MRI_short: newim = mri_to_short(1.0,flim)        ; mri_free(flim) ; break ;
         case MRI_byte:  newim = mri_to_byte_scl(1.0,0.0,flim) ; mri_free(flim) ; break ;
      }

      if( nz == 1 ) mri_add_name( fname , newim ) ;
      else {
         sprintf( buf , "%s#%d" , fname,kim ) ;
         mri_add_name( buf , newim ) ;
      }

      ADDTO_IMARR(newar,newim) ;
   }

   free(ff) ; return newar ;
}

/*---------------------------------------------------------------------------
   Stuff to read a file in "delay" mode -- 01 Jan 1997.
-----------------------------------------------------------------------------*/

#ifdef USE_MRI_DELAY

/**** If possible, throw the data away for later retrieval from disk ****/

void mri_purge_delay( MRI_IMAGE * im )
{
   void * ar ;

   /** if no delay filename,
       or if it is marked as already set for delay input, do nothing **/

   if( im->fname == NULL ||
       (im->fondisk & INPUT_DELAY) != 0 ) return ;

   /** get the data pointer, throw data way, clear the data pointer **/

   ar = mri_data_pointer( im ) ;
   if( ar != NULL ){ free(ar) ; mri_clear_data_pointer(im) ; }

   /** mark as set for delay input **/

   im->fondisk |= INPUT_DELAY ;
   return ;
}

/**** if possible, read data from delay input file ****/

void mri_input_delay( MRI_IMAGE * im )
{
   FILE * imfile ;
   void * imar ;

   /** if no delay input file,
       or is marked as already read in, do nothing **/

   if( im->fname == NULL ||
       (im->fondisk & INPUT_DELAY) == 0 ) return ;

   /** open the delay input file **/

   imfile = fopen( im->fname , "r" ) ;
   if( imfile == NULL ){
      fprintf( stderr , "couldn't open file %s\n" , im->fname ) ;
      return ;
   }

   /** make space for the array **/

   imar = (void *) malloc( im->nvox * im->pixel_size ) ;
   if( imar == NULL ){
      fprintf( stderr ,
               "malloc fails for delay image from file %s\n" , im->fname ) ;
      fclose( imfile ) ;
      return ;
   }
   mri_fix_data_pointer( imar , im ) ;

   /** read from the file into the array **/

   fseek( imfile , im->foffset , SEEK_SET ) ;
   fread( imar , im->pixel_size , im->nvox , imfile ) ;
   fclose( imfile ) ;

   /** swap bytes, if so marked **/

   if( (im->fondisk & BSWAP_DELAY) ) mri_swapbytes( im ) ;

   /** mark as already read from disk **/

   im->fondisk ^= INPUT_DELAY ;

#if 0
fprintf(stderr,"delayed input from file %s at offset %d\n",im->fname,im->foffset);
#endif
   return ;
}

/**** like mri_read_file, but returns delayed images for 3D: files ****/

MRI_IMARR * mri_read_file_delay( char * fname )
{
   MRI_IMARR * newar ;
   MRI_IMAGE * newim ;
   char * new_fname ;

   new_fname = imsized_fname( fname ) ;
   if( new_fname == NULL ) return NULL ;

   if( strlen(new_fname) > 9 && new_fname[0] == '3' && new_fname[1] == 'D' ){

      newar = mri_read_3D_delay( new_fname ) ;   /* read from a 3D file */

   } else if( strlen(new_fname) > 9 &&
              new_fname[0] == '3' && new_fname[1] == 'A' && new_fname[3] == ':' ){

      newar = mri_read_3A( new_fname ) ;

   } else {
      newim = mri_read( new_fname ) ;      /* read from a 2D file */
      if( newim == NULL ){ free(new_fname) ; return NULL ; }
      INIT_IMARR(newar) ;
      ADDTO_IMARR(newar,newim) ;
   }
   free(new_fname) ;
   return newar ;
}

/**** like mri_read_3D, but returns delayed images ****/

MRI_IMARR * mri_read_3D_delay( char * tname )
{
   int hglobal , himage , nx , ny , nz ;
   char fname[256] , buf[512] ;
   int ngood , length , kim , datum_type , datum_len , swap ;
   MRI_IMARR * newar ;
   MRI_IMAGE * newim ;
   FILE      * imfile ;

   /*** get info from 3D tname ***/

   if( tname == NULL || strlen(tname) < 10 ) return NULL ;

   switch( tname[2] ){  /* allow for 3D: or 3Ds: or 3Db: */

      default:
      case ':':
         ngood = sscanf( tname , "3D:%d:%d:%d:%d:%d:%s" ,
                         &hglobal , &himage , &nx , &ny , &nz , fname ) ;

         swap       = 0 ;
         datum_type = MRI_short ;
         datum_len  = sizeof(short) ;  /* better be 2 */
         break ;

      case 's':
         ngood = sscanf( tname , "3Ds:%d:%d:%d:%d:%d:%s" ,
                         &hglobal , &himage , &nx , &ny , &nz , fname ) ;

         swap       = 1 ;
         datum_type = MRI_short ;
         datum_len  = sizeof(short) ;  /* better be 2 */
         break ;

      case 'b':
         ngood = sscanf( tname , "3Db:%d:%d:%d:%d:%d:%s" ,
                         &hglobal , &himage , &nx , &ny , &nz , fname ) ;

         swap       = 0 ;
         datum_type = MRI_byte ;
         datum_len  = sizeof(byte) ;  /* better be 1 */
         break ;

      case 'f':
         ngood = sscanf( tname , "3Df:%d:%d:%d:%d:%d:%s" ,
                         &hglobal , &himage , &nx , &ny , &nz , fname ) ;

         swap       = 0 ;
         datum_type = MRI_float ;
         datum_len  = sizeof(float) ;  /* better be 4 */
         break ;

      case 'i':
         ngood = sscanf( tname , "3Di:%d:%d:%d:%d:%d:%s" ,
                         &hglobal , &himage , &nx , &ny , &nz , fname ) ;

         swap       = 0 ;
         datum_type = MRI_int ;
         datum_len  = sizeof(int) ;  /* better be 4 */
         break ;

      case 'c':
         ngood = sscanf( tname , "3Dc:%d:%d:%d:%d:%d:%s" ,
                         &hglobal , &himage , &nx , &ny , &nz , fname ) ;

         swap       = 0 ;
         datum_type = MRI_complex ;
         datum_len  = sizeof(complex) ;  /* better be 8 */
         break ;
   }

   if( ngood < 6 || himage < 0 ||
       nx <= 0   || ny <= 0    || nz <= 0 ||
       strlen(fname) <= 0                   ) return NULL ;   /* bad info */

   /*** open the input file and position it ***/

   imfile = fopen( fname , "r" ) ;
   if( imfile == NULL ){
      fprintf( stderr , "couldn't open file %s\n" , fname ) ;
      return NULL ;
   }

   fseek( imfile , 0L , SEEK_END ) ;  /* get the length of the file */
   length = ftell( imfile ) ;

   /** 13 Apr 1999: modified to allow actual hglobal < -1
                    as long as hglobal+himage >= 0       **/

#if 0                 /* the old code */
   if( hglobal < 0 ){
      hglobal = length - nz*(datum_len*nx*ny+himage) ;
      if( hglobal < 0 ) hglobal = 0 ;
   }
#else                 /* 13 Apr 1999 */
   if( hglobal == -1 || hglobal+himage < 0 ){
      hglobal = length - nz*(datum_len*nx*ny+himage) ;
      if( hglobal < 0 ) hglobal = 0 ;
   }
#endif

   ngood = hglobal + nz*(datum_len*nx*ny+himage) ;
   if( length < ngood ){
      fprintf( stderr,
        "file %s is %d bytes long but must be at least %d bytes long\n"
        "for hglobal=%d himage=%d nx=%d ny=%d nz=%d and voxel=%d bytes\n",
        fname,length,ngood,hglobal,himage,nx,ny,nz,datum_len ) ;
      fclose( imfile ) ;
      return NULL ;
   }
   fclose( imfile ) ;

   /*** put pointers to data in the file into the images ***/

   INIT_IMARR(newar) ;

   for( kim=0 ; kim < nz ; kim++ ){
      newim = mri_new_vol_empty( nx,ny,1 , datum_type ) ;  /* empty image */
      mri_add_fname_delay( fname , newim ) ;               /* put filename in */
      newim->fondisk = (swap) ? (INPUT_DELAY | BSWAP_DELAY) /* mark read type */
                              : (INPUT_DELAY) ;
      newim->foffset = hglobal + (kim+1)*himage + datum_len*nx*ny*kim ;

      if( nz == 1 ) mri_add_name( fname , newim ) ;
      else {
         sprintf( buf , "%s#%d" , fname,kim ) ;
         mri_add_name( buf , newim ) ;
      }

      ADDTO_IMARR(newar,newim) ;
   }

   return newar ;
}
#endif /* USE_MRI_DELAY */
